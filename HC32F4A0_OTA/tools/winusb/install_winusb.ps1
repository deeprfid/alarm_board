<#
    install_winusb.ps1 - self-sign + install usb_new_winusb.inf (F460 WinUSB channel)

    What it does (local only):
      1) checks it runs elevated
      2) creates (or reuses) a self-signed code-signing certificate
      3) trusts that certificate machine-wide (Root + TrustedPublisher)
      4) builds usb_new_winusb.cat (Inf2Cat if the WDK is present, else New-FileCatalog)
      5) signs the catalog, then installs the driver package with pnputil

    Usage (elevated PowerShell):
        powershell -ExecutionPolicy Bypass -File install_winusb.ps1
        powershell -ExecutionPolicy Bypass -File install_winusb.ps1 -UseTestSigning
        powershell -ExecutionPolicy Bypass -File install_winusb.ps1 -NoTimestamp

    ASCII only on purpose: PowerShell 5.1 reads this file with the ANSI codepage.
#>
[CmdletBinding()]
param(
    [string]$Subject = 'CN=HDSC WinUSB Test',
    [switch]$UseTestSigning,
    [switch]$NoTimestamp
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Definition
$inf = Join-Path $here 'usb_new_winusb.inf'
$cat = Join-Path $here 'usb_new_winusb.cat'

function Assert-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $pr = New-Object Security.Principal.WindowsPrincipal($id)
    if (-not $pr.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        Write-Host '[X] This script must run elevated (Run as administrator).' -ForegroundColor Red
        exit 1
    }
}

function Get-OrCreateCert([string]$subj) {
    $c = Get-ChildItem Cert:\LocalMachine\My | Where-Object { $_.Subject -eq $subj -and $_.HasPrivateKey } | Sort-Object NotAfter -Descending | Select-Object -First 1
    if ($c) { Write-Host ('[*] reusing certificate: ' + $c.Thumbprint); return $c }
    Write-Host '[*] creating self-signed code-signing certificate ...'
    $ext = @('2.5.29.37={text}1.3.6.1.5.5.7.3.3', '2.5.29.19={text}')
    $new = New-SelfSignedCertificate -Type Custom -Subject $subj -KeyUsage DigitalSignature -KeyAlgorithm RSA -KeyLength 2048 -CertStoreLocation 'Cert:\LocalMachine\My' -NotAfter (Get-Date).AddYears(5) -TextExtension $ext
    return $new
}

function Trust-Cert($cert) {
    foreach ($store in @('Root', 'TrustedPublisher')) {
        $path = 'Cert:\LocalMachine\' + $store
        $have = Get-ChildItem $path | Where-Object { $_.Thumbprint -eq $cert.Thumbprint }
        if ($have) { Write-Host ('[*] already trusted in ' + $store); continue }
        $tmp = Join-Path $env:TEMP ($cert.Thumbprint + '.cer')
        Export-Certificate -Cert $cert -FilePath $tmp | Out-Null
        Import-Certificate -FilePath $tmp -CertStoreLocation $path | Out-Null
        Remove-Item $tmp -Force -ErrorAction SilentlyContinue
        Write-Host ('[*] imported into LocalMachine\' + $store)
    }
}

function New-Catalog {
    $inf2cat = Get-Command Inf2Cat.exe -ErrorAction SilentlyContinue
    if ($inf2cat) {
        Write-Host '[*] building catalog with Inf2Cat ...'
        & $inf2cat.Source ('/driver:' + $here) /os:10_X64,10_ARM64 | Out-Host
        if (-not (Test-Path $cat)) { throw 'Inf2Cat did not produce the catalog' }
        return
    }
    Write-Host '[*] Inf2Cat not found, using New-FileCatalog (Windows 10+) ...'
    if (Test-Path $cat) { Remove-Item $cat -Force }
    New-FileCatalog -Path $here -CatalogFilePath $cat -CatalogVersion 2.0 | Out-Null
}

function Sign-Catalog($cert) {
    Write-Host '[*] signing the catalog ...'
    if ($NoTimestamp) {
        Set-AuthenticodeSignature -FilePath $cat -Certificate $cert -HashAlgorithm SHA256 | Out-Null
    } else {
        try {
            Set-AuthenticodeSignature -FilePath $cat -Certificate $cert -HashAlgorithm SHA256 -TimestampServer 'http://timestamp.digicert.com' | Out-Null
        } catch {
            Write-Host '[!] timestamp failed (offline?), signing without timestamp'
            Set-AuthenticodeSignature -FilePath $cat -Certificate $cert -HashAlgorithm SHA256 | Out-Null
        }
    }
    $sig = Get-AuthenticodeSignature $cat
    Write-Host ('[*] catalog signature status: ' + $sig.Status)
}

function Install-Package {
    Write-Host '[*] pnputil /add-driver /install ...'
    $out = & pnputil.exe /add-driver $inf /install 2>&1
    $out | Out-Host
    if ($LASTEXITCODE -ne 0) {
        Write-Host '[X] install failed. Options:' -ForegroundColor Yellow
        Write-Host '    1) Device Manager: uninstall the device WITH driver, replug, retry'
        Write-Host '    2) re-run with -UseTestSigning (needs Secure Boot off, then reboot)'
        Write-Host '    3) use Zadig (WinUSB) instead - no signing involved'
        return $false
    }
    return $true
}

function Show-Result {
    Write-Host ''
    Write-Host '[*] current state for VID_2E88 / PID_4608:'
    Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object { $_.InstanceId -like 'USB\VID_2E88&PID_4608*' } | Select-Object Status, Class, FriendlyName, InstanceId | Format-List | Out-Host
    Write-Host '[*] verify from the repo with:  python tools\usb_winusb_test.py --list'
}

Assert-Admin
if (-not (Test-Path $inf)) { throw ('INF not found: ' + $inf) }

if ($UseTestSigning) {
    Write-Host '[*] enabling test signing (bcdedit /set testsigning on) ...'
    & bcdedit.exe /set testsigning on | Out-Host
    Write-Host '[!] reboot required, then re-run this script without -UseTestSigning.'
    exit 0
}

$cert = Get-OrCreateCert $Subject
Trust-Cert $cert
New-Catalog
Sign-Catalog $cert
$ok = Install-Package
Show-Result
if (-not $ok) { exit 2 }
Write-Host '[OK] done.' -ForegroundColor Green
