# Midnight Hoops: estado de tareas

Rama `feature/midnight-hoops`, base `a9fea0c`.

`PASS` significa que hay evidencia ejecutada (pruebas, GPU o captura del juego real), no que alguien lo haya
jugado. Nadie ha jugado todavía con estos cambios; ver `VALIDATION.md`.

| Tarea | Estado | Evidencia y lo que falta |
|-------|--------|--------------------------|
| MH-01 Retícula cuadrada | PASS | Pruebas de estado; visible en captura de juego y oculta en F2. Sin observar: menú, pausa, editor, OpenXR |
| MH-02 Arco de carga | PASS | Pruebas de estado; captura al 70 %. Falta cargar y lanzar con la tecla E |
| MH-03 Framebuffer y posprocesado | PASS | Shader igual a la referencia CPU; capturas de los presets. Falta redimensionar la ventana |
| MH-04 Presets y F1 | PASS | Pruebas de valores, límites y persistencia. El panel F1 no se ha manejado a mano |
| MH-05 Subtítulos | PARTIAL | Sistema probado y visto en captura. Textos sin verificar; sin Pixelify Sans |
| MH-06 Objeto en primera persona | PARTIAL | Experimental, **desactivado por defecto**. Probado con un frasco en captura |
| MH-07 Texturas nearest | IMPLEMENTED_UNVERIFIED | Sin comparación visual de cerca |
| Ruido espacial, granulado, viñeta, tono frío/cálido | IMPLEMENTED_UNVERIFIED | Apagados salvo que se activen en F1 |
| Niebla por distancia (shader Phong) | IMPLEMENTED_UNVERIFIED | Apagada por defecto; a las distancias de la sala apenas se aprecia |
| Persistencia (`retro_settings.ini`) | PASS | Prueba de ida y vuelta y de valores inválidos |
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
- Retícula, subtítulos y aviso no dependen del preset; "Original" restaura la imagen y el filtrado.
- El audio no se detiene en pausa ni al volver al menú (comportamiento anterior); el menú principal no
  muestra subtítulos y la pausa sí.
