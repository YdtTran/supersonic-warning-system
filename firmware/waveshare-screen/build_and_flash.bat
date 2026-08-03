@echo off
setlocal enabledelayedexpansion

:: Waveshare Screen Build & Flash Script using CMake & Ninja (ESP-IDF v6.0.2)
:: Usage:
::   build_and_flash.bat               - Incremental Build & Flash (default port: COM9)
::   build_and_flash.bat build [PORT]  - Incremental Build & Flash (default port: COM9)
::   build_and_flash.bat flash [PORT]  - Flash target directly (default port: COM9)
::   build_and_flash.bat monitor [PORT]- Start serial monitor (default port: COM9)
::   build_and_flash.bat all [PORT]    - Incremental Build, Flash & Monitor (default port: COM9)
::   build_and_flash.bat clean         - Clean build directory

set DEFAULT_PORT=COM9
set ACTION=%~1
if "%ACTION%"=="" set ACTION=build

set TARGET_PORT=%~2
if "%TARGET_PORT%"=="" set TARGET_PORT=%DEFAULT_PORT%

echo ===================================================
echo [ESP-IDF] Project: waveshare-screen
echo [ESP-IDF] Action:  %ACTION%
echo [ESP-IDF] Port:    %TARGET_PORT%
echo ===================================================

:: Activate ESP-IDF PowerShell profile if idf.py is not in PATH
where idf.py >nul 2>&1
if errorlevel 1 (
    echo [ESP-IDF] Activating ESP-IDF v6.0.2 environment...
    powershell -ExecutionPolicy Bypass -NoProfile -Command "& 'C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1'; if (-not (Test-Path 'build')) { idf.py set-target esp32s3 }; if ('%ACTION%' -eq 'build') { idf.py build } elseif ('%ACTION%' -eq 'flash') { idf.py -p %TARGET_PORT% flash } elseif ('%ACTION%' -eq 'monitor') { idf.py -p %TARGET_PORT% monitor } elseif ('%ACTION%' -eq 'all') { idf.py -p %TARGET_PORT% flash monitor } elseif ('%ACTION%' -eq 'clean') { idf.py fullclean }"
    exit /b !ERRORLEVEL!
)

if not exist build (
    echo [ESP-IDF] Initializing target esp32s3 for first build...
    idf.py set-target esp32s3
)

if "%ACTION%"=="build" (
    idf.py build
) else if "%ACTION%"=="flash" (
    idf.py -p %TARGET_PORT% flash
) else if "%ACTION%"=="monitor" (
    idf.py -p %TARGET_PORT% monitor
) else if "%ACTION%"=="all" (
    idf.py -p %TARGET_PORT% flash monitor
) else if "%ACTION%"=="clean" (
    idf.py fullclean
) else (
    echo Unknown action: %ACTION%
    echo Available actions: build, flash, monitor, all, clean
    exit /b 1
)

exit /b %ERRORLEVEL%
