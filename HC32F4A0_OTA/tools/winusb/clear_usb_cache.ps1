<#
    clear_usb_cache.ps1 - reset Windows' cached binding state for the F460 USB device.
    Use it when the device got bound to a wrong/old driver (classic "cached failed
    enumeration" case, which is why the WinUSB composition uses PID 0x4608).
    Usage (elevated): powershell -ExecutionPolicy Bypass -File clear_usb_cache.ps1 -RemoveDevice
    ASCII only (PowerShell 5.1 reads this file with the ANSI codepage).
#>
[CmdletBinding()]
param(
    [string]$Vid = '2E88',
    [string]$Pid = '4608',
    [switch]$RemoveDevice
)
$ErrorActionPreference = 'Continue'

$id = [Security.Principal.WindowsIdentity]::GetCurrent()
$pr = New-Object Security.Principal.WindowsPrincipal($id)
if (-not $pr.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host '[X] run as administrator' -ForegroundColor Red
    exit 1
}

$pattern = 'USB\VID_' + $Vid + '&PID_' + $Pid + '*'
Write-Host ('[*] devices matching ' + $pattern + ':')
$devs = @(Get-PnpDevice -ErrorAction SilentlyContinue | Where-Object { $_.InstanceId -like $pattern })
if ($devs.Count -eq 0) { Write-Host '    (none - device plugged in and enumerated?)' }
foreach ($d in $devs) {
    Write-Host ('    ' + $d.Status + '  ' + $d.Class + '  ' + $d.FriendlyName + '  ' + $d.InstanceId)
}

if ($RemoveDevice) {
    foreach ($d in $devs) {
        Write-Host ('[*] removing device node ' + $d.InstanceId)
        $done = $false
        try {
            Remove-PnpDevice -InstanceId $d.InstanceId -Confirm:$false -ErrorAction Stop
            $done = $true
        } catch { $done = $false }
        if (-not $done) { & pnputil.exe /remove-device $d.InstanceId | Out-Host }
    }
}

Write-Host '[i] now unplug the USB cable, wait ~3 seconds, plug it back in.'
Write-Host '[i] then run:  python tools\usb_winusb_test.py --list'
