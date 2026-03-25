@echo off
setlocal

:: Ensure we are in the script's directory (handles run-as-admin or double clicks)
cd /d "%~dp0"

:: ─────────────────────────────────────────────────────────────
:: LAN Audio Bridge & Mixer — Build & Run
:: ─────────────────────────────────────────────────────────────

:: Try local third_party first, otherwise fallback to C:\libs
if exist "%~dp0third_party\portaudio\build" (
    set PORTAUDIO_ROOT=%~dp0third_party\portaudio\build
) else (
    set PORTAUDIO_ROOT=C:\libs\portaudio\build
)

:: ─────────────────────────────────────────────────────────────

echo.
echo  === LAN Audio Bridge ^& Mixer ===
echo.

:: Check CMake is available
where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERR] CMake not found. Install it from https://cmake.org/download/
    pause
    exit /b 1
)

:: Check PORTAUDIO_ROOT is set
if not exist "%PORTAUDIO_ROOT%" (
    echo [ERR] PORTAUDIO_ROOT not found: %PORTAUDIO_ROOT%
    echo       Edit start.bat and set PORTAUDIO_ROOT to your PortAudio build directory.
    pause
    exit /b 1
)

:: Create build directory
if not exist build mkdir build
cd build

:: Configure
echo [INFO] Configuring...
cmake .. -DPORTAUDIO_ROOT="%PORTAUDIO_ROOT%" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo [ERR] CMake configuration failed.
    pause
    exit /b 1
)

:: Build
echo.
echo [INFO] Building...
cmake --build . --config Release
if errorlevel 1 (
    echo [ERR] Build failed.
    pause
    exit /b 1
)

:: Run
echo.
echo [INFO] Launching lan_audio_bridge.exe...
echo.

if exist Release\lan_audio_bridge.exe (
    Release\lan_audio_bridge.exe
) else if exist lan_audio_bridge.exe (
    lan_audio_bridge.exe
) else (
    echo [ERR] Binary not found after build. Check build output above.
    pause
    exit /b 1
)

endlocal
