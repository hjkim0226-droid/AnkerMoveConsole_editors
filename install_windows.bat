@echo off
setlocal
cd /d "%~dp0"

echo ==========================================
echo Installing Anchor Grid Plugin (ScriptUI)
echo ==========================================

:: Check for Administrator privileges
net session >nul 2>&1
if %errorLevel% == 0 (
    echo Admin privileges confirmed.
) else (
    echo ==========================================
    echo ERROR: ADMIN PRIVILEGES REQUIRED
    echo ==========================================
    echo Please right-click 'install.bat' and select
    echo 'Run as Administrator'.
    echo ==========================================
    pause
    exit /b
)

echo 1. Cleaning up old installation...
if exist "%PROGRAMFILES%\Adobe\Common\Plug-ins\7.0\MediaCore\AnchorRadialMenu.aex" (
    echo - Removing old plugin...
    del /f /q "%PROGRAMFILES%\Adobe\Common\Plug-ins\7.0\MediaCore\AnchorRadialMenu.aex"
)
if exist "%PROGRAMFILES(X86)%\Common Files\Adobe\CEP\extensions\com.anchor.grid" (
    echo - Removing old CEP extension...
    rmdir /s /q "%PROGRAMFILES(X86)%\Common Files\Adobe\CEP\extensions\com.anchor.grid"
)

echo 2. Installing Plugin (.aex)...
if not exist "plugin\AnchorRadialMenu.aex" (
    echo ERROR: Source file 'plugin\AnchorRadialMenu.aex' not found!
    echo Please extract the ZIP file completely before running this script.
    pause
    exit /b
)

mkdir "%PROGRAMFILES%\Adobe\Common\Plug-ins\7.0\MediaCore" 2>nul
copy /Y plugin\AnchorRadialMenu.aex "%PROGRAMFILES%\Adobe\Common\Plug-ins\7.0\MediaCore\"

echo 3. Installing CEP Panel...
if not exist "cep" (
    echo ERROR: Source folder 'cep' not found!
    echo Please extract the ZIP file completely before running this script.
    pause
    exit /b
)

mkdir "%PROGRAMFILES(X86)%\Common Files\Adobe\CEP\extensions\com.anchor.grid" 2>nul
xcopy /E /I /Y cep "%PROGRAMFILES(X86)%\Common Files\Adobe\CEP\extensions\com.anchor.grid"

echo 4. Enabling CEP Debug Mode...
reg add "HKCU\Software\Adobe\CSXS.11" /v PlayerDebugMode /t REG_SZ /d 1 /f >nul 2>&1
reg add "HKCU\Software\Adobe\CSXS.12" /v PlayerDebugMode /t REG_SZ /d 1 /f >nul 2>&1

echo ==========================================
echo Installation Complete!
echo ==========================================
echo.
echo Installed:
echo   - AnchorRadialMenu.aex (Plugin)
echo   - com.anchor.grid (CEP Panel)
echo.
echo Usage:
echo   - Press Y key to show anchor grid
echo   - Click a position to set anchor point
echo   - Press ESC to cancel
echo.
echo Please restart After Effects.
echo ==========================================
pause
