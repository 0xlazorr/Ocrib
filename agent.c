#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <time.h>
#include <netdb.h>
#include <pwd.h>

#define DEFAULT_PORT 8080
#define DEFAULT_HOST "127.0.0.1"
#define AUTH_KEY_FILE "auth.key"

size_t write_chunk_to_disk(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fwrite(ptr, size, nmemb, stream);
}

void send_desktop_notification(const char *filename)
{
    char command[2048];
#ifdef __APPLE__
    snprintf(command, sizeof(command), "osascript -e 'display notification \"Download Complete: %s\" with title \"Ocrib Agent\"'", filename);
    system(command);
#else
    if (system("which notify-send > /dev/null 2>&1") == 0) {
        snprintf(command, sizeof(command), "notify-send 'Ocrib Agent' 'Download Complete: %s'", filename);
        system(command);
    }
#endif
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
    char *server_host = DEFAULT_HOST;
    int server_port = DEFAULT_PORT;

    if (argc > 1) server_host = argv[1];
    if (argc > 2) server_port = atoi(argv[2]);

    char token[128] = {0};
    int is_paired = 0;

    FILE *token_file = fopen(AUTH_KEY_FILE, "r");
    if (token_file)
    {
        if (fgets(token, 128, token_file)) {
            trim_newline(token);
            if (strlen(token) > 0) is_paired = 1;
        }
        fclose(token_file);
    }

    printf("Ocrib Agent Starting...\n");
    printf("Connecting to %s:%d\n", server_host, server_port);

    while (1)
    {
        int sock;
        struct sockaddr_in serv_addr;
        char buffer[2048] = {0};
        struct hostent *server;

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) { sleep(5); continue; }

        server = gethostbyname(server_host);
        if (server == NULL) { close(sock); sleep(5); continue; }

        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
        serv_addr.sin_port = htons(server_port);

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            close(sock);
            sleep(5);
            continue;
        }

        if (!is_paired)
        {
            srand(time(NULL));
            int pin = 100000 + rand() % 900000;
            struct passwd *pw = getpwuid(getuid());
            char *account_name = pw ? pw->pw_name : "Unknown User";

            printf("\n========================================\n");
            printf(" NEW DEVICE DETECTED (Account: %s)\n", account_name);
            printf(" Enter this PIN on your web dashboard:\n");
            printf(" ---> %d <--- \n", pin);
            printf("========================================\n\n");

            char pin_msg[512];
            snprintf(pin_msg, sizeof(pin_msg), "PIN:%d|NAME:%s", pin, account_name);
            send(sock, pin_msg, strlen(pin_msg), 0);

            int bytes = read(sock, buffer, 2048);
            if (bytes > 0 && strncmp(buffer, "VERIFIED:", 9) == 0)
            {
                strcpy(token, buffer + 9);
                FILE *new_file = fopen(AUTH_KEY_FILE, "w");
                if (new_file) { fprintf(new_file, "%s", token); fclose(new_file); }
                is_paired = 1;
                printf("Pairing successful!\n");
            }
            else { close(sock); sleep(2); continue; }
        }
        else
        {
            char auth_msg[256];
            snprintf(auth_msg, sizeof(auth_msg), "TOKEN:%s", token);
            send(sock, auth_msg, strlen(auth_msg), 0);
            printf("Ocrib Active. Account: %s\n", getpwuid(getuid())->pw_name);
        }

        while (1)
        {
            memset(buffer, 0, 2048);
            int bytes = read(sock, buffer, 2048);
            if (bytes <= 0) break; 

            printf("\nIncoming task: %s\n", buffer);
            char *filename = strrchr(buffer, '/');
            filename = (filename != NULL) ? filename + 1 : "downloaded_file";
            char *q = strchr(filename, '?'); if (q) *q = '\0';

            CURL *curl = curl_easy_init();
            if (curl)
            {
                FILE *fp = fopen(filename, "wb");
                if (fp) {
                    curl_easy_setopt(curl, CURLOPT_URL, buffer);
                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_chunk_to_disk);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
                    if (curl_easy_perform(curl) == CURLE_OK) {
                        printf("Saved: %s\n", filename);
                        send_desktop_notification(filename);
                    }
                    fclose(fp);
                }
                curl_easy_cleanup(curl);
            }
        }
        close(sock);
        sleep(5);
    }
    return 0;
}