#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Desinstala el plugin Internet Access Control de Veyon.
#>

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   Desinstalador de Addon - Internet Access Control       " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Error "Este script requiere permisos de Administrador."
    exit 1
}

$veyonPath = "$env:ProgramFiles\Veyon"
if (-not (Test-Path $veyonPath)) {
    $veyonPath = "${env:ProgramFiles(x86)}\Veyon"
}

$pluginsDir = Join-Path $veyonPath "plugins"
$targetDll = Join-Path $pluginsDir "internetaccesscontrol.dll"

# Detener servicio
Write-Host "[*] Deteniendo VeyonService..." -ForegroundColor Yellow
$wcli = Join-Path $veyonPath "veyon-wcli.exe"
if (Test-Path $wcli) {
    & $wcli service stop 2>$null | Out-Null
}
Get-Service -Name "VeyonService" -ErrorAction SilentlyContinue | Stop-Service -Force -ErrorAction SilentlyContinue

# Limpiar reglas de firewall huérfanas
Write-Host "[*] Limpiando reglas residuales de firewall..." -ForegroundColor Yellow
netsh advfirewall firewall delete rule name="Veyon_IAC_Block_Web_TCP" 2>$null | Out-Null
netsh advfirewall firewall delete rule name="Veyon_IAC_Block_Web_UDP" 2>$null | Out-Null
netsh advfirewall firewall delete rule name="Veyon_IAC_Block_All_NonLAN" 2>$null | Out-Null
netsh advfirewall firewall delete rule name="Veyon_IAC_Block_DNS" 2>$null | Out-Null

# Eliminar archivo
if (Test-Path $targetDll) {
    Remove-Item -Path $targetDll -Force
    Write-Host "[+] Archivo $targetDll eliminado correctamente." -ForegroundColor Green
} else {
    Write-Host "[-] El plugin no estaba instalado en $targetDll." -ForegroundColor DarkGray
}

# Reiniciar servicio
Write-Host "[*] Iniciando VeyonService..." -ForegroundColor Yellow
if (Test-Path $wcli) {
    & $wcli service start 2>$null | Out-Null
}
Get-Service -Name "VeyonService" -ErrorAction SilentlyContinue | Start-Service -ErrorAction SilentlyContinue

Write-Host "[+] Desinstalación completada con éxito." -ForegroundColor Green
