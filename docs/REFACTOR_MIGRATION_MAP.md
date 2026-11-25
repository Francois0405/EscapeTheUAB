# Mapa de migración del refactor

Plan **propuesto** sobre la base `62a7018571559df2490bf2a879734bed647bfe10`, fechado el 2026-11-25. El contexto, los contratos, los no objetivos y las marcas de evidencia ([C], [H], [D], [P]) están en [ARCHITECTURE.md](ARCHITECTURE.md).

**Este documento no registra ninguna etapa como ejecutada.** Cada etapa se anota en §8 cuando tenga commit y resultados reales.

## 1. Estado de partida

- El inicio de un refactor **provisional** está autorizado por el usuario.
- La QA manual sigue **PENDIENTE** y el GO funcional también (ARCHITECTURE.md §2).
- No se consolida en `master` mientras no haya GO funcional.
- A la fecha, las ramas locales `refactor/project-architecture` e `itsCarlosDev/cmake-core-libraries-codex` apuntan a la misma base, sin commits propios [C]. Conviene coordinar antes de editar CMake o `main.cpp`.

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
- Para deshacer, usar `git revert` de los commits de la etapa en orden inverso. No usar `reset --hard` ni reescribir ramas compartidas.
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
| **P0. Gate** | Registrar la autorización provisional. La QA manual (agarre y lanzamiento de objeto normal y de Hirity en host y cliente, con `request → accepted → holding` y registros) queda abierta y bloquea el GO funcional | Autorización registrada en ARCHITECTURE.md §2 |
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

Los nombres son orientativos. Conviene un commit por biblioteca, empezando por las de menos dependencias.

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

- **Las definiciones no siguen a las fuentes.** Hoy son `PRIVATE` de `EntornVGI` [C]: `GLM_ENABLE_EXPERIMENTAL`, `NOMINMAX`, `ENTORNVGI_HAS_MESHOPTIMIZER`, `ENTORNVGI_USE_NFD_EXTENDED`/`..._LEGACY_NFD`, `ENTORNVGI_RUNTIME_ASSETS_DIR`, `ENTORNVGI_SOURCE_ASSETS_DIR` y los avisos (`/W3`, `-Wall -Wextra`). Cada biblioteca debe declarar las que necesite.
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
| (sin etapas registradas) | | | |
