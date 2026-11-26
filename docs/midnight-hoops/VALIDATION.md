# Midnight Hoops: validación

Windows 11, MSVC 14.51, RTX 5070 Ti. Linux y macOS no se han compilado.

## Validación manual comunicada por el equipo

Sobre la versión publicada de la rama (`0b0475b`), el equipo la probó y comunicó dos cosas:

- **Aprobada** la estética gráfica configurada (los valores que hoy son el preset oficial).
- **Rechazado** el indicador circular de carga. Se ha sustituido por un anillo de bloques que **el equipo todavía
  no ha visto**.

No se comunicó nada sobre el resto de puntos, así que siguen pendientes.

## Pruebas automáticas ejecutadas (pulido visual)

Ninguna de ellas sustituye una partida.

| Comprobación | Resultado |
|--------------|-----------|
| `run-windows.ps1` Debug y Release | Correctas |
| CTest Debug y Release | 25/25 en cada una |
| `EntornVGI.slnx` con MSBuild, Debug y Release | Compila; binarios no ejecutados |
| Presets (`RetroTests`) | Valores oficiales; Cinematic y Original campo a campo sin cambios; ini anterior respetado; claves ausentes tomadas del preset guardado; ini sin versión y sin retocar adopta su preset actual |
| Indicador (`RuntimeUiTests`) | 20 bloques: 5/10/15 a 25/50/75 %, 20 solo al máximo, sentido horario, sin solapes, lado en píxeles enteros |
| Recorrido de capturas en el juego Release | Código 0, `glGetError` 0 y ningún error de FBO en las 15 fases; el `retro_settings.ini` existente no cambió |
| Capturas al 25, 50, 75 y 100 % | `charge_025.png` a `charge_100.png` y la hoja ampliada `charge_sheet.png`: bloques nítidos, amarillos al máximo |
| Redimensionado | 1280x720 → interno 640x360; 2560x1061 → 869x360; 800x900 → 320x360; sin errores de FBO |
| Menú principal y pausa | Sin retícula (`pause.png`, `main_menu.png` revisadas) |
| Niebla oficial 0,040 frente a 0, pared del fondo | Cambia el 6,3 % de los píxeles de la zona; nivel medio 6,79 → 6,43. Efecto muy sutil en esta sala |

## Comprobaciones anteriores que no se han repetido

| Comprobación | Resultado |
|--------------|-----------|
| Shader en GPU frente a la referencia de CPU (ventana oculta, 128x72) | 72 combinaciones de modo, patrón, 3/4/5/8 bits e intensidad 0,85/1,5, más exposición +1 EV, con tolerancia 1,5/255. El shader no ha cambiado desde entonces. El arnés es temporal y no está en el repositorio |
| Shader de posprocesado roto a propósito | Se usa el blit anterior |

## Capturas y rendimiento

Reproducir (no guarda ajustes, silencia el audio y cierra solo; la hoja `charge_sheet.png` se montó aparte):

```powershell
$env:ENTORNVGI_RETRO_CAPTURE = "$PWD\build\midnight-hoops-captures\pulido"
.\build\windows-release\bin\EntornVGI.exe
Remove-Item Env:ENTORNVGI_RETRO_CAPTURE
```

Las capturas están en `build/`, fuera de Git. Medición del pulido visual: Release, ventana 1920x1009, cámara
inicial, sin vsync, 300 fotogramas por fase.

| Preset | Resolución interna | Frametime | FPS | GPU (solo escena) |
|--------|--------------------|-----------|-----|-------------------|
| PSX Classic (oficial) | 685x360 | 0,97 ms | 1030 | 0,86 ms |
| Retro Cinematic | 1028x540 | 1,98 ms | 506 | 1,89 ms |
| Original | 960x540 | 1,84 ms | 544 | 1,77 ms |

- El temporizador de GPU mide el dibujo de la escena, no el pase de posprocesado.
- Cinematic dibuja un 7 % más de píxeles que Original porque sigue la proporción de la ventana.
- Con el viewmodel desactivado (valor por defecto) el objeto sostenido aparece detrás de la retícula y el anillo.
- Es una sola pasada en una GPU potente; no dice nada sobre equipos modestos.
- Original estira la imagen 16:9 a la proporción de la ventana: es el comportamiento anterior, conservado a
  propósito. Por eso la estantería se ve más ancha que en Cinematic.
- Con anchos internos no enteros respecto a la ventana (1920/685) el dithering puede mostrar columnas desiguales.
- La niebla solo existe en el shader Phong; con Gouraud o Flat no hace nada.
- Las capturas ocultan los paneles de red y de objetos dinámicos que el juego muestra por defecto.

## Pendiente: QA manual

- [ ] Retícula oculta en editor, F1 y OpenXR.
- [ ] **Aprobar el nuevo anillo de bloques.**
- [ ] Anillo con la tecla E: se completa, cambia a amarillo y desaparece al lanzar, cancelar o perder el objeto.
- [ ] Anillo con otra escala de Windows (125 %, 150 %).
- [ ] Instalación sin `retro_settings.ini`: arranca con el aspecto oficial.
- [ ] Panel F1: cambiar parámetros, "Restaurar preset" y comprobar que se conservan al reiniciar.
- [ ] Legibilidad de pistas y carteles con el preset oficial (360p, niebla) y las alternativas; aliasing lejano con texturas nearest.
- [ ] Maximizar, minimizar y arrastrar el borde; buscar columnas desiguales en el dithering.
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
  (captura `held_charge_subtitle.png`); como hipótesis sin comprobar, el revisor pudo leerlo sin decodificar UTF-8. No informó de más problemas.
- Las fases de captura añadidas después (redimensionado, pausa y menú) no se auditaron.
- Pulido visual (sobre `0b0475b`, sin commit): una pasada, sin defectos accionables. No compiló ni ejecutó pruebas.
- Cambios posteriores (`Parse` con ficheros anteriores sin retocar, `CODE_MAP.md`, `UI_PANELS.md`): segunda
  pasada sobre el árbol de trabajo, sin defectos accionables (revisión estática). Una comprobación aparte de la
  documentación contra el código corrigió dos descripciones.
- Caso no cubierto: un ini «Personalizado» escrito por la primera versión (`f4f0296`) usa un formato que ya no
  existe y carga el aspecto oficial.
