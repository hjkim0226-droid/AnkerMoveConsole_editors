@echo off
setlocal
cd /d "%~dp0"

echo ==========================================
echo Installing Anchor Snap Plugin
echo ==========================================

:: Check for Administrator privileges
net session >nul 2>&1
if %errorLevel% == 0 (
    echo Admin privileges confirmed.
) else (
    echo ==========================================
    echo ERROR: ADMIN PRIVILEGES REQUIRED
    echo ==========================================
    echo Please right-click 'install_windows.bat' and select
    echo 'Run as Administrator'.
    echo ==========================================
    pause
    exit /b
)

echo 1. Cleaning up old installation...
:: Clean old AnchorRadialMenu
if exist "%PROGRAMFILES%\Adobe\Common\Plug-ins\7.0\MediaCore\AnchorRadialMenu.aex" (
    echo - Removing old AnchorRadialMenu plugin...
    del /f /q "%PROGRAMFILES%\Adobe\Common\Plug-ins\7.0\MediaCore\AnchorRadialMenu.aex"
)
if exist "%PROGRAMFILES(X86)%\Common Files\Adobe\CEP\extensions\com.anchor.grid" (
    echo - Removing old com.anchor.grid extension...
    rmdir /s /q "%PROGRAMFILES(X86)%\Common Files\Adobe\CEP\extensions\com.anchor.grid"
)
:: Clean current AnchorSnap
if exist "%PROGRAMFILES%\Adobe\Common\Plug-ins\7.0\MediaCore\AnchorSnap.aex" (
    echo - Removing existing AnchorSnap plugin...
    del /f /q "%PROGRAMFILES%\Adobe\Common\Plug-ins\7.0\MediaCore\AnchorSnap.aex"
)
if exist "%PROGRAMFILES(X86)%\Common Files\Adobe\CEP\extensions\com.anchor.snap" (
    echo - Removing existing com.anchor.snap extension...
    rmdir /s /q "%PROGRAMFILES(X86)%\Common Files\Adobe\CEP\extensions\com.anchor.snap"
)

echo 2. Installing Plugin (.aex)...
if not exist "plugin\AnchorSnap.aex" (
    echo ERROR: Source file 'plugin\AnchorSnap.aex' not found!
    echo Please extract the ZIP file completely before running this script.
    pause
    exit /b
)

mkdir "%PROGRAMFILES%\Adobe\Common\Plug-ins\7.0\MediaCore" 2>nul
copy /Y plugin\AnchorSnap.aex "%PROGRAMFILES%\Adobe\Common\Plug-ins\7.0\MediaCore\"

echo 3. Installing CEP Panel...
if not exist "cep" (
    echo ERROR: Source folder 'cep' not found!
    echo Please extract the ZIP file completely before running this script.
    pause
    exit /b
)

mkdir "%PROGRAMFILES(X86)%\Common Files\Adobe\CEP\extensions\com.anchor.snap" 2>nul
xcopy /E /I /Y cep "%PROGRAMFILES(X86)%\Common Files\Adobe\CEP\extensions\com.anchor.snap"

echo 4. Enabling CEP Debug Mode...
reg add "HKCU\Software\Adobe\CSXS.11" /v PlayerDebugMode /t REG_SZ /d 1 /f >nul 2>&1
reg add "HKCU\Software\Adobe\CSXS.12" /v PlayerDebugMode /t REG_SZ /d 1 /f >nul 2>&1

echo ==========================================
echo Installation Complete!
echo ==========================================
echo.
echo Installed:
echo   - AnchorSnap.aex (Plugin)
echo   - com.anchor.snap (CEP Panel)
echo.
echo Usage:
echo   - Press Y key: Anchor Grid (set anchor point)
echo   - Press ; key: Snap Control (search effects)
echo   - Press ESC to cancel
echo.
echo Please restart After Effects.
echo ==========================================
pause
