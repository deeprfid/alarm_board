@echo off
rem uninstall_winusb.cmd - double-click entry point (self-elevates)
setlocal
net session >nul 2>&1
if errorlevel 1 (
    powershell -NoProfile -Command "Start-Process -Verb RunAs -FilePath '%~f0'"
    exit /b
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0uninstall_winusb.ps1" %*
echo.
pause
