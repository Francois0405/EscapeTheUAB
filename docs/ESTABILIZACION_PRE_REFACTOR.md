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

La validación automatizada de audio y el copiado del recurso al directorio de ejecución quedan completados en la validación de la corrección documentada abajo. La interacción visual multijugador con Hiriti sigue pendiente de una sesión gráfica host/cliente.

### Compatibilidad de `heehee.ogg`

La única falla reportada por la validación manual del commit `18d935d` era `AudioDecoderTests` al abrir `assets/audio/hirity/appear/heehee.ogg`. FFprobe identificó el archivo original como Ogg Opus, 48 kHz, estéreo y 4,56 s. El recurso estaba completo y coincidía con el blob versionado; el problema era el códec Opus, que no es compatible con el decodificador stb_vorbis usado por `AudioDecoderTests`. Los otros dos OGG de aparición son Vorbis.

Se convirtió el recurso con la instalación local de FFmpeg 9.0.1 (`libvorbis`, calidad 4), conservando nombre, ruta, frecuencia, canales y contenido sonoro. Antes de sustituir el original, FFprobe informó Vorbis, 48 kHz, 2 canales y 4,5535 s; FFmpeg decodificó el archivo temporal completo sin error. El resultado convertido mide 27.991 bytes. No se añadió dependencia al motor ni se cambió el test. `AudioDecoderTests` y CTest pasaron en ambas configuraciones y confirmaron que el recurso copiado coincide con el fuente.

## Auditoría de ramas remotas restantes

`MENU_RENDER`, `PUZZLE`, `Puzle2`, `VR`, `feat/linterna-texturas-ps1`, `feat/multijugador-lan-vr`, `feat/optimizacion-rendimiento-opengl` y `master` son antecesores de esta integración. `Issue#5` queda clasificada como `REDUNDANT`: su único commit exclusivo añade un indicador de mano basado en `getInteractuable()` y `assets/mano.png`, pero la integración ya tiene `dibujarHUDInteraccion()` con selección de objetivo más avanzada. La implementación antigua carga una textura por frame sin liberarla y no aporta funcionalidad exclusiva necesaria; no se importó su código ni el PNG. No quedan ramas remotas `PENDING` o `BLOCKED` según `git branch -r --no-merged HEAD` y la revisión de ese commit.

## Validación automatizada de Windows antes de corregir el OGG

Esta tabla conserva el resultado manual reportado para el commit `18d935d`, que ya incluía `d2e55e8` y `EscalaUITests`, pero aún tenía el OGG Opus incompatible:

```powershell
.\run-windows.cmd -NoRun
.\run-windows.cmd -Configuration Release -NoRun
```

CMake informó `ENet: encontrado; transporte real habilitado`, `OpenXR: ON` y `meshoptimizer::meshoptimizer`. vcpkg encontró ENet, Bullet, meshoptimizer y OpenXR ya instalados para `x64-windows`; no se desactivó ninguna dependencia.

| Validación | Resultado |
|---|---|
| Build Debug | PASS |
| Build Release | PASS |
| CTest Debug | PASS, 19/20; falló `AudioDecoderTests` con el OGG Opus original |
| CTest Release | PASS, 19/20; falló `AudioDecoderTests` con el OGG Opus original |
| `NetJuegoTests`, Debug/Release | PASS, 20/20 en cada configuración |
| `ENetLoopbackTests`, Debug/Release | PASS, 10/10 en cada configuración |
| `EscalaUITests` | PASS (validación manual reportada) |
| `git diff --check` después de integrar las ramas | PASS |

Los conjuntos completos incluyen `SceneLoaderTests`, `ObjLoaderTests`, tests del editor, `PhysicsTests`, `AnimacionHiritiTests`, `AudioDecoderTests`, `OpenXRTests`, `NetworkTests` y ENet loopback.

La fila 19/20 registra el resultado previo a convertir `heehee.ogg`; el resto de esta tabla también es solo la referencia reportada para ese estado.

## Validación final de la corrección de audio

Los lanzadores oficiales se ejecutaron sobre el recurso Vorbis convertido. vcpkg conservó ENet real, Bullet, Meshoptimizer y OpenXR habilitados. La primera configuración falló porque Windows denegó la extracción de herramientas en la caché global de vcpkg; al usar `VCPKG_DOWNLOADS` dentro de `build/` y repetir el mismo lanzador con acceso de ejecución concedido, Debug y Release configuraron y compilaron correctamente.

| Validación del árbol con la corrección | Debug | Release |
|---|---:|---:|
| `run-windows.cmd -NoRun` | PASS | PASS |
| `AudioDecoderTests` | PASS | PASS |
| CTest completo | PASS, 20/20 | PASS, 20/20 |
| `NetJuegoTests --repeat until-fail:20` | PASS, 20/20 | PASS, 20/20 |
| `ENetLoopbackTests --repeat until-fail:10` | PASS, 10/10 | PASS, 10/10 |

`EscalaUITests`, `AnimacionHiritiTests`, `PhysicsTests`, `SceneLoaderTests` y las demás pruebas están incluidas en el CTest completo y pasaron en ambas configuraciones. El archivo sincronizado a `build/windows-debug/bin/assets/audio/hirity/appear/heehee.ogg` y a su contraparte Release tiene el mismo SHA-256 que el recurso fuente (`4ADE833823A979755824E79F7B92C9E8E574685500097B438E6C9C21D4921E08`).

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

Kaspersky informó que terminó el proceso de la detección anterior, guardó una copia de seguridad y eliminó el `SceneLoaderTests.exe` generado manualmente en `build/lighting-validation`. No se restauró ese archivo, no se añadió una exclusión y no se concluyó que fuera un falso positivo. El usuario confirmó que el análisis rápido terminó sin amenazas y autorizó continuar mientras el análisis completo permanece en curso. Kaspersky permaneció activo. No se observó una nueva detección durante la conversión ni la validación documentada; este resultado no determina retrospectivamente si la detección anterior fue un falso positivo.

## Regresión de agarre y autoridad de Hirity

La QA manual del commit `af5b98d` reportó agarres defectuosos y a Hirity descontrolado en cliente. `af5b98d` no contiene cambios de gameplay frente a `18d935d`; la regresión es anterior a la corrección del OGG. La ruta de agarre remoto ya aparece en la implementación multijugador integrada: el host crea la sujeción Bullet autoritativa, pero la réplica de cliente es cinemática. Dos huecos explicaban los síntomas: (1) el destino remoto de escritorio seguía la mano sintética del avatar, aproximadamente a 0,5 m de la cabeza, en vez del punto de agarre local situado a 1,08 m delante del rayo; (2) el control de Hirity consultaba `ObjetoAgarrado()`, que solo identifica la sujeción local y no una restricción remota almacenada por cuerpo. El host podía entonces mantener el seguimiento autónomo o evaluar la reaparición mientras el cliente tenía a Hirity sujeto. `ca26104` limita correctamente la simulación autónoma al host/offline, pero no vinculó ese control con el propietario remoto. El cooldown de 30 s de `d2e55e8` hacía más visible el segundo problema; no se encontró una modificación de gameplay en `af5b98d`.

La corrección comparte `EscenaFisica::DestinoDeAgarre()` entre agarres locales y remotos. Los agarres de escritorio siguen la cabeza y el rayo; los agarres OpenXR siguen la pose real del mando derecho. La sujeción remota continúa siendo una restricción física en el cuerpo dinámico autoritativo del host: el cliente no añade una restricción al cuerpo cinemático. `ObjetoDinamicoSujeto()` consulta el estado de sujeción por cuerpo y, además, Hirity considera su propietario de red; esto suspende seguimiento y reaparición mientras el host u otro cliente lo sostiene. Un agarre físico aceptado descubre a Hirity incluso si el cliente/host no había coincidido en su detección visual. Al soltar, el host conserva la liberación, la velocidad de lanzamiento, el contacto y la reanudación condicionada del seguimiento. El objetivo de seguimiento en multijugador sigue siendo el jugador local del host, porque el host conserva la autoridad y usa su propia cápsula para el seguimiento/reaparición.

Se añadió diagnóstico opcional, sin mensajes por frame, con `ENTORNVGI_DEBUG_GRAB=1`. Las líneas `[grab] request`, `accepted`, `reject`, `holding`, `released`, `replica`, `hiriti` y `hiriti-respawn` informan ID, peer/propietario, posición, destino, clase de cuerpo, seguimiento y tiempo desde reaparición. La prueba de física ahora verifica que una sujeción remota reporta el cuerpo sujeto sin fingir agarre local, alcanza el destino común y restaura gravedad al liberar. `AnimacionHiritiTests` cubre el descubrimiento/agarre validado por física.

Validación de esta corrección en `integration/pre-refactor` sobre el árbol modificado:

| Prueba | Resultado |
|---|---|
| Lanzador oficial Debug (`run-windows.cmd -NoRun`) | PASS; ENet, Bullet, Meshoptimizer y OpenXR habilitados |
| Lanzador oficial Release (`run-windows.cmd -Configuration Release -NoRun`) | PASS; mismas dependencias habilitadas |
| CTest Debug | PASS, 20/20 |
| CTest Release | PASS, 20/20 |
| `NetJuegoTests`, CTest integrado Debug/Release | PASS, 1/1 en cada configuración; la repetición hasta 20 no se ejecutó sobre este HEAD |
| `ENetLoopbackTests`, CTest integrado Debug/Release | PASS, 1/1 en cada configuración; la repetición hasta 10 no se ejecutó sobre este HEAD |
| `PhysicsTests` y `AnimacionHiritiTests` | PASS en CTest completo de ambas configuraciones |
| Dos instancias Release: host/cliente en `127.0.0.1:17777` | PASS de conexión observado en F4: host escuchando con 1 remoto y cliente conectado con 1 remoto; ambas ventanas mostraron la escena |
| Agarre desde el cliente y sincronización física | NO VERIFICADO: tras una pulsación E con el objeto resaltado, el panel siguió indicando objeto libre y no hubo transición `[grab] request/accepted/reject`; Orca marcó la entrada sintética como `unverified` |
| Repeticiones de estabilidad de red | NO RUN sobre este HEAD: NetJuegoTests ×20 y ENetLoopbackTests ×10 por configuración siguen pendientes |

El commit `6b6af8f` corrige tres defectos confirmados estáticamente: la simulación autoritativa del host ya no se pausa solo por perder foco mientras sigue en PLAY (la entrada local sí se desactiva); la réplica conserva el subtipo local `Rebotador` cuando el host informa un cuerpo dinámico; y el host limita a 15 m/s el pequeño exceso de redondeo float32 sin aceptar excesos mayores. Los tests de NetJuego cubren estas reglas y el flujo de recepción de lanzamientos. El merge integrado es `2942290`.

La conexión host/cliente y la carga de la escena sí se observaron en ventanas reales. El agarre no quedó confirmado: el diagnóstico no registró una solicitud y Orca declaró no verificada la pulsación E, por lo que el resultado no distingue entre una entrada que no llegó a GLFW y una interacción rechazada antes de la ruta instrumentada. Antes de consolidar, ejecutar desde teclado físico el checklist de agarre en host y cliente; con `ENTORNVGI_DEBUG_GRAB=1`, confirmar `[grab] request` seguido de `accepted` y `holding ... host-dynamic-constraint-active`, y verificar posición/propiedad en ambas instancias. Para Hirity, confirmar `held=yes`, seguimiento suspendido mientras está sujeto y su reanudación al liberar. Los automatizados no sustituyen esa prueba gráfica.

### Reproducción gráfica adicional del 25 de noviembre

Se repitió la sesión con dos procesos Release en `127.0.0.1:17777`, después de entrar explícitamente en JUGAR en ambas ventanas; las instancias mostraron el mismo identificador de escena `ET2-876E0A88`. En una lectura simultánea de los registros, Hirity tenía el mismo ID persistente (`2738114548`) y la misma posición `(-6.49998,1.15,-2.85)` en ambas instancias: el host lo registró como `authoritative-dynamic`, y el cliente como `kinematic-replica`, sin propietario asignado. Durante la sesión también se observaron muestras intermedias de su movimiento del host al cliente. Esto confirma que la posición del snapshot llegó a la réplica del cliente en esta sesión local; no sustituye una revisión visual del movimiento durante el gameplay.

El agarre desde el cliente sigue **NO VERIFICADO**. El diagnóstico de pulsación E registró `game=1`, `allowed=1`, foco activo y la entrada permitida; sin embargo, el raycast alcanzó una puerta animada (`rayAnimatedIndex=4`) o cuerpos estáticos/suelo antes de seleccionar un objeto dinámico, por lo que no se emitió una solicitud de agarre. No se observó `accepted`, cambio de propietario ni sujeción Bullet; tampoco se verificaron lanzamiento, transferencia de ownership, desconexión sosteniendo un objeto ni agarre de Hirity. Orca pudo capturar las ventanas y entregar teclas/arrastres, pero marcó la entrada sintética como no verificada y el movimiento de ratón con cursor capturado produjo giros no controlables con precisión. Se cerraron las dos instancias de prueba; no queda una sesión usando el puerto 17777.

Se añadió al diagnóstico activado por `ENTORNVGI_DEBUG_GRAB=1` una línea por pulsación E que registra rol, gates de entrada, objetivo HUD y cuerpo/índice/ID/distancia/origen/dirección del raycast. No se imprime por frame. Los lanzadores oficiales Debug y Release y CTest completo (20/20 en ambas configuraciones) pasaron después de este cambio. No se considera resuelta ni descartada la regresión de agarre: repetir el protocolo con teclado/ratón físicos o un proveedor de control verificado y confirmar `[grab] request` → `[grab] accepted` → propietario cliente → restricción autoritativa del host → movimiento remoto y liberación. Hasta entonces, no iniciar el refactor arquitectónico ni consolidar en `master`.

En una repetición posterior con ambas instancias Release activas en `127.0.0.1:17777`, una pulsación E del cliente quedó registrada por el propio juego con `game=1`, `allowed=1` y `focused=1`. El raycast de esa pulsación alcanzó el cuerpo 24 sin ID de objeto dinámico (`rayDynamicIndex=-1`, `rayObjectId=none`), el objetivo de interacción fue nulo (`action=0`) y el host no registró una solicitud. Esto confirma que la entrada y el nuevo diagnóstico funcionan; no prueba un fallo de ownership ni de la restricción, porque no se seleccionó un objeto interactivo. Sigue pendiente apuntar a un cuerpo dinámico y completar el ciclo de agarre, movimiento y liberación. La sesión mantuvo el puerto UDP 17777 ocupado por el host durante esta comprobación.

Se amplió `PhysicsTests` para apuntar con Bullet a una réplica cinemática después de aplicar un snapshot, tanto para un objeto `Dinamico` como para `Rebotador` (Hirity). La prueba confirma en ambas configuraciones Windows que el raycast aislado identifica el índice dinámico de la réplica y que el cuerpo continúa siendo cinemático. Compilación con `run-windows.cmd -NoRun` y resultado focalizado por CTest: Debug 1/1 PASS; Release 1/1 PASS. Esto descarta que la máscara de colisión del raycast excluya por sí sola las réplicas cinemáticas en ese escenario de física; no valida la ruta completa de input, solicitud ENet, autorización/segundo raycast del host, ownership, restricción, lanzamiento ni animación/seguimiento de Hirity en gameplay.

Validación automatizada del commit con el test nuevo: lanzadores oficiales Debug y Release completados con ENet, Bullet, Meshoptimizer y OpenXR habilitados; CTest completo 20/20 en ambas configuraciones. Repeticiones secuenciales: `NetJuegoTests` 20/20 en Debug y 20/20 en Release; `ENetLoopbackTests` 10/10 en Debug y 10/10 en Release. No hubo fallos ni concurrencia de pruebas de red. Estos resultados no sustituyen la validación gráfica de agarre ni verifican comunicación entre dos equipos físicos.

### Validación del debugger persistente — 25 de noviembre de 2026

Se integró `itsCarlosDev/multiplayer-debugger-codex` en el merge `4a2030877bfaedc51b5d731fe5a265e84140feef`, con persistencia optativa por rol/PID, límite de 1 MiB por archivo, marca monotónica `mono_ns`, eventos de agarre/liberación/desconexión y panel F4 de hasta ocho objetos dinámicos. El panel indica explícitamente que el seguimiento de Hirity es desconocido para la réplica cliente; no presenta ese dato como seguimiento confirmado. El logger conserva la salida de consola existente. La suite `GrabDiagnosticsTests` cubre escritura, marca temporal, separación de procesos y límite del archivo.

Validación ejecutada sobre el merge integrado:

| Prueba | Debug | Release |
|---|---:|---:|
| Lanzador oficial `run-windows.cmd -NoRun` | PASS | PASS |
| ENet, Bullet, Meshoptimizer y OpenXR configurados | PASS | PASS |
| CTest completo | 21/21 PASS | 21/21 PASS |
| `NetJuegoTests --repeat until-fail:20` | 20/20 PASS | 20/20 PASS |
| `ENetLoopbackTests --repeat until-fail:10` | 10/10 PASS | 10/10 PASS |
| `GrabDiagnosticsTests`, `PhysicsTests`, `EscalaUITests`, `AnimacionHiritiTests`, `AudioDecoderTests` | PASS en CTest completo | PASS en CTest completo |

Durante la validación del merge `4a20308`, el ejecutable Release generado en `build/windows-release/bin/EntornVGI.exe` se registró con tamaño 3.265.024 bytes y SHA-256 `69AD4893E5A94D6D42FF5767C95DE5D72A574EDB60A7C6F3B3436E3ECAD76ABD`. Ese archivo fue reemplazado después por el build de la corrección F4 descrito más abajo. Los ciclos de repetición de red se ejecutaron secuencialmente. Estas pruebas verifican la lógica automatizada y el transporte ENet local, no la interacción física ni el renderizado entre dos instancias gráficas.

El agarre/movimiento/lanzamiento de Hirity desde el cliente fue reportado satisfactorio en una sesión local anterior. En cambio, no se ha registrado una selección confirmada de objeto normal ni una comparación visual simultánea de transformaciones host/cliente. Tampoco se ha demostrado con movimiento observado que el comportamiento autónomo de Hirity reanude tras soltarlo; `tracking=eligible` solo indica elegibilidad. Por ello, el multiplayer debugger y sus pruebas automatizadas están implementados, pero la verificación manual integral permanece **PENDIENTE**. La comunicación entre equipos físicos/Tailscale y OpenXR con runtime/hardware también permanece pendiente.

Durante las builds MSVC mostró advertencias C4996 por llamadas a `getenv` en `GrabDiagnostics.cpp`; no impidieron las compilaciones y las pruebas pasaron. No se observaron nuevas alertas del antivirus durante esta validación; el antivirus permaneció activo.

La auditoría cruzada de `b908e48` detectó que la tabla dinámica titulada F4 solo se dibujaba junto con los paneles generales F1. Se corrigió la condición: cuando el panel multijugador está visible en FPS, la tabla dinámica aparece aunque F1 esté cerrado. La tabla dejó de dibujarse en el bloque F1, evitando duplicados. La variable `mostrarPanelRed` empieza activa; el panel multijugador se dibuja mientras esa variable está activa, sin depender del estado PLAY/menú/pausa. La tabla dinámica además requiere `modoJuegoFPS()`, que también se cumple en esos estados según la configuración actual. F4 y el cierre de la ventana multijugador ocultan ambos paneles, y F4 permite volver a abrirlos. La colocación visual en esos estados sigue pendiente de comprobar.

Tras la corrección, `run-windows.cmd -NoRun` y `run-windows.cmd -Configuration Release -NoRun` pasaron con ENet real, Bullet, Meshoptimizer y OpenXR habilitados. CTest completo volvió a pasar 21/21 en Debug y 21/21 en Release. El primer intento Release no pudo enlazar porque el host de QA seguía abierto; se cerró normalmente el proceso de prueba y el siguiente intento oficial pasó. La tabla F4 se verificó por compilación y revisión de su condición de dibujo, no mediante captura visual de GLFW; queda pendiente confirmar su presentación gráfica. El ejecutable Release de esta corrección está en `build/windows-release/bin/EntornVGI.exe`, tamaño 3.265.024 bytes, SHA-256 `9B3B9273333CF6C45CD41DE946C97ADCEECDB1A45FEA32A767B7B2AF1453DA47`, enlazado desde el código de `942d0d0`.

En el código de `942d0d0`, las repeticiones secuenciales de `NetJuegoTests` pasaron 20/20 en Debug y 20/20 en Release; `ENetLoopbackTests` pasó 10/10 en cada configuración. No se ejecutaron en paralelo.

Se intentó iniciar el host y cliente Release con ambos flags del logger persistente habilitados, pero Orca no enumeró las ventanas GLFW por PID y no fue posible enviar una interacción verificable. Como el logger escribe al producirse eventos y no se observó ninguno, no se crearon archivos de log: la persistencia está probada por `GrabDiagnosticsTests`, pero su smoke test dentro del juego queda **PENDIENTE**. Las instancias iniciadas se cerraron mediante WM_CLOSE; no quedan procesos de QA activos.
