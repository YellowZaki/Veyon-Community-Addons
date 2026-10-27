# Veyon Addon: Remote Script Execution & Package Deployer (Open Source)

Addon de código abierto y completamente libre para **Veyon (versión 4.11.4)** en sistemas Windows 10 y Windows 11.

Permite al profesor o administrador enviar scripts (PowerShell, Batch/CMD, VBScript o ejecutables) y **paquetes completos de archivos o carpetas** dependientes a los equipos de los alumnos, ejecutándolos **de forma 100% silenciosa y con máximos privilegios de Administrador (`NT AUTHORITY\SYSTEM`)**.

---

## 🌟 Características Principales

1. **Ejecución Elevada Silenciosa (Sin UAC y Sin Ventanas):**
   - Se ejecuta a través del servicio de Veyon heredando privilegios **`SYSTEM`** (superiores a los de un administrador local común).
   - Bandera `CREATE_NO_WINDOW` / `SW_HIDE`: **no parpadea ninguna consola**, ventana ni icono en la barra de tareas del alumno.
   - **Sin avisos de UAC:** Las acciones ejecutadas bajo `SYSTEM` nunca muestran solicitudes de elevación ni requieren credenciales de usuario.

2. **Empaquetado y Despliegue de Dependencias (Archivos y Carpetas):**
   - Permite adjuntar archivos sueltos (`.exe`, `.msi`, `.reg`, `.ini`, `.json`, `.dll`, etc.) o **carpetas enteras con subdirectorios**.
   - Los archivos se empaquetan en un contenedor comprimido seguro (`qCompress`), optimizando la transferencia en red a decenas de máquinas.
   - En el alumno, el paquete se extrae en una carpeta temporal aislada (`C:\Windows\Temp\VeyonDeploy_{UUID}\`) y el proceso se lanza con esa carpeta como **Directorio de Trabajo (`Working Directory`)**.
   - El script puede invocar los archivos mediante rutas relativas directas (ej: `.\instalador.msi`, `.\config.json`).

3. **Múltiples Intérpretes Soportados:**
   - **PowerShell (`.ps1`):** Ejecución con `Bypass` de políticas (`-ExecutionPolicy Bypass -NoProfile -WindowStyle Hidden`).
   - **Batch / CMD (`.bat`, `.cmd`):** Ejecución directa mediante `cmd.exe /c`.
   - **VBScript (`.vbs`):** Ejecución desatendida mediante `cscript.exe //NoLogo`.
   - **Ejecutables directos (`.exe`):** Binarios portables o instaladores silenciosos.
   - **Detección automática:** Identifica el intérprete por la extensión del archivo o permite seleccionarlo en el desplegable.

4. **Monitorización y Captura de Salida en Tiempo Real:**
   - Reporte inmediato por cada equipo: *Enviando*, *Ejecutando*, *Completado con éxito (Exit Code 0)* o *Error*.
   - Captura completa de la salida estándar (`stdout`) y salida de error (`stderr`).
   - Visor integrado en Veyon Master con botón para copiar los logs al portapapeles.

5. **Limpieza Automática y Seguridad:**
   - Opción activada por defecto para eliminar automáticamente la carpeta temporal del alumno una vez concluida la ejecución, sin dejar residuos en disco.
   - Límite de tiempo configurable (*Timeout* en segundos o sin límite para despliegues largos).
   - Transmisión autenticada y cifrada a través de las claves de Veyon (`authkeys`).

---

## 📁 Estructura del Addon

```
remote-script-execution/
├── CMakeLists.txt                               # Compilación in-tree y out-of-tree
├── RemoteScriptExecutionConfiguration.h         # Propiedades y configuración del addon
├── RemoteScriptExecutionConfigurationPage.h     # Página en Veyon Configurator
├── RemoteScriptExecutionConfigurationPage.cpp
├── RemoteScriptExecutionConfigurationPage.ui
├── RemoteScriptExecutionFeaturePlugin.h         # Plugin principal (FeatureProviderInterface)
├── RemoteScriptExecutionFeaturePlugin.cpp
├── RemoteScriptDialog.h                         # Ventana principal del profesor en Veyon Master
├── RemoteScriptDialog.cpp
├── RemoteScriptDialog.ui
├── ScriptPackage.h                              # Gestor de empaquetado, serialización y compresión
├── ScriptPackage.cpp
├── ScriptRunner.h                               # Ejecutor en cliente con privilegios SYSTEM y flags ocultos
├── ScriptRunner.cpp
├── remotescriptexecution.qrc                    # Recursos e iconos embebidos
├── resources/
│   └── remote-script-execution.png              # Icono oficial del addon
├── compat/
│   └── QtCrypto                                 # Stub de compatibilidad para CryptoCore
└── scripts/
    ├── install-plugin.ps1                       # Instalador automático en Windows
    └── uninstall-plugin.ps1                     # Desinstalador limpio
```

---

## 🚀 Ejemplos de Uso Prácticos

### Ejemplo 1: Instalar un programa silenciosamente con archivo adjunto
* **Archivos adjuntos:** Añades `vlc-setup.exe` mediante el botón *Añadir archivo(s)...*.
* **Script (PowerShell):**
  ```powershell
  Start-Process -FilePath ".\vlc-setup.exe" -ArgumentList "/S /NCRC" -Wait
  Write-Host "VLC instalado correctamente en la estación."
  ```

### Ejemplo 2: Desplegar configuración con archivo JSON y modificar el Registro
* **Archivos adjuntos:** Añades `settings.json`.
* **Script (PowerShell):**
  ```powershell
  $dest = "C:\ProgramData\MiAplicacion"
  New-Item -ItemType Directory -Path $dest -Force | Out-Null
  Copy-Item -Path ".\settings.json" -Destination "$dest\settings.json" -Force
  Set-ItemProperty -Path "HKLM:\Software\MiAplicacion" -Name "Enabled" -Value 1 -Type DWord
  Write-Host "Configuración aplicada con éxito."
  ```

### Ejemplo 3: Script de mantenimiento rápido en Batch
* **Script (Batch/CMD):**
  ```cmd
  ipconfig /flushdns
  net stop wuauserv
  echo Limpieza completada con exito.
  ```

---

## 🛠️ Instalación y Compilación

### Instalación Rápida (con DLL ya compilada)
Ejecuta PowerShell como Administrador en la carpeta del addon:
```powershell
.\scripts\install-plugin.ps1
```
El instalador detendrá el servicio de Veyon, copiará `remotescriptexecution.dll` en `C:\Program Files\Veyon\plugins\` y reiniciará el servicio.

### Compilación Standalone con CMake
```powershell
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```
