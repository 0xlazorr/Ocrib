; Ocrib Agent Windows Installer NSIS Script
; Build with: makensis installer.nsi

!define APP_NAME "Ocrib Agent"
!define SERVICE_NAME "OcribAgentService"
!define EXE_NAME "remote_agent.exe"
!define UNINSTALLER_NAME "uninstall.exe"

Name "${APP_NAME}"
OutFile "ocrib-agent-installer.exe"
InstallDir "$PROGRAMFILES\Ocrib"
InstallDirRegKey HKLM "Software\Ocrib" "Install_Dir"

RequestExecutionLevel admin

; Modern UI 2
!include "MUI2.nsh"

!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Welcome to Ocrib Agent Setup"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Section "Install" SecInstall
    SetOutPath "$INSTDIR"
    
    ; Bundle the compiled binary
    File "remote_agent.exe"
    
    ; Write registry install path
    WriteRegStr HKLM "Software\Ocrib" "Install_Dir" "$INSTDIR"
    
    ; Write Uninstaller registry key for Windows Control Panel
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Ocrib" "DisplayName" "${APP_NAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Ocrib" "UninstallString" '"$INSTDIR\${UNINSTALLER_NAME}"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Ocrib" "QuietUninstallString" '"$INSTDIR\${UNINSTALLER_NAME}" /S'
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Ocrib" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Ocrib" "NoRepair" 1
    
    ; Create Uninstaller
    WriteUninstaller "$INSTDIR\${UNINSTALLER_NAME}"
    
    DetailPrint "Registering Windows Service..."
    ; Register service using sc.exe (built-in Windows utility)
    ; Note: binPath must have a space after the equals sign
    ExecWait 'cmd.exe /c sc create ${SERVICE_NAME} binPath= "$INSTDIR\${EXE_NAME}" start= autoDisplayName= "Ocrib Agent Service"'
    
    DetailPrint "Starting Ocrib Agent Service..."
    ExecWait 'cmd.exe /c sc start ${SERVICE_NAME}'
    
SectionEnd

Section "Uninstall"
    DetailPrint "Stopping Ocrib Agent Service..."
    ExecWait 'cmd.exe /c sc stop ${SERVICE_NAME}'
    
    DetailPrint "Deleting Windows Service..."
    ExecWait 'cmd.exe /c sc delete ${SERVICE_NAME}'
    
    ; Remove files and directories
    Delete "$INSTDIR\${EXE_NAME}"
    Delete "$INSTDIR\${UNINSTALLER_NAME}"
    RMDir "$INSTDIR"
    
    ; Clean up ProgramData config folders
    RMDir /r "$COMMONFILES\Ocrib" ; In case files were stored here
    
    ; Remove Registry keys
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Ocrib"
    DeleteRegKey HKLM "Software\Ocrib"
SectionEnd
