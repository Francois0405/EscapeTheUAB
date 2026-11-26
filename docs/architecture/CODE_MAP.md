# Mapa del código

Documento de referencia sobre el código **actual**. Si discrepa del código, manda el código.

| Para | Documento |
|---|---|
| Dónde está cada cosa y cómo funciona | este |
| Ciclo, audio y físicas de Hirity | [../HIRITY.md](../HIRITY.md) |
| Ciclo, lógica y efectos de Sanity | [../SANITY.md](../SANITY.md) |
| Paneles ImGui | [UI_PANELS.md](UI_PANELS.md) |
| Estado y validación del trabajo visual retro | [../midnight-hoops/TASKS.md](../midnight-hoops/TASKS.md), [VALIDATION.md](../midnight-hoops/VALIDATION.md) |
| Reglas de trabajo del equipo | [../TEAM_GUIDE.md](../TEAM_GUIDE.md) |
| Historia del refactor (no describe el código actual) | [../ARCHITECTURE.md](../ARCHITECTURE.md), [../REFACTOR_MIGRATION_MAP.md](../REFACTOR_MIGRATION_MAP.md), [../PORTING_MAP.md](../PORTING_MAP.md) |

## 1. Vista general

Un ejecutable, `EntornVGI`, y bibliotecas internas definidas en `src/CMakeLists.txt`:

| Target | Contenido |
|---|---|
| `EntornVGI_scene_loader` | `src/scene/SceneLoader.cpp` |
| `EntornVGI_glb_model` | `src/rendering/GlbModel.cpp` |
| `EntornVGI_animated_glb` | `src/animation/GlbAnimatedObjects.cpp` |
| `EntornVGI_physics` | `src/physics/PhysicsWorld.cpp`, `PhysicsScene.cpp` |
| `EntornVGI_editor_core` | `ModelBounds`, `ObjectPicker`, `PlacementValidator`, `EditorVisibility` |
| `EntornVGI_network` | `src/networking/transport/` (tiene su propio `CMakeLists.txt`) |
| `EntornVGI_network_game` | `src/networking/replication/NetGame.cpp` |
| `EntornVGI_network_preferences`, `EntornVGI_grab_diagnostics` | Preferencias de red y registro de agarre |
| `EntornVGI` | Todo lo demás, ImGui y ImGuizmo |
| `EntornVGI_runtime_resources` | Copia `assets/` a `build/<preset>/bin/assets/` |

**Regla de dependencias:** fuera de `src/app/` y `src/legacy_vgi/` ningún módulo incluye cabeceras de estado de
la aplicación. Reciben referencias y callbacks en un contexto que construye `src/app/adapters/`.

## 2. Módulos y archivos

### `src/app/` — composición

| Archivo | Responsabilidad |
|---|---|
| `main.cpp` | `main()`: raíz de recursos, audio, subtítulos, ajustes retro, argumentos, ventana, bucle, guardado y cierre |
| `Application.*` | `ParseNetworkArguments`, `CreateMainWindow`, `InitializeGraphicsResources`, `RegisterWindowCallbacks`, `InitializeImGui`, `StartGame`, `LoadMenuBackground` |
| `MainLoop.*` | `RunMainLoop`: orden del fotograma, selección de ruta de render y filtrado de texturas |
| `Shutdown.*` | Liberación de recursos con el contexto OpenGL vivo |
| `AppState.cpp` | **Definición** de todo el estado global compartido |
| `AppIncludes.h` | Includes agregados que usan las unidades de `app/` |
| `WorkingDirectory.*` | Busca la carpeta que contiene `assets/` y la fija como directorio de trabajo |
| `WindowCallbacks.*` | Foco, cierre, pantalla completa, errores GLFW y mensajes de depuración GL |
| `DemoDoor.*`, `VrToggle.*` | Puerta de demostración; activar o desactivar OpenXR |

`src/app/state/` declara el estado por responsabilidad: `SceneState.h`, `RenderState.h`, `PlayerState.h`,
`GameplayState.h`, `NetworkState.h`, `VrState.h`. Todo se define en `AppState.cpp` (una sola unidad, porque
unos objetos se construyen a partir de otros). El estado de la base académica está en
`src/legacy_vgi/LegacyVgiState.*`.

`src/app/adapters/` construye los contextos:

| Adaptador | Enlaza con |
|---|---|
| `PlayerAdapters` | `gameplay/player` (cámara, movimiento, balanceo, preset de arranque) |
| `InteractionAdapters` | `gameplay/interaction` |
| `HirityAdapters` | `gameplay/hirity` |
| `SceneAdapters` | `scene/SceneRuntime` |
| `SessionAdapters` | `networking/session` |
| `RenderAdapters` | `rendering/RuntimeRenderer` y `LegacyVgiSceneRenderer` |
| `GraphicsAdapters` | `rendering/LegacyVgiGraphicsBootstrap` (`InitGL`) |
| `GrabTrace` | `diagnostics/GrabDiagnostics` |

Resto de `app/`: `input/` (`KeyboardInput`, `MouseInput`, `CursorCapture`), `render/` (`SceneLighting`,
`MaterialTextures`, `HandheldDevices`: teléfono, linterna y rayo VR; `GpuFrameTimer`; `RetroCapture`),
`ui/` (`UiFrame`, `InteractionHudTexture`).

### Resto de `src/`

| Carpeta | Archivos y función |
|---|---|
| `gameplay/player/` | `FpsMovement.h` (velocidad, estamina, balanceo; puro), `PlayerRuntime.*` (cámara y cápsula) |
| `gameplay/sanity/` | `Sanity.h` (degradación y efectos derivados), `SanityEffects.h` (eventos visuales temporizados) |
| `gameplay/interaction/` | `InteractionRuntime.*` (objetivo, coger, soltar, lanzar, puertas), `ThrowCharge.h` (tiempos y velocidades) |
| `gameplay/hirity/` | `HiritiRuntime.*`, `HiritiAnimation.h` (mirada, seguimiento, reaparición), `HiritiAudioTimer.h`, `HiritiSoundVolume.h` |
| `scene/` | `SceneLoader.*` (JSON → datos), `SceneRuntime.*` (carga transaccional), `escena.*` (dibujo VGI), `objLoader.*` |
| `physics/` | `PhysicsWorld.*` (Bullet, pimpl), `PhysicsScene.*` (puente escena ↔ cuerpos), `ColliderDrawing.*` |
| `animation/` | `GlbAnimatedObjects.*`, `GlbAnimation.h`, `DoorAnimation.h` |
| `rendering/` | Ver §6. Además `GlbModel`, `GlbDrawing`, `FrustumCulling.h`, `UniformCache.h`, `RenderMetrics.h`, `ObjMeshOptimization`; base VGI: `visualitzacio`, `glut_geometry`, `material`, `shader`, `quatern` |
| `ui/` | `RuntimeUi.*` (HUD, menús, subtítulos), `RuntimeUiLogic.h` (decisiones puras), `DebugPanels.cpp`, `NetworkPanels.cpp`, `StatusBar.*`, `UiScale.h` |
| `editor/` | `SceneEditor.*`, `SceneSerializer.*`, `SceneHistory.h`, `EditorCamera.h` y el núcleo sin ventana |
| `audio/` | `AudioManager.*` (miniaudio), `SubtitleManager.*` |
| `networking/` | Ver §8, con la lista de archivos |
| `vr/` | `OpenXRManager.*`, `VrView.h`, `StereoView.h` |
| `diagnostics/` | `GrabDiagnostics.*` |
| `platform/` | `platform_compat.h`, `file_dialog_compat.h` |
| `legacy_vgi/` | Entorno académico: `LegacyVgiState`, `VgiMainWindow`, `VgiMenus`, `VgiMenuCallbacks`, `VgiKeyboard`, `VgiFileActions`, `VgiMaterial`, `VgiStatusBar`, `VgiTimer`, `constants.h`, `framework.h` |

Controles de la base VGI movidos desde `main.cpp`, cada uno con su contexto:

| Archivo | Función |
|---|---|
| `rendering/RuntimeRenderControls.*` | Modos de iluminación, materiales, texturas, luces, selección y caché de shaders |
| `rendering/LegacyVgiViewControls.*` | Menús de cámara, vista y proyección |
| `rendering/LegacyVgiGeometryControls.*` | Menús de objeto, transformaciones geométricas y visibilidad |

### Fuera de `src/`

| Ruta | Función |
|---|---|
| `assets/` | `models/`, `scenes/`, `shaders/`, `textures/`, `audio/`, `animations/`, `ui/`, `subtitles/`, `licenses/` |
| `tests/` | Pruebas CTest (§11) |
| `tools/` | Ver la tabla siguiente |
| `third_party/` | ImGui, ImGuizmo, SOIL2, cgltf, miniaudio, nativefiledialog, nlohmann, stb |
| `visualstudio/`, `EntornVGI.slnx` | Proyecto Visual Studio **generado** |
| `archive/legacy/` | Material histórico; no se compila ni se carga |

| Herramienta | Función |
|---|---|
| `tools/generate_vs_project.py` | Regenera `visualstudio/` a partir de la compilación de CMake |
| `tools/exportar_glb.py` | Exportación de modelos GLB desde Blender |
| `tools/medir_obj.py` | `halfSize` y `offset` de un OBJ para la física |
| `tools/preparar_sector_laboratorio.py` | Suelo y rejilla del laboratorio (modifica la escena) |
| `tools/preparar_texturas_escapeuab.py` | Prepara texturas Poly Haven |
| `tools/instalar_assets_laboratorio_escapeuab.py` | Importa assets del laboratorio |
| `tools/refactor/01-types-and-files.txt`, `02-application.txt`, `03-module-functions.txt`, `04-neutral-methods.txt`, `05-members.txt`, `06-remaining-types.txt` | Registro de renombrados del refactor, para portar ramas antiguas |
| `tools/refactor/cppmap.py`, `includecase.py`, `literals.py` | Aplicación y comprobaciones de esos renombrados |

## 3. Dónde está lo que antes vivía en `main.cpp`

| Responsabilidad | Ahora |
|---|---|
| Estado global | `app/AppState.cpp`, `app/state/` |
| Argumentos, ventana, GL, ImGui | `app/Application.cpp` |
| Bucle principal | `app/MainLoop.cpp` |
| Limpieza | `app/Shutdown.cpp` |
| Teclado, ratón, cursor | `app/input/` |
| Luces y oclusores | `app/render/SceneLighting.cpp` |
| Interacción, agarre, lanzamiento | `gameplay/interaction/` |
| Hirity y audio ambiental | `gameplay/hirity/` |
| Jugador FPS y cámara | `gameplay/player/` |
| Carga de escenas | `scene/SceneRuntime.cpp` |
| Red y autoridad | `networking/session/`, `networking/replication/` |
| Dibujo de escena, estéreo, VR | `rendering/LegacyVgiSceneRenderer.cpp`, `rendering/RuntimeRenderer.cpp` |
| HUD, menús, paneles | `ui/`, `app/ui/UiFrame.cpp` |
| Menús y teclado académicos | `legacy_vgi/` |

El detalle función a función está en [../PORTING_MAP.md](../PORTING_MAP.md).

## 4. Arranque y ciclo principal

**Arranque** (`main.cpp`):

1. `SetWorkingDirectoryToAssetRoot`.
2. `audioManager.init()`, carga de `assets/subtitles/hirity.json`, aviso de reproducción hacia subtítulos.
3. Lectura de `retro_settings.ini`, si existe.
4. `ParseNetworkArguments` (`--host`, `--connect HOST[:PUERTO]`, `--port`, `--name`).
5. `CreateMainWindow` (OpenGL 4.6 core, respaldo 3.3; macOS 4.1), `InitializeGraphicsResources`, callbacks, ImGui.
6. `StartGame`: preset VGI de arranque, escena `assets/scenes/aula_informatica_calibrada.json`, cápsula del
   jugador y red si se pidió. Si falla, el programa termina.
7. Framebuffer interno y `RetroPostFX`. El juego empieza en el menú principal.

**Cada fotograma** (`MainLoop.cpp`):

1. `glfwPollEvents`, pausa, editor, cursor, carga del lanzamiento.
2. Solo en `GAME_STATE_PLAYING`: agachado, movimiento, estamina, puertas, eventos y gatillos XR, cinemáticos,
   objeto agarrado, paso de Bullet, cámara desde la cápsula.
3. Red (siempre).
4. Hirity, audio ambiental, objetivo de interacción, luces.
5. Diagnóstico de capturas (si está activo), interfaz ImGui y peticiones del editor.
6. Render: OpenXR o escritorio (framebuffer interno → posprocesado o blit), ImGui a resolución nativa, swap.

**Cierre:** guardar `retro_settings.ini`, destruir `RetroPostFX`, `ShutdownApplication` (ImGui, red, OpenXR,
NFD, modelos, física, recursos GL, shaders, ventana).

## 5. Estado global y contextos

El estado es global y estático (§2). Los módulos reciben un `struct` de referencias y callbacks:
`HudContext`, `MenuContext`, `SubtitleContext`, `MultiplayerContext`, `DynamicObjectsContext`,
`DebugPanelsContext` y `FrameContext` (`ui/RuntimeUi.h`), `InteractionContext`, `PlayerContext`,
`HiritiContext`, `SessionContext`, `SceneRuntimeContext`, `SceneRenderContext`.

Los contextos de `src/ui` usan callbacks para lo que puede cambiar a mitad de fotograma. Se inicializan por
posición: **añadir un campo obliga a actualizar el inicializador** en `UiFrame.cpp` o en el adaptador.

## 6. Pipeline gráfico

```text
OnPaint ─> framebuffer interno (Ps1Framebuffer) ─> RetroPostFX o blit nearest ─> ventana
                                                                                   └─> ImGui (HUD nítido)
```

- `rendering/RuntimeRenderer.cpp`: `OnPaint`, estéreo (F2), bucle por ojo de OpenXR.
- `rendering/LegacyVgiSceneRenderer.cpp`: objetos OBJ, puertas GLB, teléfono, avatares, viewmodel, niebla.
- Shaders de escena: `assets/shaders/` (`phong`, `gouraud`, `flat`, `skybox`, `eixos`). El número de luces (17)
  cambia a la vez en C++ y shaders.
- `rendering/Ps1Framebuffer.*`: color RGB nearest y profundidad/stencil; `Resize` recrea solo si cambia y
  vuelve a 960×540 si el framebuffer no queda completo.
- `rendering/RetroPostFX.*` con `assets/shaders/retro_post.vert/.frag`: ajustes de color, viñeta, granulado,
  cuantización y dithering por píxel **interno**, sin ruido temporal. Si el shader falla se usa el blit.
- `rendering/RetroSettings.h`: presets, límites, persistencia y la referencia en CPU del shader.

**Cuándo hay efectos** (`retro::IsActive`): preset distinto de Original y sin editor, sin estéreo F2 y sin
sesión OpenXR. Sin efectos se dibuja a 960×540 con el blit anterior. Activos, el alto es el del preset y el
ancho sigue la proporción de la ventana.

| Preset | Papel | Resumen |
|---|---|---|
| `PsxClassic` | **Oficial**, valor inicial | 360p, 4 bits, Bayer 8×8 cromático 1,00, saturación 0,90, contraste 1,12, viñeta 0,10, niebla 0,040, texturas nearest |
| `Cinematic` | Depuración | 540p, 5 bits, Bayer 4×4 0,85, 0,92 / 1,08, sin viñeta ni niebla |
| `Original` | Depuración | Presentación anterior: sin shader, sin nearest, 960×540 |

La niebla es el uniform `uFogDensity` de `phong_shdrML.frag` (los 3 primeros metros quedan limpios); Gouraud y
Flat no la tienen. El filtrado nearest se aplica solo a las texturas de material del entorno
(`sceneMaterialTextures`), desde `MainLoop.cpp`.

**HUD** (`ui/RuntimeUi.cpp`): retícula cuadrada y, al cargar un lanzamiento, un anillo de 20 bloques
alineados a píxeles físicos (geometría en `RuntimeUiLogic.h`).

**Viewmodel** (`rendering/HeldViewModel.h`): experimental y desactivado por defecto. Cambia solo la matriz de
dibujo del objeto que sostiene el jugador local. Hiriti y los objetos de más de 0,6 m usan la vista física.

## 7. Física e interacción

- Bullet con Z hacia arriba, metros y paso fijo de 1/60 s. Las cajas vienen de `physics.halfSize` y
  `physics.offset` del JSON, en espacio del modelo.
- `PhysicsScene` relaciona cuerpos con **índices** de `sceneObjects`. `Hiriti` se reconoce por el id `"hiriti"`.
- E: interactuar o coger; con un objeto, mantener carga (1,25 s máximo) y soltar lanza (3 a 15 m/s).
- `PlayerHasGrab` cubre el agarre local y, en un cliente, el objeto que el host le ha asignado.
- OpenXR usa el gatillo derecho para coger y lanzar y el izquierdo para la linterna.

## 8. Multijugador y autoridad

| Archivo (`src/networking/`) | Función |
|---|---|
| `transport/NetProtocol.*` | Formato binario de mensajes |
| `transport/NetSession.*` | Conexiones, peers y fiabilidad |
| `transport/NetService.*` | Hilo de red y colas hacia el juego |
| `transport/NetTransport.h`, `ENetTransport.*` | Interfaz de transporte e implementación ENet (UDP, puerto 7777) |
| `replication/NetGame.*`, `NetGameLogic.h` | Sincronización del juego y reglas puras |
| `replication/NetAdapters.h` | Puertas GLB vistas desde la red |
| `replication/NetAvatar.*` | Dibujo de jugadores remotos y objeto de prueba |
| `replication/Game/HostPhysicsPublication.h` | Detecta cuándo un cuerpo del host se duerme, para publicar su estado final |
| `replication/Game/RemotePhysicsTimeline.h` | Interpolación de la física en el cliente |
| `replication/Game/SceneNetworkState.h` | Estado de red de los objetos de la escena |
| `session/NetworkSessionRuntime.*`, `SessionRules.h` | Arranque, parada y actualización de la sesión; reglas puras |
| `preferences/NetworkPreferences.*` | Última dirección usada |

**El host manda**: simula la física y valida agarres y lanzamientos; en el cliente los dinámicos son
cinemáticos. La identidad de escena depende del nombre del fichero y de los `id`: renombrarlos rompe la
compatibilidad. Los cambios visuales no pasan por la red. Más en
[../MULTIJUGADOR_LAN.md](../MULTIJUGADOR_LAN.md) y [../MULTIPLAYER_SYNC.md](../MULTIPLAYER_SYNC.md).

## 9. Editor, escenas, audio y subtítulos

- **Editor** (F6): `editor/SceneEditor.cpp`; guarda con temporal, copia `.bak` y copia al árbol fuente. En red
  solo el host puede editar. [../EDITOR_PAREDES_PUERTAS.md](../EDITOR_PAREDES_PUERTAS.md).
- **Escenas:** `assets/scenes/*.json` (`objects`, `animated`, `architecture`, `playerSpawn`,
  `indirectLighting`). La carga es transaccional.
- **Audio:** `AudioManager::playOneShot` devuelve un identificador; `isPlaying`, `playbackCursorSeconds` y
  `stopOneShot` actúan sobre él. El audio no se detiene en pausa.
- **Subtítulos:** `SubtitleManager` recibe cada reproducción real y muestra la de mayor prioridad mientras
  suena. Datos en `assets/subtitles/hirity.json` (`audio`, `speaker`, `text`, `language`, `priority`,
  `category`, `verified`, `segments` opcional). Lo no verificado se marca en pantalla.

## 10. Configuración y persistencia

| Qué | Dónde | Cuándo |
|---|---|---|
| Ajustes gráficos y de subtítulos | `retro_settings.ini` en la raíz de recursos: `build/<preset>/bin/` con CMake; con MSBuild u otra disposición puede ser otra carpeta (está en `.gitignore`) | Se lee al arrancar y se escribe **al cerrar** |
| Última dirección de red | `multijugador.ini` en la carpeta de configuración del usuario | Al conectar como cliente |
| Posición de ventanas ImGui | `imgui.ini` en el directorio de trabajo | Automático |
| Registro de agarre | Carpeta de estado del usuario | Con las variables de depuración |

`retro_settings.ini` es `clave=valor`, con `version=2`. El preset elegido y los ajustes retocados
(`modified=1`) **se respetan**; un fichero anterior a la versión 2 y sin retocar adopta la definición actual de
su preset. Las claves que falten salen del preset guardado y los valores inválidos se corrigen. Para
adoptar el aspecto oficial en una instalación anterior: elegir `PSX Classic (oficial)` en F1, o borrar el
fichero. El juego lo escribe en cada salida normal, así que su existencia no implica que alguien eligiera algo.

| Variable de entorno | Efecto |
|---|---|
| `ENTORNVGI_RETRO_CAPTURE=<carpeta>` | Recorre fases, guarda PNG y `metricas.csv` y cierra. No guarda ajustes |
| `ENTORNVGI_DEBUG_GRAB`, `ENTORNVGI_DEBUG_GRAB_PERSIST` | Registro de agarre |
| `ESCAPETHEUAB_EDITOR_DEBUG` | Diagnóstico de entrada del editor |

## 11. Compilación, pruebas y Visual Studio

```powershell
.\run-windows.cmd -NoRun
.\run-windows.cmd -Configuration Release -NoRun
ctest --test-dir build/windows-debug --output-on-failure
ctest --test-dir build/windows-release --output-on-failure
python tools/generate_vs_project.py          # tras añadir o quitar fuentes
python tools/generate_vs_project.py --check
```

Linux y macOS: `bash run-linux.sh`, `bash run-macos.sh`.

Pruebas (25 en Windows con ENet): escena, editor (4), GLB, física, OBJ (2; `ObjLoaderTests` se omite sin
contexto gráfico), OpenXR, vista VR, movimiento, escala UI, UI en ejecución (`RuntimeUiTests`: retícula y
anillo), retro (`RetroTests`: presets, dithering, viewmodel, subtítulos), Hirity, audio (2), red (3:
`NetworkTests`, `NetJuegoTests`, `ENetLoopbackTests`), preferencias de red, jugador, diagnóstico de agarre y el lanzador de Windows.

No hay pruebas de `app/`, del dibujo ni de los shaders de escena.

Para añadir un `.cpp`: `src/CMakeLists.txt` (y `tests/CMakeLists.txt` si se prueba) y regenerar el proyecto
Visual Studio. MSBuild deja sus binarios en `build/msbuild/`.

## 12. Limitaciones y pendientes

- Sin puzles todavía; no existe `gameplay/puzzles/`.
- Subtítulos sin verificar contra el audio; sin tipografía propia; no se ven en el visor VR.
- Viewmodel experimental: sin validar con paredes ni al lanzar.
- OpenXR: sesión solo implementada en Windows y sin validar con visor.
- Linux y macOS sin compilar tras el refactor y el trabajo retro.
- Pasos sin sonido: faltan los WAV y nada llama a `playFootstep`.
- El panel de red aparece al arrancar; los paneles F1 no se pueden ocultar uno a uno.
- La escena arranca con demos de puerta y cubo en la ruta de dibujo.
- Derechos de distribución de varios assets sin confirmar: [../../assets/licenses/README.md](../../assets/licenses/README.md).
- Estado de la QA manual: [../midnight-hoops/VALIDATION.md](../midnight-hoops/VALIDATION.md).
