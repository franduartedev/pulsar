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
diez capturas de presets en los artefactos `pulsar-*-motion-titles-qa`.
Comparar sus PNG `preset-*-settled.png` con la evidencia de MISIÓN 027 antes
de aprobar visualmente: el texto no debe recortarse y el secundario debe
conservar legibilidad sobre fondo claro y oscuro. La CI confirma regresiones
técnicas; no reemplaza esta revisión humana.

## Limitaciones pendientes

- La placa de contraste es fija y deliberadamente conservadora; una futura
  medición dinámica de luminancia por fotograma no forma parte de 028A.
- `Fill` usa el recorte y los controles Pos X/Y existentes; no añade
  keyframes. Eso queda reservado para MISIÓN 028B.
