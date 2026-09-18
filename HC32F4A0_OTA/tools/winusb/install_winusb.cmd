@echo off
rem install_winusb.cmd - double-click entry point (self-elevates)
setlocal
net session >nul 2>&1
if errorlevel 1 (
    powershell -NoProfile -Command "Start-Process -Verb RunAs -FilePath '%~f0'"
    exit /b
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install_winusb.ps1" %*
echo.
pause
