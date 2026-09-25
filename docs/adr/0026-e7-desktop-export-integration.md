# ADR 0026: E7 desktop export integration

## Estado

Aceptada para E7. QA-001 y cualquier UX de inserción multi-video quedan fuera.

## Decisión

`DesktopController::exportProject()` valida destino y vídeo exportable, pausa
la reproducción y captura `ProjectSnapshot`, destino y proyecto activo por
valor antes de iniciar un `std::thread` dedicado. El worker construye sus
propias factories FFmpeg, `ExportJob` y `FilesystemExportOutputPublisher`;
ningún objeto FFmpeg ni `ProjectDocument` mutable cruza desde el GUI thread.

El progreso llega al GUI mediante queued invocation y se publica como máximo
aproximadamente cada 1%; `exportRunning`, `exportProgress`, `exportStatus` y
`exportDestination` son estado independiente del playhead. Un contador de
generación descarta callbacks tardíos. Cancelar sólo activa una bandera
cooperativa; E5 realiza abort, cleanup y preservación del destino.

El destructor solicita cancelación y hace `join()` antes de liberar la sesión.
La política Beta no guarda el proyecto y conserva `projectDirty`; tampoco
reanuda automáticamente playback.

## Resolución de paths

El adapter desktop de vídeo y el resolver de audio aplican la política
existente: `pulsar-sound:<id>` usa `builtin_sound_runtime_path()`, y locators
normales se resuelven con `resolve_runtime_media_path(active_project_file,
locator)`. El snapshot conserva el locator lógico. Desktop no implementa
rename, temporales ni publicación: esas responsabilidades permanecen en
E5.

## Evidencia

`pulsar.desktop.export_integration` ejecuta el controller real sin ventana:
valida proyecto sin vídeo, apertura de fixture, export MP4 real, progreso
monotónico, dirty intacto, rechazo de segundo export y destrucción segura con
worker activo. `pulsar.desktop.smoke` conserva el arranque headless. La
interfaz QML añade SaveFileDialog MP4, EXPORTAR, CANCELAR y barra de progreso;
el diálogo reutiliza la conversión de paths ya usada por Guardar proyecto.
