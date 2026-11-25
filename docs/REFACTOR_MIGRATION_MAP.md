# Mapa de migración del refactor

Plan **propuesto** sobre la base `62a7018571559df2490bf2a879734bed647bfe10`, fechado el 2026-11-25. El contexto, los contratos, los no objetivos y las marcas de evidencia ([C], [H], [D], [P]) están en [ARCHITECTURE.md](ARCHITECTURE.md).

**Estado a 2026-11-25: inventario y diseño completos; extracción de bibliotecas de P2 completada e integrada en `4418c2d` (§6.4), con comprobaciones de la puerta de P2 aún abiertas; P3 iniciada, con el corte S3 integrado en `0ccaea3` (§9). P0 no está superada y no hay GO funcional.** El estado de cada etapa se anota en §8; una etapa solo se da por superada con commit y resultados reales.

## 1. Estado de partida

- **P0 no está superada.** La QA manual de gameplay sigue **PENDIENTE**, incluidos el agarre y lanzamiento de un objeto normal desde el cliente y la paridad visual entre host y cliente (ARCHITECTURE.md §2).
- La autorización explícita del usuario del 2026-11-25 dispensa esa puerta **solo para iniciar un refactor provisional y aislado**. No permite declarar P0 superada, no es un GO funcional y no autoriza publicar en `master`.
- El **GO funcional** sigue pendiente y no se consolida ni se publica en `master` mientras falte.
- La extracción de bibliotecas de P2 está integrada en la rama local `refactor/project-architecture` como `4418c2d08c1186ce896309c9487315a9e3ec4a21`, a partir de `0aa6329` en `itsCarlosDev/cmake-core-libraries-codex` [C]. Detalle, evidencia y límites en §6.4.
- **Las líneas base de P1 siguen incompletas**, en particular las capturas y las referencias de ejecución. La puerta de P2 compara contra ellas.
- P3 se ha iniciado, al amparo de la autorización provisional, con la puerta de P2 abierta y P1 incompleta: el corte S3 (`RemotePhysicsTimeline`) está integrado como `0ccaea3` (§9).
- Conviene coordinar antes de editar CMake o `main.cpp`.

## 2. Reglas

1. Un solo ejecutable; el target sigue llamándose `EntornVGI` y la salida sigue en `build/<preset>/bin/`.
2. Primero bibliotecas con los ficheros en su sitio; después costuras y pruebas; **mover ficheros al final**.
3. Un commit (o una serie corta y contigua) por etapa, revertible, sin arreglos funcionales mezclados.
4. Un único propietario de `main.cpp`, `main.h`, `EntornVGI/CMakeLists.txt` y `tests/CMakeLists.txt` por etapa.
5. No se avanza con una puerta en rojo. Lo que no se ejecuta se declara como no ejecutado.

## 3. Puntos de control y vuelta atrás

| Punto | Referencia | Estado |
|---|---|---|
| C0 | Rama `checkpoint/pre-refactor-stabilization-2026-11-25` en `62a7018` | Existe en el repositorio local [C] |
| C1…Cn | Un punto por etapa superada, p. ej. etiqueta `arch/p2` | [P] No creados; el repositorio no tiene etiquetas |

Procedimiento [P]:

- Antes de abrir una etapa, anotar el SHA de partida.
- Para deshacer, usar `git revert` de los commits de la etapa en orden inverso. No usar `reset --hard` ni reescribir ramas compartidas. Para el cambio de P2: `git revert 4418c2d` en `refactor/project-architecture` (SHA de partida: `8dc63b5`). Para el corte S3: `git revert 0ccaea3` (SHA de partida: `f9cd52b`), antes de revertir `4418c2d`.
- Si ya hay etapas posteriores encima, revertir primero las posteriores.
- Tras revertir, configurar en un directorio de build limpio y repetir la puerta común.
- El editor puede escribir en `EntornVGI/assets` (copia al árbol fuente). Revertir código no revierte escenas guardadas: revisar `git status` sobre `assets/` después de cada sesión de QA.
- Volver a C0 es el último recurso y requiere decisión del usuario.

## 4. Puerta común de cada etapa

Ejecutar sobre el commit de la etapa y guardar el resultado con su SHA:

1. `git diff --check`.
2. `.\run-windows.cmd -NoRun` y `.\run-windows.cmd -Configuration Release -NoRun`, con **directorio de build limpio** si cambian targets o se mueven ficheros.
3. CTest completo en Debug y Release (referencia histórica: 21 registradas en Windows con ENet [D]). Anotar las omitidas.
4. En etapas que toquen red o sus bibliotecas: `NetJuegoTests --repeat until-fail:20` y `ENetLoopbackTests --repeat until-fail:10`, en secuencia.
5. Arranque real del juego desde `bin/`: menú, entrada en PLAY, escena calibrada.
6. Comparar con las líneas base de P1.

macOS, Linux y VR se declaran **no validados** mientras no se ejecuten en esas plataformas.

## 5. Etapas [P]

| Etapa | Cambio | Puerta de salida adicional |
|---|---|---|
| **P0. Gate** | QA manual de gameplay: agarre y lanzamiento de un objeto normal y de Hirity en host y cliente, con `request → accepted → holding` y registros; paridad visual host/cliente; reanudación de Hirity al soltarlo | QA superada con evidencia. **PENDIENTE**. La dispensa del 2026-11-25 (§1) solo permite iniciar el trabajo provisional; registrar la autorización no supera la puerta |
| **P1. Línea base** | Sin cambios de código: avisos de compilación, CTest, capturas 1080p/4K, identidad `ET2-…` de cada escena, bytes de referencia de los paquetes, un JSON guardado por el editor | Referencias archivadas y comparables |
| **P2. Bibliotecas sin mover ficheros** | Primera migración real (§6) | Mismas pruebas registradas y mismo resultado que en P1 |
| **P3. Red de seguridad** | Extraer **sin cambiar** a funciones con parámetros explícitos la autoridad de agarre, la aplicación de réplicas, `identificadorEscenaRed` y la máquina de Hirity; añadir pruebas de caracterización y de enlace de shaders | Pruebas nuevas en verde antes y después de cada extracción |
| **P4. Estado** | `main.h` pasa a `extern` más un `.cpp`, o a una estructura; agrupar estado de jugador, escena, render y aplicación | Debug y Release; arranque y menús sin cambios |
| **P5. Hojas** | Audio, escala de UI, métricas, diagnóstico de agarre, colisionadores, panel F4, panel de red, HUD | Revisión visual |
| **P6. Base VGI** | Trasladar el bloque VGI de `main.cpp` íntegro a unidades propias. El ciclo `material`/`glut_geometry` puede romperse moviendo `SetColor4d`, en commit aparte | Lista de menús y atajos VGI |
| **P7. Render** | Caché de recursos, luces, oclusores, dibujo de escena, FBO PS1, estéreo y VR | Capturas contra P1; 13 luces visibles |
| **P8. Gameplay** | Control del jugador, después interacción y agarre, después Hirity | Pruebas de P3 y QA offline |
| **P9. Red** | Sincronización de escena, réplica cliente, autoridad del host. **Sin tocar el formato en cable** | Bytes e identidades de P1 idénticos; QA host/cliente |
| **P10. Aplicación** | `main()` queda como composición; RAII conservando el orden de limpieza | Cierre limpio; editor con cambios sin guardar |
| **P11. Mover ficheros** | `git mv` por módulo solo cuando su target y sus pruebas existan; actualizar CMake, pruebas, `.vscode` y documentación. Los recursos no se mueven | Build desde cero; revisar mayúsculas y Unicode |
| **P12. Nombres** | Opcional y último, por módulo. Excluye IDs, claves JSON, protocolo, rutas de assets, textos de jugador y base VGI | Bytes e identidades sin cambios |
| **Aparte** | Reorganizar assets, limpiar artefactos versionados, decidir sobre `.slnx`/`.vcxproj` | Decisión y rama propias |

P6 a P9 no empiezan sin las pruebas de P3 en verde y sin P4.

### Correspondencia de origen a destino

| Origen actual | Destino propuesto | Etapa |
|---|---|---|
| `main()`, callbacks, `SetWorkingDirectoryToAssetRoot` | `app` (raíz de recursos en `core`) | P10 |
| `main.h` | Estado explícito | P4 |
| `moverJugadorFPS`, `sincronizarJugadorYCamara`, `crearJugadorFisico` | `gameplay` (control del jugador) | P8 |
| `buscarObjetivoInteraccion`, `interactuar`, carga y lanzamiento | `gameplay` (interacción y agarre) | P3, P8 |
| `actualizarHiriti` y sus sonidos | `gameplay` (Hirity, emite eventos) | P3, P8 |
| `iniciarRed`, `detenerRed`, `actualizarRed`, captura y aplicación de estados | `networking` | P3, P9 |
| `cargarYAplicarEscena` | `scene` (carga transaccional) y caché en `rendering` | P7, P9 |
| `dibuixa_Escena`, `OnPaint`, luces, oclusores, FBO, estéreo, VR | `rendering`, `vr` | P7 |
| `draw_Menu_ImGui`, HUD, paneles | `ui`, `diagnostics` | P5 |
| `ShowEntornVGIWindow`, `On*`, `Teclat_*`, `InitGL` | `legacy_vgi` | P6 |

## 6. P2 en detalle: bibliotecas CMake

Patrón: el de `EntornVGI_network`. Todas **STATIC**, definidas sin mover fuentes, enlazadas por el ejecutable y por las pruebas en lugar de listar `.cpp`.

### 6.1 Agrupación propuesta según dependencias actuales

Propuesta original sobre la base; lo integrado se describe en §6.4. Los nombres son orientativos. Conviene un commit por biblioteca, empezando por las de menos dependencias.

| Biblioteca | Fuentes | Depende de | Notas |
|---|---|---|---|
| `etu_scene` | `SceneLoader.cpp` | GLM | Necesita la cabecera `MundoFisico.h`, sin enlazar Bullet |
| `etu_animation` | `ModeloGLB.cpp`, `ObjetosAnimadosGLB.cpp` | GLM, cgltf | |
| `etu_physics` | `MundoFisico.cpp`, `EscenaFisica.cpp` | `etu_scene`, `etu_animation`, `EntornVGI_bullet` | |
| `etu_serializer` | `Editor/SerializadorEscenas.cpp` | `etu_scene` | Aislada por sus rutas compiladas (§6.2) |
| `etu_editor_core` | `LimitesModelo`, `SelectorObjetos`, `VisibilidadEditor`, `ValidadorColocacion` | `etu_scene`, `etu_animation` | `EditorEscenas.cpp` e ImGuizmo pueden quedar en el ejecutable hasta que haya un target de ImGui |
| `etu_gamenet` | `NetJuego.cpp` | `EntornVGI_network`, GLM | Arrastra `VistaVR.h` |
| `etu_diagnostics`, `etu_netprefs` | `GrabDiagnostics.cpp`, `PreferenciasRed.cpp` | | |
| `etu_vr` | `OpenXRManager.cpp` | `EntornVGI_openxr`, OpenGL, GLEW | |
| `etu_vgi_geometry` | `objLoader`, `ObjMeshOptimization`, `glut_geometry`, `material` | OpenGL, GLFW, GLEW, SOIL2, meshoptimizer opcional | `material` y `glut_geometry` van juntas por el ciclo. Verificar los símbolos externos al separarlas [H] |

`main.cpp`, `AudioManager`, `DibujoGLB`, `DibujoColisionadores`, `NetAvatar`, `escena`, `visualitzacio`, `shader` y `quatern` pueden seguir en el ejecutable en P2.

### 6.2 Trampas conocidas

- **Las definiciones no siguen a las fuentes.** En la base `62a7018` eran `PRIVATE` de `EntornVGI` [C]: `GLM_ENABLE_EXPERIMENTAL`, `NOMINMAX`, `ENTORNVGI_HAS_MESHOPTIMIZER`, `ENTORNVGI_USE_NFD_EXTENDED`/`..._LEGACY_NFD`, `ENTORNVGI_RUNTIME_ASSETS_DIR`, `ENTORNVGI_SOURCE_ASSETS_DIR` y los avisos (`/W3`, `-Wall -Wextra`). Cada biblioteca debe declarar las que necesite.
- Sin `ENTORNVGI_HAS_MESHOPTIMIZER`, el cargador OBJ vuelve en silencio a la ruta sin optimizar [H].
- **Rutas del serializador.** Sin las dos definiciones de directorios, el guardado deja de copiar al árbol fuente, sin error de compilación [C]. `EditorPersistenceTests` inyecta directorios temporales y `EditorTests` compila el serializador sin definirlas. Una sola biblioteca con las rutas reales cambiaría ambas pruebas, y una prueba que guardara bajo `bin/assets` escribiría en el árbol fuente [H]. Mantener el aislamiento: seguir compilando el `.cpp` en esas pruebas o usar una variante por configuración.
- `OpenXRManager.cpp` obtiene `ENTORNVGI_HAS_OPENXR` de `EntornVGI_openxr`; la biblioteca debe enlazarlo.
- `CMAKE_CXX_STANDARD 20` se fija en `EntornVGI/CMakeLists.txt` y no alcanza a `tests/`. Usar `target_compile_features(... PUBLIC cxx_std_20)` o subirlo a la raíz.
- No estrechar todavía el include público de `EntornVGI_network`; hacerlo en un cambio posterior.
- Las bibliotecas estáticas con dependencias mutuas o registro por inicializadores pueden enlazar de forma distinta según la plataforma [H].
- No cambiar el nombre del target, el directorio de salida ni `EntornVGI_runtime_resources`: los lanzadores, `WindowsLauncherDoesNotRunStaleBuild` y `.vscode` dependen de esas rutas.
- No añadir bibliotecas nuevas a `.slnx`/`.vcxproj`; su estado se decide aparte.

### 6.3 Puerta de salida de P2

- Mismo conjunto de pruebas registradas que en P1, en Debug y Release, desde directorio limpio.
- Las líneas de configuración siguen informando ENet, OpenXR y meshoptimizer como en P1.
- La ruta de meshoptimizer sigue activa.
- El guardado del editor sigue copiando al árbol fuente (comprobar y descartar el cambio).
- El juego arranca desde `bin/`.
- Sin cambios en `.cpp`/`.h`; si alguno es imprescindible, va en un commit separado y justificado.

### 6.4 Cambio integrado (2026-11-25)

Commit `4418c2d08c1186ce896309c9487315a9e3ec4a21` en `refactor/project-architecture`; commit de origen `0aa6329`, con el mismo parche [C].

**Contenido [C]**

| Target estático | Fuentes | Enlaza |
|---|---|---|
| `EntornVGI_scene_loader` | `SceneLoader.cpp` | GLM |
| `EntornVGI_glb_model` | `ModeloGLB.cpp` | GLM (cgltf como include privado) |
| `EntornVGI_editor_core` | `LimitesModelo.cpp`, `SelectorObjetos.cpp`, `ValidadorColocacion.cpp`, `VisibilidadEditor.cpp` | GLM, `EntornVGI_glb_model`, `EntornVGI_scene_loader` |

- `EntornVGI_editor_core` declara su cierre de dependencias de GLB y escena.
- Los avisos (`/W3`, o `-Wall -Wextra -Wno-unused-parameter`) se conservan y se aplican al ejecutable y a las tres bibliotecas.
- Se conservan las dos variantes de prueba del serializador: `EditorPersistenceTests` con directorios temporales y `EditorTests` sin directorios. `SerializadorEscenas.cpp` sigue compilándose en el ejecutable y en esas dos pruebas.
- Solo cambian `EntornVGI/CMakeLists.txt` y `tests/CMakeLists.txt`. **No se ha movido ni modificado ningún fichero fuente.**
- **Sin cambio de comportamiento:** no cambia el código fuente [C]; la verificación aportada es de compilación y CTest, no de juego.

**Alcance:** las demás agrupaciones de §6.1 (animación de objetos, física, serializador, red de juego, diagnóstico, VR, geometría VGI) no se han creado como bibliotecas. `PhysicsTests` sigue compilando `ModeloGLB.cpp` directamente [C].

**Validación informada por el supervisor** sobre el diff exacto validado; no reejecutada ni verificada al redactar este documento:

| Comprobación | Resultado informado |
|---|---|
| `.\run-windows.cmd -NoRun` (Debug) | Salida 0 |
| CTest completo Debug | 21/21 |
| `.\run-windows.cmd -Configuration Release -NoRun` | Salida 0 |
| CTest completo Release | 21/21 |
| Dependencias | ENet, Bullet, meshoptimizer y OpenXR habilitados |
| Entorno | Raíz de vcpkg `D:\Visual Studio\VC\vcpkg`; descargas locales al worktree de validación |
| Revisión de Claude (segunda ronda) | Sin bloqueos; dos observaciones menores opcionales |
| Revisión de Codex | Sin bloqueos |

**Puerta de P2 (§4 y §6.3): no cerrada.**

- Cubierto por lo informado: compilación y CTest en Debug y Release, y dependencias habilitadas.
- Sin evidencia registrada: directorio de build limpio, arranque del juego desde `bin/`, copia al árbol fuente al guardar desde el editor y comparación con las líneas base de P1.

**Limitaciones:** sin compilación nueva en macOS ni Linux; sin QA manual visual ni de gameplay; sin prueba con hardware OpenXR; líneas base completas de capturas y ejecución de P1 pendientes. **Esto no constituye un GO funcional.**

## 7. Riesgos

| Riesgo | Nivel | Mitigación |
|---|---|---|
| Refactor con agarre de cliente sin verificar | Crítico | Líneas base de P1; QA de P0 abierta |
| Autoridad, réplica y Hirity sin pruebas, con `static` locales y orden implícito | Crítico | P3; mover sin reescribir |
| Cambiar nombre de escena, `id` o serialización | Alto | Identidades y bytes de P1 |
| Mover assets rompe la raíz, las rutas literales y el destino del guardado | Alto | No se mueven |
| `main.h` con definiciones | Alto | P4 antes de extraer unidades que usen ese estado |
| Orden de inicialización y destrucción | Alto | RAII solo en P10 |
| Estado OpenGL implícito | Alto | Capturas por modo de shader |
| Constantes de luces repartidas | Medio | Cambio atómico |
| Recursos antiguos en `bin/` ocultan rutas rotas | Medio | Build limpio |
| macOS, Linux y VR sin validar | Medio | Declararlo; validar en cada plataforma |
| Varios agentes sobre los mismos ficheros | Medio | Propietario único |

## 8. Registro de ejecución

| Etapa | Commit | Resultados | Pendiente |
|---|---|---|---|
| P0 | Sin commit de superación | **No superada.** Dispensada solo para iniciar el refactor provisional y aislado | Agarre y lanzamiento de objeto normal desde el cliente, paridad visual host/cliente, reanudación de Hirity y el resto de la QA de ARCHITECTURE.md §2 |
| P1 | Sin commit | **Incompleta** | Líneas base completas de capturas y ejecución, y el resto de referencias de §5 sin registrar |
| P2 | `4418c2d` (origen `0aa6329`) | Extracción de bibliotecas completada e integrada; Debug y Release compilan y CTest 21/21 en ambas, según el supervisor (§6.4) | Build limpio, arranque desde `bin/`, guardado del editor y comparación con P1 |
| P3 | `0ccaea3` (origen `7c8b174`), solo corte S3 | **Parcial.** `RemotePhysicsTimeline` integrado; Debug y Release PASS y CTest 21/21 en ambas, según el coordinador (§9) | Autoridad de agarre, resto de la aplicación de réplicas, `identificadorEscenaRed`, máquina de Hirity, pruebas de enlace de shaders, repeticiones de red y QA de gameplay |

## 9. P3, corte S3: `RemotePhysicsTimeline` (2026-11-25)

S3 es la etiqueta de coordinación de este corte; no implica que existan otros cortes terminados. Commit `0ccaea3b7667521ea83d6fc74572933fc9683d16` en `refactor/project-architecture`; commit de origen `7c8b174`, con el mismo árbol y el mismo parche [C].

**Cambios [C]**

| Fichero | Cambio |
|---|---|
| `EntornVGI/Network/Game/RemotePhysicsTimeline.h` (nuevo) | Solo cabecera, espacio de nombres `netjuego::timeline`: tipos de muestra y buffer, y las funciones `StoreRemotePhysicsSample`, `InterpolateRemotePhysicsState` y `ApplyReliableFinalPhysicsState` |
| `EntornVGI/main.cpp` | La estructura local, las dos funciones de buffer e interpolación y el tratamiento del estado final fiable se sustituyen por llamadas a la cabecera |
| `tests/NetJuegoTests.cpp` | Comprobaciones nuevas: muestras inválidas o antiguas, tiempo reconstruido desde el tick, desbordamiento del contador, límite de 8 muestras, interpolación sin extrapolar y estado final fiable |

- No cambia ningún fichero CMake. La cabecera no pertenece a ningún target propio, tampoco a `EntornVGI_network`, y depende de `NetJuegoLogica.h`: aunque está bajo `Network/`, es de la capa de red del juego.
- Las comprobaciones nuevas viven dentro de `NetJuegoTests`, así que CTest sigue en 21 pruebas.
- El buffer, su vaciado, el suavizado y la aplicación a la física siguen en `main.cpp`.
- Los nombres en inglés son de identificadores nuevos; no renombran contratos ni adelantan P12.

**Equivalencia:** este corte sí modifica código fuente. Por lectura del diff, la lógica se traslada con los mismos pasos y constantes (retardo de 0,12 s, 8 muestras, sin extrapolación) y la llamada conserva la guarda de buffer no vacío [C]. No se ha verificado en ejecución del juego. Las pruebas nuevas llegan en el mismo commit y cubren la cabecera extraída, no el código anterior de `main.cpp`, por lo que no se cumple para este corte el «verde antes y después» de la puerta de P3.

**Validación sobre el SHA integrado `0ccaea3`, informada por el coordinador**; no reejecutada al redactar este documento:

| Comprobación | Resultado informado |
|---|---|
| `.\run-windows.cmd -NoRun` (Debug) | PASS |
| `.\run-windows.cmd -Configuration Release -NoRun` | PASS |
| CTest Debug | 21/21 |
| CTest Release | 21/21 |
| Revisión estática de Claude | Sin bloqueos, más ajustes de cobertura |

**Sin evidencia registrada:** `NetJuegoTests --repeat until-fail:20` y `ENetLoopbackTests --repeat until-fail:10` (§4 los pide para cambios de red), arranque del juego desde `bin/` y comparación con las líneas base de P1.

**Pendiente:** pruebas gráficas y de gameplay, incluida la réplica en cliente; compilación y prueba en Linux y macOS; OpenXR con hardware. **Es un avance arquitectónico provisional: no es un GO funcional ni autoriza consolidar o publicar en `master`.**
