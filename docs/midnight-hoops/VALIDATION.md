# Midnight Hoops: validación

## Comprobaciones automáticas (26-11-2026, Windows 11, MSVC 14.51)

| Comprobación | Resultado |
|--------------|-----------|
| `run-windows.ps1` Debug y Release | Correctas |
| CTest Debug y Release | 24/24 en cada una (incluye `RetroTests`: presets, persistencia, Bayer, subtítulos) |
| `EntornVGI.slnx` con MSBuild, Debug y Release | Compila. Proyecto regenerado con `tools/generate_vs_project.py`; no se ejecutaron sus binarios |
| Shader en GPU frente a la referencia de CPU (ventana oculta, 128x72, 2/4/5 bits) | Coincide en sin dithering y Bayer 4/8 con tolerancia de 1,5/255. En ruido espacial solo niveles y extremos. A 5 bits el driver redondea hasta 1/255 (164 por 164,5) |
| Arranque Debug y Release (~20 s en el menú) y cierre normal | Código 0, sin errores de shader ni de subtítulos |

En el arranque el bloque de render (redimensionado, posprocesado y filtrado) corre detrás del menú en cada
fotograma. **No** se ejercitaron viewmodel, subtítulos, arco de carga ni retícula.

La comprobación de GPU usó un arnés temporal que no está en el repositorio.
GPUs del equipo: RTX 5070 Ti y Radeon integrada; no se registró cuál usó el contexto.
Linux y macOS no se compilaron.

## Pendiente: QA manual

No se ha jugado ninguna partida con estos cambios ni hay capturas comparativas.

- [ ] Retícula visible solo jugando en escritorio (no en menú, pausa, editor, F1, F2 ni OpenXR).
- [ ] Arco: aparece al cargar, se completa, desaparece al lanzar, cancelar o perder el objeto.
- [ ] Los tres presets: legibilidad de pistas y carteles, sin parpadeo del dithering ni aliasing lejano.
- [ ] Redimensionar, maximizar y minimizar la ventana sin deformaciones.
- [ ] Entrar y salir del editor; F2 y F3.
- [ ] Viewmodel con objetos pequeños, grandes y con Hirity: orientación, tamaño, recorte con paredes.
- [ ] Multijugador LAN: el otro jugador ve el objeto en su sitio físico; el cliente ve su viewmodel.
- [ ] OpenXR: el visor se ve igual que antes.
- [ ] Los ajustes se conservan al reiniciar (`retro_settings.ini` junto a los recursos).

## Pendiente: validar subtítulos escuchando

Los textos de diálogo son transcripciones automáticas (Whisper `small`) y las descripciones de sonidos
salen del nombre del recurso. Corregir `assets/subtitles/hirity.json` y poner `validated: true`.

| Audio | Duración | Duda |
|-------|----------|------|
| `story/holahirity.ogg` | 4,0 s | ¿"tu asistente personal"? |
| `story/holahiritygallego.ogg` | 5,2 s | Muy dudosa |
| `story/agarrame.ogg` | 4,9 s | ¿"la tecla E"? |
| `story/lohashechobien.ogg` | 3,7 s | El juego no lo reproduce hoy |
| `searching/heydondeestashirity.ogg` | 3,8 s | Texto y quién habla (figura como "Voz") |
| `searching/venaquihirity.ogg` | 2,6 s | Quién habla |
| `damage/damagehirity.ogg` | 3,0 s | Dudosa |
| `damage/damegehiritylong.ogg` | 4,5 s | Texto |
| `ambient/cough.ogg`, `ambient/pasosfuertes.ogg` | 2,4 / 6,2 s | Descripción |
| `appear/anotopat.ogg` | 2,1 s | Voz: idioma (¿catalán?), texto y quién habla |
| `appear/heehee.ogg`, `rickroll.ogg` | 4,6 / 2,1 s | Descripción |

## Rendimiento

No medido. Para compararlo: misma escena y cámara, ventana del mismo tamaño, cada preset en F1, y anotar
FPS y "tiempo GPU" del panel de depuración. Ese temporizador mide la escena, no el pase de posprocesado.

## Revisión independiente

Auditoría de Codex (`codex@openai-codex` 1.0.6, `review --scope working-tree`, solo lectura), una pasada.

- P2 confirmado y corregido: "texturas nearest" solo cambiaba la ampliación. Ahora usa
  `GL_NEAREST_MIPMAP_LINEAR` al reducir.
- No informó de otros problemas. Las correcciones posteriores (incluida la restauración del filtrado en
  editor y OpenXR, y la viñeta a 0) no se volvieron a auditar.
