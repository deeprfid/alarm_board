<#
    uninstall_winusb.ps1 - remove the usb_new_winusb driver package and its test certificate.
    Usage (elevated): powershell -ExecutionPolicy Bypass -File uninstall_winusb.ps1
    ASCII only (PowerShell 5.1 reads this file with the ANSI codepage).
#>
[CmdletBinding()]
param(
    [string]$Subject = 'CN=HDSC WinUSB Test',
    [switch]$KeepCertificate
)
$ErrorActionPreference = 'Continue'

$id = [Security.Principal.WindowsIdentity]::GetCurrent()
$pr = New-Object Security.Principal.WindowsPrincipal($id)
if (-not $pr.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host '[X] run as administrator' -ForegroundColor Red
    exit 1
}

Write-Host '[*] driver packages whose original name mentions usb_new_winusb:'
$list = & pnputil.exe /enum-drivers 2>&1
$targets = @()
$cur = @{}
foreach ($line in $list) {
    if ($line -match '^\s*Published Name\s*:\s*(\S+)') { $cur = @{ oem = $Matches[1] } }
    elseif ($line -match '^\s*Original Name\s*:\s*(\S+)') { $cur.orig = $Matches[1] }
    elseif ($line -match '^\s*$') {
        if ($cur.orig -and $cur.orig -match 'usb_new_winusb') { $targets += $cur }
        $cur = @{}
    }
}
if ($targets.Count -eq 0) { Write-Host '    (none)' }
foreach ($t in $targets) {
    Write-Host ('    removing ' + $t.oem + ' (' + $t.orig + ')')
    & pnputil.exe /delete-driver $t.oem /uninstall /force | Out-Host
}

Write-Host '[*] devices with VID_2E88&PID_4608:'
Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object { $_.InstanceId -like 'USB\VID_2E88&PID_4608*' } | Select-Object Status, FriendlyName, InstanceId | Format-List | Out-Host

if (-not $KeepCertificate) {
    foreach ($store in @('Root', 'TrustedPublisher', 'My')) {
        $path = 'Cert:\LocalMachine\' + $store
        Get-ChildItem $path -ErrorAction SilentlyContinue | Where-Object { $_.Subject -eq $Subject } | ForEach-Object {
            Write-Host ('    removing cert from ' + $store + ': ' + $_.Thumbprint)
            Remove-Item $_.PSPath -Force
        }
    }
}
Write-Host '[OK] uninstalled. Unplug/replug the device.' -ForegroundColor Green
