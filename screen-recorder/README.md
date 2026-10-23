# Veyon Addon: Screen Recorder (Open Source)

Recreación de código abierto y completamente libre del addon comercial **Screen Recorder** para **Veyon (versión 4.11.4)** en sistemas Windows 10 y Windows 11.

Permite grabar en archivos de video (MP4 con H.264 o AVI con Motion-JPEG) las pantallas de los alumnos seleccionados o de todo el aula de forma simultánea, sin dependencias externas pesadas y con procesamiento en segundo plano multihilo.

---

## 🌟 Características Principales

1. **Grabación de pantallas de alumnos en tiempo real:**
   - Permite grabar uno, varios o todos los equipos conectados desde la interfaz de **Veyon Master**.
   - Captura el framebuffer directamente a través del motor de Veyon (`ComputerControlInterface`).
   - Sincronización automática de FPS y resolución (tasa fija o nativa).

2. **Doble motor de codificación de video (Cero dependencias externas):**
   - **Estándar MP4 (H.264):** Utiliza **Windows Media Foundation** (`IMFSinkWriter`), aprovechando aceleración por hardware (NVENC/Intel QSV/AMD AMF) o el codificador de software integrado en Windows. Produce videos ligeros, de alta calidad y reproducibles en cualquier dispositivo.
   - **Motion-JPEG (AVI):** Motor propio en C++ puro con compresión JPEG integrada de Qt. Formato universal de máxima compatibilidad, que actúa además como respaldo automático si Media Foundation no está disponible.

3. **Panel de control en tiempo real en Veyon Master (`ScreenRecorderControlWidget`):**
   - Ventana flotante con cronómetro digital en tiempo real (`00:00:00`).
   - Indicador visual animado de grabación activa.
   - Contador de pantallas grabándose simultáneamente.
   - Botón directo para **Detener grabación**.
   - Botón directo para **Abrir carpeta con grabaciones** en el Explorador de Windows.

4. **Integración completa en Veyon Configurator:**
   - **Ajustes de salida:** Directorio de destino configurable (por defecto en *Videos/Veyon Recordings*), subdirectorios por nombre de equipo, resolución fija o adaptativa y tasa de cuadros por segundo (FPS).
   - **Ajustes de codificación:** Selector de formato (MP4 H.264 / AVI MJPEG) y tasa de bits (Bitrate en kbps).
   - **Notificaciones al alumno:** Opción para mostrar un aviso informativo en el escritorio del alumno cuando comience o finalice la grabación.

5. **Arquitectura multihilo no bloqueante:**
   - La codificación de video y escritura en disco se ejecuta en hilos independientes en segundo plano (`ScreenRecorderEncodingWorker`).
   - La interfaz de Veyon Master permanece completamente fluida a 60 FPS sin retrasos ni congelaciones incluso al grabar decenas de equipos a la vez.

---

## 📁 Estructura del Proyecto

```
screen-recorder/
├── CMakeLists.txt                               # Configuración de compilación in-tree y out-of-tree
├── ScreenRecorderConfiguration.h                # Propiedades de configuración y persistencia
├── ScreenRecorderConfigurationPage.h            # Interfaz de configuración en Veyon Configurator
├── ScreenRecorderConfigurationPage.cpp
├── ScreenRecorderConfigurationPage.ui
├── ScreenRecorderControlWidget.h                # Panel de control flotante en Veyon Master
├── ScreenRecorderControlWidget.cpp
├── ScreenRecorderControlWidget.ui
├── ScreenRecorderFeaturePlugin.h                # Punto de entrada del plugin (FeatureProviderInterface)
├── ScreenRecorderFeaturePlugin.cpp
├── ScreenRecorderEncodingWorker.h               # Worker en segundo plano con cola de frames segura
├── ScreenRecorderEncodingWorker.cpp
├── VideoWriter.h                                # Interfaz abstracta para codificadores de video
├── MediaFoundationVideoWriter.h                 # Codificador H.264 MP4 vía Windows Media Foundation
├── MediaFoundationVideoWriter.cpp
├── AviVideoWriter.h                             # Codificador Motion-JPEG AVI autónomo
├── AviVideoWriter.cpp
├── screenrecorder.qrc                           # Recursos e iconos embebidos
├── resources/                                   # Iconos vectoriales renderizados en PNG
│   ├── media-record.png
│   ├── media-record-active.png
│   └── screenrecorder.png
├── translations/                                # Traducciones oficiales
│   ├── ScreenRecorder_es.ts                     # Español
│   └── ScreenRecorder_en.ts                     # Inglés
├── compat/                                      # Cabeceras de compatibilidad
│   └── QtCrypto                                 # Stub para Veyon CryptoCore
└── scripts/
    ├── install-plugin.ps1                       # Instalador automático en Windows
    └── uninstall-plugin.ps1                     # Desinstalador limpio
```

---

## 🚀 Instalación y Despliegue

### Opción A: Despliegue con el archivo compilado (`screenrecorder.dll`)

1. Copia `screenrecorder.dll` en la carpeta `plugins` de Veyon en el equipo del profesor:
   ```
   C:\Program Files\Veyon\plugins\screenrecorder.dll
   ```
2. O ejecuta el script de PowerShell incluido (como Administrador):
   ```powershell
   cd screen-recorder\scripts
   .\install-plugin.ps1
   ```
3. Inicia **Veyon Master** o **Veyon Configurator**. Verás el nuevo botón **"Grabar"** en la barra de herramientas.

---

## ⚙️ Configuración en Veyon Configurator

Abre **Veyon Configurator** -> pestaña **Complementos / Add-ons** -> **Grabador de pantalla**:

| Parámetro | Valor por defecto | Descripción |
| :--- | :--- | :--- |
| **Directorio de salida** | `Videos/Veyon Recordings` | Ruta donde se guardan los archivos de video resultantes. |
| **Subdirectorios por equipo** | Activado (`true`) | Crea una subcarpeta con el nombre del equipo para organizar sus grabaciones. |
| **Usar resolución fija** | Desactivado (`false`) | Escala todos los videos a una resolución establecida (ej. 1920x1080). |
| **Mantener relación de aspecto** | Activado (`true`) | Añade bandas negras simétricas si la pantalla del alumno difiere de la resolución elegida. |
| **Cuadros por segundo (FPS)** | `15` fps | Frecuencia de muestreo de fotogramas (1 a 60 fps). |
| **Formato de codificación** | `MP4 (H.264)` | Formato del archivo (MP4 vía Media Foundation o AVI Motion-JPEG). |
| **Tasa de bits (Bitrate)** | `2500` kbps | Calidad y compresión del video H.264. |
| **Notificar a los alumnos** | Desactivado (`false`) | Muestra un aviso en el escritorio del alumno informándole de la grabación. |

---

## 🛠️ Compilación desde el código fuente

El addon se puede compilar de dos formas:

### Compilación independiente (*Out-of-tree*)
Requiere **Qt 6** (Core, Gui, Widgets, Network), un compilador C++17 (MinGW o MSVC) y CMake 3.18+.

```bash
mkdir build && cd build
cmake ../screen-recorder \
  -G "Ninja" \
  -DCMAKE_BUILD_TYPE=Release \
  -DVEYON_SOURCE_DIR=/ruta/a/veyon-core
cmake --build . --config Release
```

### Compilación integrada (*In-tree*)
Si estás compilando Veyon desde sus fuentes completas, copia o enlaza `screen-recorder` dentro de la carpeta `plugins/` de Veyon y agrégalo a `plugins/CMakeLists.txt`.
