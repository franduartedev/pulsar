# ADR 0025: E6 end-to-end export correctness

## Estado

Aceptada para E6. No introduce UI ni inicia E7.

## Decisión

La evidencia E6 usa tres fixtures MP4 deterministas (`e6_video_a.mp4`,
`e6_video_b.mp4` y `e6_video_rational.mp4`) con segmentos de color y tonos
distintos. El proyecto de
referencia se construye mediante `import`, `add`, `trim`, `split` y `move`:

* vídeo A `[0,1)` en `[0,1)`, gap `[1,2)`, vídeo B `[1,2)` en `[2,3)`, y A
  `[1,2)` en `[3,4)`;
* una pista de audio independiente solapa el audio embebido, incluye ganancia
  `-6 dB`, un clip muteado y `pulsar-sound:click_soft` en `[1.0,1.12)`.

Además, pruebas aisladas sobre el mismo pipeline verifican gain sin audio
embebido (`-6/0 = 0.501`, `+6/0 = 1.995`, tolerancias 0.10 y 0.20), overlap
de dos tonos (`RMS overlap/solo = 1.413`), y Core Sound aislado
(`RMS antes/durante/después = 0.0000/0.0774/0.0000`).

El snapshot se guarda y se reabre con `FileProjectRepository`; la exportación
recibe sólo el snapshot restaurado. Una mutación posterior del documento se
verifica como invisible para el worker.

El helper E2E inspecciona el MP4 real por decodificación: muestrea color por
PTS (tolerancia de 35 unidades RGB por H.264/YUV), comprueba explícitamente
los frames anterior y de frontera en `1.000`, `2.000` y `3.000` s, mide RMS de audio en
ventanas seguras y verifica duración, CFR, time-base `1/48000`, codecs y
perfil AAC-LC. La cancelación se solicita desde el primer callback de progreso,
sin sleeps ni threads de UI. Se verifica que un destino previo sobrevive tanto
a cancelación como a `MissingMedia`, y que los temporales sibling desaparecen.

El caso aislado de sincronía usa el cambio visual A rojo→verde y el inicio de
`pulsar-sound:click_soft` en `1.000` s; la búsqueda de audio decodificado
observó `1.010` s, delta `0.010` s, con tolerancia máxima `0.080` s
(un frame de 25 fps más una ventana de análisis AAC). Un pipeline adicional
exporta `e6_video_rational.mp4` a `30000/1001`, comprueba FPS nominal exacto,
duración del `ExportPlan`, audio presente y colores en `0.5`/`1.5` s.

La cobertura de priming/padding y las pruebas de codec más exhaustivas
permanecen en E4 congeladas; E6 sólo añade el pipeline racional mínimo y no
duplica la suite de writer.

## Límites y deudas heredadas

No se cambia la semántica editorial ni se agrega caché completo de vídeo,
PCM o paquetes. El resolver de `pulsar-sound:` se inyecta como boundary de
plataforma en el test. Se conservan las deudas E5 registradas: reserva del
`.part` no es cross-process atómica y la durabilidad de archivo/directorio no
se fuerza con `fsync` en Linux.
