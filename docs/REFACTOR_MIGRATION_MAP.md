# Mapa de migración del refactor

Plan **propuesto** sobre la base `62a7018571559df2490bf2a879734bed647bfe10`, fechado el 2026-11-25. El contexto, los contratos, los no objetivos y las marcas de evidencia ([C], [H], [D], [P]) están en [ARCHITECTURE.md](ARCHITECTURE.md).

**Estado a 2026-11-25: inventario y diseño completos; targets de P2/B1 integrados (`4418c2d`, `1baca0e`, `0f0927d`); P3 con S2 (`181ed93`), S3 (`0ccaea3`) y S4 (`fe4d7da`) integrados; B2 de transporte integrado (`41ca5dc`). Las puertas de P1/P2 siguen abiertas, P0 no está superada y no hay GO funcional.** El estado de cada etapa se anota en §8; una etapa solo se da por superada con commit y resultados reales.

El árbol destino, la tabla de rutas, los lotes de movimiento físico y los cortes S1 a S10 están en [REFACTOR_PHYSICAL_PLAN.md](REFACTOR_PHYSICAL_PLAN.md). Es un objetivo, no está realizado. [ARCHITECTURE.md](ARCHITECTURE.md) (§1 y §7) está alineado con él: la reorganización física y los nombres en inglés del código propio son resultados obligatorios.

## 1. Estado de partida

- **P0 no está superada.** La QA manual de gameplay sigue **PENDIENTE**, incluidos el agarre y lanzamiento de un objeto normal desde el cliente y la paridad visual entre host y cliente (ARCHITECTURE.md §2).
- La autorización explícita del usuario del 2026-11-25 dispensó esa puerta para iniciar un refactor provisional y aislado. El usuario autoriza además **continuar todas las etapas arquitectónicas provisionales verificables técnicamente** (B0 a B5, B2b, G1, S1 a S10, P3-SH y P4 a P12), cada una con su puerta (§4), sin esperar la QA manual ni pedir aprobación tras cada corte superado. Nada de esto permite declarar P0 superada, no es un GO funcional y no autoriza publicar en `master`.
- El **GO funcional** sigue pendiente y no se consolida ni se publica en `master` mientras falte.
- La extracción de bibliotecas de P2 está integrada en la rama local `refactor/project-architecture` como `4418c2d08c1186ce896309c9487315a9e3ec4a21`, a partir de `0aa6329` en `itsCarlosDev/cmake-core-libraries-codex` [C]. Detalle, evidencia y límites en §6.4.
- **Las líneas base de P1 siguen incompletas**, en particular las capturas y las referencias de ejecución. La puerta de P2 compara contra ellas.
- P3 se ha iniciado, al amparo de la autorización provisional, con la puerta de P2 abierta y P1 incompleta: el corte S3 (`RemotePhysicsTimeline`) está integrado como `0ccaea3` (§9).
- El corte S4 (mezcla de réplica y transición a dormido) está integrado como `fe4d7da` (§10).
- `origin/Issue#5` (`469e822`) **no se integra**: es redundante. Su icono no añade una capacidad que falte al HUD actual, sus `setInteractuable`/`getInteractuable` no tienen llamadores en el árbol, desactiva `glfwSetErrorCallback(error_callback)` (regresión) y crea una textura GL por fotograma sin liberarla. No se porta nada; la rama se conserva como respaldo ([plan físico](REFACTOR_PHYSICAL_PLAN.md) §2.5).
- Conviene coordinar antes de editar CMake o `main.cpp`.

## 2. Reglas

1. Un solo ejecutable; el target sigue llamándose `EntornVGI` y la salida sigue en `build/<preset>/bin/`.
2. Primero bibliotecas con los ficheros en su sitio; después costuras y pruebas; **mover ficheros al final**. Enmienda (2026-11-25): un módulo que ya tenga target y pruebas se mueve antes, en los lotes B2 a B5 del [plan físico](REFACTOR_PHYSICAL_PLAN.md) §3; al final quedan los remanentes.
3. Un commit (o una serie corta y contigua) por etapa, revertible, sin arreglos funcionales mezclados.
4. El supervisor (Codex) es el único que edita e integra `main.cpp`, `main.h` y los ficheros CMake compartidos. Claude Sonnet hace la revisión de arquitectura y la revisión cruzada; los agentes de funcionalidad trabajan en rutas disjuntas.
5. Cada etapa tiene sus propias puertas (§4 y §5): una puerta en rojo detiene esa etapa y las que dependen de ella, no el trabajo independiente. La QA manual es una puerta de entrega funcional, no de etapa. Lo que no se ejecuta se declara como no ejecutado.
6. Una regresión atribuible al refactor se **revierte** (`git revert` del corte, §3); no se tapa con un arreglo mezclado.

## 3. Puntos de control y vuelta atrás

| Punto | Referencia | Estado |
|---|---|---|
| C0 | Rama `checkpoint/pre-refactor-stabilization-2026-11-25` en `62a7018` | Existe en el repositorio local [C] |
| C1…Cn | Un punto por etapa superada, p. ej. etiqueta `arch/p2` | [P] No creados; el repositorio no tiene etiquetas |

Procedimiento [P]:

- Antes de abrir una etapa, anotar el SHA de partida.
- Para deshacer, usar `git revert` de los commits de la etapa en orden inverso. No usar `reset --hard` ni reescribir ramas compartidas. Para el cambio de P2: `git revert 4418c2d` en `refactor/project-architecture` (SHA de partida: `8dc63b5`). Para el corte S4: `git revert fe4d7da` (SHA de partida: `7ace570`), antes de revertir `0ccaea3`. Para el corte S3: `git revert 0ccaea3` (SHA de partida: `f9cd52b`), antes de revertir `4418c2d`.
- Si ya hay etapas posteriores encima, revertir primero las posteriores.
- Tras revertir, configurar en un directorio de build limpio y repetir la puerta común.
- El editor puede escribir en `assets` (copia al árbol fuente). Revertir código no revierte escenas guardadas: revisar `git status` sobre `assets/` después de cada sesión de QA.
- Volver a C0 es el último recurso y requiere decisión del usuario.

## 4. Puerta común de cada etapa

Ejecutar sobre el commit de la etapa y guardar el resultado con su SHA. Los puntos 1 a 4 bloquean la integración de la etapa a la que apliquen. Los puntos 5 y 6 requieren pantalla o referencias de P1. Para el arranque y lo demás que exige pantalla, qué bloquea lo decide la tabla de niveles del [plan físico](REFACTOR_PHYSICAL_PLAN.md) §4.4: el nivel Escritorio bloquea recursos, render, UI y `app`. En los demás lotes y cortes, incluidos S1 a S10 aunque editen `main.cpp`, el arranque se ejecuta si hay pantalla; si no, se declara no ejecutado, queda pendiente y no bloquea. La columna de validación del plan §3.1 (letra S) indica qué ejecutar, no qué bloquea. Además se mantienen las puertas propias de cada etapa (§5 y §6.3). Una etapa integrada con comprobaciones pendientes no se declara superada.

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
| **P0. Gate** | QA manual de gameplay: agarre y lanzamiento de un objeto normal y de Hirity en host y cliente, con `request → accepted → holding` y registros; paridad visual host/cliente; reanudación de Hirity al soltarlo | QA superada con evidencia. **PENDIENTE**. La autorización del usuario (§1) permite iniciar y continuar el trabajo arquitectónico provisional; registrarla no supera la puerta. P0 bloquea el GO funcional y `master`, no las etapas |
| **P1. Línea base** | Sin cambios de código: avisos de compilación, CTest, capturas 1080p/4K, identidad `ET2-…` de cada escena, bytes de referencia de los paquetes, un JSON guardado por el editor | Referencias archivadas y comparables |
| **P2. Bibliotecas sin mover ficheros** | Primera migración real (§6) | Mismas pruebas registradas y mismo resultado que en P1 |
| **P3. Red de seguridad** | Extraer **sin cambiar** a funciones con parámetros explícitos la autoridad de agarre, la aplicación de réplicas, `identificadorEscenaRed` y la máquina de Hirity; añadir pruebas de caracterización y de enlace de shaders | Pruebas nuevas en verde antes y después de cada extracción |
| **P4. Estado** | `main.h` pasa a `extern` más un `.cpp`, o a una estructura; agrupar estado de jugador, escena, render y aplicación | Debug y Release; arranque y menús sin cambios |
| **P5. Hojas** | Audio, escala de UI, métricas, diagnóstico de agarre, colisionadores, panel F4, panel de red, HUD | Revisión visual |
| **P6. Base VGI** | Trasladar el bloque VGI de `main.cpp` íntegro a unidades propias. El ciclo `material`/`glut_geometry` puede romperse moviendo `SetColor4d`, en commit aparte | Lista de menús y atajos VGI |
| **P7. Render** | Caché de recursos, luces, oclusores, dibujo de escena, FBO PS1, estéreo y VR | Capturas contra P1; 13 luces visibles |
| **P8. Gameplay** | Control del jugador, después interacción y agarre, después Hirity | Pruebas de P3 (incluida la puerta de S10) en verde. La QA offline queda pendiente para la entrega funcional |
| **P9. Red** | Sincronización de escena, réplica cliente, autoridad del host. **Sin tocar el formato en cable** | Bytes e identidades de P1 idénticos. La QA host/cliente queda pendiente para la entrega funcional |
| **P10. Aplicación** | `main()` queda como composición; RAII conservando el orden de limpieza | Cierre limpio; editor con cambios sin guardar |
| **P11. Mover ficheros** | `git mv` por módulo solo cuando su target y sus pruebas existan; actualizar CMake, pruebas, `.vscode` y documentación. Enmienda (decisión del usuario): el movimiento físico es obligatorio y los recursos **sí** se mueven en el árbol fuente, sin cambiar nombres ni la disposición de `bin/`; parte se adelanta como lotes B2 a B5 del [plan físico](REFACTOR_PHYSICAL_PLAN.md) | Build desde cero; revisar mayúsculas y Unicode |
| **P12. Nombres** | Último, por módulo. **Obligatorio** para el código propio (decisión del usuario): código y módulos del equipo, pruebas, herramientas y documentación. Excluye IDs persistentes, claves JSON, protocolo, nombres de escena, rutas de assets, textos de jugador, base académica VGI (`legacy_vgi`, solo la original) y terceros (`third_party/`, sin modificar). En la documentación cambian nombres de fichero y carpeta, no el idioma del cuerpo | Bytes e identidades sin cambios |
| **Aparte** | Reorganizar el contenido de assets, limpiar artefactos versionados, decidir sobre `.slnx`/`.vcxproj` | Decisión y rama propias |

P6 a P9 no empiezan sin las pruebas de los cortes de P3 que les correspondan en verde y sin P4. P4 a P7 esperan a los cortes S1, S2 y S5 a S9. S10 precede a P8 y, por tanto, a P9: es una extracción que preserva el comportamiento, con pruebas de caracterización en verde antes y después y revisión cruzada, y no espera a la QA manual ([plan físico](REFACTOR_PHYSICAL_PLAN.md) §3).

**Propietario en serie de `NetJuegoLogica.h`:** B2 (cambia sus includes) y los cortes S1, S2 y S5 no lo editan a la vez; quien vaya segundo parte del SHA ya integrado.

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
| Mover assets rompe la raíz, las rutas literales y el destino del guardado | Alto | Se mueven solo en B4, con variables de raíz, ficheros sonda y hashes de `bin/` ([plan físico](REFACTOR_PHYSICAL_PLAN.md) §7) |
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
| P0 | Sin commit de superación | **No superada.** No detiene las etapas arquitectónicas provisionales autorizadas (§1) | Agarre y lanzamiento de objeto normal desde el cliente, paridad visual host/cliente, reanudación de Hirity y el resto de la QA de ARCHITECTURE.md §2 |
| P1 | Sin commit | **Incompleta** | Líneas base completas de capturas y ejecución, y el resto de referencias de §5 sin registrar |
| B1 | `1baca0e` + `0f0927d` + `a4b03be` | Claude: no encontró defecto de código; los sondas, objetivos y tests están correctos. Debug/Release oficiales PASS; CTest 21/21; NetJuegoTests 20/20 y ENetLoopbackTests 10/10 por configuración sobre `a4b03be` | G1 condicionado a build limpio Debug/Release. Se difiere la sustitución de rutas literales de tests a un commit antes de B4; no bloquea B2. P1 no tiene un manifiesto formal. Arranque desde `bin/` y capturas P1 pendientes |
| P2 | `4418c2d` (origen `0aa6329`) | Extracción de bibliotecas completada e integrada; Debug y Release compilan y CTest 21/21 en ambas, según el supervisor (§6.4) | Build limpio, arranque desde `bin/`, guardado del editor y comparación con P1 |
| P3 | S1 `52f00c4` (origen `6abf1c7`), S2 `181ed93`, S3 `0ccaea3`, S4 `fe4d7da` | **Parcial.** S1–S4 integrados; S1 necesita validación oficial en el HEAD integrado. S2–S4 conservan la evidencia descrita en §§9–10 y 12 | S5 a S10, pruebas de enlace de shaders y QA de gameplay/gráfica |

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

## 10. P3, corte S4: mezcla de réplica y transición a dormido (2026-11-25)

Commit `fe4d7da4679f24ed7b912d11b92fd0285d0bcc00` en `refactor/project-architecture`; commit de origen `943e6581e97cd39a0fa289f6e7e6d34ea2d6c7c4`. Es un cherry-pick con el mismo patch-id estable (`7b6478ec…`) [C]. Los árboles de ambos commits difieren solo en `docs/REFACTOR_MIGRATION_MAP.md`, por el commit documental `7ace570` situado entre sus padres [C].

**Cambios [C]**

| Fichero | Cambio |
|---|---|
| `EntornVGI/Network/Game/HostPhysicsPublication.h` (nuevo) | `netjuego::publication::ObserveHostSleepTransition`: devuelve verdadero solo en la transición de despierto a dormido |
| `EntornVGI/Network/Game/RemotePhysicsTimeline.h` | `RemotePhysicsPose`, `RemotePhysicsBlendFactor` y `BlendRemotePhysicsPose` |
| `EntornVGI/main.cpp` | Sustituye ambos bloques por llamadas a las cabeceras. Pasa de 10 170 a 10 171 líneas |
| `tests/NetJuegoTests.cpp` | Comprobaciones nuevas: delta 0 (posición y rotación), factor literal 0,834701 con delta 0,1, pose mezclada, primera observación, repeticiones, transición e IDs independientes |

No cambia ningún fichero CMake; CTest sigue en 21 pruebas.

**Equivalencia por lectura [C]:** misma fórmula `1 - exp(-18*delta)`, mismas llamadas `mix` y `slerp` y misma regla de transición. Las guardas del llamador (`flags & 1` y objeto local encontrado) siguen en `main.cpp` y no tienen prueba automática. `ultimoTiempoFisica` sigue sin reiniciarse entre sesiones (comportamiento previo). Las pruebas llegan en el mismo commit, así que tampoco aquí se cumple el «verde antes y después» de P3.

**Evidencia del corte de origen (`943e658`), informada; no reejecutada al redactar este documento**

| Comprobación | Resultado informado | Sobre qué se hizo, según los mensajes de coordinación |
|---|---|---|
| Lanzadores oficiales Debug y Release | PASS | Árbol revisado **antes** de crear el commit: comunicado a las 15:25:06Z; `943e658` es de las 15:29:50Z |
| CTest Debug y Release | 21/21 en ambas | El mismo árbol sin confirmar |
| Revisión estática de Claude | Sin bloqueos (NO BLOCKERS) | Diff sin confirmar sobre `0ccaea3` |
| `NetJuegoTests` repetido 20 veces, Debug y Release | PASS | SHA de ejecución no indicado (comunicado a las 15:31:20Z) |
| `ENetLoopbackTests` repetido 10 veces, Debug y Release | PASS | SHA de ejecución no indicado (comunicado a las 15:31:20Z) |

El coordinador presenta esta evidencia como la del corte de origen. Durante la revisión, `HostPhysicsPublication.h` aún no estaba versionado y se advirtió que debía incluirse; está presente en `fe4d7da` [C].

**Sobre `fe4d7da`:** este registro no contiene una ejecución propia. La evidencia se traslada porque el parche de `943e658` y el de `fe4d7da` son equivalentes y entre ambos árboles solo cambia documentación, no por haberla repetido.

**Pendiente:** QA gráfica y de gameplay (incluida la réplica en cliente), comparación con las líneas base de P1, Linux, macOS y OpenXR. **No es un GO funcional ni autoriza consolidar o publicar en `master`.**

## 11. B1, targets de build y recursos (2026-11-25)

La extracción B1 está integrada en `1baca0e` (targets iniciales, origen `9e76e6c`) y `0f0927d` (target de `NetJuego.cpp`, origen `fe42dc7`). Las pruebas se ejecutaron en el worktree dedicado de B1 con el mismo árbol que el commit de origen; después se aplicó el commit a la rama central como `0f0927d`.

- `EntornVGI_network_game` compila `NetJuego.cpp` una vez y publica dependencias a `EntornVGI_network` y GLM; el ejecutable y `NetJuegoTests` lo enlazan.
- Se mantienen 21 pruebas registradas, sin duplicar `NetJuego.cpp` entre consumidores.
- La revisión estática de Claude al cambio exacto no encontró bloqueos. La revisión G1 de la integración completa `0f0927d` está solicitada y pendiente.
- El lanzador oficial Debug y Release terminó con código 0; ENet, Bullet, OpenXR y meshoptimizer se detectaron habilitados.
- CTest completo pasó 21/21 en ambas configuraciones. `NetJuegoTests` pasó 20/20 y `ENetLoopbackTests` 10/10, ejecutados secuencialmente en Debug y Release.
- La puerta de P2 sigue abierta: no se ha repetido desde un directorio limpio, no se ha arrancado el juego desde `bin/` ni se han capturado las comparaciones P1. No hubo validación en macOS/Linux ni con hardware OpenXR.

## 12. P3, corte S2: estado de red de escena (2026-11-25)

Commit integrado `181ed931506123df5fb944a8ca2dbcbe55a50dd9` (origen `273fbab43d25fc956b35f12e685dd71464767f53`). Crea `EntornVGI/Network/Game/SceneNetworkState.h` y sustituye en `main.cpp` la captura de estados y la búsqueda única de objetos dinámicos por helpers con dependencias explícitas. `NetJuegoTests.cpp` cubre IDs duplicados, objeto Hirity/Rebotador, collider ausente y copia de pose y dimensiones no predeterminadas.

Claude revisó el cambio y no encontró bloqueos; los casos de regresión fueron reforzados antes de integrar. Los lanzadores oficiales Debug y Release y CTest completo pasaron tras los ajustes finales de nomenclatura (21/21 por configuración). Las repeticiones NetJuego 20/20 y ENetLoopback 10/10 se ejecutaron antes de ese último cambio exclusivamente de identificadores; no se han repetido sobre el commit central `181ed93`. No hubo ejecución del juego.

## 13. B2, traslado físico del transporte ENet (2026-11-25)

Commit `41ca5dc` (`refactor: relocate network transport module`) sobre `refactor/project-architecture`, padre `f12ede6`. Mueve con `git mv` los nueve ficheros de transporte y su `CMakeLists.txt` desde `EntornVGI/Network/` a `src/networking/transport/`. `EntornVGI/Network/` conserva `GrabDiagnostics`, `PreferenciasRed` y `Game/`.

`EntornVGI/CMakeLists.txt` añade el subdirectorio con una ruta de binario explícita después de definir las opciones de red. El target sigue llamándose `EntornVGI_network` y publica los include roots `src/` y `EntornVGI/`. Se actualizaron las rutas de include en `NetJuego.cpp`, `NetJuego.h`, `NetJuegoLogica.h`, `NetworkTests.cpp` y `ENetLoopbackTests.cpp`. No se editó `main.cpp`, el protocolo, el comportamiento ni los registros de tests.

**Revisión:** Claude Sonnet revisó el diff exacto en solo lectura y dio PASS sin BLOCKER ni defectos de código. Exigió stagear también las modificaciones de CMake/includes junto a las renombradas; las 16 rutas cambiadas quedaron incluidas en `41ca5dc`.

**Validación del árbol central antes del commit:**

| Comprobación | Debug | Release |
|---|---|---|
| Lanzador oficial `run-windows.cmd -NoRun` | PASS, código 0 | PASS, código 0 |
| CTest completo | 21/21 | 21/21 |
| `NetJuegoTests --repeat until-fail:20` | 20/20 | 20/20 |
| `ENetLoopbackTests --repeat until-fail:10` | 10/10 | 10/10 |

Los lanzadores confirmaron ENet, Bullet, OpenXR y meshoptimizer habilitados. Se observaron avisos ya existentes de `getenv`, `strncpy` y redefinición de `NOMINMAX`; no impidieron la compilación. `git diff --check` pasó. No se ejecutó el juego: el lote solo traslada el transporte. No hay validación Linux/macOS ni OpenXR con hardware. La evidencia corresponde al árbol exacto que se confirmó en `41ca5dc`, no a una ejecución posterior del hash confirmado.

## 14. B3, traslado físico de dependencias vendorizadas (2026-11-25)

Commit integrado `f34616d` (`refactor: relocate vendored dependencies (B3)`), origen `11602bf`, sobre `9e13716`; follow-up documental `a095e9a` corrige el estado de la licencia de Dear ImGui. Los blobs de las dependencias movidas se conservaron idénticos (23 renombres R100): Dear ImGui pasa de `include/ImGui` a `third_party/imgui`, ImGuizmo de `EntornVGI/third_party` a `third_party/imguizmo` y nlohmann/json a `third_party/nlohmann`. Se actualizaron CMake, includes y referencias de Visual Studio. El upstream identifica la licencia MIT, pero el snapshot no trae el `LICENSE.txt` al que remite `imgui.cpp`; no se inventó ni descargó una licencia.

La revisión estática Claude no encontró bloqueos. La rama de implementación informó build oficial y CTest 21/21 en Windows Debug/Release; el HEAD central integrado B3/S1 acaba de validar con ambos lanzadores oficiales y CTest 21/21 por configuración. Linux/macOS no se probaron. El arranque de la rama informó que `assets/textures/fonde-menu.png` contiene datos WebP incompatibles con SOIL2; el archivo coincide con el checkpoint y B3 no lo causó. La validación gráfica sigue pendiente.

## 15. P3, corte S1: fingerprint de escena de red (2026-11-25)

Commit integrado `52f00c4` (`refactor: extract scene network fingerprint from main.cpp`), origen `6abf1c7`. `SceneNetworkFingerprint` vive en `EntornVGI/NetJuegoLogica.h`; `main.cpp` llama al helper y `NetJuegoTests` fija los vectores FNV-1a/ET2, incluyendo la escena calibrada. Se conservan el nombre de escena, IDs ordenados, endianess y formato `ET2-%08X`.

La validación del agente fue una compilación no oficial del target y CTest focalizado Debug 1/1; el resultado central actual sustituye esa evidencia: Debug y Release compilaron con los lanzadores oficiales y CTest completo pasó 21/21 en ambas. La prueba de escena usa `ENTORNVGI_SOURCE_ASSETS_DIR` apuntando a `assets`; debe actualizarse en el lote que mueva assets. No hubo QA de juego ni validación Linux/macOS.

**Métrica verificada con `rg -n '^'` sobre blobs Git en HEAD `52f00c4`:** `main.cpp` tenía 10.132 líneas frente a 10.212 en `62a7018` (−80). Los conteos anteriores 8.870/8.947 provenían de un conteo incorrecto del pipeline de PowerShell.
## 16. B4, traslado físico de código propio a `src/` (2026-11-25)

Commit `852920c` mueve 65 fuentes y cabeceras de editor, escena, física, animación, gameplay, audio, networking/replicación, render, UI y VR bajo módulos reales de `src/`; conserva `main.cpp`, `main.h`, los archivos VGI heredados y los assets en sus ubicaciones actuales. Se actualizaron includes, CMake/CTest y entradas existentes de Visual Studio. `983defa` agrega `$(ProjectDir)` a los includes del `.vcxproj`, requerido para que fuentes ya listadas bajo `src/` encuentren headers heredados en `EntornVGI/`.

**Validación del HEAD con el movimiento:** lanzadores oficiales Debug/Release PASS; CTest 21/21 en ambas configuraciones. No se ejecutaron las repeticiones de red porque el cambio es solo de rutas. Revisión estática Claude: sin BLOCKER; el include de Visual Studio se corrigió en `983defa`. Las guías `CAMBIOS_FPS_JSON.md` y `EDITOR_PAREDES_PUERTAS.md` conservan algunas rutas antiguas y el `.vcxproj` sigue siendo un manifiesto heredado incompleto; queda como mantenimiento pendiente. No hubo compilación independiente con el generador Visual Studio.

Antes de la extracción UI, `main.cpp` tenía 10.132 líneas en `852920c`; tras `1e2912a` tiene 9.304 (−828 líneas netas), contado con `rg -n '^'` sobre blobs Git. El lote UI trasladó 869 líneas antiguas y añadió 41 líneas de integración. Los siguientes cortes y la línea base actual se registran en §§17–18.

## 17. B4, traslado de recursos al árbol raíz (2026-11-25)

Commit integrado `bafc838e4134469b8bd159b77b7f53bba1b772b1` (origen `1edaeb1d47b5b1ea5539e39dc8bacd204720c0d7`). Se movieron físicamente 321 recursos conservando sus blobs: 302 desde `EntornVGI/assets/`, 11 shaders, 7 texturas y `res/ui/hand.png`. El árbol resultante es `assets/{animations,audio,licenses,models,scenes,shaders,textures,ui}`. CMake copia el contenido completo a `build/<preset>/bin/assets/`; se actualizaron detección de raíz, consumidores de shaders/UI, tests, scripts, documentación y directorios de trabajo de los IDE.

El primer CTest encontró rutas antiguas en `GlbTests`; el commit `7c04c59384930fc8da9ff049b34e0f38068eb59a` corrige las rutas de `Puerta.glb` y `Puerta_Lab.glb`. Validación central final: lanzadores oficiales Debug y Release PASS; CTest 22/22 en ambas configuraciones. Los dos builds sincronizaron los 321 recursos a `bin/assets/`. El worktree de origen se bloqueó al descargar CMake 4.4.0 desde vcpkg (`curl` 7); no se reintentó allí. Los builds centrales posteriores sí terminaron. No se validó Linux/macOS ni el arranque visual del juego.

## 18. P7 parcial, callbacks de controles de renderizado (2026-11-25)

El commit `006e520827eb7197e9cc4be32f119809017fc5d0` (origen Claude `651a976c20af6e5d49b3b6f622630d3bdd5cc8e5`) mueve los 29 callbacks desde `OnIluminacioLlumfixe` hasta `OnShaderPBinaryRead` a `src/rendering/RuntimeRenderControls.{h,cpp}`. Los métodos reciben referencias explícitas al estado de iluminación/materiales/texturas/shader y una función para actualizar intensidad; `main.cpp` mantiene los nombres de callback usados por los menús como adaptadores. No se incluye `main.h` en el módulo. La diferencia de `main.cpp` es 558 líneas retiradas y 59 añadidas, neto −499.

`a629c75eaa6d7c12b7555773cefd9db43215aeea` registra el nuevo módulo en `.vcxproj` y `.filters` y limpia whitespace trasladado. Validación posterior a estos cambios: lanzadores oficiales Debug/Release PASS y CTest 22/22 en ambas configuraciones. No se añadieron ni debilitaron pruebas. **Métrica verificada por `rg -n '^'` sobre blobs:** `main.cpp` tiene 8.806 líneas frente a 10.212 en el checkpoint `62a7018` (−1.406, aproximadamente 13,8 %). El módulo contiene 568 líneas de implementación y 72 de cabecera. Son callbacks de renderizado extraídos; el pipeline de dibujo, la composición y el resto de responsabilidades en `main.cpp` siguen pendientes. No hubo validación gráfica, Linux/macOS ni ejecución con hardware OpenXR.

## 19. P7 parcial, geometría, transformaciones y visibilidad VGI (2026-11-25)

El commit central `5cca160f36239f59528d9bf4b6d89b9a2b416b6c` (origen Codex `fe3283142b0ba37fbe2acd2b0807d92180d4c4ff`) mueve los 29 callbacks de objetos, curvas, transformaciones y ocultación desde `OnObjecteCap` hasta `OnOcultacionsZBuffer` a `src/rendering/LegacyVgiGeometryControls.{h,cpp}`. `main.cpp` conserva adaptadores sin argumentos para los menús. El nuevo contexto recibe referencias al estado de VGI y una referencia a la función compartida que lee `.crv`, sin incluir `main.h`. Se reportó equivalencia de los cuerpos de callback respecto a la base, salvo calificación de miembros y whitespace.

La extracción elimina 561 líneas de `main.cpp` y añade 64 de adaptadores/contexto, neto −497. CMake, `.vcxproj` y `.filters` registran el módulo. Los lanzadores oficiales Debug/Release pasaron; CTest pasó 22/22 en ambas configuraciones. No hubo repeticiones de red porque el corte no toca red, y no hubo validación visual o en otra plataforma.

**Métrica actual verificada por `rg -n '^'` sobre el blob de HEAD `5cca160`:** `main.cpp` tiene 8.309 líneas, −1.903 (−18,6 %) respecto a las 10.212 del checkpoint `62a7018`. Los dos módulos de controles de renderizado suman 58 callbacks y están en `src/rendering/`; siguen pendientes el pipeline de dibujo, iluminación runtime fuera de estos menús, cámara, física, networking, composición y más trabajo de nomenclatura.

## 20. P7 parcial, viewport y pipeline de dibujo (2026-11-25)

Commit central `f6496cb9b5ac774194650ed5fb1cb6bc9e852705` (origen Claude `18132631bd3d12046549dd56626381a0c512f34b`) crea `src/rendering/RuntimeRenderer.{h,cpp}` y traslada `OnSize`, `OnFramebufferSize`, `dibujarEscenaEstereo`, `dibujarEscenaVR`, `copiarOjoAVentana` y `OnPaint`. Los callbacks que GLFW conserva en `main.cpp` delegan al contexto; no se trasladó `InitGL`, que sigue inicializando el estado global y el orden de carga GPU. La extracción no cambia protocolo ni estado de juego; conserva FBO PS1, composición nativa y bucle por ojo OpenXR según revisión del diff.

La integración retiró 347 líneas y añadió 60 adaptadores en `main.cpp` (neto −287; conteo medido por contenido del archivo). Los lanzadores oficiales Debug y Release terminaron con código 0; CTest completo pasó 22/22 en cada configuración. ENet y OpenXR se detectaron habilitados. La primera llamada Debug sin elevar falló al crear `build/windows-debug/CMakeFiles/pkgRedirects` en el worktree C:; el mismo lanzador oficial con acceso autorizado completó. No se ejecutó el juego ni se probó hardware OpenXR; InitGL y otras responsabilidades siguen pendientes.

Métrica de `main.cpp` en el commit central: 8.022 líneas frente a 10.212 en `62a7018` (−2.190; −21,4 %). No se repitieron tests de red porque esta extracción no toca su código.

## 21. Traslado del cargador OBJ y adaptador de plataforma (2026-11-25)

Commits `0026d9c` y `2400658` trasladan físicamente `objLoader.cpp/.h` de `EntornVGI/` a `src/scene/` y `platform_compat.h` a `src/platform/`. El segundo commit actualiza CMake, `ObjLoaderTests`, includes heredados, y los manifiestos Visual Studio. No se renombraron APIs ni cambió el comportamiento. Se corrigió además el destino documentado del adaptador y la ruta citada por la herramienta de tiling.

Validación sobre el árbol de ambos commits: lanzadores oficiales Debug/Release PASS; CTest completo 22/22 en ambas configuraciones. La invocación focalizada de `ObjLoaderTests` devolvió SKIP al no inicializar su requisito gráfico; en las dos ejecuciones completas posteriores `ObjLoaderTests` pasó (6,57 s Debug; 2,52 s Release). `git diff --check` pasó. `main.cpp` permanece en 8.022 líneas. La primera compilación Debug detectó que RuntimeUiTests necesitaba el include root de `src/legacy_vgi`; `79e5197` lo añade. Los builds y CTest completos reportados arriba son posteriores a ese arreglo. No hubo ejecución gráfica interactiva ni validación en macOS/Linux.

## 22. P7 parcial, controles de cámara, vista y proyección (2026-11-25)

El commit central `1ea25530175663840320f0dd6ae2b9d523067e6b` extrae 21 callbacks a `src/rendering/LegacyVgiViewControls.{h,cpp}`: cámara esférica/navegación/geode, controles de vista y proyección. `main.cpp` conserva adaptadores y compone un contexto explícito con 30 dependencias; no se incluye `main.h`. `InitGL` permanece en el punto de entrada porque inicializa el estado global y la carga GPU ordenada. La revisión estática de Codex encontró una definición por callback y adaptadores correspondientes; Claude revisó el diff y no encontró bloqueos.

La verificación por conteo del archivo actual da 7.787 líneas frente a 8.022 antes de esta extracción (−235 netas). El seguimiento `192b270ad1a27bc76ce45345a6e891fb824b2205` mueve dos líneas de comentarios desde `main.cpp` al módulo, sin cambios de código ejecutable. Lanzadores oficiales Windows Debug/Release y CTest completo 22/22 por configuración pasaron sobre el árbol de código de `1ea2553`; el commit de seguimiento solo altera comentarios. No hubo prueba gráfica de cámara/viewport ni validación Linux, macOS u OpenXR con hardware.
## 23. P7 parcial, runtime de Hirity y audio ambiental (2026-11-25)

El commit integrado `ba048ed` (origen Claude `7bcc195`) extrae del `main.cpp` la selección/reproducción de sonidos de Hirity, atenuación por distancia, actualización de descubrimiento/seguimiento, suspensión mientras se manipula, reaparición autoritativa y temporizador de audio ambiental a `src/gameplay/hirity/HiritiRuntime.{h,cpp}`. `src/gameplay/hirity/VolumenSonidoHiriti.h` contiene la fórmula pura de volumen. El nuevo `HiritiContext` recibe estado prestado y callbacks explícitos de cámara, reloj y diagnóstico; `main.cpp` mantiene los adaptadores y la propiedad del estado. La API pública nueva usa nombres en inglés; `84904ab` alineó los nombres de parámetros.

La extracción retiró 255 líneas de lógica y añadió 34 líneas de contexto/adaptadores: `main.cpp` pasa de 7.787 a 7.566 líneas (−221 netas). `AnimacionHiritiTests` añade aserciones para volumen completo hasta 2 m, atenuación lineal hasta 20 m y límite mínimo. En el HEAD integrado `84904abf7128d67c3417591b6126102697dd903e`, ambos lanzadores oficiales Windows Debug/Release terminaron con código 0; ENet, Bullet, meshoptimizer y OpenXR se detectaron habilitados. CTest completo pasó 22/22 en Debug y 22/22 en Release. No hubo QA gráfica, de gameplay, Linux, macOS ni OpenXR con hardware.
## 24. P7 parcial, interacción física y lanzamiento (2026-11-25)

El commit central `5d4646c` (origen Claude `0088389`) extrae el flujo de selección e interacción, agarre y mantenimiento de la restricción, ownership host/cliente, puertas, carga y lanzamiento a `src/gameplay/interaction/InteractionRuntime.{h,cpp}`. `ThrowCharge.h` separa la aritmética pura de carga, velocidad y avance; `InteractionContext` recibe explícitamente estado y callbacks, y `main.cpp` conserva adaptadores para los llamadores existentes. Se mantienen los nombres legacy de los tipos de objetivo/acción para no alterar consumidores; las funciones nuevas del módulo usan nombres ingleses. `NetJuegoTests` añadió casos de saturación de carga y límite de velocidad sin reemplazar las pruebas de red.

La extracción retiró 172 líneas y añadió 39 de contexto/adaptadores en `main.cpp`, neto −133: 7.566 → 7.433 líneas. Sobre el commit central `5d4646c`, los lanzadores oficiales Debug/Release terminaron con código 0; ENet, Bullet, meshoptimizer y OpenXR se detectaron activos. CTest pasó 22/22 en ambas configuraciones. Repeticiones secuenciales: `NetJuegoTests` 20/20 en Debug y Release; `ENetLoopbackTests` 10/10 en Debug y Release. No hubo QA de gameplay con ventanas reales, Linux, macOS ni OpenXR con hardware.

## 25. P7 parcial, runtime de sesión y replicación de red (2026-11-25)

El commit central `e796ab0` (origen Claude Sonnet 5.5 `84baafe`) mueve la captura/aplicación de estado de escena, construcción de pose local y ciclo de vida/actualización de sesión desde `main.cpp` a `src/networking/session/NetworkSessionRuntime.{h,cpp}`. `SessionContext` presta los estados existentes mediante referencias y callbacks explícitos; `SessionRuntimeState` concentra estado temporal de la sesión. `SessionRules.h` contiene reglas puras de raycast remoto, anclaje, carga de lanzamiento, tick y flags, cubiertas por nuevos casos de `NetJuegoTests`. Transporte, protocolo, autoridad del host y orden de actualización se conservan; `main.cpp` retiene los adaptadores y el punto de llamada por fotograma.

La extracción retiró 403 líneas y añadió 32 de contexto/adaptadores en `main.cpp`, neto −371: 7.433 → 7.062 líneas. CMake y los manifiestos `.vcxproj`/`.filters` registran las tres fuentes nuevas. En la rama central integrada, los lanzadores oficiales Windows Debug y Release terminaron con código 0 y confirmaron ENet, Bullet, meshoptimizer y OpenXR habilitados. CTest completo pasó 22/22 en ambas configuraciones. Repeticiones secuenciales: `NetJuegoTests` 20/20 Debug y 20/20 Release; `ENetLoopbackTests` 10/10 Debug y 10/10 Release. `git diff --check` pasó. El agente Codex revisó el commit y reportó que no encontró hallazgos. No se ejecutó el juego; el QA multijugador visual/manual, la compilación directa del proyecto Visual Studio heredado, Linux/macOS y OpenXR con hardware permanecen pendientes.

## 26. P7 parcial, runtime de jugador FPS (2026-11-25)

El commit central `e702058` (origen Codex `a3a6e1e`) extrae a `src/gameplay/player/PlayerRuntime.{h,cpp}` el preset de juego, el spawn y creación del cuerpo del jugador, la inclinación FPS, el movimiento WASD, la sincronización de cámara y cápsula Bullet, el movimiento de sway y el balanceo/desplazamiento visual. `PlayerContext` presta las referencias al estado global y callbacks de cámara; la escena física se pasa como referencia al `unique_ptr` reemplazable para no conservar un puntero obsoleto al cambiar de escena. `main.cpp` conserva adaptadores para los llamadores actuales. `PlayerRuntimeTests` cubre la mirada y el sway extraídos.

La diferencia verificada del archivo es 7.062 → 6.914 líneas (−148 netas). CMake, `.vcxproj` y `.filters` registran el módulo y el nuevo test. En el árbol central integrado, los lanzadores oficiales Debug/Release pasaron con ENet, Bullet, meshoptimizer y OpenXR habilitados; CTest completo pasó 23/23 en cada configuración. Revisión independiente Claude Sonnet 5.5 del commit exacto: NO-BLOCKER, sin REQUIRED; las observaciones restantes son opciones de cobertura/documentación. `git diff --check` pasó. No hubo QA manual del juego, MSBuild independiente del proyecto heredado, macOS/Linux ni OpenXR con hardware.

## 27. P7 parcial, runtime de carga y aplicación de escenas (2026-11-25)

El commit central `a72bdc7` (origen Claude Sonnet 5.5 `222347a`) extrae la carga transaccional de escenas, reconstrucción de Bullet y carga de modelos OBJ para el editor a `src/scene/SceneRuntime.{h,cpp}`. `SceneRuntimeContext` recibe referencias prestadas y callbacks; `main.cpp` mantiene adaptadores. Se preservan las etapas temporales OBJ/GLB/Bullet, el reemplazo de la escena solo tras validar, la liberación de texturas/modelos/GLB, la subida GPU con contexto activo, el spawn fallback y recreación del jugador, roots del editor, flags/uniforms y parada de red tras cambiar de escena.

`main.cpp` pasa de 6.914 a 6.760 líneas (−154 netas; 178 retiradas y 24 de adapters). CMake y `.vcxproj`/`.filters` registran el módulo. En el worktree central integrado, los lanzadores oficiales Debug/Release pasaron con ENet, Bullet, meshoptimizer y OpenXR habilitados; CTest completo pasó 23/23 en ambas configuraciones. El primer configure Debug del worktree del agente falló transitoriamente al consultar los puertos de vcpkg; un único reintento del mismo lanzador pasó, y las validaciones centrales posteriores pasaron sin ese fallo. Revisión independiente Codex: NO-BLOCKER y sin cambios requeridos. No hubo QA visual/manual, MSBuild independiente, Linux/macOS ni OpenXR con hardware.
