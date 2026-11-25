# Guía para el equipo

Cómo moverse por el proyecto después del refactor. Si vienes de una rama anterior (con `EntornVGI/`),
empieza por [PORTING_MAP.md](PORTING_MAP.md).

## Estructura y responsabilidades

| Carpeta | Qué hay |
|---|---|
| `src/app/` | `main.cpp` (solo composición), arranque (`Application`), bucle (`MainLoop`), cierre (`Shutdown`) |
| `src/app/state/` | Estado compartido, una cabecera por responsabilidad (`SceneState.h`, `PlayerState.h`...). Se define en `AppState.cpp` |
| `src/app/adapters/` | Unen el estado con los módulos: construyen los contextos que estos reciben |
| `src/app/input/` | Teclado, ratón y captura del cursor |
| `src/app/render/`, `src/app/ui/` | Código de render e interfaz que todavía lee el estado compartido |
| `src/gameplay/` | `player/`, `interaction/` (coger, lanzar, puertas), `hirity/` |
| `src/scene/` | Carga y aplicación de escenas JSON y OBJ |
| `src/physics/` | Mundo Bullet y física de la escena |
| `src/animation/` | Animaciones GLB y puertas |
| `src/rendering/` | Escena, vista, controles de render, GLB, shaders en CPU, framebuffer PS1 |
| `src/ui/` | HUD, menús y paneles (reciben contextos) |
| `src/editor/` | Editor de escenas (F6) |
| `src/networking/` | `transport/` (protocolo, ENet), `replication/`, `session/`, `preferences/` |
| `src/audio/`, `src/vr/`, `src/diagnostics/`, `src/platform/` | Audio, OpenXR, registro de agarre, compatibilidad |
| `src/legacy_vgi/` | Entorno académico VGI (menús, teclado heredado, ventana "EntornVGI") |
| `assets/` | Todo lo que el juego carga en ejecución |
| `tools/` | Utilidades de assets, generador del proyecto Visual Studio, mapas del refactor |
| `archive/legacy/` | Ficheros sin uso. Nada se compila ni se carga desde aquí |

Cada cabecera de `src/app/state/` compila por sí sola porque incluye el agregado `src/app/AppIncludes.h`; dividir
ese agregado es deuda pendiente.

**Regla de dependencias:** fuera de `src/app/` y `src/legacy_vgi/` ningún módulo incluye cabeceras de la
aplicación. Si un módulo necesita estado, pásaselo en su contexto y enlázalo en `src/app/adapters/`.

## Dónde trabajar

| Quiero... | Ficheros |
|---|---|
| Cambiar menús, HUD o paneles | `src/ui/RuntimeUi.cpp`; lo que se les pasa, en `src/app/ui/UiFrame.cpp`. Menú de pausa VGI: `src/legacy_vgi/VgiMenus.cpp` |
| Colocar los modelos de Marc | En el editor (F6), carpeta `Marc/` del explorador de assets. Si no caben junto a la cámara, aparecen en una zona de preparación a la derecha: reduce la escala y muévelos |
| Hacer puzzles | No existe todavía un módulo. Créalo en `src/gameplay/puzzles/` con su contexto, enlázalo en `src/app/adapters/` y actualízalo desde `src/app/MainLoop.cpp`. La interacción con objetos está en `src/gameplay/interaction/` |
| Cambiar a Hirity | `src/gameplay/hirity/`, `src/app/adapters/HirityAdapters.cpp`, sonidos en `assets/audio/hirity/` |
| Trabajar con audio | `src/audio/AudioManager.*`. Los pasos (`assets/audio/paso_1..3.wav`) no existen y nada llama a `playFootstep` |
| Cambiar escenas o modelos | `assets/scenes/*.json`, `assets/models/`. El juego arranca con `aula_informatica_calibrada.json`. Carga: `src/scene/`. [Editor](EDITOR_PAREDES_PUERTAS.md), [animaciones](ANIMACIONES_GLB.md) |
| Mejorar gráficos | `assets/shaders/`, `src/rendering/`, luces en `src/app/render/SceneLighting.cpp`. El número de luces cambia a la vez en C++ y shaders (ver ARCHITECTURE.md §4.3) |
| Trabajar en multijugador | `src/networking/`. [LAN](MULTIJUGADOR_LAN.md), [sincronización](MULTIPLAYER_SYNC.md). El host manda en la física |
| Trabajar en VR | `src/vr/`, bucle por ojo en `src/rendering/RuntimeRenderer.cpp`. [OpenXR](OPENXR.md) |
| Física | `src/physics/`. [Bullet](FISICAS_BULLET.md) |

Añadir un `.cpp`: apúntalo en `src/CMakeLists.txt` y regenera el proyecto de Visual Studio.

**Contratos que no se traducen ni cambian sin acuerdo:** claves JSON, IDs de objetos, nombres de recursos,
mensajes de red y nombres de uniforms.

## Compilar, ejecutar y probar

```powershell
.\run-windows.cmd                          # Debug, compila y ejecuta
.\run-windows.cmd -Configuration Release   # Release
.\run-windows.cmd -NoRun                   # solo compilar
ctest --test-dir build\windows-debug --output-on-failure
```

Linux y macOS: `bash run-linux.sh`, `bash run-macos.sh` (no validados tras el refactor).
Si `ctest` no está en el PATH, usa una consola "Developer PowerShell" de Visual Studio.

**Visual Studio.** CMake es el sistema principal; puedes abrir la carpeta directamente. También existe
`EntornVGI.slnx`: su proyecto se genera, no se edita a mano. Después de cambiar fuentes:

```powershell
.\run-windows.cmd -NoRun
python tools\generate_vs_project.py
```

Es solo x64 y necesita el componente vcpkg de Visual Studio. Un conflicto de merge en `visualstudio/` se
resuelve regenerando, no editando; antes de un merge ejecuta `python tools\generate_vs_project.py --check`.

Los binarios de MSBuild salen en `build\msbuild\<Debug|Release>\` y la primera compilación instala las
dependencias de `vcpkg.json`.

## Controles y diagnóstico

F1 paneles, F2 estéreo de prueba, F3 OpenXR, **F4 multijugador y objetos dinámicos**, F6 editor,
E interactuar (mantener para cargar el lanzamiento), F linterna, Esc pausa, WASD, Shift, Ctrl.

F4 muestra el panel de red y la tabla de objetos dinámicos (propietario, pose del host y pose aplicada).
Para guardar el registro de agarres, antes de arrancar:

```powershell
$env:ENTORNVGI_DEBUG_GRAB = '1'; $env:ENTORNVGI_DEBUG_GRAB_PERSIST = '1'
```

Detalles en [MULTIPLAYER_DEBUG_GRAB.md](MULTIPLAYER_DEBUG_GRAB.md).

Probar dos jugadores en un equipo:

```powershell
build\windows-release\bin\EntornVGI.exe --host --port 7777 --name Host
build\windows-release\bin\EntornVGI.exe --connect 127.0.0.1:7777 --name Cliente
```

## Ramas

Mientras el refactor no esté en `master`, parte de `refactor/project-architecture`; después, de `master`.
Una rama por tema y de vida corta:

| Rama | Tema |
|---|---|
| `feature/menus` | Menús y HUD |
| `feature/puzzles` | Puzzles (`src/gameplay/puzzles/`) |
| `feature/hirity` | Hirity |
| `feature/audio` | Audio |
| `feature/graphics` | Shaders, luces, aspecto |
| `feature/levels` | Escenas y modelos |
| `feature/multiplayer` | Red |
| `feature/vr` | VR |

Antes de pedir merge: Debug y Release compilan, CTest pasa y has probado en el juego lo que cambiaste.
`src/app/state/` y `src/CMakeLists.txt` son los puntos donde más se pisan las ramas: cambios pequeños.
