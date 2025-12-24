@echo off
setlocal

echo ========================================
echo CommandLogger Build Script
echo ========================================

set PROJECT_DIR=%~dp0
set SDK_PATH=%PROJECT_DIR%..\..\cpp\include\AE_SDK
set PIPL_TOOL=%PROJECT_DIR%..\..\cpp\resources\PiPLtool.exe

echo.
echo [1/4] Generating PiPL resource...
cd /d "%PROJECT_DIR%"
"%PIPL_TOOL%" CommandLogger_PiPL.r CommandLogger_PiPL_temp.rc
if errorlevel 1 (
    echo ERROR: PiPL generation failed
    exit /b 1
)

echo.
echo [2/4] Creating build directory...
if not exist build mkdir build
cd build

echo.
echo [3/4] Running CMake...
cmake -G "Visual Studio 17 2022" -A x64 ..
if errorlevel 1 (
    echo ERROR: CMake configuration failed
    exit /b 1
)

echo.
echo [4/4] Building...
cmake --build . --config Release
if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo ========================================
echo Build successful!
echo Output: %PROJECT_DIR%build\output\Release\CommandLogger.aex
echo ========================================

echo.
echo To install, copy CommandLogger.aex to:
echo   C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\
echo.

endlocal
