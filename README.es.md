<p align="center">
  <img src="docs/media/pulse-arc-mark.png" alt="PULSAR" width="140">
</p>

<h1 align="center">PULSAR</h1>

<p align="center">
  <strong>Editor de video nativo para Linux: local, rápido y predecible.</strong><br>
  Importá, cortá, agregá títulos y sonido, y exportá un MP4 listo para subir. Sin cuenta y sin marca de agua.
</p>

<p align="center">
  <img src="docs/media/workspace.jpg" alt="PULSAR editando un proyecto de 2:38: biblioteca, visor, inspector y timeline multipista" width="100%">
</p>

> **Beta.** PULSAR 0.1.0-beta es para quienes quieran probarlo con material real y contarnos qué falta.
> La interfaz está en español. [English →](README.md)

## Por qué PULSAR

- **Terminá un video completo en una sola herramienta.** Videos, fotos, placas de color, música, efectos y títulos en una timeline multipista, y exportás H.264 / AAC.
- **Lo que ves es lo que exportás.** Títulos, fundidos, escalado y niveles de audio pasan por el mismo código en la vista previa y en el archivo final; está verificado con tests.
- **Exacto y deshacible.** El tiempo es aritmética racional exacta: los cortes y el avance cuadro a cuadro caen donde los ponés, y cada edición es un paso de deshacer.
- **Seguro.** Autoguardado y recuperación tras un cierre inesperado, salida de audio que no satura, proyectos guardados de forma atómica.
- **Liviano y local.** C++ nativo con Qt 6 y FFmpeg, sin cuenta y sin red. Con 100 clips sigue fluido.

## Qué podés hacer hoy

| | |
|---|---|
| **Editar** | Insertar en el cabezal o al final, cortar, recortar, mover con imán, borrar cerrando el hueco, duplicar, avanzar un cuadro, deshacer y rehacer. |
| **Biblioteca** | Importar archivos o una carpeta entera, arrastrar a la timeline, miniaturas, filtros y búsqueda. Video, fotos (PNG, JPEG, WebP), placas de color y audio. |
| **Títulos** | Texto en su propia pista, tamaño, color, sombra, caja y opacidad, y **ubicación libre arrastrando en el visor**. |
| **Sonido** | Pistas de audio, separar el audio de un video, ganancia y silencio por clip, **fundido de entrada y salida**, ondas y 37 efectos originales que podés escuchar antes de usarlos. |
| **Exportar** | Original / 1080p / 720p en MP4 H.264 + AAC, con escalado real que nunca agranda. |
| **Proyectos** | Guardar y reabrir archivos `.pulsar`; recuperar una sesión sin guardar después de un cierre inesperado. |

<p align="center">
  <img src="docs/media/title-inspector.jpg" alt="Título seleccionado: pad de posición y estilo en el Inspector" width="49%">
  <img src="docs/media/sound-library.jpg" alt="Biblioteca de sonidos con categorías y botón de escucha" width="49%">
</p>
<p align="center"><sub>Las capturas usan material de prueba generado.</sub></p>

No está en esta beta: Windows / macOS, varias pistas de video, overlays de logo / PNG, transiciones, efectos, subtítulos,
corrección de color, proxies. La [hoja de ruta](docs/release/roadmap.md) dice qué viene y las
[notas de la versión](docs/release/0.1.0-beta.md) listan las limitaciones conocidas.

## Cómo correrlo

1. Descargá `PULSAR-0.1.0-beta-linux-x86_64.tar.gz` (o el AppImage) desde la página de Releases.
2. `tar xzf PULSAR-0.1.0-beta-linux-x86_64.tar.gz && cd PULSAR-0.1.0-beta`
3. `./pulsar`

No hace falta instalar Qt ni FFmpeg. Probado en Ubuntu 24.04; necesita glibc 2.39 o superior.
El AppImage necesita FUSE 2 o `--appimage-extract-and-run`.

## Probalo y contanos

Lo más útil es **editar un video real** y contarnos dónde te trabaste: [`docs/release/creator-test.md`](docs/release/creator-test.md).
¿Encontraste un error? Abrí un issue con la plantilla **Bug report** (versión con `./pulsar --version`, distro y pasos).
¿Te falta algo? Usá **Feature request**.

## Compilar, arquitectura y contribuir

Los pasos para compilar y la arquitectura están en el [README en inglés](README.md#build-from-source). Para contribuir mirá [`CONTRIBUTING.md`](CONTRIBUTING.md).

## Licencia

Sin definir todavía: el autor debe elegir una licencia antes de que otras personas puedan reutilizar el código.
