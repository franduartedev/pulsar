# MISIÓN 028A — Aspect-Aware Motion Titles & Vertical Video Polish

## Alcance

Esta entrega corrige el layout y la legibilidad de los Motion Titles sin tocar
el motor de códecs ni introducir keyframes visuales (028B). Parte de
`fd9f829` y mantiene el formato de proyecto: los títulos continúan usando sus
coordenadas normalizadas y los clips usan el `VisualTransform` ya persistido.

## Auditoría

- `title_rasterizer.cpp` calculaba la fuente desde la altura del canvas y
  medía un ancho máximo, pero no reducía la fuente para una palabra sin
  espacios. En vertical podía terminar dibujándola fuera del rectángulo.
- `title_compositor.cpp` ya es el camino compartido de preview y export y
  cachea el raster por título y tamaño de canvas. No se modifica.
- `VisualTransform` ya ofrece `Contain` y `Fill`, y `project_codec.cpp` ya
  persiste `FIT_FILL` desde v15; los proyectos anteriores conservan el
  comportamiento `Contain` por defecto. No se cambia la serialización.
- El Inspector ya seleccionaba ambos modos; ahora informa explícitamente que
  **Rellenar** recorta y que Pos X/Y controlan ese encuadre.

## Validación automatizada añadida

`pulsar.desktop.title_parity` cubre los cinco presets con una palabra larga e
indivisible en 1920x1080, 1080x1920, 720x1280 y 1080x1080. Verifica que el
bloque visible permanezca dentro de las zonas seguras. También comprueba que
un subtítulo sin caja incorpora una placa oscura de contraste. El test de
paridad preview/export existente sigue usando el mismo rasterizador y
compositor para ambos caminos.

## Evidencia esperada

La prueba `pulsar.desktop.motion_showcase` regenera los MP4 16:9 y 9:16 y las
diez capturas de presets en los artefactos `pulsar-*-motion-titles-qa`. Para
el MP4 vertical, el fixture activa explícitamente `Fill` sobre sus cuatro
clips horizontales: prueba el encuadre sin barras y su persistencia, sin
modificar el valor por defecto `Contain` de proyectos existentes.
Comparar sus PNG `preset-*-settled.png` con la evidencia de MISIÓN 027 antes
de aprobar visualmente: el texto no debe recortarse y el secundario debe
conservar legibilidad sobre fondo claro y oscuro. La CI confirma regresiones
técnicas; no reemplaza esta revisión humana.

## Resultado de validación final

El commit funcional validado es `605c7bb`.

- Linux CI: [44080310719](https://github.com/franduartedev/pulsar/actions/runs/44080310719) — **SUCCESS**. Incluye build, suite, captura de escritorio, showcase y paquete.
- Windows CI: [44080316019](https://github.com/franduartedev/pulsar/actions/runs/44080316019) — **SUCCESS**. Incluye build MSVC, suite, bundle, smoke test, Agent Bridge y showcase.

Artefactos descargados:

| Plataforma | Directorio | MP4 16:9 SHA-256 | MP4 9:16 SHA-256 |
| --- | --- | --- | --- |
| Linux | `artifacts/motion-titles-028a-linux-44080310719/` | `961577a582a331c0b5dfe77264d66bff4e26b6d51675d03366010b9cb49036ca` | `3825e39d124c1d92a65e69338ddb9eb0b134a945cdb346d43c30fe502857a311` |
| Windows | `artifacts/motion-titles-028a-windows-44080316019/` | `bb1a5f7660bbd45acd5e56ce1f7f9bd61a9aeb4545d838aee73c6721d3e93957` | `96938d78a6fdb793ac1c0b4d5ff30be2cf17d3edc39a03ac2d2ee8bbb11eb27d` |

Revisión visual humana sobre los frames Linux: respecto del baseline 027,
`ZONALERO` queda completo en 9:16, el subtítulo tiene una placa de contraste
y el export de fixture con `Fill` elimina las bandas negras. La evidencia usa
fixtures `e6_*` etiquetados del repositorio; no se declara como metraje
ZONALERO verificado.

## Limitaciones pendientes

- La placa de contraste es fija y deliberadamente conservadora; una futura
  medición dinámica de luminancia por fotograma no forma parte de 028A.
- `Fill` usa el recorte y los controles Pos X/Y existentes; no añade
  keyframes. Eso queda reservado para MISIÓN 028B.
- El runner Windows no tiene endpoint de audio habilitado; esa limitación fue
  informada por CI y no afecta la validación de títulos o encuadre.
