@echo off
setlocal enabledelayedexpansion

:: Waveshare Screen Build & Flash Script using PlatformIO (framework = arduino, espidf)
:: Usage:
::   build_and_flash.bat               - Build only (env: yolo_uno)
::   build_and_flash.bat build [PORT]  - Build only
::   build_and_flash.bat flash [PORT]  - Upload to board (default port: COM9)
::   build_and_flash.bat monitor [PORT]- Start serial monitor (default port: COM9)
::   build_and_flash.bat all [PORT]    - Build, upload & monitor (default port: COM9)
::   build_and_flash.bat clean         - Clean build directory

set DEFAULT_PORT=COM9
set PIO_ENV=yolo_uno
set ACTION=%~1
if "%ACTION%"=="" set ACTION=build

set TARGET_PORT=%~2
if "%TARGET_PORT%"=="" set TARGET_PORT=%DEFAULT_PORT%

echo ===================================================
echo [PlatformIO] Project: waveshare-screen
echo [PlatformIO] Env:     %PIO_ENV%
echo [PlatformIO] Action:  %ACTION%
echo [PlatformIO] Port:    %TARGET_PORT%
echo ===================================================

where pio >nul 2>&1
if errorlevel 1 (
    set "PIO_CMD=C:\Users\trand\.platformio\penv\Scripts\pio.exe"
) else (
    set "PIO_CMD=pio"
)

if "%ACTION%"=="build" (
    "%PIO_CMD%" run -e %PIO_ENV%
) else if "%ACTION%"=="flash" (
    "%PIO_CMD%" run -e %PIO_ENV% -t upload --upload-port %TARGET_PORT%
) else if "%ACTION%"=="monitor" (
    "%PIO_CMD%" device monitor -e %PIO_ENV% -p %TARGET_PORT%
) else if "%ACTION%"=="all" (
    "%PIO_CMD%" run -e %PIO_ENV% -t upload -t monitor --upload-port %TARGET_PORT%
) else if "%ACTION%"=="clean" (
    "%PIO_CMD%" run -e %PIO_ENV% -t clean
) else (
    echo Unknown action: %ACTION%
    echo Available actions: build, flash, monitor, all, clean
    exit /b 1
)

exit /b %ERRORLEVEL%
