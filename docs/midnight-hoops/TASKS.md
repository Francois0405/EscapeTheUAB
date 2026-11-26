# Midnight Hoops: estado de tareas

Rama `feature/midnight-hoops`, base `a9fea0c`.

## Pulido visual y documentación

| Tarea | Estado | Evidencia y lo que falta |
|-------|--------|--------------------------|
| T1 Preset oficial (PSX Classic) | IMPLEMENTED_UNVERIFIED | Pruebas automáticas y captura. Falta arrancar sin ini a mano |
| T2 Anillo de carga pixel art | IMPLEMENTED_UNVERIFIED | Pruebas y capturas al 25/50/75/100 %. Pendiente de aprobación del equipo |
| T3 Inventario de paneles | PASS | [UI_PANELS.md](../architecture/UI_PANELS.md). No se ha eliminado ninguno: decide Carlos |
| T4 Contexto para IAs | PASS | `AGENTS.md`, `CODEX.md`, `README.md`, [CODE_MAP.md](../architecture/CODE_MAP.md) |
| T5 Validación | PARTIAL | Compilación y pruebas completas; falta la QA manual |

Aprobado por el equipo: la estética gráfica. Rechazado: el indicador circular anterior (sustituido).

## Actualización original

`PASS` solo cuando todos los criterios obligatorios tienen evidencia ejecutada. Nadie ha jugado todavía con
estos cambios; ver `VALIDATION.md`.

| Tarea | Estado | Evidencia y lo que falta |
|-------|--------|--------------------------|
| MH-01 Retícula cuadrada | IMPLEMENTED_UNVERIFIED | Pruebas de estado; visible en juego, oculta en menú, pausa y F2. Sin observar: editor, OpenXR, ImGui capturando |
| MH-02 Indicador de carga | IMPLEMENTED_UNVERIFIED | Ahora es un anillo de bloques (ver T2). Falta cargar, lanzar y cancelar con la tecla E |
| MH-03 Framebuffer y posprocesado | IMPLEMENTED_UNVERIFIED | Shader igual a la referencia CPU; presets, tres tamaños de ventana y fallo de shader probados; F2 sin efectos. Sin observar: editor, OpenXR |
| MH-04 Presets y F1 | IMPLEMENTED_UNVERIFIED | Pruebas de valores, límites y persistencia. El panel F1 no se ha manejado a mano |
| MH-05 Subtítulos | PARTIAL | Sistema probado y visto en captura. Textos sin verificar; sin Pixelify Sans |
| MH-06 Objeto en primera persona | PARTIAL | Experimental, **desactivado por defecto**. Probado con un frasco en captura |
| MH-07 Texturas nearest | IMPLEMENTED_UNVERIFIED | Sin comparación visual de cerca |
| Ruido espacial, granulado, viñeta, tono frío/cálido | IMPLEMENTED_UNVERIFIED | Viñeta 0,10 en el oficial; el resto apagado |
| Niebla por distancia (shader Phong) | IMPLEMENTED_UNVERIFIED | 0,040 en el preset oficial; sutil a las distancias de la sala |
| Persistencia (`retro_settings.ini`) | IMPLEMENTED_UNVERIFIED | Serialización probada; falta reiniciar el juego tras cambiar algo en F1 |
| Aviso al agarrar | PASS | Visto en captura |
| Subtítulos en el visor OpenXR | TODO | Fuera del alcance |
| Conos de luz, blue noise | TODO | Opcionales |

## Dónde está cada cosa

- Ajustes y matemática de referencia: `src/rendering/RetroSettings.h`
- Posprocesado: `assets/shaders/retro_post.*`, `src/rendering/RetroPostFX.*`
- Viewmodel: `src/rendering/HeldViewModel.h`, `src/rendering/LegacyVgiSceneRenderer.cpp`
- Subtítulos: `src/audio/SubtitleManager.*`, `assets/subtitles/hirity.json`
- HUD y F1: `src/ui/RuntimeUi*.{h,cpp}`, `src/ui/DebugPanels.cpp`
- Capturas y métricas: `src/app/render/RetroCapture.*`

## Decisiones

- Editor, F2 y OpenXR no usan el posprocesado, el filtrado nearest ni la niebla: son el modo "VR Comfort".
- Los textos sin verificar se muestran con "(sin verificar)"; se pueden ocultar en F1.
- Hiriti y los objetos de más de 0,6 m conservan la representación física aunque se active el viewmodel.
- El preset oficial es `PsxClassic`; los ficheros de ajustes ya guardados no se migran.
- Retícula, subtítulos y aviso no dependen del preset; "Original" restaura la imagen y el filtrado.
- El audio no se detiene en pausa ni al volver al menú (comportamiento anterior); el menú principal no
  muestra subtítulos y la pausa sí.
