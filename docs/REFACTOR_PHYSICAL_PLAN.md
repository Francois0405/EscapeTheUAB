# Plan de reorganización física de EscapeTheUAB

**El árbol y los lotes de este documento son un objetivo [P]. No están realizados:** no se ha movido, renombrado ni modificado ningún fichero de código, CMake o recursos para redactarlo. **La QA manual (P0) sigue PENDIENTE y no hay GO funcional.**

Complementa a [REFACTOR_MIGRATION_MAP.md](REFACTOR_MIGRATION_MAP.md) (etapas y registro de ejecución) y a [ARCHITECTURE.md](ARCHITECTURE.md) (arquitectura actual y contratos).

## 0. Base, evidencia y alineación con la documentación

| Dato | Valor |
|---|---|
| Base de la auditoría física y de este documento | `7ace570` |
| Estado de `refactor/project-architecture` | `2d83c7d`: `fe4d7da` (corte S4, [mapa §10](REFACTOR_MIGRATION_MAP.md)) más el commit documental de este plan |
| Método | Lectura estática y de objetos git. No se compiló ni se probó nada para este documento |

Marcas: **[C]** confirmado leyendo ficheros o git; **[H]** inferencia o aproximación no ejecutada; **[D]** afirmación histórica no reverificada; **[P]** propuesta; **Decidido**: acordado y todavía no ejecutado. Salvo lo recogido como decidido en la tabla siguiente, en §2.5, en el alcance de §6 y en ARCHITECTURE §1.5 a §1.8, las secciones 1 a 8 son [P].

| Asunto | Estado a 2026-11-25 |
|---|---|
| Reorganización física obligatoria y su alcance | **Decidido** (usuario, vía coordinador) |
| Nombres en inglés obligatorios para el código propio (P12) | **Decidido** (usuario, vía coordinador) |
| Lotes B2 a B5 antes de que termine la modularización | **Decidido** (orden encargado por el coordinador) |
| Terceros en `third_party/` sin modificar; `legacy_vgi` solo con la base académica original | **Decidido** (coordinador) |
| El cuerpo de la documentación conserva su idioma; nombres propios en inglés | **Decidido** (coordinador) |
| `origin/Issue#5` no se integra; la rama se conserva | **Decidido** (coordinador) |
| B2 y S1, S2, S5 en serie por `NetJuegoLogica.h` | **Decidido** (coordinador) |
| S10 tras la QA de agarre y como puerta de P8; P4 a P7 sin esperarlo | **Propuesta, pendiente de aprobación** |
| QA manual (P0) y GO funcional | **PENDIENTES** |

**Objetivo del usuario (transmitido por el coordinador):** reorganizar físicamente fuentes, cabeceras, recursos, shaders, pruebas, documentación, scripts y terceros; modularizar `main.cpp`; unificar en inglés los nombres del código propio; reducir globales y acoplamiento; permitir propiedad independiente por equipos. Extraer targets CMake es solo un paso intermedio.

**Enmiendas aplicadas** (decisiones transmitidas por el coordinador, 2026-11-25):

| Documento | Texto previo | Enmienda | Estado |
|---|---|---|---|
| Mapa §2 y §5 (P11) | Los ficheros se mueven al final; los recursos no se mueven | Un módulo con target y pruebas se mueve antes (B2 a B5); los recursos se mueven en B4 | Hecha |
| Mapa §5 (P12) | Nombres: opcional | Obligatorio para el código propio | Hecha |
| ARCHITECTURE §1 y §7 | Los ficheros se quedan donde están en las primeras etapas; no se renombran ni mueven assets, shaders ni texturas | Movimiento físico obligatorio; se mantienen nombres de recursos, ejecutable único, `bin/` y contrato de recursos en ejecución | Hecha |

## 1. Árbol destino del repositorio

```text
EscapeTheUAB/
├─ CMakeLists.txt  CMakePresets.json  vcpkg.json        (raíz de build, se conservan)
├─ run-windows.cmd  run-windows.ps1  run-linux.sh  run-macos.sh   (lanzadores, se conservan)
├─ README.md  .gitignore  .vscode/
├─ cmake/                      RuntimeResources.cmake, Warnings.cmake
├─ src/                        única raíz de include: "modulo/Fichero.h"
│  ├─ app/                     composición, bucle, argumentos, raíz de recursos, estado
│  ├─ core/                    tipos sin dependencias de motor; platform/
│  │                           (soporte gráfico propio usado por legacy_vgi: ubicación abierta, I3)
│  ├─ scene/                   datos de escena, carga y serialización
│  ├─ animation/               modelos GLB y animación
│  ├─ physics/                 mundo Bullet y física de escena
│  ├─ gameplay/                jugador, interacción, agarre, Hirity, temporizadores
│  ├─ networking/
│  │  ├─ transport/            protocolo, sesión, servicio, ENet
│  │  └─ game/                 sincronización de escena, réplica, autoridad del host
│  ├─ rendering/               render, modelos GLB, avatares, estéreo
│  ├─ audio/
│  ├─ vr/                      OpenXR y vistas VR
│  ├─ ui/                      HUD, menús, escala
│  ├─ editor/
│  ├─ diagnostics/             trazas de agarre, colisionadores, paneles
│  └─ legacy_vgi/              solo la base académica VGI original, sin traducir
├─ resources/
│  ├─ assets/                  {animations, audio, licenses, models, scenes}
│  ├─ shaders/
│  ├─ textures/
│  └─ ui/                      hand.png, hand.svg
├─ tests/
│  ├─ unit/
│  ├─ integration/
│  └─ graphics/
├─ tools/                      export_glb.py, measure_obj.py
│  └─ assets/                  scripts de preparación de assets
├─ docs/
├─ third_party/                imgui/ imguizmo/ nlohmann/ cgltf/ miniaudio/ SOIL2/ nativefiledialog/  (sin modificar)
└─ build/<preset>/bin/         (generado, no versionado)
      EntornVGI[.exe]  assets/  shaders/  textures/  res/ui/hand.png
```

Se conservan: un único ejecutable `EntornVGI`, `build/<preset>/bin/`, la disposición de recursos dentro de `bin/`, los presets y los lanzadores.

Cada módulo de `src/` y cada categoría de `tests/` tiene su propio `CMakeLists.txt` (patrón actual de `Network/`), para que los dos ficheros CMake centrales dejen de ser un cuello de botella.

**Nombres de carpeta prohibidos** bajo `src/`, `tests/`, `tools/` y `resources/`: `debug`, `release`, `bin`, `obj`, `log`, `logs`, `build`, `out`, `x64`. Las reglas actuales de `.gitignore` los excluyen [C].

## 2. Rutas actuales y destino

Columna **Sec.**: lote en que se mueve (§3). Los ficheros se renombran al inglés en el mismo commit que los mueve; los identificadores, en P12. `{h,cpp}` indica la pareja.

### 2.1 Código

| Actual (`EntornVGI/`) | Destino (`src/`) | Sec. | Nota |
|---|---|---|---|
| `Network/NetProtocol.{h,cpp}`, `NetSession.{h,cpp}`, `NetService.{h,cpp}`, `NetTransport.h`, `ENetTransport.{h,cpp}`, `CMakeLists.txt` | `networking/transport/`, mismos nombres | B2 | |
| `SceneLoader.{h,cpp}` | `scene/SceneLoader.{h,cpp}` | B5 | Antes, extraer los enums a `core` |
| `Editor/SerializadorEscenas.{h,cpp}` | `scene/SceneSerializer.{h,cpp}` | B5 | **I1** |
| `ModeloGLB.{h,cpp}` | `animation/GlbModel.{h,cpp}` | B5 | |
| `ObjetosAnimadosGLB.{h,cpp}` | `animation/GlbAnimatedObjects.{h,cpp}` | B5 | |
| `AnimacionGLB.h`, `AnimacionPuerta.h` | `animation/GlbAnimation.h`, `DoorAnimation.h` | B5 | |
| `MundoFisico.{h,cpp}` | `physics/PhysicsWorld.{h,cpp}` | B5 | **I2** |
| `EscenaFisica.{h,cpp}` | `physics/ScenePhysics.{h,cpp}` | B5 | |
| (enums de tipo y forma de cuerpo en `MundoFisico.h`) | `core/PhysicsBodyTypes.h` | B5, commit previo | Enum compartido |
| `Editor/EditorEscenas.{h,cpp}` | `editor/SceneEditor.{h,cpp}` | B5 | |
| `Editor/CamaraEditor.h`, `HistorialEscena.h` | `editor/EditorCamera.h`, `SceneHistory.h` | B5 | |
| `Editor/LimitesModelo.{h,cpp}`, `SelectorObjetos.{h,cpp}` | `editor/ModelBounds`, `ObjectPicker` | B5 | |
| `Editor/ValidadorColocacion.{h,cpp}`, `VisibilidadEditor.{h,cpp}` | `editor/PlacementValidator`, `EditorVisibility` | B5 | |
| `RenderMetrics.h`, `UniformCache.h` | Capa de soporte del equipo, ubicación abierta | Antes de P6 | **I3**. Código propio; incluyen GLEW |
| `AudioManager.{h,cpp}` | `audio/` | P5 | Hoy incluye `MovimientoFPS.h` |
| `EscalaUI.h` | `ui/UiScale.h` | P5 | |
| `MovimientoFPS.h`, `AnimacionHiriti.h`, `TemporizadorAmbiente.h` | `gameplay/PlayerMovement.h`, `HiritiBehavior.h`, `AmbientTimer.h` | P5 | |
| `Network/GrabDiagnostics.{h,cpp}` | `diagnostics/` | P5 | |
| `DibujoColisionadores.{h,cpp}` | `diagnostics/ColliderRenderer.{h,cpp}` | P5 | |
| `escena`, `visualitzacio`, `glut_geometry`, `material`, `shader`, `quatern` (`.{h,cpp}`) | `legacy_vgi/`, mismos nombres | P6 | Ciclo `material` ↔ `glut_geometry` |
| `constants.h`, `framework.h` | `legacy_vgi/` | P6 | Contienen cambios del equipo (**I3**) |
| `objLoader.{h,cpp}` | `legacy_vgi/` | P6 | Llamadas del equipo dentro (**I3**) |
| `platform_compat.h` | `core/platform/PlatformCompat.h` | P6 | Código propio (`2dbf344`). Lo incluyen `framework.h` y `objLoader.h`: cambian solo rutas de include, preservando bytes |
| `ObjMeshOptimization.{h,cpp}` | Capa de soporte del equipo, ubicación abierta | P6 | **I3**. Código propio (`906b312`) |
| `third_party/stb_vorbis_compat.h`, `stb_vorbis_impl.c` | `audio/StbVorbisCompat.h`, `StbVorbisImpl.c` | P5 | Adaptadores propios (`443a321`), no son terceros |
| `DibujoGLB.{h,cpp}` | `rendering/GlbRenderer.{h,cpp}` | P7 | |
| `NetAvatar.{h,cpp}` | `rendering/AvatarRenderer.{h,cpp}` | P7 | |
| `FrustumCulling.h` | `rendering/` | P7 | |
| `VistaEstereo.h` | `rendering/StereoView.h` | P7 | |
| `OpenXRManager.{h,cpp}`, `VistaVR.h` | `vr/OpenXrManager.{h,cpp}`, `VrView.h` | P7 | `NetJuegoLogica.h` incluye `VistaVR.h` |
| `NetJuego.{h,cpp}`, `NetJuegoLogica.h`, `NetAdaptadores.h` | `networking/game/NetGame`, `NetGameLogic.h`, `NetAdapters.h` | P9 | |
| `Network/Game/RemotePhysicsTimeline.h`, `HostPhysicsPublication.h` | `networking/game/` | P9 | Creados por S3 y S4 |
| `Network/PreferenciasRed.{h,cpp}` | `networking/game/NetworkPreferences.{h,cpp}` | P9 | El contenido de `multijugador.ini` no cambia |
| Cabeceras nuevas de P3 | Nacen en `Network/Game/`; pasan a `networking/game/` o `gameplay/` | P8, P9 | |
| `main.cpp`, `main.h` | `app/` y los destinos de §8 | P4 a P10 | |
| `file_dialog_compat.h` | `app/` | P10 | B3 le cambia un include |
| `CMakeLists.txt` | Se reparte entre la raíz, `cmake/` y `src/*/` | P11 | |

### 2.2 Recursos, terceros, herramientas y documentación

| Actual | Destino | Sec. |
|---|---|---|
| `EntornVGI/assets/` | `resources/assets/` | B4 |
| `EntornVGI/shaders/` | `resources/shaders/` | B4 |
| `EntornVGI/textures/` | `resources/textures/` | B4 |
| `res/ui/hand.png`, `res/ui/hand.svg` | `resources/ui/` | B4 |
| `EntornVGI/third_party/ImGuizmo.{h,cpp}`, `LICENSE-ImGuizmo`, `README-ImGuizmo.md` | `third_party/imguizmo/` | B3 |
| `EntornVGI/third_party/nlohmann/` | `third_party/nlohmann/` | B3 |
| `include/ImGui/`, `include/ImGui Version.txt` | `third_party/imgui/` | B3, **I5** |
| `third_party/README.md` | Se queda; es un documento del equipo y B3 puede actualizarlo | B3. Los fuentes de terceros no se modifican |
| `instalar_assets_laboratorio_escapeuab.py` | `tools/assets/install_lab_assets.py` | B2b |
| `preparar_sector_laboratorio.py` | `tools/assets/prepare_lab_sector.py` | B2b, **I6** |
| `preparar_texturas_escapeuab.py` | `tools/assets/prepare_textures.py` | B2b |
| `aplicar_tiling_escapeuab.py` | `tools/assets/apply_tiling.py` | B2b, **I6** |
| `tools/exportar_glb.py`, `tools/medir_obj.py` | `tools/export_glb.py`, `tools/measure_obj.py` | P12: `ModeloGLB.cpp` cita el primero en un mensaje y `EscenaFisica.h` el segundo en un comentario [C], así que renombrar toca código |
| `docs/ANIMACIONES_GLB.md`, `FISICAS_BULLET.md` | `docs/world/GLB_ANIMATIONS.md`, `BULLET_PHYSICS.md` | B2b, **I7** |
| `docs/CAMBIOS_FPS_JSON.md`, `LINTERNA_Y_BRAZO_INTERACCION.md` | `docs/gameplay/FPS_AND_JSON_SCENES.md`, `FLASHLIGHT_AND_INTERACTION_ARM.md` | B2b, **I7** |
| `docs/EDITOR_PAREDES_PUERTAS.md` | `docs/editor/WALLS_AND_DOORS.md` | B2b, **I7** |
| `docs/MULTIJUGADOR_LAN.md`, `VALIDACION_TAILSCALE.md` | `docs/multiplayer/LAN_MULTIPLAYER.md`, `TAILSCALE_VALIDATION.md` | B2b, **I7** |
| `docs/MULTIPLAYER_SYNC.md`, `MULTIPLAYER_DEBUG_GRAB.md`, `AVATAR_CHEMICAL_PROTECTION.md` | `docs/multiplayer/`, mismos nombres | B2b, **I7** |
| `docs/OPENXR.md` | `docs/vr/OPENXR.md` | B2b |
| `docs/ESTABILIZACION_PRE_REFACTOR.md` | `docs/PRE_REFACTOR_STABILIZATION.md` | B2b |
| `docs/ARCHITECTURE.md`, `REFACTOR_MIGRATION_MAP.md`, `REFACTOR_PHYSICAL_PLAN.md` | Se quedan en `docs/` | No mover mientras haya cortes en curso |

Los nombres de destino son [P]. **Decidido:** se renombran ficheros y carpetas, no se traduce el cuerpo; cada documento conserva su idioma actual, y los nombres oficiales (OpenXR, Bullet, Tailscale, GLB) y los términos de contrato se conservan. Renombrar o mover guías obliga a actualizar, en el mismo commit, los enlaces entre ellas y desde `ARCHITECTURE.md`, y las citas de `README.md`, `tests/CMakeLists.txt`, `tests/OpenXRTests.cpp` y `tools/exportar_glb.py` [C]: B2b no es solo documentación y `tests/CMakeLists.txt` tiene propietario en serie.

### 2.3 Pruebas

Los nombres de `add_test` no cambian durante los movimientos, para poder comparar con B0.

| Categoría | Pruebas CTest (21) | Ficheros |
|---|---|---|
| `tests/graphics/` | `ObjLoaderTests` | `ObjLoaderTests.cpp` |
| `tests/integration/` | `SceneLoaderTests`, `GlbTests`, `PhysicsTests`, `AudioDecoderTests`, `NetJuegoTests`, `ENetLoopbackTests`, `EditorGizmoInteractionTests`, `WindowsLauncherDoesNotRunStaleBuild` | sus `.cpp`, `LauncherTestStub.cpp`, `WindowsLauncherFailureTest.ps1` |
| `tests/unit/` | `GrabDiagnosticsTests`, `EditorPersistenceTests`, `EditorTests`, `EditorPlacementTests`, `ObjMeshOptimizationTests`, `OpenXRTests`, `VistaVRTests`, `MovimientoFPSTests`, `EscalaUITests`, `AnimacionHiritiTests`, `NetworkTests`, `NetworkPreferencesTests` | sus `.cpp` |

Se mueven en P11. En ese movimiento, los ficheros fuente con nombre en español pasan a inglés (`AnimacionHiritiTests` → `HiritiBehaviorTests`, `EscalaUITests` → `UiScaleTests`, `MovimientoFPSTests` → `PlayerMovementTests`, `VistaVRTests` → `VrViewTests`, `NetJuegoTests` → `NetGameTests` [P]); los nombres de target y de `add_test` cambian en P12.

### 2.4 Incertidumbres: inventario final antes de mover

| Id | Pregunta abierta |
|---|---|
| I1 | El serializador, ¿va en `scene` o en `editor`? Hoy se compila tres veces con definiciones distintas [C] |
| I2 | ¿Qué hacer con las demos (`DemoCuboCaida`, puerta de demostración) mezcladas con producción? |
| I3 | (Reúne las antiguas I3 e I4.) La base académica usa código del equipo: `platform_compat.h`, `ObjMeshOptimization`, `RenderMetrics.h` y `UniformCache.h` [C]. Hay que admitir la arista `legacy_vgi` → capa de soporte del equipo y decidir dónde vive esa capa: `UniformCache` y `RenderMetrics` incluyen GLEW, así que no encajan en un `core` sin dependencias de motor. Además hay cambios del equipo dentro de ficheros académicos (llamadas de optimización y caché en `objLoader.cpp`, las 17 luces en `constants.h`): el fichero queda en `legacy_vgi` por su origen y ese código no se renombra, con adaptadores en inglés [H] |
| I5 | Comparar `include/ImGui` con la versión original para detectar parches locales. El cambio de `origin/Issue#5` en `imgui_impl_opengl3.cpp` es solo de espaciado [C]. `include/ImGui/nfd.h` es la cabecera NFD antigua |
| I6 | `preparar_sector_laboratorio.py` calcula la raíz con `Path(__file__)` [C]: **moverlo lo rompe** sin editarlo. Los otros tres usan el directorio de trabajo. `aplicar_tiling_escapeuab.py` parchea `main.cpp` y quedará obsoleto |
| I7 | Reparto exacto de las guías y revisión de enlaces y rutas citadas |
| I8 | Decidir (borrar o conservar, en commits propios): `.slnx`, `.vcxproj*`, `include/{GLFW,gl,glm}`, `include/glfw Version.txt`, `lib/`, `x64/`, `third_party/OpenXR-SDK-main` (428 ficheros sin referencia en CMake [C]), `res/` salvo `ui/`, `.bak`, `.TMP`, `imgui.ini*`, `GLSL_Error.VERT`, `ModelLoader.cpp` (fuera de todo target [C]), `tree.txt`, gitlink `ImDesignManager`. Mover rompe el `.vcxproj` [H] |

Criterio de origen usado arriba: la importación inicial (`6991d95`) contiene `objLoader`, `glut_geometry`, `escena`, `visualitzacio`, `material`, `shader`, `quatern`, `constants.h` y `framework.h` [C]; lo añadido en commits posteriores del equipo se trata como propio [H].

### 2.5 Decisiones resueltas

**Decidido** por el coordinador el 2026-11-25. La numeración I9 a I11 se conserva como referencia.

**I9. `origin/Issue#5` (`469e822`): redundante, no se integra.** La rama se conserva como respaldo; no se porta nada. Evidencia, comprobada contra `2d83c7d`:

| Contenido de la rama | Situación en el árbol actual |
|---|---|
| Icono `assets/mano.png` al mirar un cuerpo marcado como interactuable | Imagen distinta: es la fotografía de una mano (100 359 bytes) y la actual `res/ui/hand.png` una silueta blanca (20 974 bytes) [C]. No aporta una capacidad de juego necesaria que falte: el HUD actual es más avanzado, ya que `dibujarHUDInteraccion` parte de `ObjetivoInteraccion`, escala con la interfaz, muestra texto según el contexto y cubre también las puertas [C] |
| `MundoFisico::setInteractuable` / `getInteractuable` y su uso en `EscenaFisica.cpp` | Sin referencias en el árbol actual [C] (`BuscarObjetoInteractuable` es otra función) |
| `cargarTexturaPNG` llamada desde `draw_Menu_ImGui` | Mientras se permite interactuar, crea una textura GL en cada fotograma y nunca la libera [C] |
| Se comenta el **registro** del callback: la llamada `glfwSetErrorCallback(error_callback)` | No es una edición de un comentario: desactiva el callback de errores de GLFW. El árbol actual mantiene la llamada [C]; integrarlo sería una regresión |
| Reordenación del `if` de la tecla E | Equivalente [H] |
| `main.h` | Conversión de ISO-8859 a UTF-8; sin comentarios ni espacios, no hay diferencias de contenido [C] |
| `imgui_impl_opengl3.cpp`, `ObjetosAnimadosGLB.cpp` | Solo espaciado [C] |

**I10. Qué es de quién** (política en §6):

- El código de terceros permanece bajo `third_party/`, sin modificar y sin traducir.
- `src/legacy_vgi/` contiene **solo** la base académica VGI original, que conserva sus identificadores y API.
- El código escrito por el equipo comprende módulos, adaptadores, pruebas, herramientas y documentación. Sus identificadores nuevos o propios y sus nombres de fichero y carpeta van en inglés.

**I11. Idioma de la documentación:** el cuerpo **no** se traduce de forma global; cada documento conserva su idioma actual (español o inglés). Los nombres de fichero y carpeta de la documentación del equipo pasan a inglés donde esté previsto un renombrado seguro, conservando términos de contrato y nombres oficiales.

## 3. Lotes, orden y dependencias

### 3.1 Orden

| Orden | Lote | Contenido | Depende de | Validación (§4.3) |
|---|---|---|---|---|
| 1 | **B0** (P1 mecánica) | Línea base: lista de pruebas, líneas de configuración, hashes de `bin/`, `compile_commands.json`, métricas de §8. Capturar del ejecutable valores `ET2-`/`HashId` | — | V, N, R, S |
| 2 | **B1** | Prerrequisitos de movimiento (§3.2) | B0 | V, N, R |
| — | **G1** | Revisión Claude de B1 sobre su SHA exacto (§3.3) | B1 | — |
| 3 | **B2** | Transporte a `src/networking/transport/` | G1. **En serie con S1, S2 y S5** (`NetJuegoLogica.h`) | V, N, R, M |
| 4 | **B2b** | Scripts a `tools/assets/` y guías en subcarpetas, con nombres en inglés y sin traducir el contenido | G1, I6, I7; propietario de `tests/CMakeLists.txt` | Enlaces, rutas de scripts, V |
| 5 | **B3** | `third_party/` consolidado | G1, I5, I8 | V, R, M, S |
| 6 | **B4** | Recursos a `resources/` | G1 | V, R, M, S |
| en paralelo | **P3** | S1, S2, S5, S6, S7, S8, S9 y enlace de shaders (§3.4) | S3 y S4 cerradas | V, N, S |
| 7 | **B5** | Mover escena, animación, física y editor | G1, S1 y S2, y una ventana exclusiva de `main.cpp` entre cortes | V, N, R, M, S |
| 8 | **P4** | `main.h`: de definiciones en cabecera a estado explícito | S1, S2 y S5 a S9 (el mapa ordena P3 antes de P4); va tras B5 solo por turno | V, S |
| 9 | **P5** | Hojas: audio, escala UI, cabeceras de gameplay, diagnóstico, paneles, HUD | P4 | V, N, S |
| 10 | **P6** | a) sacar la lógica de juego del bloque VGI; b) mover el bloque a `legacy_vgi/` | P4, I3 | V, R, M, S |
| 11 | **P7** | Render y VR | P6 | V, S |
| 12 | **S10** | Ramas por rol de interacción, lanzamiento y solicitudes | **QA humana de agarre en cliente (P0)**; puerta propuesta [P] | V, N, S |
| 13 | **P8** | Gameplay | P7, S10 | V, S |
| 14 | **P9** | Red de juego, sin tocar el formato en cable | P8 | V, N, S |
| 15 | **P10** | `app` como raíz de composición; RAII con el mismo orden de limpieza | P9 | V, N, S |
| 16 | **P11** | Remanentes: pruebas por categoría, CMake por módulo, vaciar `EntornVGI/` | P10 | V, N, R, M, S |
| 17 | **P12** (obligatorio) | Identificadores en inglés, un módulo por commit | P11 | V, N, R, M, S |

Respecto al orden anterior del mapa, B2 a B5 adelantan movimientos de módulos que ya tienen target y pruebas, según el orden de lotes encargado por el coordinador. Ajuste **todavía pendiente de aprobación**:

- El mapa exige P3 en verde antes de P6 a P9. Aquí S1, S2 y S5 a S9 bastan para P4 a P7; S10 solo bloquea P8 en adelante.
- B5 **no** depende técnicamente de S5 a S9: puede intercalarse entre dos cortes. El orden de la tabla entre B5, los cortes de P3 y P4 es **serialización** (un solo propietario de `main.cpp` y CMake), no dependencia.

Rollback de cualquier lote: anotar el SHA de partida; un commit o serie corta contigua; `git revert` en orden inverso (los lotes posteriores primero); reconstruir en un directorio limpio; revisar `git status` sobre los assets tras usar el editor.

### 3.2 B1 y B2

**B1** (un único propietario de los ficheros CMake; sin `main.cpp`, `main.h`, código de producción ni movimientos):

1. Targets estáticos para `ObjetosAnimadosGLB`, física (`MundoFisico`, `EscenaFisica`, con Bullet), `NetJuego` (con `EntornVGI_network` y GLM), `GrabDiagnostics` y `PreferenciasRed`. Las pruebas los enlazan en lugar de listar `.cpp`. Declarar `cxx_std_20` y avisos; `GLM_ENABLE_EXPERIMENTAL` y `NOMINMAX` por paridad con el ejecutable (de estas unidades, `GrabDiagnostics.cpp` incluye `windows.h` [C]).
2. Variables de raíz de recursos y comprobación de ficheros sonda (§7), con los valores actuales.
3. En un commit aparte: sustituir en `GlbTests`, `PhysicsTests`, `SceneLoaderTests` y `ObjLoaderTests` las rutas literales `EntornVGI/...` por las macros.
4. `.gitignore`: añadir `!resources/assets/models/**/*.obj` sin quitar la excepción actual.

**B2:** `git mv` de los nueve ficheros de transporte y su `CMakeLists.txt`; `add_subdirectory` con directorio binario explícito, después de definir `ENTORNVGI_ENABLE_NETWORK`; `EntornVGI_network` añade `src/` a su include público y **conserva** `EntornVGI/` (estrecharlo es otro cambio). Includes a actualizar [C]: `NetJuegoLogica.h`, `NetJuego.h`, `NetJuego.cpp`, `NetworkTests.cpp`, `ENetLoopbackTests.cpp`. No toca `main.cpp`.

**Decidido: B2 va en serie con S1, S2 y S5 por `NetJuegoLogica.h`, con un solo propietario del fichero cada vez.** Orden recomendado [P]: primero S1, que puede avanzar durante B1 mientras B2 espera a G1; el que vaya segundo parte del SHA ya integrado y repite su validación.

### 3.3 G1: revisión independiente antes del primer `git mv`

Solo lectura, sobre el SHA de B1, por quien no lo escribió. B2 no empieza sin:

- Debug y Release desde directorio limpio; mismas pruebas, número y omisiones que en B0; `ObjLoaderTests` superada, no omitida por el cambio.
- Configuración que sigue informando ENet, OpenXR y meshoptimizer.
- Definiciones y avisos por fichero iguales a B0, o cada diferencia justificada.
- Manifiesto de recursos de `bin/` idéntico y definiciones de directorios de assets sin cambio textual.
- La configuración se detiene si falta un fichero sonda.
- Solo cambian ficheros CMake, las cuatro pruebas citadas y `.gitignore`.
- 156 ficheros versionados e ignorados, como en la base [C].

### 3.4 P3: cortes de caracterización y extracción

Reglas: un corte por commit; fijar los valores esperados contra el comportamiento actual; los nombres de evento y textos de `trazarAgarre` quedan idénticos y en el mismo orden (son la evidencia de QA); estado `static`, sonidos y aleatoriedad permanecen en `main.cpp` salvo indicación.

| Corte | Alcance (funciones de `main.cpp`) | Riesgo | Pruebas |
|---|---|---|---|
| **S1** | `identificadorEscenaRed` → `NetJuegoLogica.h` | Muy bajo | `NetJuegoTests`: independencia del orden, el nombre importa, altas y bajas de ID, nombre vacío, formato y longitud. El caso dorado de la escena calibrada exige valores capturados del ejecutable previo y cambiar `tests/CMakeLists.txt` |
| **S2** | `capturarEstadoEscenaRed`, `indiceUnicoObjetoRed` → cabecera nueva (propuesta: `SceneNetworkSync.h`) | Muy bajo | ID duplicado o reservado, escala o masa inválidas, tipo por colisionador; conservar salida parcial y orden |
| S3 | Buffer e interpolación remota | — | **Cerrado** (`0ccaea3`) |
| S4 | Mezcla de réplica y transición a dormido | — | **Cerrado** (`fe4d7da`) |
| **S5** | Reglas puras de agarre del host: validación de rayo (0,85 m a mano o cabeza; alineación ≥ 0,60 con la mano derecha), elección de anclaje, conversión carga ↔ velocidad | Bajo | Valores límite, NaN, empate → cabeza, poses de escritorio y VR. `desdeCabeza` solo se aplica si el agarre físico tuvo éxito |
| **S6** | Auxiliares de Hirity: candidatos de reaparición, direcciones de sondeo, atenuación de volumen, aviso de agarre | Bajo a medio | `AnimacionHiritiTests` con consultas físicas simuladas |
| **S7** | Decisión de mantenimiento del agarre dentro del bucle de propietarios de `actualizarRed` | Medio | `NetJuegoTests` y `PhysicsTests`; conservar la copia que se itera y el orden de consultas |
| **S8** | `aplicarEstadosEscenaRed` con reconstrucción inyectada | Medio | Validación todo o nada, última actualización gana, reversión, sin reconstruir si está vacío |
| **S9** | `actualizarHiriti` como paso de estado (después de S6) | Alto | Registro del orden de llamadas; el sonido se emite de forma síncrona, no diferida |
| **S10** | Ramas por rol de `interactuar`, `lanzarObjetoCargado` y el bucle de solicitudes: solo tablas de decisión | Máximo | `NetJuegoTests`, incluida la liberación al desconectar un cliente que posee un objeto. **Después de la QA de agarre en cliente** |
| P3-SH | Pruebas de enlace de los shaders no cubiertos (las de Phong existen en `ObjLoaderTests` [C]) | Bajo | Requiere contexto gráfico |

S1 y S2 pueden ir junto a B1 solo con casos sintéticos.

S4 se implementó de forma distinta a la propuesta inicial: el auxiliar de mezcla no tiene parámetro «activo»; la guarda `flags & 1` sigue en `main.cpp` y el caso «inactivo salta a la pose objetivo» no tiene prueba [C].

Hipótesis registradas durante el análisis de los cortes, **[H], no verificadas aquí y no corregidas dentro del refactor:** las reconstrucciones de física pedidas por el editor usan autoridad por defecto aunque sea cliente; la línea temporal de réplica solo se reancla con un estado final; el cliente marca Hirity como lanzado al encolar la liberación; un host aún arrancando entra por la rama offline de `interactuar`.

## 4. Dependencias y gates

### 4.1 Módulos objetivo (la flecha va del que depende al dependido)

```text
app ──> networking/game ──> gameplay ──> physics ──> scene ──> core
app ──> rendering, audio, ui, vr, editor, diagnostics
physics ──> animation ──> core
networking/game ──> networking/transport ──> core
rendering ──> legacy_vgi        (solo app y rendering ven legacy_vgi)
legacy_vgi ──> soporte del equipo   (core/platform y la capa abierta de I3)
```

Aristas actuales que lo incumplen [C] y hay que resolver antes de mover: `SceneLoader.h` → `MundoFisico.h`; `EditorEscenas.cpp` → `EscenaFisica.h` y `EscalaUI.h`; `NetJuegoLogica.h` → `VistaVR.h`; `AudioManager.h` → `MovimientoFPS.h`; la base académica → código del equipo (I3); doce includes con `../`. Una raíz de include única no impone límites: añadir una comprobación de includes con la tabla de aristas permitidas.

La carga transaccional de escena toca caché de modelos y física: su orquestación va en `app`, no en `scene`.

### 4.2 Lotes

Solo se dibujan dependencias. La serialización por propietario de `main.cpp` y CMake no aparece como flecha.

```text
B0 ─> B1 ─> G1 ─┬─> B2
                ├─> B2b
                ├─> B3
                ├─> B4
                └─> B5  <── S1, S2
S6 ─> S9
S1, S2, S5..S9 ─> P4 ─┬─> P5
                      └─> P6 ─> P7 ─> P8 ─> P9 ─> P10 ─> P11 ─> P12
QA de agarre en cliente ──> S10 ──> P8
B2, B3, B4, B5, P5 ──> P11
```

Pares en serie sin dependencia fija de orden: **B2 ↔ S1, S2 y S5** por `NetJuegoLogica.h`; **B2b ↔ B1** por `tests/CMakeLists.txt`.

### 4.3 Comandos de validación (propuestos, no ejecutados para este documento)

Requieren PowerShell 7 en una consola x64 de Visual Studio con `$env:VCPKG_ROOT` y `$env:NINJA` definidos (el lanzador los fija solo en su propio proceso). No hay `testPresets` [C]. Usar un worktree limpio. La validación es por SHA: tras un rebase se repite.

```powershell
# V
git diff --check <base>..HEAD
.\run-windows.cmd -NoRun
.\run-windows.cmd -Configuration Release -NoRun
ctest --test-dir build/windows-debug -N
ctest --test-dir build/windows-debug --output-on-failure
ctest --test-dir build/windows-release --output-on-failure

# N (repetir también en build/windows-release)
ctest --test-dir build/windows-debug -R '^NetJuegoTests$' --repeat until-fail:20
ctest --test-dir build/windows-debug -R '^ENetLoopbackTests$' --repeat until-fail:10

# R
cmake --preset windows-debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
$bin = (Resolve-Path build/windows-debug/bin).Path
Get-ChildItem "$bin/assets","$bin/shaders","$bin/textures","$bin/res" -Recurse -File |
  ForEach-Object { '{0}  {1}' -f (Get-FileHash $_.FullName).Hash, [IO.Path]::GetRelativePath($bin, $_.FullName) } |
  Sort-Object | Set-Content <manifiesto-fuera-del-repo>
git ls-files -ci --exclude-standard | Measure-Object

# M
git diff -M90% --name-status <base> HEAD
git diff -M90% -U0 <base> HEAD -- '*.cpp' '*.h'
git grep -nE '#include\s+"\.\./' -- src
git status --short --ignored -- src resources tools tests
```

**S** (requiere pantalla): arrancar desde `bin/` → menú → PLAY con la escena calibrada; guardar desde el editor, comprobar que cambia la copia fuente y descartar el cambio.

### 4.4 Gates mecánicos frente a humanos y de hardware

| Nivel | Incluye | Bloquea |
|---|---|---|
| Mecánico | Build, CTest, hashes, definiciones, renombres, includes, identidades y bytes | Todos los lotes |
| Escritorio (pantalla y GPU) | Arranque, guardado del editor, pruebas OpenGL que no se omitan, capturas, valores `ET2-` de referencia | Recursos, render, UI y `app` |
| QA manual (P0) | Agarre y lanzamiento desde cliente, Hirity, paridad visual host/cliente, F4, 1080p/4K | GO funcional y `master`. Propuesta [P]: también **S10 y por tanto P8 en adelante** |
| Hardware y plataforma | Dos equipos y Tailscale, visor OpenXR, macOS, Linux | No bloquea trabajo estructural en Windows; se declara no validado |

La ruta NFD antigua (`file_dialog_compat.h`) solo se compila en Linux con dependencias del sistema [C]: B3 exige una configuración en Linux o declarar el hueco.

Pueden empezar sin QA manual: resolver las incertidumbres abiertas de §2.4, B0, B1, G1, B2, B2b y los cortes S1 y S2.

## 5. Roles y propiedad

| Rol | Responsabilidad |
|---|---|
| Supervisor (Codex) | Integra, secuencia y arbitra `main.cpp` y CMake |
| Implementadores (Codex) | Implementan un lote o corte cada uno y lo validan |
| Revisor y documentación (Claude Sonnet) | Revisa arquitectura y diffs sobre el SHA exacto; mantiene `docs/` |

**Máximo 3 o 4 agentes concurrentes.** Quien revisa un lote no es quien lo escribió.

**Propiedad en serie** (un solo propietario a la vez):

- `main.cpp`, `main.h`.
- `CMakeLists.txt` raíz, `EntornVGI/CMakeLists.txt`, `EntornVGI/Network/CMakeLists.txt`, `tests/CMakeLists.txt`, `CMakePresets.json`, `.gitignore`.
- Enums y tipos compartidos: tipos y formas de cuerpo (`MundoFisico.h`), estructuras de `SceneLoader.h`, `EstadoJuego`.
- `NetJuegoLogica.h` (S1, S2, S5, B2) y `tests/NetJuegoTests.cpp` (S1, S2, S5, S7, S8, S10): serializar, o crear un fichero de prueba por corte (esto último cambia el número de pruebas respecto a B0).
- `docs/REFACTOR_MIGRATION_MAP.md`.

Propiedad por equipos tras los movimientos: integración (`app`, `core`, `third_party`); mundo (`scene`, `animation`, `physics`); red (`networking`); gameplay (`gameplay`, `audio`); render (`rendering`, `vr`); herramientas (`editor`, `ui`, `diagnostics`). `legacy_vgi` congelado.

## 6. Política de nombres en inglés

- Identificadores **nuevos**: en inglés (práctica de S3 y S4). El análisis de cortes recomendaba conservar el español en las extracciones; **este plan adopta el inglés** y deja constancia de la discrepancia.
- **Alcance obligatorio (decidido, §2.5):** código y módulos del equipo, adaptadores, pruebas, herramientas y documentación.
- **No se traduce:** identificadores y API originales de la base académica VGI, incluido el bloque VGI de `main.cpp` y el cargador OBJ del profesorado, que quedan en `legacy_vgi`; el código de terceros, que permanece sin modificar en `third_party/`.
- **Adaptadores nuevos** del equipo sobre API heredadas: nombres en inglés y fuera de `legacy_vgi`.
- **Documentación:** nombres de fichero y carpeta en inglés donde haya un renombrado seguro; el cuerpo conserva su idioma.
- Ficheros: se renombran en el commit que los mueve, salvo los casos que obligan a tocar código (§2.2). Identificadores existentes: en P12, un módulo por commit, sin cambios de comportamiento.
- Espacios de nombres [P]: `netjuego` → `netgame`, `fisica` → `physics`, `juego` → `gameplay`, `fps` → `movement`, `animacion` → `animation`.
- Pendiente: la capa de soporte del equipo que usa la base académica y el código propio incrustado en ella (I3).

**Contratos que no se renombran:** `id` de escena (incluido `"hiriti"`) y nombres de fichero de escena, que alimentan `ET2-`; claves JSON; tipos, eventos y formato del protocolo; rutas y nombres de assets y shaders; argumentos de línea de comandos; variables de entorno; claves de `multijugador.ini`; nombres de evento y textos de los registros de agarre; textos visibles para el jugador; target y ejecutable `EntornVGI`; opciones CMake `ENTORNVGI_*`; nombres de `add_test` hasta P12.

Codificación: 14 fuentes y shaders propios no son UTF-8, entre ellos `main.h` [C]. Las reescrituras de includes deben preservar los bytes; convertir la codificación, si se hace, en un commit previo y propio. No añadir `/utf-8` antes.

## 7. Cambios de CMake y rutas

Raíz anclada a `CMAKE_SOURCE_DIR` (hoy equivale a `PROJECT_SOURCE_DIR`):

| Variable [P] | Valor antes de B4 | Valor tras B4 |
|---|---|---|
| `ENTORNVGI_RESOURCE_ROOT` | `${CMAKE_SOURCE_DIR}/EntornVGI` | `${CMAKE_SOURCE_DIR}/resources` |
| `ENTORNVGI_UI_RESOURCE_ROOT` | `${CMAKE_SOURCE_DIR}/res/ui` | `${CMAKE_SOURCE_DIR}/resources/ui` |
| `ENTORNVGI_RUNTIME_ROOT` | `${CMAKE_BINARY_DIR}/bin` | sin cambio |

- **Directorios de assets:** `ENTORNVGI_SOURCE_ASSETS_DIR = <RESOURCE_ROOT>/assets`; `ENTORNVGI_RUNTIME_ASSETS_DIR = <RUNTIME_ROOT>/assets`. Hoy dependen de `CMAKE_CURRENT_SOURCE_DIR` [C]: mover ese CMake o los assets haría que el editor dejara de copiar al árbol fuente **sin error**, y `EditorPersistenceTests` no lo detecta porque usa directorios temporales.
- **Ficheros sonda:** `FATAL_ERROR` en configuración si faltan `<RESOURCE_ROOT>/shaders/gouraud_shdrML.vert` o `<RESOURCE_ROOT>/assets/scenes/aula_informatica_calibrada.json`, los mismos que busca `SetWorkingDirectoryToAssetRoot` en ejecución.
- **Rutas de ejecución:** sin cambios. El directorio de trabajo sigue siendo la raíz de recursos; las rutas `assets/...`, `./shaders/...`, `./textures/...`, `res/ui/hand.png` y las de modelos relativas al JSON no se tocan.
- **Copia:** `EntornVGI_runtime_resources` conserva su nombre y sus destinos en `bin/`; la copia de DLL tras el build se conserva.
- **Macros de prueba:** se conservan `ESCAPETHEUAB_SOURCE_DIR` y `ESCAPETHEUAB_RUNTIME_ASSET_ROOT`; añadir [P] `ESCAPETHEUAB_SOURCE_ASSETS_DIR`, `ESCAPETHEUAB_SOURCE_SHADERS_DIR` y `ESCAPETHEUAB_UI_RESOURCE_DIR`. Mantener el aislamiento de `EditorPersistenceTests` y la variante sin directorios de `EditorTests`.
- **Hashes:** manifiesto SHA-256 de `bin/` idéntico antes y después de B3 y B4. El `GLOB` copia también `.bak`, `.blend` y `*.pre-tiling.bak` [C]; limpiarlos cambia el manifiesto y va aparte.
- **B3:** `IMGUI_DIR`; include de `third_party/` para `EntornVGI_scene_loader` y las pruebas que compilan el serializador; includes en `SceneLoader.cpp`, `SerializadorEscenas.cpp`, `EditorEscenas.cpp`, `EditorPersistenceTests.cpp` y `file_dialog_compat.h`; fuentes de `EditorGizmoInteractionTests`; includes de `ObjLoaderTests`; orden `BEFORE` del include de NFD. El contenido de los terceros no se modifica.
- **P5 (audio):** al mover los adaptadores de Vorbis, actualizar la ruta de reserva `ENTORNVGI_STB_IMPLEMENTATION_SOURCE` del CMake raíz, el include `<stb_vorbis_compat.h>` de `AudioManager.cpp` y `AudioDecoderTests.cpp` y sus directorios de include. Hasta entonces `EntornVGI/third_party/` conserva esos dos ficheros.
- **B4:** además de lo anterior, mover la excepción `*.obj` de `.gitignore`, actualizar `.vscode` (hoy usa `EntornVGI/` como directorio de trabajo; con `resources/` la búsqueda de respaldo encuentra escenas y shaders pero no `res/ui/hand.png` [H]), rutas de los scripts, README y guías.
- Los targets nuevos no heredan las definiciones `PRIVATE` del ejecutable: declararlas y comparar `compile_commands.json`.

## 8. `main.cpp`

**Líneas:** 10.170 en `7ace570` (base de la auditoría) y **10.171 en `fe4d7da`** (tras S4). `main.h`: 413 en ambos [C].

Rangos en `fe4d7da`: los límites salen de inicios de función [C]; la responsabilidad y el destino de cada bloque son aproximados [H].

| Líneas | N.º | Responsabilidad | Destino [P] | Lote |
|---|---|---|---|---|
| 1–108 | 108 | Includes, globales de escena, raíz de recursos | `app` | P10 |
| 109–1056 | 948 | Texturas de material, estado de vista, audio, demos, luces, oclusores, linterna, interacción, agarre, Hirity, cursor | `rendering`, `gameplay`, `audio`, `diagnostics` | P5, P7, P8 |
| 1057–1306 | 250 | Estado de escena, cámara y pose VR, captura y aplicación de estado de red, identidad | `app`, `networking/game` | S1, S2, S8, P9 |
| 1307–1653 | 347 | Sesión, autoridad de agarre, snapshots, réplica | `networking/game` | S5, S7, S10, P9 |
| 1654–1856 | 203 | Cuerpo del jugador, movimiento, lanzamiento, balanceo | `gameplay` | S10, P8 |
| 1857–2062 | 206 | Carga de escena, reconstrucción de física, cargadores del editor, callbacks de ventana | `app` | P7, P10 |
| 2063–2483 | 421 | `InitGL`, redimensionado | `app`, `rendering` | P6, P7 |
| 2484–3148 | 665 | Estéreo, VR, `OnPaint`, dibujo de escena | `rendering`, `vr` | P7 |
| 3149–3669 | 521 | Barra de estado, HUD, paneles de red y dinámicos | `ui`, `diagnostics` | P5 |
| 3670–4135 | 466 | Menús del juego | `ui` | P5 |
| 4136–5204 | 1.069 | Ventanas ImGui VGI | `legacy_vgi` | P6 |
| 5205–6833 | 1.629 | Manejadores de menú VGI | `legacy_vgi` | P6 |
| 6834–8968 | 2.135 | Teclado | `legacy_vgi`, `app` | P6 |
| 8969–9591 | 623 | Ratón, temporizador, utilidades | `legacy_vgi`, `app` | P6 |
| 9592–10171 | 580 | `main()`: inicialización, FBO PS1, bucle, limpieza | `app` | P10 |

El bloque 4136–9591 (5.456 líneas) **no es solo VGI**: contiene teclas de juego, mirada FPS y menús de juego; `InitGL` carga shaders. Separarlo primero (P6a) o `legacy_vgi` dependerá de gameplay.

Restricciones al partir el fichero:

- Los objetos globales con constructor (`mundoFisico`, `escenaFisica`, `redJuego`, `audioManager`, `editorEscenas`, `serializadorEscena`, `gestorOpenXR` y el resto) permanecen en un mismo fichero y en su orden hasta P10: el orden de construcción y destrucción importa.
- Los globales de `main.h` hoy se inicializan a cero: inicializar por valor cualquier estructura que los agrupe.
- Varios son globales de la base VGI con nombres muy genéricos (`n`, `t`, `w`, `h`, `mode`, `window`, `shader`, `buffer`): al hacerlos `extern` pueden colisionar. **No se renombran.** Si se agrupan o se acota su visibilidad, las unidades de `legacy_vgi` deben seguir usándolos sin calificar.
- Subir antes `texturaFondoMenu` y `EstadoJuego`/`estadoActual`, definidos en mitad del fichero.
- Conservar la vida de los `static` locales y el orden del fotograma y de la limpieza.
- Prueba de movimiento puro: las líneas quitadas y añadidas coinciden salvo includes, declaraciones y espacios de nombres. Todo lo demás es cambio de comportamiento.

### 8.1 Métricas a actualizar en cada lote

| Métrica | `fe4d7da` | Cómo medir (Git Bash) |
|---|---|---|
| Líneas de `main.cpp` | 10.171 | `git show <sha>:<ruta>/main.cpp \| wc -l` |
| Líneas de `main.h` | 413 | `git show <sha>:<ruta>/main.h \| wc -l` |
| Includes locales en `main.cpp` | 36 | `git show <sha>:<ruta>/main.cpp \| grep -c '^#include "'` |
| Includes con `../` bajo `EntornVGI/` (y `src/`) | 12 | `git grep -E '#include[[:space:]]+"\.\./' <sha> -- EntornVGI src \| wc -l` |
| Líneas de `tests/CMakeLists.txt` que citan `.cpp` de producción | 18 | `git show <sha>:tests/CMakeLists.txt \| grep -c 'EntornVGI/.*\.cpp'` |
| Ficheros versionados en `EntornVGI/` | 423 | `git ls-tree -r --name-only <sha> EntornVGI \| wc -l` |
| Pruebas CTest registradas | 21 [D] | `ctest -N` |
| Versionados pero ignorados | 156 (medido en `7ace570`) | `git ls-files -ci --exclude-standard \| wc -l` |
| Globales mutables | ≈ 222 [H], base `62a7018` | Sin comando fiable. B0 debe fijar uno [P] |
