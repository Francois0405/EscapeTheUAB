# Midnight Hoops: validación

Windows 11, MSVC 14.51, RTX 5070 Ti. Linux y macOS no se han compilado.

## Comprobaciones ejecutadas

| Comprobación | Resultado |
|--------------|-----------|
| `run-windows.ps1` Debug y Release | Correctas |
| CTest Debug y Release | 25/25 en cada una |
| `EntornVGI.slnx` con MSBuild, Debug y Release | Compila (proyecto generado con `tools/generate_vs_project.py`); binarios no ejecutados |
| Shader en GPU frente a la referencia de CPU (ventana oculta, 128x72) | 72 combinaciones de modo, patrón, 3/4/5/8 bits e intensidad 0,85/1,5, más exposición +1 EV. Tolerancia 1,5/255. En ruido espacial solo se comprueban los niveles |
| Recorrido de capturas en el juego Release | Cierre con código 0, `glGetError` 0 en las 7 fases |

El arnés de GPU es temporal y no está en el repositorio.

## Capturas y rendimiento

Reproducir (no guarda ajustes, silencia el audio y cierra solo):

```powershell
$env:ENTORNVGI_RETRO_CAPTURE = "$PWD\build\midnight-hoops-captures"
.\build\windows-release\bin\EntornVGI.exe
Remove-Item Env:ENTORNVGI_RETRO_CAPTURE
```

Medición del 26-11-2026: Release, ventana 1920x1009, cámara inicial, sin vsync, 300 fotogramas por fase.

| Preset | Resolución interna | Frametime | FPS | GPU (solo escena) |
|--------|--------------------|-----------|-----|-------------------|
| Original | 960x540 | 1,80 ms | 556 | 1,71 ms |
| Retro Cinematic | 1028x540 | 1,94 ms | 516 | 1,85 ms |
| PSX Classic | 685x360 | 0,95 ms | 1052 | 0,85 ms |

- El temporizador de GPU mide el dibujo de la escena, no el pase de posprocesado.
- Cinematic dibuja un 7 % más de píxeles que Original porque sigue la proporción de la ventana.
- Es una sola pasada en una GPU potente; no dice nada sobre equipos modestos.
- Original repetido al final coincide con el inicial salvo el subtítulo que aún sonaba y el balanceo del móvil.

## Pendiente: QA manual

- [ ] Retícula oculta en menú, pausa, editor, F1 y OpenXR.
- [ ] Arco con la tecla E: se completa, cambia a amarillo y desaparece al lanzar, cancelar o perder el objeto.
- [ ] Panel F1: cambiar parámetros, "Restaurar preset" y comprobar que se conservan al reiniciar.
- [ ] Legibilidad de pistas y carteles en los tres presets; aliasing lejano con texturas nearest.
- [ ] Redimensionar, maximizar y minimizar.
- [ ] Entrar y salir del editor; F2 y F3.
- [ ] Activar el viewmodel en F1: paredes cercanas, lanzamiento, salto visual al soltar, solape con subtítulos.
- [ ] Multijugador LAN: el otro jugador ve el objeto físico; el cliente ve su retícula, arco y viewmodel.
- [ ] OpenXR: el visor se ve igual que antes.

## Pendiente: validar subtítulos escuchando

Los diálogos son transcripciones automáticas (Whisper `small`) y las descripciones de sonidos salen del
nombre del recurso. Corregir `assets/subtitles/hirity.json` y poner `verified: true`. Los segmentos del
saludo en gallego usan los tiempos aproximados de Whisper (0-3 s y 3-5,3 s).

| Audio | Duración | Duda |
|-------|----------|------|
| `story/holahirity.ogg` | 4,0 s | ¿"tu asistente personal"? |
| `story/holahiritygallego.ogg` | 5,2 s | Muy dudosa, texto y tiempos |
| `story/agarrame.ogg` | 4,9 s | ¿"la tecla E"? |
| `story/lohashechobien.ogg` | 3,7 s | El juego no lo reproduce hoy |
| `searching/heydondeestashirity.ogg` | 3,8 s | Texto y quién habla (figura como "Voz") |
| `searching/venaquihirity.ogg` | 2,6 s | Quién habla |
| `damage/damagehirity.ogg` | 3,0 s | Dudosa |
| `damage/damegehiritylong.ogg` | 4,5 s | Texto |
| `ambient/cough.ogg`, `ambient/pasosfuertes.ogg` | 2,4 / 6,2 s | Descripción |
| `appear/anotopat.ogg` | 2,1 s | Idioma (¿catalán?), texto y quién habla |
| `appear/heehee.ogg`, `rickroll.ogg` | 4,6 / 2,1 s | Descripción |

## Revisión independiente

Codex (`codex@openai-codex` 1.0.6, solo lectura).

- Primera pasada, sobre el árbol de trabajo anterior a `f4f0296`: un P2 confirmado y corregido ("texturas
  nearest" no afectaba a la reducción).
- Segunda pasada, contra `a9fea0c` (hasta `cc0d6f2`): un P2 sobre caracteres corruptos en
  `hirity.json`, **descartado**. El fichero es UTF-8 válido, sin U+FFFD, y el juego muestra bien las tildes
  (captura `held_charge_subtitle.png`); el revisor lo leyó sin decodificar UTF-8. No informó de más problemas.
