# Arquitectura de EscapeTheUAB

Documento inicial. Describe la arquitectura **actual** y propone una dirección. La arquitectura propuesta **todavía no existe** en el código. El plan por etapas está en [REFACTOR_MIGRATION_MAP.md](REFACTOR_MIGRATION_MAP.md) y el árbol destino con sus lotes de movimiento en [REFACTOR_PHYSICAL_PLAN.md](REFACTOR_PHYSICAL_PLAN.md).

## 0. Base, método y evidencia

| Dato | Valor |
|---|---|
| Fecha del inventario | 2026-11-25 |
| Commit base | `62a7018571559df2490bf2a879734bed647bfe10` |
| Método | Lectura estática. Para este documento no se compiló, no se ejecutaron pruebas y no se arrancó el juego |
| Fuentes | Auditoría de arquitectura de Claude (Sonnet 5.5) sobre la misma base; conclusiones de viabilidad de Codex transmitidas por el coordinador (§1) |

El inventario es una foto de ese commit. Cuando el código cambie, manda el código.

Marcas de evidencia:

- **[C]** confirmado leyendo código o ficheros del commit base.
- **[H]** inferencia, heurística o consecuencia no ejecutada.
- **[D]** afirmación histórica de la documentación del repositorio, no reverificada.
- **[P]** propuesta: no está implementada.

Las tablas descriptivas sin marca son [C] según la auditoría. Al redactar este documento se volvieron a comprobar: la detección de la raíz de recursos, el hash de identidad de escena, las constantes de cabecera y los códigos de evento del protocolo, las constantes de las 17 luces, el ciclo `material`/`glut_geometry`, que solo `main.cpp` incluye `main.h`, el orden del fotograma y de la limpieza, los ficheros de CMake y el procesamiento de eventos OpenXR. El resto se toma de la auditoría. La auditoría declara que no leyó línea a línea el bloque VGI de `main.cpp` ni el interior de varios `.cpp` grandes (`glut_geometry`, `objLoader`, `escena`, `ModeloGLB`, `ObjetosAnimadosGLB`, `SceneLoader`, `MundoFisico`, `EditorEscenas`, `OpenXRManager`).

## 1. Decisiones vigentes

Las conclusiones de viabilidad de Codex fijan el marco:

1. Se conserva **un único ejecutable** (`EntornVGI`).
2. En ejecución, las rutas de recursos siguen **ancladas a la raíz de recursos**. En el árbol fuente, los ficheros se mueven por lotes según el plan físico.
3. La **primera migración son bibliotecas CMake** sobre los ficheros en su sitio.
4. Los ficheros se mueven **después** de que existan las costuras entre targets y sus pruebas; un módulo que ya las tenga no espera al resto.

Decisiones del usuario, transmitidas por el coordinador el 2026-11-25 (los puntos 2 y 4 se enmendaron para alinearse con ellas):

5. La **reorganización física es un resultado obligatorio**. Extraer targets CMake es solo un paso intermedio.
6. El alcance incluye fuentes y cabeceras, recursos, shaders, pruebas, documentación, herramientas y terceros.
7. Los **nombres en inglés son obligatorios para el código propio** (etapa P12): código y módulos del equipo, adaptadores, pruebas, herramientas y documentación, incluidos sus nombres de fichero y carpeta. `src/legacy_vgi/` contiene solo la base académica VGI original, que conserva sus identificadores y API. El código de terceros permanece en `third_party/`, sin modificar ni traducir. El **cuerpo** de la documentación conserva su idioma actual; los nombres oficiales y los términos de contrato se mantienen. Los contratos de §4 y los textos visibles para el jugador no cambian.
8. Se mantienen el **ejecutable único**, la disposición `build/<preset>/bin/` y el contrato de recursos en ejecución, incluida la comprobación de la raíz (§4.2).

## 2. Autorización y estado de aprobación

| Asunto | Estado a 2026-11-25 |
|---|---|
| Inicio de un refactor **provisional** | **Autorizado por el usuario** aunque la QA manual siga pendiente (transmitido por el coordinador al encargar este documento) |
| Continuación de las etapas arquitectónicas provisionales | **Autorizada por el usuario**: B0 a B5, B2b, G1, S1 a S10, P3-SH y P4 a P12, mientras cada una sea verificable técnicamente y supere su puerta propia ([mapa §4](REFACTOR_MIGRATION_MAP.md)), sin esperar la QA manual ni pedir aprobación después de cada corte superado |
| QA manual integral | **PENDIENTE** |
| GO funcional | **PENDIENTE** |
| Consolidación en `master` | Sin autorización registrada. [ESTABILIZACION_PRE_REFACTOR.md](ESTABILIZACION_PRE_REFACTOR.md) pide no consolidar hasta completar la validación de agarre [D] |

`ESTABILIZACION_PRE_REFACTOR.md` dice textualmente que no se inicie el refactor hasta repetir el protocolo de agarre, y la auditoría calificó la ejecución como NO-GO salvo autorización expresa. La autorización del usuario cubre el inicio y la **continuación provisional** de las etapas arquitectónicas citadas en la tabla. No convierte ninguna comprobación pendiente en superada: P0 y la QA manual de gameplay, el GO funcional y la publicación en `master` siguen **pendientes**. La QA manual es una puerta de entrega funcional, no una parada del refactor.

Pendiente según ese documento [D]: selección, agarre y lanzamiento confirmados de un objeto normal desde el cliente; comparación visual simultánea de transformaciones host/cliente; reanudación observada del seguimiento de Hirity al soltarlo; prueba del registro persistente dentro del juego; presentación gráfica de la tabla F4; 1080p/4K y pantalla completa; dos equipos físicos y Tailscale; OpenXR con runtime y visor.

**Consecuencia:** un fallo de agarre detectado durante el refactor no se podrá atribuir con seguridad al refactor o al estado previo si antes no se fijan las líneas base de la etapa P1 del mapa de migración.

## 3. Arquitectura actual

### 3.1 Targets de compilación

| Target | Tipo | Contenido |
|---|---|---|
| `EntornVGI` | Ejecutable | Casi todo el código propio e ImGui |
| `EntornVGI_network` | Biblioteca estática | `NetProtocol`, `NetSession`, `NetService`, `ENetTransport` |
| `EntornVGI_bullet`, `EntornVGI_openxr` | Interfaz | Dependencias externas |
| `EntornVGI_runtime_resources` | Custom | Copia `assets/` a `build/<preset>/bin/assets/` |
| `soil2`, `nfd` (solo Linux) | Terceros | |

No existe `src/main.cpp` ni ninguna biblioteca propia de escena, física, gameplay o render.

### 3.2 Vista de componentes

```text
                     EntornVGI (ejecutable)
 main.cpp + main.h  (composición, estado global, gameplay, autoridad, render, UI, entrada)
   ├─> Editor/*        ──> SceneLoader, ObjetosAnimadosGLB, ImGui, ImGuizmo, GLFW
   ├─> EscenaFisica    ──> MundoFisico (Bullet, pimpl), SceneLoader, ObjetosAnimadosGLB
   ├─> NetJuego        ──> NetJuegoLogica.h ──> VistaVR.h
   │                   └─> EntornVGI_network: Service ─> Session ─> Protocol
   │                                          └─> ENetTransport (ITransport)
   ├─> OpenXRManager   ──> VistaVR.h
   ├─> AudioManager    ──> MovimientoFPS.h
   ├─> DibujoGLB, DibujoColisionadores, NetAvatar, objLoader ─> ObjMeshOptimization
   └─> Base VGI: escena, visualitzacio, glut_geometry <──> material, shader, quatern
```

Las aristas de llamadas entre unidades salen de la lista de `#include` y de un análisis heurístico de funciones libres que no cubre métodos de clase [H].

### 3.3 Código propio por área (`EntornVGI/`)

| Área | Ficheros |
|---|---|
| Composición y todo lo no extraído | `main.cpp` (9 304 líneas en el HEAD 1e2912a; 10 212 en el checkpoint 62a7018), `main.h` (413) |
| Base académica VGI | `escena`, `visualitzacio`, `glut_geometry`, `material`, `shader`, `quatern`, `constants.h`, `framework.h`, `*_compat.h` |
| Modelos y animación | `objLoader`, `ObjMeshOptimization`, `ModeloGLB`, `ObjetosAnimadosGLB`, `DibujoGLB`, `AnimacionGLB.h`, `AnimacionPuerta.h` |
| Escena y física | `SceneLoader`, `MundoFisico`, `EscenaFisica`, `DibujoColisionadores` |
| Gameplay puro (cabeceras) | `MovimientoFPS.h`, `AnimacionHiriti.h`, `TemporizadorAmbiente.h` |
| Render auxiliar | `FrustumCulling.h`, `UniformCache.h`, `RenderMetrics.h` |
| Red | `src/networking/transport/`; `Network/` conserva `GrabDiagnostics`, `PreferenciasRed` y `Game`; `NetJuego`, `NetJuegoLogica.h`, `NetAdaptadores.h`, `NetAvatar` |
| Editor | `Editor/` |
| VR | `OpenXRManager`, `VistaVR.h`, `VistaEstereo.h` |
| Audio y UI | `AudioManager`, `EscalaUI.h` |

`Network/GrabDiagnostics` y `Network/PreferenciasRed` están en `Network/`, pero se compilan en el ejecutable, no en `EntornVGI_network`.

### 3.4 Responsabilidades actuales de `main.cpp`

| Zona aproximada | Responsabilidad | Símbolos de referencia |
|---|---|---|
| 1–104 | Includes, recursos globales, raíz de recursos | `SetWorkingDirectoryToAssetRoot` |
| 107–1056 | Métricas GPU, luces, oclusores, interacción, agarre, Hirity, cursor | `actualizarLucesDeEscena`, `actualizarOclusoresPhong`, `interactuar`, `actualizarHiriti` |
| 1056–2077 | Movimiento, pose VR, **replicación y autoridad**, lanzamiento, carga de escena | `iniciarRed`, `actualizarRed`, `lanzarObjetoCargado`, `cargarYAplicarEscena` |
| 2078–2505 | Callbacks de ventana, inicialización GL | `InitGL` |
| 2506–3189 | Estéreo, VR, dibujo de escena | `dibujarEscenaVR`, `OnPaint`, `dibuixa_Escena` |
| 3190–4176 | HUD, menús del juego, panel F4 | `draw_Menu_ImGui` |
| 4177–9632 | Interfaz y entrada de la base VGI (más de la mitad del fichero) | `ShowEntornVGIWindow`, `On*`, `Teclat_*`, `OnKeyDown` |
| 9633–10212 | `main()`: inicialización, FBO PS1 960×540, bucle, limpieza | `main` |

La lógica de mayor riesgo (autoridad de agarre, aplicación de réplicas, Hirity, carga de escena) vive aquí y **ninguna prueba incluye `main.cpp`**.

### 3.5 Orden del fotograma (contrato de comportamiento)

1. `glfwPollEvents`, pausa, editor, cursor, carga de lanzamiento.
2. Solo en `ESTADO_JUGANDO`: agachado, movimiento, animaciones, eventos y gatillos XR, cinemáticos, agarre local, paso de física, cámara desde la cápsula.
3. `actualizarRed` (siempre).
4. Hirity, audio ambiente, objetivo de interacción, luces.
5. ImGui y peticiones del editor.
6. Render VR o escritorio (FBO PS1 y blit), ImGui nativo, swap.

La física solo avanza en `ESTADO_JUGANDO`: un host en menú o pausa congela el mundo para todos (`PausarMundoJuego`). Con `--host` se arranca en el menú principal.

Orden de limpieza: ImGui, red, OpenXR, NFD, modelos y escena, jugador y `escenaFisica`, recursos GL, programas de shader, ventana, GLFW.

### 3.6 Estado global y acoplamientos

- Aproximadamente 222 variables mutables de ámbito de namespace entre `main.h` y `main.cpp` [H].
- `main.h` **define** variables en la cabecera. Funciona porque solo la incluye `main.cpp`; un segundo `.cpp` que la incluya rompe el enlace.
- La cámara es estado de gameplay: red, agarre, raycast, VR, audio y reaparición leen `n` (punto mirado) y `opvN` (ojo).
- Hay `static` locales con estado que sobreviven a reconexiones y cambios de escena.
- Ciclo real de enlace `material.cpp` ↔ `glut_geometry.cpp` (`SetColor4d` / `SeleccionaColorMaterial`).
- `SceneLoader.h` incluye `MundoFisico.h` por los enums de tipo y forma de cuerpo.
- `NetJuegoLogica.h` incluye `VistaVR.h` y mezcla códec, reglas de pausa, arbitraje y geometría de avatares.
- `AudioManager.h` incluye `MovimientoFPS.h`.
- `EntornVGI_network` publica como include todo `EntornVGI/`.
- Acoplamiento por índice (`EstadoDinamico::indiceObjeto` es la posición en `objetosEscena`) y por nombre (literal `"hiriti"` en `main.cpp`).

Costuras ya útiles que se conservan: `net::ITransport`, `netjuego::IPuertasEscena`, pimpl en `MundoFisico` y `GestorOpenXR`, callbacks de carga del editor.

## 4. Contratos que no se alteran

### 4.1 Red

| Contrato | Valor |
|---|---|
| Cabecera | magic `0x5445`, versión 2, 10 bytes, little-endian, paquete ≤1200 |
| Canales ENet | 0 pose (no fiable), 1 eventos (fiable), 2 dinámica (no fiable) |
| Tipos de mensaje | Hello 1, Welcome 2, Reject 3, PoseUpdate 10, PoseSnapshot 11, DynamicSnapshot 12, Event 20, PeerDisconnected 30, RecoveryRequest 31, RecoverySnapshot 32 |
| Límites | 8 poses, 16 dinámicos, 256 B de payload, 64 entradas de recuperación, nombre 32, escena 64 |
| Eventos | puerta `0x0100–0x0101`; objeto de prueba `0x0110–0x0113`; objeto de escena `0x0120`; agarrar/soltar `0x0130–0x0131`; propiedad `0x0132`; estado físico final `0x0133` |
| IDs | `HashId` = FNV-1a 32 del `id` JSON; rango reservado `0x53480000–0x5348000F` |
| Identidad de escena | `ET2-%08X`: hash del nombre del fichero de escena (sin extensión) y de los IDs ordenados de los objetos. Los `animated` no entran |
| Cadencias | pose de cliente ≤30 Hz; snapshots de host 25 Hz; interpolación 100 ms (avatares) y 120 ms (física) |
| Autoridad | Solo el host simula y valida agarres; lanzamiento ≤15 m/s |
| CLI | `--host`, `--connect HOST[:PUERTO]`, `--port`, `--name`; puerto por defecto 7777 |

Renombrar un fichero de escena o un `id` rompe la compatibilidad entre host y cliente.

### 4.2 Datos y rutas

| Elemento | Contrato |
|---|---|
| Raíz de recursos | Se exige `assets/shaders/gouraud_shdrML.vert` **y** `assets/scenes/aula_informatica_calibrada.json`; después todo es relativo al directorio de trabajo |
| Disposición de salida | `build/<preset>/bin/` con el ejecutable y los recursos copiados |
| Escena JSON | Claves raíz `version, name, playerSpawn, indirectLighting, architecture, objects, animated`; nombres de campos sin cambios; rutas de modelo relativas al JSON |
| Guardado del editor | Temporal, copia `.bak`, sustitución atómica y copia al árbol fuente mediante `ENTORNVGI_RUNTIME_ASSETS_DIR` y `ENTORNVGI_SOURCE_ASSETS_DIR` |
| Preferencias y registros | `multijugador.ini` en la carpeta de usuario; registros de agarre según [MULTIPLAYER_DEBUG_GRAB.md](MULTIPLAYER_DEBUG_GRAB.md) |
| Variables de entorno | `ENTORNVGI_DEBUG_GRAB`, `ENTORNVGI_DEBUG_GRAB_PERSIST`, `ESCAPETHEUAB_EDITOR_DEBUG` |

### 4.3 Comportamiento

- **Física:** Z arriba, metros, g = −9,81, paso fijo 1/60. En el cliente los dinámicos son cinemáticos. `EscenaFisica::Crear` es atómico; reconstruir la física reemplaza todos los cuerpos.
- **Luces:** `NUM_MAX_LLUMS = 17`, `MaxLights 17` en `flat.vert`, `gouraud.vert`, `phong.vert` y `phong.frag`, y `primerSlot = 4` cambian juntos. La escena calibrada usa 13 luces [D].
- **Shaders:** GLSL 330 core; los nombres de uniforms, el bloque `std140` de oclusores y los índices de atributos son contrato C++↔GLSL.
- **Controles:** F1 paneles, F2 estéreo, F3 OpenXR, F4 red, F6 editor, E interactuar, F linterna, Esc pausa, WASD, Shift, Ctrl. Los atajos VGI se conservan.
- **Funcionalidad académica VGI:** cámaras, proyecciones, primitivas, curvas, materiales, luces, shaders y menú "EntornVGI". El render del juego depende de ella (`Vista_Navega`, `Projeccio_Perspectiva`, `Iluminacio`, `dibuixa_EscenaGL`), así que no se puede retirar. Las partes incompletas de la base se conservan como están.

## 5. Módulos objetivo y dirección de dependencias [P]

Nada de esta sección está implementado. Los nombres y las carpetas pueden ajustarse al ejecutarlo.

| Módulo | Responsabilidad | Origen actual | Puede depender de |
|---|---|---|---|
| `app` | Ventana, argumentos, composición, bucle, ciclo de vida | `main()`, callbacks | Todos |
| `core` | Tipos y matemática sin dependencias de motor: tipos de cuerpo, IDs, hash, rutas | Dispersos; solo se crea cuando tenga contenido | Nada propio (GLM) |
| `scene` | Datos de escena, carga y guardado | `SceneLoader`, `SerializadorEscenas`, `cargarYAplicarEscena` | `core` |
| `animation` | Modelos GLB y animación | `ModeloGLB`, `ObjetosAnimadosGLB`, `Animacion*` | `core` |
| `physics` | Mundo Bullet y física de escena | `MundoFisico`, `EscenaFisica` | `core`, `scene`, `animation` |
| `gameplay` | Control del jugador, interacción, agarre, Hirity, temporizadores | `main.cpp`, `MovimientoFPS.h`, `AnimacionHiriti.h` | `core`, `scene`, `physics` |
| `networking/transport` | Protocolo, sesión, servicio, ENet | `EntornVGI_network` | `core` |
| `networking` (juego) | Sincronización de escena, réplica cliente, autoridad del host | `NetJuego`, `NetJuegoLogica.h`, `actualizarRed` | `transport`, `gameplay`, `physics`, `scene` |
| `rendering` | Render, luces, oclusores, FBO PS1, modelos GLB, avatares | `dibuixa_Escena`, `DibujoGLB`, `NetAvatar` | Consume estado |
| `audio` | Reproducción | `AudioManager` | `core` |
| `vr` | OpenXR, vistas, entrada VR | `OpenXRManager`, `VistaVR.h` | `core` |
| `ui` | HUD, menús, paneles | `draw_Menu_ImGui` | Consume estado |
| `editor` | Editor de escenas | `Editor/` | `scene`, `animation` |
| `diagnostics` | Registro de agarre, panel de dinámicos, colisionadores | `GrabDiagnostics`, `DibujoColisionadores` | Lectura de estado |
| `legacy_vgi` | Solo la base académica original, sin traducir ni renombrar | Base VGI, `objLoader` y bloque VGI de `main.cpp` | Sus ficheros y la capa de soporte del equipo |

Dirección (la flecha va **del que depende al dependido**):

```text
app ──> networking(juego) ──> gameplay ──> physics ──> scene ──> core
app ──> rendering, audio, ui, vr, editor, diagnostics
physics ──> animation ──> core
networking(juego) ──> networking/transport ──> core
rendering ──> legacy_vgi          (solo rendering y app ven legacy_vgi)
legacy_vgi ──> soporte del equipo
```

La «capa de soporte del equipo» es código propio que hoy usa la base académica (compatibilidad de plataforma, optimización de mallas, caché de uniforms, métricas). Parte depende de GLEW, así que no encaja en `core` tal como se define arriba; su ubicación está abierta ([plan físico](REFACTOR_PHYSICAL_PLAN.md), I3).

Reglas:

- `gameplay` no incluye `rendering`, `audio`, `ui` ni `vr`; emite estado o eventos.
- `transport` no conoce gameplay, OpenGL, Bullet ni OpenXR.
- `editor` y `diagnostics` solo los compone `app`.
- No se introducen dependencias circulares nuevas.

Interfaces mínimas, sin ECS ni bus de eventos genérico: `ViewPose`/`PlayerState` en lugar de leer `n`/`opvN`; `SceneState` pasado por referencia; una política de autoridad offline/host/cliente en lugar de comprobaciones dispersas; una salida de eventos de audio para Hirity.

Esta es la dirección **objetivo**. La primera etapa de bibliotecas sigue las dependencias reales de §3.6, no esta tabla.

## 6. Propiedad de `main.cpp`

**Hoy [C]:** `EntornVGI/main.cpp` es el punto de composición y además contiene reglas de gameplay, autoridad de red, render, UI y entrada.

**Objetivo [P]:** queda como composición (inicializar, conectar módulos, ordenar el fotograma, destruir). El orden de §3.5 y el orden de limpieza permanecen visibles en un solo sitio.

**Reglas durante la migración:**

- El **supervisor (Codex) es el único que edita e integra** `main.cpp`, `main.h` y los ficheros CMake compartidos.
- Claude Sonnet hace la revisión de arquitectura y la revisión cruzada.
- Los agentes de funcionalidad trabajan en rutas disjuntas y proponen los cambios en esos ficheros compartidos; no los editan.
- Se extrae primero con parámetros explícitos y sin reescribir; la limpieza posterior va en otro commit.
- Los arreglos funcionales no se mezclan con movimientos.

## 7. No objetivos

- Sin cambios de gameplay, equilibrio, controles, arte, audio ni iluminación.
- Sin renombrar assets, shaders ni texturas: sus nombres y rutas relativas son contratos. Sí se mueven dentro del árbol fuente (lote B4 del [plan físico](REFACTOR_PHYSICAL_PLAN.md)) sin cambiar las rutas en ejecución. Sin borrar los que parezcan no usados (el editor enumera `assets/models` y el audio se referencia desde código).
- Sin cambios de protocolo ni de contrato JSON, IDs o nombres de escena.
- Sin modificar ni traducir código de terceros, y sin traducir el cuerpo de la documentación existente.
- Sin sustituir bibliotecas, motor de render ni sistema de build.
- Sin corregir dentro del refactor las observaciones de §10.
- Sin completar la funcionalidad VGI incompleta.
- Sin reescribir la historia de git ni limpiar artefactos versionados en el mismo cambio.

## 8. Plataformas y OpenXR

| Plataforma | Estado y advertencias |
|---|---|
| Windows | Contexto 4.6 core con respaldo 3.3. Única plataforma con builds y pruebas reportadas [D] |
| macOS | 4.1 core forward-compatible, ImGui con `#version 150`, filtrado de X11 en GLEW, frameworks de audio. **Sin compilar ni ejecutar** [D]. Hay llamadas de función fija en curvas y `glLineWidth` > 1 que podrían fallar en core [H] |
| Linux | Dependencias del sistema, NFD GTK3 vendorizado (`ENTORNVGI_USE_LEGACY_NFD`). **Sin validar** [D] |
| OpenXR | La sesión solo está implementada para Windows (WGL). Opcional: activado por defecto salvo en macOS. Activación explícita con F3. **Nunca probado con visor** [D] |

- El código de entrada VR usa gatillo derecho (agarrar y lanzar) e izquierdo (linterna). [OPENXR.md](OPENXR.md) y la cabecera de `OpenXRManager.h` aún dicen que no hay mandos: documentación desfasada.
- `ProcesarEventos()` solo se llama en `ESTADO_JUGANDO`, mientras que el render VR se intenta siempre. En menú o pausa podrían no atenderse cambios de sesión XR [H].
- Al mover ficheros: cuidado con sistemas de ficheros que no distinguen mayúsculas (`skybox.VERT`, `skybox.FRAG`) y con la normalización Unicode en macOS (`peça*`).
- `EntornVGI.slnx` y `.vcxproj` no listan varias fuentes actuales; probablemente no enlazan [H].

## 9. Pruebas: qué existe y qué se ha reportado

CTest registra en el commit base, en Windows y con ENet disponible, 21 pruebas. `ENetLoopbackTests` solo se registra con ENet.

**Resultados históricos [D]**, tomados de `ESTABILIZACION_PRE_REFACTOR.md` y no repetidos para este documento: para el código de `942d0d0`, CTest 21/21 en Debug y Release, `NetJuegoTests` repetido 20/20 y `ENetLoopbackTests` 10/10 en ambas configuraciones. Entre `942d0d0` y la base `62a7018` no hay diferencias fuera de `docs/` y `README.md` [C].

Cobertura que falta:

- Sin prueba directa: `main.cpp`, `DibujoGLB`, `DibujoColisionadores`, `NetAvatar`, `AudioManager` (solo el decodificador), `AnimacionPuerta.h`, compilación y enlace de shaders.
- `ObjMeshOptimizationTests` comprueba solo con `assert()`: en Release no verifica nada.
- `ObjLoaderTests` devuelve 77 (omitida) sin contexto gráfico; una omisión no es un éxito.
- 13 targets de prueba listan `.cpp` de producción por ruta.

## 10. Observaciones registradas (no se corrigen en el refactor)

| # | Observación | Evidencia |
|---|---|---|
| O1 | La identidad de escena no incluye los IDs `animated`; escenas con puertas distintas podrían pasar el saludo | [C]; efecto [H] |
| O2 | `ObjMeshOptimizationTests` no verifica en Release | [C] |
| O3 | No suenan pasos: `playFootstep` no se llama y los WAV no están en git | [C] |
| O4 | Eventos XR no atendidos fuera de PLAY | [C] estructura; efecto [H] |
| O5 | `FijarPropietarioDinamico` ignora el resultado de `Encolar` | [C]; efecto [H] |
| O6 | Demos de puerta y cubo en la ruta de render de producción | [C] |
| O7 | `.vscode/launch.json` referencia tareas que no existen | [C] |
| O8 | `.gitignore` excluye patrones con excepciones y ficheros ya seguidos; revisar al añadir recursos | [C] |
| O9 | F11 no tendría efecto en modo FPS; la casilla "Full Screen?" solo cambia una variable | [C]; efecto [H] |

Deriva adicional, para tratar aparte y con permiso: README describe como escena inicial `habitacion_beta.json` y su apartado de estado, fechado el 5 de noviembre, dice que no se implementaron Bullet Physics ni OpenXR; hay copias `.bak`/`.TMP`, `x64/` versionado, un gitlink `ImDesignManager` sin `.gitmodules`, `third_party/OpenXR-SDK-main` sin referencias en el build y `ModelLoader.cpp` fuera de todo target.
