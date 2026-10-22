# Veyon Addon: Internet Access Control (Open Source)

Recreación de código abierto y completamente libre del addon comercial **Internet Access Control** para **Veyon 4.11.4** en sistemas Windows 10 y Windows 11.

---

## 🌟 Características Principales

1. **Integración 100% nativa con Veyon:**
   - **Veyon Master:** Añade el botón conmutador (*Mode button*) "Bloquear Internet" / "Desbloquear Internet" en la barra de herramientas. Permite activar o desactivar el acceso web en todos o en equipos individuales seleccionados con un solo clic.
   - **Indicador de estado en tiempo real:** Los equipos con Internet bloqueado reflejan el estado activo en la interfaz del profesor.
   - **Veyon Configurator:** Incorpora una pestaña dedicada para configurar puertos bloqueados, subredes locales excluidas, lista blanca de dominios/IPs y opciones de limpieza automática.

2. **Aislamiento seguro de red sin afectar a Veyon:**
   - Utiliza la **API COM nativa de Windows Firewall** (`INetFwPolicy2` / `INetFwRules`) con respaldo automático mediante `netsh advfirewall`.
   - **Preservación estricta de la Red Local (LAN / RFC 1918):** Bloquea las conexiones web salientes (`TCP/UDP 80, 443, 8080, 8443`) dirigidas a rangos de Internet público, pero **mantiene intactas** las conexiones a la subred local (`10.0.0.0/8`, `172.16.0.0/12`, `192.168.0.0/16`, `127.0.0.1`).
   - La comunicación entre el equipo del profesor y los alumnos a través de los puertos de Veyon (`11100`, `11200`, `11300`, etc.) **nunca se interrumpe**.

3. **Ejecución con privilegios de sistema (*System Worker*):**
   - El servicio de Veyon delega la inyección de reglas de cortafuegos al *Managed System Worker*, el cual se ejecuta bajo la cuenta `LocalSystem`. No requiere confirmación de UAC ni permisos de administrador en las cuentas de usuario de los alumnos.

4. **Fail-safe y limpieza automática:**
   - Si el servicio de Veyon se reinicia o el equipo se apaga, el addon limpia las reglas residuales para evitar bloqueos permanentes accidentales.

---

## 📁 Estructura del Proyecto

```
internet-access-control/
├── CMakeLists.txt                               # Definición de compilación in-tree y out-of-tree
├── InternetAccessControlConfiguration.h         # Propiedades y persistencia de configuración
├── InternetAccessControlConfigurationPage.h     # Interfaz de configuración en Veyon Configurator
├── InternetAccessControlConfigurationPage.cpp
├── InternetAccessControlConfigurationPage.ui
├── InternetAccessControlFeaturePlugin.h         # Punto de entrada del plugin (FeatureProviderInterface)
├── InternetAccessControlFeaturePlugin.cpp
├── WindowsFirewallController.h                  # Controlador de bajo nivel de Windows Firewall (COM/netsh)
├── WindowsFirewallController.cpp
├── internetaccesscontrol.qrc                    # Recursos e iconos embebidos
├── resources/                                   # Iconos en alta resolución
│   ├── internet-web.png
│   └── internet-access-blocked.png
├── translations/                                # Archivos de traducción
│   ├── InternetAccessControl_es.ts              # Español
│   └── InternetAccessControl_en.ts              # Inglés
└── scripts/
    ├── install-plugin.ps1                       # Script de instalación desatendida en clientes
    ├── uninstall-plugin.ps1                     # Script de desinstalación limpia
    └── test-firewall-rules.ps1                  # Script de verificación y prueba de firewall
```

---

## 🚀 Instalación y Despliegue

### Opción A: Despliegue rápido con archivo compilado (`internetaccesscontrol.dll`)

1. Copia `internetaccesscontrol.dll` en la carpeta `plugins` de Veyon en cada equipo (Profesor y Alumnos):
   ```
   C:\Program Files\Veyon\plugins\internetaccesscontrol.dll
   ```
2. O ejecuta el script automático incluido en PowerShell (como Administrador):
   ```powershell
   cd internet-access-control\scripts
   .\install-plugin.ps1
   ```
3. Reinicia `Veyon Master` y el servicio de Veyon.

---

## 🛠️ Compilación del Plugin

### Método 1: Compilación Automática en la Nube (GitHub Actions)
El repositorio incluye el flujo de trabajo `.github/workflows/build-internet-access-control.yml`. 
Al subir el código a un repositorio de GitHub, se compilará automáticamente en un entorno con Windows y Qt 6, generando el archivo `.zip` con la DLL lista para descargar en la pestaña **Actions**.

### Método 2: Compilación Local en Windows

**Requisitos:**
* CMake 3.18 o superior.
* Compilador MinGW-w64 (GCC 13+) o MSVC.
* Qt 6.7+ (módulos Core, Gui, Widgets, Network).

**Pasos:**
```powershell
mkdir build
cd build
cmake ..\internet-access-control -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```
El archivo `internetaccesscontrol.dll` resultante se encontrará en el directorio `build/`.

---

## 🧪 Verificación y Diagnóstico

Para comprobar que las reglas de cortafuegos funcionan en tu entorno sin interferir con la red del aula, ejecuta:
```powershell
cd internet-access-control\scripts
.\test-firewall-rules.ps1
```
El script comprobará la conectividad web antes, durante y después del bloqueo, así como la disponibilidad ininterrumpida de la red local.
