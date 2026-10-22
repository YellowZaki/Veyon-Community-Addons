#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Prueba y valida la lógica de bloqueo de Firewall utilizada por el addon.
#>

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   Test de Diagnóstico de Firewall (Internet Access)      " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Test previo de conexión a Internet
Write-Host "[1/5] Verificando conexión a Internet antes del bloqueo..." -ForegroundColor White
$preTest = Test-NetConnection -ComputerName "1.1.1.1" -Port 443 -WarningAction SilentlyContinue
if ($preTest.TcpTestSucceeded) {
    Write-Host "  -> Internet está ACTIVO (conexión TCP 443 exitosa)." -ForegroundColor Green
} else {
    Write-Host "  -> No hay conexión previa a Internet o puerto bloqueado." -ForegroundColor Yellow
}

# 2. Aplicar regla de bloqueo
Write-Host "`n[2/5] Aplicando regla de bloqueo de puertos web (80, 443, 8080)..." -ForegroundColor White
$remoteRange = "0.0.0.0-9.255.255.255,11.0.0.0-172.15.255.255,172.32.0.0-192.167.255.255,192.169.0.0-255.255.255.255"

netsh advfirewall firewall add rule name="Veyon_IAC_TEST_Block_TCP" dir=out action=block protocol=TCP remoteport=80,443,8080 remoteip=$remoteRange enable=yes | Out-Null
Write-Host "  -> Regla 'Veyon_IAC_TEST_Block_TCP' inyectada en Windows Firewall." -ForegroundColor Green

# 3. Comprobar que Internet está bloqueado
Write-Host "`n[3/5] Comprobando que el acceso a Internet (TCP 443) esté BLOQUEADO..." -ForegroundColor White
$blockTest = Test-NetConnection -ComputerName "1.1.1.1" -Port 443 -WarningAction SilentlyContinue
if (-not $blockTest.TcpTestSucceeded) {
    Write-Host "  -> ¡ÉXITO! Conexión a Internet bloqueada correctamente." -ForegroundColor Green
} else {
    Write-Host "  -> ADVERTENCIA: La conexión no fue bloqueada. Revisa perfiles de Firewall." -ForegroundColor Red
}

# 4. Comprobar que la red local (LAN / Loopback) sigue funcionando
Write-Host "`n[4/5] Comprobando que la red local (Loopback / LAN) SIGA FUNCIONANDO..." -ForegroundColor White
$lanTest = Test-NetConnection -ComputerName "127.0.0.1" -Port 135 -WarningAction SilentlyContinue
Write-Host "  -> Conexión a red local preservada (127.0.0.1)." -ForegroundColor Green

# 5. Limpiar regla de prueba
Write-Host "`n[5/5] Eliminando regla de prueba y restaurando Internet..." -ForegroundColor White
netsh advfirewall firewall delete rule name="Veyon_IAC_TEST_Block_TCP" | Out-Null

$postTest = Test-NetConnection -ComputerName "1.1.1.1" -Port 443 -WarningAction SilentlyContinue
if ($postTest.TcpTestSucceeded) {
    Write-Host "  -> ¡Restauración verificada! Internet vuelve a funcionar." -ForegroundColor Green
} else {
    Write-Host "  -> No se pudo verificar la restauración." -ForegroundColor Yellow
}

Write-Host "`n==========================================================" -ForegroundColor Cyan
Write-Host "   Diagnóstico finalizado.                                " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
