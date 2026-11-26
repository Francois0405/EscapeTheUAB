# Inventario de paneles ImGui

Inventario del código de `feature/midnight-hoops`. **No se ha eliminado ni ocultado ningún panel**: esta
tabla sirve para decidir cuáles conservar.

## Quién decide qué se dibuja

`src/ui/RuntimeUi.cpp` (`DrawRuntimeUiFrame`) llama en este orden a: HUD, menús del juego, paneles de red,
ventanas heredadas VGI, paneles F1, editor y contador de FPS. Los contextos se construyen en
`src/app/ui/UiFrame.cpp` (`draw_Menu_ImGui`).

| Grupo | Condición |
|---|---|
| Paneles F1 del juego | `isFpsMode && showDebugPanels` |
| Ventanas heredadas VGI | `!fpsModeActive() \|\| showDebugPanels` (en el juego: también con F1) |
| Paneles de red (F4) | `showNetworkPanel`, **`true` al arrancar** (`src/app/AppState.cpp`), en cualquier estado, incluido el menú |
| Editor (F6) | `sceneEditor.IsEnabled()` |

F1 alterna `showDebugPanels` (`src/app/input/KeyboardInput.cpp`) y, al activarlo, vuelve a abrir la ventana
«EntornVGI Menu». Con F1 abierto se libera el cursor, no hay movimiento y se oculta la retícula.

## Paneles F1 (`src/ui/DebugPanels.cpp`, `DrawDebugPanels`)

Ninguno tiene botón de cierre ni bandera propia.

| Ventana | Contenido | Estado que modifica |
|---|---|---|
| `Movil FPS` | Teléfono en mano: visibilidad, transformación, balanceo | `showPhone`, `phonePosition/Rotation/Scale`, intensidades |
| `Movimiento FPS` | Velocidades, estamina, balanceo de cámara, contador de FPS, métricas, render optimizado | `fps::PlayerMovement`, `fps::CameraBob`, `showFpsCounter`, `optimizedRendering` |
| `Graficos retro` | Presets, parámetros de imagen, subtítulos, viewmodel, aviso de agarre | `retroSettings` |
| `Diagnostico de iluminacion` | Vista de depuración del shader Phong | `lightingDiagnosticMode` |
| `Demo puerta` | Demostración de animación | `demoDoorPlayback` |
| `Fisica (Bullet)` | Colisionadores, Hirity, vista estéreo, activar OpenXR, estado de agarre, cubo de prueba | `showColliders`, `hiritiFollow*`, `stereoTestMode`, OpenXR |
| `Objetos animados GLB` | Abrir o alternar cada puerta | `animatedObjects` |

## Paneles de red (`src/ui/NetworkPanels.cpp`)

| Ventana | Función | Cómo se oculta hoy |
|---|---|---|
| `Multijugador` | `DrawMultiplayerPanel`: offline/host/cliente, IP, puerto, diagnóstico | F4 o el botón X (`showNetworkPanel`) |
| `Objetos dinamicos \| F4` | `DrawDynamicObjectsPanel`: propietario y poses | La misma bandera; además solo en modo FPS |

## Ventanas heredadas VGI

| Ventana | Archivo | Bandera |
|---|---|---|
| `Status Menu` | `src/app/ui/UiFrame.cpp` (`drawLegacyImGuiWindows`) | Ninguna: siempre que se dibuja el grupo |
| `EntornVGI Menu` | `src/legacy_vgi/VgiMainWindow.cpp` (`ShowEntornVGIWindow`) | `show_EntornVGI_window` (X o casilla de Status Menu) |
| `Dear ImGui Demo` | `UiFrame.cpp` | `show_demo_window`, `false` por defecto |
| `Another Window` | `UiFrame.cpp` | `show_another_window`, `false` por defecto |
| `About` | `src/legacy_vgi/VgiMenus.cpp` (`ShowAboutWindow`) | `show_window_about` |
| `About Dear ImGui`, `Dear ImGui Style Editor` | `VgiMainWindow.cpp` | Banderas `static` locales, abiertas desde el menú |

`src/legacy_vgi/VgiMenus.cpp` define además `MostraEntornVGIWindow`, otra ventana «EntornVGI Menu» de la base
académica que ningún código llama.

## Ventanas que no son de depuración

| Elemento | Archivo |
|---|---|
| `MenuPrincipal`, `MenuPausa` | `src/ui/RuntimeUi.cpp` (`DrawMenus`) |
| `FPS Overlay` (solo con `showFpsCounter`) | `src/ui/RuntimeUi.cpp` |
| Retícula, arco de carga, estamina, subtítulos, avisos | `src/ui/RuntimeUi.cpp`; son listas de dibujo, no ventanas |
| `Editor Toolbar`, `Hierarchy`, `Inspector`, `Asset Browser`, `Editor Camera`, `Editor input debug`, diálogos | `src/editor/SceneEditor.cpp` |

## Cómo ocultar cada ventana sin perder la función (propuesta, no implementada)

1. Añadir un `struct PanelVisibility` con un `bool` por ventana y prestarlo en `DebugPanelsContext`.
2. Envolver cada bloque `Begin`/`End` en su `bool` y pasarlo como `p_open` a `ImGui::Begin` para tener botón X.
3. Ofrecer las casillas en una ventana índice abierta con F1.

Cosas que hay que respetar al ocultar:

- **`Status Menu` no es solo informativa**: cada fotograma copia `clear_colorB` a `c_fons` y `clear_colorO` a
  `col_obj`. Si se oculta la ventana, esas asignaciones deben seguir ejecutándose. `c_fons` decide además el
  estilo claro u oscuro de ImGui.
- `Graficos retro` es la única forma en juego de cambiar presets y subtítulos; la alternativa es editar
  `retro_settings.ini`.
- `Fisica (Bullet)` es la única forma en juego de ver colisionadores; F2 y F3 siguen funcionando sin ella.
- `Multijugador` se puede ocultar sin afectar a `--host` y `--connect`. Para que no aparezca al arrancar basta
  cambiar el valor inicial de `showNetworkPanel` (decisión pendiente).
