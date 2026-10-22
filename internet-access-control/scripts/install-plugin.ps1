#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Instala el plugin Internet Access Control en la instalación de Veyon.
.DESCRIPTION
    Detiene el servicio de Veyon, copia la librería 'internetaccesscontrol.dll'
    al directorio de plugins de Veyon y reinicia el servicio.
#>

param(
    [string]$DllPath = "$PSScriptRoot\..\internetaccesscontrol.dll"
)

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   Instalador de Veyon Addon - Internet Access Control    " -ForegroundColor Cyan
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
    Write-Error "No se encontró el directorio de instalación de Veyon en $env:ProgramFiles\Veyon."
    exit 1
}

$pluginsDir = Join-Path $veyonPath "plugins"
if (-not (Test-Path $pluginsDir)) {
    New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null
}

# 3. Comprobar existencia de la DLL
if (-not (Test-Path $DllPath)) {
    # Buscar en carpetas de build típicas
    $alternatives = @(
        "$PSScriptRoot\..\build\internetaccesscontrol.dll",
        "$PSScriptRoot\..\build\bin\internetaccesscontrol.dll",
        "$PSScriptRoot\..\bin\internetaccesscontrol.dll",
        "$PSScriptRoot\internetaccesscontrol.dll"
    )
    foreach ($alt in $alternatives) {
        if (Test-Path $alt) {
            $DllPath = $alt
            break
        }
    }
}

if (-not (Test-Path $DllPath)) {
    Write-Error "No se encontró el archivo 'internetaccesscontrol.dll' en '$DllPath'. Asegúrate de haber compilado o descargado el plugin primero."
    exit 1
}

Write-Host "[+] Archivo DLL encontrado: $DllPath" -ForegroundColor Green
Write-Host "[+] Directorio destino: $pluginsDir" -ForegroundColor Green

# 4. Detener servicio Veyon
Write-Host "[*] Deteniendo el servicio Veyon..." -ForegroundColor Yellow
$wcli = Join-Path $veyonPath "veyon-wcli.exe"
if (Test-Path $wcli) {
    & $wcli service stop 2>$null | Out-Null
}
Get-Service -Name "VeyonService" -ErrorAction SilentlyContinue | Stop-Service -Force -ErrorAction SilentlyContinue

Start-Sleep -Seconds 1

# 5. Copiar DLL
$destFile = Join-Path $pluginsDir "internetaccesscontrol.dll"
Write-Host "[*] Copiando DLL a $destFile..." -ForegroundColor Yellow
Copy-Item -Path $DllPath -Destination $destFile -Force

# 6. Reanudar servicio Veyon
Write-Host "[*] Iniciando el servicio Veyon..." -ForegroundColor Yellow
if (Test-Path $wcli) {
    & $wcli service start 2>$null | Out-Null
}
Get-Service -Name "VeyonService" -ErrorAction SilentlyContinue | Start-Service -ErrorAction SilentlyContinue

Write-Host "==========================================================" -ForegroundColor Green
Write-Host "  ¡Instalación completada con éxito!                     " -ForegroundColor Green
Write-Host "  El addon Internet Access Control ya está activo.       " -ForegroundColor Green
Write-Host "  Abre 'Veyon Master' o 'Veyon Configurator' para verlo. " -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
