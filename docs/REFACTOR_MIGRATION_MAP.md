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
- El editor puede escribir en `EntornVGI/assets` (copia al árbol fuente). Revertir código no revierte escenas guardadas: revisar `git status` sobre `assets/` después de cada sesión de QA.
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
| P3 | S2 `181ed93` (origen `273fbab`), S3 `0ccaea3` (origen `7c8b174`), S4 `fe4d7da` (origen `943e658`) | **Parcial.** S2, S3 y S4 integrados. S2: Debug/Release y CTest 21/21 por configuración tras la revisión y ajuste de pruebas; repeticiones de red informadas antes del renombrado final de identificadores. S3/S4 tienen evidencia en §§9–10 | S1 y S5 a S10, pruebas de enlace de shaders y QA de gameplay/gráfica |

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
