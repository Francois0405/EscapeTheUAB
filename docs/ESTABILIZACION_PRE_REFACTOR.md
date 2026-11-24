# Estabilización antes del refactor arquitectónico

Estado de `integration/pre-refactor`, Windows, 2026-11-25. Este documento registra los cambios y la evidencia conjunta sin reemplazar las guías de [sincronización multijugador](MULTIPLAYER_SYNC.md), [LAN](MULTIJUGADOR_LAN.md), [Tailscale](VALIDACION_TAILSCALE.md) y [OpenXR](OPENXR.md).

## Causas y correcciones

### Luces de escena y límites OpenGL

La escena calibrada actual tiene 22 entradas `architecture` y 61 entradas `objects` (83 instancias de escena), además de 5 entradas animadas. Trece objetos declaran una luz y las trece están activadas. Los IDs de las 88 entradas JSON no se duplican.

El renderer antiguo permitía ocho luces totales y asignaba solo cuatro luces de escena. La capacidad es ahora 17: los slots 0–3 siguen reservados y las 13 luces de escena ocupan 4–16. No se eliminó ni desactivó ninguna luz ni se cambiaron sus datos. Si una escena futura excede la capacidad, el renderer informa del desbordamiento y deja sin slot las luces sobrantes.

El shader Phong también almacenaba los arrays de OBB directos e indirectos como uniforms por defecto. Con las 17 luces, esos arrays excedían el mínimo de fragment uniforms garantizado para OpenGL 3.3. Ambos arrays se movieron a un bloque `std140`: 12 OBB directos y 24 indirectos, cada uno con `mat4 + vec4` de 80 bytes, suman 2.880 bytes. El bloque cabe en el mínimo GL 3.3 de 16 KiB. Las capacidades 12/24 no cambiaron. `ObjLoaderTests` comprueba el tamaño del bloque, el slot 16 y conserva las pruebas gráficas de oclusión.

El cálculo de uniforms activos por defecto registrado en el código es 608 componentes, con 416 de margen frente al mínimo de 1.024. La validación de Windows enlazó el shader actual; todavía faltan pruebas en hardware de bajo límite, PC VR y macOS. `Iluminacio()` actualiza 17 luces por vista; conserva varias consultas y escrituras de uniform por llamada, así que el rendimiento debe medirse en la escena y en VR antes de considerar esa ruta cerrada.

### Colas de red y eventos asíncronos

La causa de los eventos repetidos era el uso de `swap` con un vector de salida que podía contener elementos de una consulta anterior. Al intercambiar sin limpiarlo, la salida vieja volvía a la cola interna. `TomarEstadosEscena()` y `TomarEstadosFisicosFiables()` limpian primero ese vector y luego consumen la cola.

`NetJuegoTests` ahora espera eventos observables en vez de depender de pausas fijas y, para late join, comprueba el ID y propietario del estado físico esperado. El host conserva la autoridad ENet/Bullet y la prueba no cambia el protocolo.

### Escalado de ImGui, fuentes y framebuffers

La escala se calcula en unidades lógicas GLFW:

```text
L = clamp(min(Wl / 1280, Hl / 720), 0.75, 1.5)
D = clamp(sqrt(max(0.01, DPIx * DPIy)), 0.75, 2.0)
S = clamp(sqrt(L * D), 0.75, 1.75)
```

`Wl` y `Hl` vienen de `glfwGetWindowSize`; `DPIx` y `DPIy`, de `glfwGetWindowContentScale`. El backend GLFW de ImGui conserva `DisplaySize` lógico y calcula `DisplayFramebufferScale` usando el framebuffer nativo. El juego no sustituye esos valores por el tamaño del FBO PS1.

Cada frame se restaura el estilo base de ImGui antes de aplicar `ScaleAllSizes(S)`, evitando que el redimensionado o el cambio de monitor acumulen escala. `FontGlobalScale` usa `S`; se conserva el atlas de fuentes predeterminado y no se reconstruye por monitor. Es necesario revisar visualmente la nitidez de fuentes en 4K.

En modo PLAY la escena 3D se renderiza al FBO PS1 de 960×540 y luego se escala al framebuffer nativo; ImGui se dibuja sobre la salida nativa. El editor usa el framebuffer normal. HUD, controles y geometría visible usan la misma escala lógica que sus zonas interactivas. El panel multijugador recibe posición y tamaño iniciales con `ImGuiCond_FirstUseEver`, por lo que el usuario puede moverlo y redimensionarlo. Las dimensiones iniciales y constraints de barra, jerarquía, inspector y Asset Browser del editor usan `ImGuiIO::FontGlobalScale`; un cambio de escala reajusta tamaño y posición actuales una vez, con clamp al framebuffer lógico, preservando el movimiento y redimensionado manual entre cambios de DPI.

La fórmula y el reajuste geométrico viven en `EscalaUI.h`; `EscalaUITests` cubre resolución de referencia, límites, DPI, escalado de posición/tamaño de panel y clamp al framebuffer lógico. Al cambiar la escala, el editor conserva el tamaño y posición que el usuario tenía, los ajusta una vez por la razón de escala nueva/anterior y los mantiene dentro del área lógica. La prueba nueva aún no se ha ejecutado.

### Sonidos y reaparición de Hiriti desde `origin/master`

El commit `d2e55e8` incorpora nuevos sonidos OGG de aparición, elimina los Opus sustituidos, permite controlar el volumen de cada one-shot y libera sonidos terminados o al apagar AudioManager. La búsqueda de OGG en `assets/audio/hirity/appear/` incluye los nuevos archivos; CMake copia recursivamente `assets` al directorio de ejecución. La reaparición usa un cooldown de 30 segundos, y las condiciones de distancia e interacción se mantienen bajo `puedeProcesarColisiones`, activado solo en host/offline; los clientes réplica no recorren candidatos ni intentan teletransportar cuerpos cinemáticos. `Reiniciar()` limpia también el cooldown para evitar heredarlo entre escenas. Los sonidos de interacción host y cliente usan el helper atenuado según la distancia local.

La validación automatizada de audio, el copiado efectivo de los archivos al build, la regresión de reset del cooldown y la interacción multijugador con Hiriti quedan pendientes en el commit conjunto por el gate de seguridad.

## Auditoría de ramas remotas restantes

`MENU_RENDER`, `PUZZLE`, `Puzle2`, `VR`, `feat/linterna-texturas-ps1`, `feat/multijugador-lan-vr`, `feat/optimizacion-rendimiento-opengl` y `master` son antecesores de esta integración. `Issue#5` queda clasificada como `REDUNDANT`: su único commit exclusivo añade un indicador de mano basado en `getInteractuable()` y `assets/mano.png`, pero la integración ya tiene `dibujarHUDInteraccion()` con selección de objetivo más avanzada. La implementación antigua carga una textura por frame sin liberarla y no aporta funcionalidad exclusiva necesaria; no se importó su código ni el PNG. No quedan ramas remotas `PENDING` o `BLOCKED` según `git branch -r --no-merged HEAD` y la revisión de ese commit.

## Validación automatizada de Windows

Las compilaciones de referencia se ejecutaron desde la raíz con los lanzadores oficiales antes de incorporar `d2e55e8` y registrar `EscalaUITests`:

```powershell
.\run-windows.cmd -NoRun
.\run-windows.cmd -Configuration Release -NoRun
```

CMake informó `ENet: encontrado; transporte real habilitado`, `OpenXR: ON` y `meshoptimizer::meshoptimizer`. vcpkg encontró ENet, Bullet, meshoptimizer y OpenXR ya instalados para `x64-windows`; no se desactivó ninguna dependencia.

| Validación | Resultado |
|---|---|
| Build Debug | PASS |
| Build Release | PASS |
| CTest Debug | PASS, 19/19 |
| CTest Release | PASS, 19/19 |
| `NetJuegoTests`, Debug/Release | PASS, 20/20 en cada configuración |
| `ENetLoopbackTests`, Debug/Release | PASS, 10/10 en cada configuración |
| `EscalaUITests` | NOT RUN; prueba añadida después de la validación de referencia |
| `git diff --check` después de integrar las ramas | PASS |

Los conjuntos completos incluyen `SceneLoaderTests`, `ObjLoaderTests`, tests del editor, `PhysicsTests`, `AnimacionHiritiTests`, `AudioDecoderTests`, `OpenXRTests`, `NetworkTests` y ENet loopback.

Estos PASS son evidencia del commit anterior, no de la integración actual. La validación final de Debug/Release, CTest, recursos de audio y prueba nueva DPI debe repetirse cuando Kaspersky finalice el análisis completo sin acciones pendientes.

## Validación manual pendiente

Orca no reconoció las ventanas GLFW de EscapeTheUAB para capturarlas o interactuar. Se iniciaron host y cliente Release por loopback: el host abrió UDP `0.0.0.0:7777` y el cliente abrió un socket UDP propio; esto no prueba el handshake ni el movimiento del avatar. Se cerraron los dos procesos iniciados para la prueba.

Antes de aprobar la integración en `master`, valida manualmente:

1. En Release, iniciar `EntornVGI.exe --host --port 7777 --name Host` y, en otra ventana, `EntornVGI.exe --connect 127.0.0.1:7777 --name Cliente`.
2. Confirmar ambos nombres en F4; mover avatares; recoger y lanzar objetos desde los dos jugadores; observar trayectoria, colisiones, reposo y propietario.
3. Probar sincronización de puertas y editor, reconexión, liberación de objetos sostenidos, Hirity, puzles, audio y modo offline.
4. Repetir en monitor 1080p y 4K: menú, pausa, fuentes/contraste, controles, volumen, HUD, editor, paneles, fullscreen, maximización, resize y cambio entre monitores. Confirmar que hitboxes siguen los controles visibles, que el panel multijugador se puede mover y que la escena conserva el FBO 960×540.
5. Validar OpenXR solo con runtime y visor presentes. Para LAN/Tailscale entre equipos, seguir las guías enlazadas arriba.

No se realizaron pruebas gráficas de 1080p/4K, dos equipos, Tailscale, hardware VR ni compilaciones macOS/Linux. `OpenXRTests` pasa sin demostrar una sesión real de visor.

## Incidencia de Kaspersky

Kaspersky informó que terminó el proceso de la detección anterior, guardó una copia de seguridad y eliminó el `SceneLoaderTests.exe` generado manualmente en `build/lighting-validation`. No se restauró ese archivo, no se añadió una exclusión y no se concluyó que fuera un falso positivo. La interfaz de Kaspersky muestra el análisis rápido completado sin amenazas (17.754 archivos) y el análisis completo todavía en curso: 9 %, 1.241.186 archivos analizados y aproximadamente 9 horas restantes en la última consulta de solo lectura. No se debe interpretar ese estado como un análisis completo limpio ni iniciar nuevas compilaciones o pruebas hasta que termine y no muestre acciones pendientes.
