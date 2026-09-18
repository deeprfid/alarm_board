@echo off
rem clear_usb_cache.cmd - reset cached binding for VID_2E88&PID_4608 (self-elevates)
setlocal
net session >nul 2>&1
if errorlevel 1 (
    powershell -NoProfile -Command "Start-Process -Verb RunAs -FilePath '%~f0'"
    exit /b
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0clear_usb_cache.ps1" -RemoveDevice %*
echo.
pause
