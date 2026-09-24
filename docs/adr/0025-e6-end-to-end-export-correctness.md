# ADR 0025: E6 end-to-end export correctness

## Estado

Aceptada para E6. No introduce UI ni inicia E7.

## Decisión

La evidencia E6 usa dos fixtures MP4 deterministas (`e6_video_a.mp4` y
`e6_video_b.mp4`) con segmentos de color y tonos distintos. El proyecto de
referencia se construye mediante `import`, `add`, `trim`, `split` y `move`:

* vídeo A `[0,1)` en `[0,1)`, gap `[1,2)`, vídeo B `[1,2)` en `[2,3)`, y A
  `[1,2)` en `[3,4)`;
* una pista de audio independiente solapa el audio embebido, incluye ganancia
  `-6 dB`, un clip muteado y `pulsar-sound:click_soft` en `[2.5,2.62)`.

El snapshot se guarda y se reabre con `FileProjectRepository`; la exportación
recibe sólo el snapshot restaurado. Una mutación posterior del documento se
verifica como invisible para el worker.

El helper E2E inspecciona el MP4 real por decodificación: muestrea color por
PTS (tolerancia de 35 unidades RGB por H.264/YUV), mide RMS de audio en
ventanas seguras y verifica duración, CFR, time-base `1/48000`, codecs y
perfil AAC-LC. La cancelación se solicita desde el primer callback de progreso,
sin sleeps ni threads de UI. Se verifica que un destino previo sobrevive tanto
a cancelación como a `MissingMedia`, y que los temporales sibling desaparecen.

La cobertura específica de 30000/1001, timestamps y priming/padding permanece
en las pruebas E4 congeladas; E6 usa 25 fps para el proyecto de referencia y
no duplica la suite de codec ya validada.

## Límites y deudas heredadas

No se cambia la semántica editorial ni se agrega caché completo de vídeo,
PCM o paquetes. El resolver de `pulsar-sound:` se inyecta como boundary de
plataforma en el test. Se conservan las deudas E5 registradas: reserva del
`.part` no es cross-process atómica y la durabilidad de archivo/directorio no
se fuerza con `fsync` en Linux.
