#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Desinstala el plugin Remote Script Execution de la instalación de Veyon.
#>

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Desinstalador de Veyon Addon - Remote Script Execution   " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Comprobar permisos de Administrador
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Error "Este script requiere permisos de Administrador. Ejecútalo como Administrador."
    exit 1
}

# 2. Localizar Veyon
$veyonPath = "$env:ProgramFiles\Veyon"
if (-not (Test-Path $veyonPath)) {
    $veyonPath = "${env:ProgramFiles(x86)}\Veyon"
}

if (-not (Test-Path $veyonPath)) {
    Write-Error "No se encontró el directorio de instalación de Veyon."
    exit 1
}

$destFile = Join-Path $veyonPath "plugins\remotescriptexecution.dll"

if (-not (Test-Path $destFile)) {
    Write-Host "[!] El plugin 'remotescriptexecution.dll' no está instalado en $destFile." -ForegroundColor Yellow
    exit 0
}

# 3. Detener servicio
Write-Host "[*] Deteniendo el servicio Veyon..." -ForegroundColor Yellow
$wcli = Join-Path $veyonPath "veyon-wcli.exe"
if (Test-Path $wcli) {
    & $wcli service stop 2>$null | Out-Null
}
Get-Service -Name "VeyonService" -ErrorAction SilentlyContinue | Stop-Service -Force -ErrorAction SilentlyContinue

Start-Sleep -Seconds 1

# 4. Eliminar DLL
Write-Host "[*] Eliminando $destFile..." -ForegroundColor Yellow
Remove-Item -Path $destFile -Force

# 5. Iniciar servicio
Write-Host "[*] Iniciando el servicio Veyon..." -ForegroundColor Yellow
if (Test-Path $wcli) {
    & $wcli service start 2>$null | Out-Null
}
Get-Service -Name "VeyonService" -ErrorAction SilentlyContinue | Start-Service -ErrorAction SilentlyContinue

Write-Host "==========================================================" -ForegroundColor Green
Write-Host "  ¡Desinstalación completada con éxito!                   " -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
