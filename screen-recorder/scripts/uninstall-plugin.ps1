#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Desinstala el plugin Screen Recorder de la instalación de Veyon.
#>

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "     Desinstalador de Veyon Addon - Screen Recorder       " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Error "Este script requiere permisos de Administrador. Ejecútalo como Administrador."
    exit 1
}

$veyonPath = "$env:ProgramFiles\Veyon"
if (-not (Test-Path $veyonPath)) {
    $veyonPath = "${env:ProgramFiles(x86)}\Veyon"
}

if (-not (Test-Path $veyonPath)) {
    Write-Error "No se encontró el directorio de instalación de Veyon."
    exit 1
}

$dllFile = Join-Path $veyonPath "plugins\screenrecorder.dll"
if (-not (Test-Path $dllFile)) {
    Write-Host "[!] El archivo '$dllFile' no existe. El addon ya está desinstalado." -ForegroundColor Yellow
    exit 0
}

Write-Host "[*] Deteniendo el servicio Veyon..." -ForegroundColor Yellow
$wcli = Join-Path $veyonPath "veyon-wcli.exe"
if (Test-Path $wcli) {
    & $wcli service stop 2>$null | Out-Null
}
Get-Service -Name "VeyonService" -ErrorAction SilentlyContinue | Stop-Service -Force -ErrorAction SilentlyContinue

Start-Sleep -Seconds 1

Write-Host "[*] Eliminando $dllFile..." -ForegroundColor Yellow
Remove-Item -Path $dllFile -Force

Write-Host "[*] Reiniciando servicio Veyon..." -ForegroundColor Yellow
if (Test-Path $wcli) {
    & $wcli service start 2>$null | Out-Null
}
Get-Service -Name "VeyonService" -ErrorAction SilentlyContinue | Start-Service -ErrorAction SilentlyContinue

Write-Host "==========================================================" -ForegroundColor Green
Write-Host "  ¡Desinstalación completada con éxito!                  " -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
