@echo off
echo ==========================================
echo Installing Anchor Grid Plugin...
echo ==========================================

echo 1. Installing Plugin (.aex)...
mkdir "%PROGRAMFILES%\Adobe\Common\Plug-ins\7.0\MediaCore" 2>nul
copy /Y plugin\AnchorRadialMenu.aex "%PROGRAMFILES%\Adobe\Common\Plug-ins\7.0\MediaCore\"

echo 2. Installing CEP Panel...
mkdir "%PROGRAMFILES(X86)%\Common Files\Adobe\CEP\extensions\com.anchor.grid" 2>nul
xcopy /E /I /Y cep "%PROGRAMFILES(X86)%\Common Files\Adobe\CEP\extensions\com.anchor.grid"

echo 3. Enabling Debug Mode (Registry)...
reg add "HKCU\Software\Adobe\CSXS.10" /v PlayerDebugMode /t REG_SZ /d 1 /f >nul
reg add "HKCU\Software\Adobe\CSXS.11" /v PlayerDebugMode /t REG_SZ /d 1 /f >nul
reg add "HKCU\Software\Adobe\CSXS.12" /v PlayerDebugMode /t REG_SZ /d 1 /f >nul
reg add "HKCU\Software\Adobe\CSXS.13" /v PlayerDebugMode /t REG_SZ /d 1 /f >nul
reg add "HKCU\Software\Adobe\CSXS.14" /v PlayerDebugMode /t REG_SZ /d 1 /f >nul
reg add "HKCU\Software\Adobe\CSXS.15" /v PlayerDebugMode /t REG_SZ /d 1 /f >nul
reg add "HKCU\Software\Adobe\CSXS.16" /v PlayerDebugMode /t REG_SZ /d 1 /f >nul

echo ==========================================
echo Installation Complete!!
echo Please restart After Effects.
echo ==========================================
pause
