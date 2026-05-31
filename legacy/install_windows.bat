@echo off
title Ocrib Windows Setup
echo ===========================================
echo       OCRIB: WINDOWS AGENT SETUP          
echo ===========================================
echo.
echo This script will set up Ocrib on your Windows PC.
echo.

:: Check for GCC (MinGW)
where gcc >nul 2>nul
if %errorlevel% neq 0 (
    echo [!] C Compiler (GCC) not found.
    echo Please install MinGW-w64 from: https://github.com/niXman/mingw-builds-binaries
    echo and add it to your PATH.
    pause
    exit /b
)

echo [1/2] Compiling Agent...
gcc -O2 agent.c -lcurl -o remote_agent.exe
if %errorlevel% neq 0 (
    echo.
    echo [!] Compilation failed. Make sure libcurl is installed for MinGW.
    pause
    exit /b
)

echo [2/2] Success!
echo.
echo To link this PC, run: remote_agent.exe YOUR_SERVER_DOMAIN
echo.
pause
