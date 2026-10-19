<div align="center">

<img src="packaging/linux/pulsar-256.png" alt="Logo de PULSAR" width="128" height="128">

# PULSAR

**Editor de video nativo y liviano, para una edición predecible.**

[![Versión](https://img.shields.io/badge/versi%C3%B3n-0.2.0--beta.1-3b82f6?style=for-the-badge)](CHANGELOG.md)
[![Plataforma](https://img.shields.io/badge/plataforma-Linux%20x86__64-14b8a6?style=for-the-badge&logo=linux&logoColor=white)](#-requisitos)
[![Licencia](https://img.shields.io/badge/licencia-GPL--3.0--or--later-blue?style=for-the-badge)](LICENSE)
[![Hecho con](https://img.shields.io/badge/C%2B%2B-Qt%206%20%C2%B7%20FFmpeg-00599C?style=for-the-badge&logo=qt&logoColor=white)](#-compilar-desde-el-código)

[**Descargar**](../../releases) · [Notas de la versión](docs/release/0.2.0-beta.1.md) · [Reportar un bug](../../issues/new/choose) · [Hoja de ruta](docs/release/roadmap.md) · [🇬🇧 English](README.md)

</div>

> [!NOTE]
> **Primera beta pública.** Esperá asperezas y hacé copias de seguridad de tus medios y proyectos.
> La interfaz está **en español**; el README está en inglés y en español.

PULSAR es un editor de video de escritorio nativo, escrito en C++ con Qt 6 y FFmpeg. Apunta a un
flujo de edición pequeño y predecible: **tiempos exactos, sin sorpresas al deshacer y una
exportación en la que podés confiar.**

## ✨ Lo más destacado

| | |
|---|---|
| 🎞️ **Timeline multipista** | Insertar en el cabezal o agregar al final (sin huecos), cortar, recortar, mover con imán, duplicar, deshacer / rehacer, avance por fotograma |
| 🖼️ **Edición directa en el Viewer** | Mover y escalar clips sobre la vista previa, imagen en imagen, modos *Ajustar* / *100%* |
| 🔤 **Títulos** | Texto estático en su propia pista, caja de fondo y opacidad opcionales, idéntico en vista previa y exportación |
| 🔊 **Audio** | Separar audio, ganancia / silencio / solo por clip y por pista, fundidos, formas de onda, 37 efectos de sonido originales con previsualización |
| 🗂️ **Biblioteca de medios** | Filtros y búsqueda, importación de varios archivos o carpetas, arrastrar y soltar al timeline |
| 💾 **Proyectos seguros** | Guardado atómico de proyectos `.pulsar`, autoguardado y recuperación tras un cierre inesperado |
| 📤 **Exportación** | MP4 H.264 / AAC con presets Original / 1080p / 720p; se niega a sobrescribir medios que usa el proyecto |
| 🎯 **Vista previa = exportación** | Ambas usan el mismo plan de composición: lo que ves es lo que exportás |

Además: imágenes PNG / JPEG / WebP y placas de color como clips, y una transición de fundido cruzado.

## 📦 Requisitos

- Linux **x86_64** con **glibc 2.39 o superior** (construido y probado en Ubuntu 24.04; Linux Mint 22 y Fedora reciente deberían servir). **Ubuntu 22.04, Linux Mint 21 y Debian 12 son demasiado antiguos** y fallan con `GLIBC_2.39 not found`.
- Sesión de escritorio X11 o Wayland con drivers gráficos y salida de audio funcionando.
- El **AppImage** necesita FUSE 2 (`libfuse2`; `libfuse2t64` en Ubuntu 24.04). Sin FUSE, ejecutalo con `--appimage-extract-and-run`.
- No hace falta instalar Qt ni FFmpeg: vienen incluidos.

## 🚀 Inicio rápido

Descargá los archivos y `SHA256SUMS` desde la [página de Releases](../../releases) y verificá con `sha256sum -c SHA256SUMS`.

```sh
# tar.gz (recomendado)
tar xzf PULSAR-0.2.0-beta.1-linux-x86_64.tar.gz
./PULSAR-0.2.0-beta.1/pulsar

# o AppImage
chmod +x PULSAR-0.2.0-beta.1-x86_64.AppImage
./PULSAR-0.2.0-beta.1-x86_64.AppImage     # agregá --appimage-extract-and-run si no tenés FUSE
```

`--version` muestra `PULSAR 0.2.0-beta.1`. El paquete incluye su propio FFmpeg: durante la beta evitá abrir medios de origen no confiable.

## ⚠️ Limitaciones conocidas

- **Solo Linux x86_64.** Sin versiones para Windows, macOS ni móviles.
- **Interfaz solo en español**; el selector de archivos de *Abrir proyecto* es el del toolkit (claro y en inglés).
- **La vista previa puede ir lenta** con composición transformada o multicapa (solo CPU, sin GPU ni proxies). La exportación es la referencia. No probado con 4K / HEVC / material largo.
- Sin títulos animados, subtítulos, keyframes, máscaras, herramientas de color ni control de velocidad; el fundido cruzado es la única transición.
- Sin limitador ni medidores de audio: sumar varias pistas fuertes puede saturar.
- Sin reenlace de medios faltantes, lista de proyectos recientes ni selección múltiple.
- La exportación es solo MP4 / H.264 / AAC; sin presets verticales (9:16): el lienzo sigue al primer clip.
- Imágenes: solo PNG, JPEG y WebP. Un proyecto solo de audio no se puede exportar.

> [!IMPORTANT]
> **A confirmar (se agradece tu feedback).** Ambos casos dependen de tu material y solo se comprobaron con el material de prueba del autor:
> - Las superposiciones de PNG con transparencia (alfa) funcionaron en las pruebas de vista previa y exportación del autor.
> - El video vertical de celular se mostró con la orientación correcta allí, pero las combinaciones de contenedor / códec / metadato de rotación varían según el dispositivo.
>
> Si alguno falla en tu equipo, por favor [abrí un issue](../../issues/new/choose) con los datos del archivo de origen.

Lista completa: [notas de la versión](docs/release/0.2.0-beta.1.md) (en inglés).

## 🐞 Reportar un bug / dar feedback

Usá la plantilla **Bug report** e incluí tu versión (`./pulsar --version`), distro, X11 o Wayland, CPU/GPU/RAM, el material involucrado y los pasos para reproducirlo; ejecutar desde una terminal y pegar su salida ayuda mucho. Ideas y comentarios generales de la beta van en la plantilla **Feedback**. Podés escribir en español o en inglés.

## 🛠️ Compilar desde el código

Requisitos (nombres de paquetes de Ubuntu 24.04): `cmake ninja-build g++ pkg-config`,
Qt 6 (`qt6-base-dev qt6-declarative-dev qt6-multimedia-dev` más los módulos QML
`qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-dialogs qml6-module-qtquick-layouts qml6-module-qtquick-window qml6-module-qtquick-templates qml6-module-qtqml-workerscript`),
y los archivos de desarrollo de FFmpeg (`libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev`).

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPULSAR_BUILD_DESKTOP=ON
cmake --build build
ctest --test-dir build --output-on-failure          # QT_QPA_PLATFORM=offscreen en una máquina sin pantalla
./build/apps/pulsar-desktop/pulsar-desktop
```

Solo el núcleo (sin Qt/FFmpeg, pruebas de dominio y aplicación): agregá `-DPULSAR_BUILD_FFMPEG=OFF` y
dejá `PULSAR_BUILD_DESKTOP` apagado. Compilación con sanitizers: `-DPULSAR_ENABLE_ASAN=ON -DPULSAR_ENABLE_UBSAN=ON`.
Para generar un paquete de release mirá [`packaging/linux/README.md`](packaging/linux/README.md).

## 🏗️ Arquitectura

```text
domain  <-  application  <-  adapters (FFmpeg, audio Qt, archivos de proyecto)  <-  app de escritorio (Qt Quick)
```

`domain` no tiene dependencias externas (tiempo racional exacto, modelo de timeline).
`application` gestiona comandos/deshacer, documentos de proyecto, coordinación de reproducción y
el pipeline de exportación sin depender de Qt ni FFmpeg. Mirá [`docs/architecture`](docs/architecture/README.md)
y los ADR en [`docs/adr`](docs/adr/README.md).

## 🤝 Contribuir

Ejecutá las pruebas antes de enviar cambios y respetá la dirección de dependencias entre módulos. Las
reglas de ingeniería están en [`docs/development/engineering.md`](docs/development/engineering.md).

## 📄 Licencia

PULSAR es software libre, publicado bajo la **GNU General Public License v3.0 o posterior** (`GPL-3.0-or-later`); ver
[`LICENSE`](LICENSE). Incluye FFmpeg (compilado con `--enable-gpl --enable-version3`), x264 y Qt: los componentes,
sus licencias y dónde obtener el código fuente correspondiente están en [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

---
<div align="center"><sub>PULSAR · Hoja de ruta en <a href="docs/release/roadmap.md"><code>docs/release/roadmap.md</code></a> · Cambios en <a href="CHANGELOG.md"><code>CHANGELOG.md</code></a></sub></div>
