#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <curl/curl.h>
#include <time.h>
#include <windows.h>

#define DEFAULT_PORT 8080
#define DEFAULT_HOST "127.0.0.1"
#define AUTH_KEY_FILE "auth.key"
#define SERVICE_NAME "OcribAgentService"

SERVICE_STATUS        g_ServiceStatus = {0};
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
VOID WINAPI ServiceCtrlHandler(DWORD request);
DWORD WINAPI AgentThread(LPVOID lpParam);

char server_host[256] = DEFAULT_HOST;
int server_port = DEFAULT_PORT;

size_t write_chunk_to_disk(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fwrite(ptr, size, nmemb, stream);
}

void send_desktop_notification(const char *filename)
{
    char command[2048];
    // Uses PowerShell to pop up a message box on Windows
    snprintf(command, sizeof(command), 
        "powershell -WindowStyle Hidden -Command \"Add-Type -AssemblyName System.Windows.Forms; [System.Windows.Forms.MessageBox]::Show('Download Complete: %s', 'Ocrib Agent')\"", 
        filename);
    system(command);
}

void trim_newline(char *s) {
    while(*s) {
        if(*s == '\n' || *s == '\r') {
            *s = '\0';
            break;
        }
        s++;
    }
}

int main(int argc, char *argv[])
{
    // If command line arguments are provided, set host/port and run in interactive console mode
    if (argc > 1) {
        strncpy(server_host, argv[1], sizeof(server_host) - 1);
        if (argc > 2) server_port = atoi(argv[2]);
        
        printf("Running Ocrib Agent in Console Mode...\n");
        AgentThread(NULL);
        return 0;
    }

    // Otherwise, run as a Windows Service (the standard installer launch mode)
    SERVICE_TABLE_ENTRY ServiceTable[] = 
    {
        {SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        {NULL, NULL}
    };

    StartServiceCtrlDispatcher(ServiceTable);
    return 0;
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
    g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);

    if (g_StatusHandle == NULL)
    {
        return;
    }

    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;
    g_ServiceStatus.dwWaitHint = 0;

    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == INVALID_HANDLE_VALUE)
    {
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        return;
    }

    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    // Launch worker thread
    HANDLE hThread = CreateThread(NULL, 0, AgentThread, NULL, 0, NULL);
    
    // Wait until the service is stopped
    WaitForSingleObject(g_ServiceStopEvent, INFINITE);

    // Clean up thread
    if (hThread) {
        TerminateThread(hThread, 0);
        CloseHandle(hThread);
    }

    CloseHandle(g_ServiceStopEvent);

    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = 0;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

VOID WINAPI ServiceCtrlHandler(DWORD request)
{
    switch (request)
    {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            g_ServiceStatus.dwWin32ExitCode = 0;
            g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

            SetEvent(g_ServiceStopEvent);
            break;

        default:
            break;
    }
}

DWORD WINAPI AgentThread(LPVOID lpParam)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        return 1;
    }

    char token[128] = {0};
    int is_paired = 0;

    // Determine config path (System service runs in System32 by default, so we look for C:\ProgramData\Ocrib)
    char app_data_dir[MAX_PATH];
    char key_filepath[MAX_PATH];
    if (GetEnvironmentVariable("ProgramData", app_data_dir, MAX_PATH) > 0)
    {
        snprintf(key_filepath, sizeof(key_filepath), "%s\\Ocrib", app_data_dir);
        CreateDirectory(key_filepath, NULL); // Ensure directory exists
        snprintf(key_filepath, sizeof(key_filepath), "%s\\Ocrib\\%s", app_data_dir, AUTH_KEY_FILE);
    }
    else
    {
        snprintf(key_filepath, sizeof(key_filepath), "%s", AUTH_KEY_FILE);
    }

    FILE *token_file = fopen(key_filepath, "r");
    if (token_file)
    {
        if (fgets(token, sizeof(token), token_file)) {
            trim_newline(token);
            if (strlen(token) > 0) is_paired = 1;
        }
        fclose(token_file);
    }

    while (1)
    {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) { Sleep(5000); continue; }

        struct addrinfo hints, *result = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", server_port);
        if (getaddrinfo(server_host, port_str, &hints, &result) != 0)
        {
            closesocket(sock);
            Sleep(5000);
            continue;
        }

        if (connect(sock, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR)
        {
            freeaddrinfo(result);
            closesocket(sock);
            Sleep(5000);
            continue;
        }
        freeaddrinfo(result);

        char buffer[2048] = {0};

        if (!is_paired)
        {
            srand((unsigned int)time(NULL));
            int pin = 100000 + rand() % 900000;
            
            char computer_name[MAX_COMPUTERNAME_LENGTH + 1];
            DWORD size = sizeof(computer_name);
            if (!GetComputerName(computer_name, &size)) {
                strcpy(computer_name, "WindowsDevice");
            }

            char pin_msg[512];
            snprintf(pin_msg, sizeof(pin_msg), "PIN:%d|NAME:%s", pin, computer_name);
            send(sock, pin_msg, (int)strlen(pin_msg), 0);

            int bytes = recv(sock, buffer, 2048, 0);
            if (bytes > 0 && strncmp(buffer, "VERIFIED:", 9) == 0)
            {
                // To prevent stack overflow, check length before copying
                int token_len = bytes - 9;
                if (token_len < 128) {
                    strncpy(token, buffer + 9, token_len);
                    token[token_len] = '\0';
                    trim_newline(token);
                } else {
                    closesocket(sock);
                    Sleep(2000);
                    continue;
                }

                FILE *new_file = fopen(key_filepath, "w");
                if (new_file) { fprintf(new_file, "%s", token); fclose(new_file); }
                is_paired = 1;
            }
            else { closesocket(sock); Sleep(2000); continue; }
        }
        else
        {
            char auth_msg[256];
            snprintf(auth_msg, sizeof(auth_msg), "TOKEN:%s", token);
            send(sock, auth_msg, (int)strlen(auth_msg), 0);
        }

        while (1)
        {
            memset(buffer, 0, 2048);
            int bytes = recv(sock, buffer, 2048, 0);
            if (bytes <= 0) break; 

            // Safely parse URL filename
            char *filename = strrchr(buffer, '/');
            filename = (filename != NULL) ? filename + 1 : "downloaded_file";
            char *q = strchr(filename, '?'); if (q) *q = '\0';

            // Establish local download path (Windows Downloads directory or local app directory)
            char download_path[MAX_PATH];
            char user_profile[MAX_PATH];
            if (GetEnvironmentVariable("USERPROFILE", user_profile, MAX_PATH) > 0)
            {
                snprintf(download_path, sizeof(download_path), "%s\\Downloads\\%s", user_profile, filename);
            }
            else
            {
                snprintf(download_path, sizeof(download_path), "%s", filename);
            }

            CURL *curl = curl_easy_init();
            if (curl)
            {
                FILE *fp = fopen(download_path, "wb");
                if (fp) {
                    curl_easy_setopt(curl, CURLOPT_URL, buffer);
                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_chunk_to_disk);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
                    if (curl_easy_perform(curl) == CURLE_OK) {
                        send_desktop_notification(filename);
                    }
                    fclose(fp);
                }
                curl_easy_cleanup(curl);
            }
        }
        closesocket(sock);
        Sleep(5000);
    }

    WSACleanup();
    return 0;
}
