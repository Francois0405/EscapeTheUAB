# Midnight Hoops: estado de tareas

Rama `feature/midnight-hoops`, base `a9fea0c`. Estado a 26-11-2026. Cambios sin commit.

"Implementado" no significa validado jugando: la QA manual está en `VALIDATION.md`.

| # | Tarea | Estado | Dónde |
|---|-------|--------|-------|
| 1 | Retícula cuadrada con contorno | Implementado | `src/ui/RuntimeUi.cpp` |
| 2 | Arco de carga con `ThrowChargeFraction()` | Implementado | `src/ui/RuntimeUi.cpp` |
| 3 | Framebuffer configurable, proporción de la ventana | Implementado | `src/rendering/Ps1Framebuffer.*`, `src/app/MainLoop.cpp` |
| 4 | Cuantización RGB y dithering cromático (Bayer 4/8, ruido espacial) | Implementado y comprobado en GPU | `assets/shaders/retro_post.*`, `src/rendering/RetroPostFX.*` |
| 5 | Presets Original, Retro Cinematic (por defecto), PSX Classic | Implementado | `src/rendering/RetroSettings.h` |
| 6 | Panel "Gráficos retro" en F1 y persistencia en `retro_settings.ini` | Implementado | `src/ui/DebugPanels.cpp`, `src/app/main.cpp` |
| 7 | Subtítulos ligados a la reproducción real | Implementado; textos sin validar | `src/ui/Subtitles.*`, `assets/subtitles/hirity.json`, `src/audio/AudioManager.*` |
| 8 | Objeto sostenido abajo a la derecha | Implementado | `src/rendering/LegacyVgiSceneRenderer.cpp` |
| 9 | Filtrado nearest en texturas de material del entorno | Implementado | `src/app/MainLoop.cpp` |
| 10 | Granulado, viñeta, tono frío/cálido | Implementado (granulado a 0) | shader |

## No hecho

- Niebla por distancia, conos de luz, materiales emisivos nuevos y blue noise (opcionales).
- Tipografía Pixelify Sans: se usa la fuente pixelada integrada de ImGui (cubre Latin-1).
- Marcas temporales por frase: cada audio muestra una sola línea mientras suena.
- Notificación de recogida; solo se movió la ayuda de lanzamiento abajo a la derecha.
- Subtítulos dentro del visor VR.

## Decisiones

- OpenXR y el editor no pasan por el framebuffer retro, así que el modo "VR Comfort" es el render de siempre.
- El viewmodel solo sustituye la matriz de dibujo del objeto que sostiene el jugador local. Al lanzar,
  el objeto vuelve de golpe a su posición física. Se desactiva en F1.
- Retícula, subtítulos y viewmodel no dependen del preset; "Original" restaura solo la imagen y el filtrado.
