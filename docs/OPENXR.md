# OpenXR (VR real) en EscapeTheUAB

> **Estado honesto:** el ciclo completo de OpenXR (Etapas A, B y C) está implementado y **compila en Windows**; la matemática de la
> vista tiene tests sin hardware. **El renderizado VR NO se ha probado con un visor**: está pendiente de la prueba con las Quest 3.
> Solo imagen y seguimiento de cabeza (sin manos, mandos ni teleportación).

## Lo que hay y lo que no

| Etapa | Qué | Estado |
|---|---|---|
| A | Instancia OpenXR, runtime, visor, extensión OpenGL, versión de OpenGL | Hecha. Rutas de fallo probadas sin visor |
| B | Sesión enlazada al contexto OpenGL (WGL) y eventos de estado | Hecha (solo Windows). **Sin probar con visor** |
| C | Espacio STAGE/LOCAL, vistas, swapchains + framebuffers, `READY/FOCUSED/STOPPING/pérdida`, `xrBegin/EndSession`, bucle `xrWaitFrame → xrBeginFrame → xrLocateViews → xrEndFrame`, capa de proyección | Hecha (solo Windows). **Compila; sin probar con visor** |
| – | Conversión OpenXR → motor (Z arriba), proyección asimétrica por ojo (`VistaVR.h`) | Hecha y con tests (`VistaVRTests`) |
| – | Manos, mandos, agarre con mandos, teleportación | No implementado (a propósito) |

**F2 no es VR:** es la vista estereoscópica de prueba en pantalla (`VistaEstereo.h`). OpenXR se activa aparte.

## Cómo se activa (siempre explícito)

* `F3` en modo FPS, o el botón **Activar OpenXR** del panel F1 → *Fisica (Bullet)*.
* Nunca al arrancar. Sin runtime o visor el juego sigue en pantalla y el panel explica el motivo
  (`Sin runtime OpenXR`, `Sin visor`, `Falta XR_KHR_opengl_enable`, `OpenGL insuficiente`…).
* `ElegirModoDeJuego(vrPedido, estado)` solo devuelve VR si hay sesión creada.

## Dependencias y CMake

* **Windows:** `vcpkg.json` instala `openxr-loader` (1.1.63 en el baseline actual). CMake lo encuentra con
  `find_package(OpenXR CONFIG)` y **enlaza `OpenXR::openxr_loader`** (el cargador, no solo las cabeceras). La DLL se copia junto al EXE.
* **Linux:** `sudo apt install libopenxr-dev`. Si falta, se compila sin OpenXR con un aviso (no es un error).
* **macOS:** desactivado por defecto (no hay runtime OpenXR real). Para probar solo la inicialización:
  `cmake --preset macos-debug -B build/macos-openxr -DENTORNVGI_ENABLE_OPENXR=ON -DVCPKG_MANIFEST_FEATURES=openxr`
* Opción `ENTORNVGI_ENABLE_OPENXR` (ON en Windows/Linux, OFF en macOS). Define `ENTORNVGI_HAS_OPENXR` para el código.
* **Visual Studio (`.vcxproj`):** los archivos están añadidos, pero **sin** `ENTORNVGI_HAS_OPENXR`, así que compila sin OpenXR
  (seguro). Para activarlo: `vcpkg install openxr-loader:x64-windows`, `vcpkg integrate install` y añadir
  `ENTORNVGI_HAS_OPENXR=1` a las definiciones del preprocesador. Sin verificar.

## Decisión sobre `third_party/OpenXR-SDK-main`

El commit `89c6f33` (rama VR) añadía una copia completa del SDK oficial (≈430 archivos: documentación, CI, tests, herramientas)
y su CMake apuntaba a `third_party/OpenXR-SDK/include`, que no existe; además **nunca enlazaba el cargador**, así que
`xrCreateInstance` no habría enlazado. Se sustituye por el paquete `openxr-loader` de vcpkg (licencias: Apache-2.0 / MIT / BSL-1.0,
las del SDK). La copia sigue intacta en la rama `VR` por si se necesita; no se ha borrado nada.

## Problemas del código original corregidos

| Original | Ahora |
|---|---|
| Win32/WGL sin condiciones (no compila en macOS/Linux) | Todo condicionado; Windows solo en un bloque pequeño |
| Cabeceras en ruta inexistente; sin enlazar el cargador | Paquete gestionado y `OpenXR::openxr_loader` enlazado |
| Se iniciaba en cada arranque, antes de crear el contexto OpenGL | Solo bajo petición (F3/botón) con la ventana ya creada |
| Resultados de `xr*` sin comprobar; `strcpy`; se suponía la extensión | Todo comprobado, `strncpy`, se verifica `XR_KHR_opengl_enable` |
| Sin límite de versión de OpenGL | Se compara con `minApiVersionSupported` |
| `GetDC` sin `ReleaseDC`; variables globales | `wglGetCurrentDC`; clase con destructor y limpieza ante fallos parciales |

## Tests

`OpenXRTests` (sin hardware): selección de modo, no activa nada al crearse, ausencia de runtime con el cargador real,
activación explícita, reintento y liberación tras fallos. **Requieren casco y no están automatizadas:** crear la sesión, eventos
reales, versión de OpenGL aceptada, bucle de fotogramas.

## Cómo funciona la Etapa C

* **Sin tamaños fijos:** el tamaño de cada ojo sale de `xrEnumerateViewConfigurationViews` (recomendado), el formato de
  `xrEnumerateSwapchainFormats` (prefiere `GL_SRGB8_ALPHA8`, luego `RGBA8`…), el campo de visión y la separación entre ojos de
  `xrLocateViews` (cada ojo trae su pose y su FOV, posiblemente asimétrico) y el ritmo de `xrWaitFrame`.
* **Espacio:** `STAGE` si el runtime lo ofrece (origen en el suelo), si no `LOCAL` (origen en la cabeza).
* **Coordenadas:** OpenXR es X derecha, Y arriba, −Z delante; el motor usa **Z arriba**. `VistaVR.h` ancla el origen de OpenXR al
  jugador (los **pies** con STAGE, los **ojos** con LOCAL) y lo gira con la orientación FPS (ratón), así *delante de OpenXR = delante de la
  cámara FPS*. La cabeza real mueve la **cámara**; la **cápsula de Bullet no se toca**. Unidades: 1 unidad del motor = 1 m.
* **Dibujo:** `dibujarEscenaVR()` (main.cpp) usa el mismo `configura_Escena()/dibuixa_Escena()` que la pantalla (OBJ, GLB, puertas,
  Bullet, colisionadores). Planos 0,05 m – 500 m en VR. El ojo izquierdo se copia a la ventana como espejo.
* **Sin VR o sin visor:** el juego sigue en pantalla. Si el visor no está mostrando imagen (`shouldRender = false`) se dibuja el
  escritorio normal. La sincronía vertical del monitor se apaga mientras la sesión VR está en marcha.
* **OpenGL:** la ventana pide ahora un contexto **4.6 core** y, si no existe, vuelve a 3.3 (en macOS sigue 4.1). La depuración OpenGL sigue activa.
* **F2** sigue siendo la vista estereoscópica de prueba en pantalla; **F3** activa/desactiva OpenXR.

## Tests

* `OpenXRTests` (sin hardware): selección de modo, no activa nada al crearse, ausencia de runtime, activación explícita, limpieza tras fallos.
* `VistaVRTests` (sin hardware): ejes OpenXR→motor, anclaje pies/ojos, escala, giro de cabeza, matriz de vista, proyección simétrica ==
  `glm::perspective` y bordes de una proyección asimétrica.
* **Necesitan casco:** sesión real, versión de OpenGL aceptada, swapchains, bucle de fotogramas, escala y confort.

## Prueba con Meta Quest 3 (Meta Horizon Link, Windows 11)

1. Instala **Meta Horizon Link** (antes Quest Link) en el PC, enciende las Quest 3 y conéctalas por USB-C (o Air Link). En el casco acepta
   **Activar Link**. Define el límite/suelo (Guardian) para que exista el espacio STAGE.
2. En la app Meta Horizon Link → **Ajustes → General** marca **Meta Horizon Link como runtime activo de OpenXR**.
3. En PowerShell, en la raíz del repositorio: `.\run-windows.cmd` (compila con vcpkg → `openxr-loader`; en la configuración debe salir `OpenXR: ON`).
4. Con el juego en pantalla y la ventana con el foco, pulsa **F3** (o el botón del panel *Fisica (Bullet)*, F1).
   En consola se verá `[OpenXR] Sesion creada: ...` y un **Diagnostico** (también en el panel, desplegable *Diagnostico OpenXR*).
5. Ponte el casco: la sesión pasa a `READY` → `SYNCHRONIZED` → `VISIBLE` → `FOCUSED` y la habitación aparece dentro del visor.
   Camina con WASD y gira con el ratón como siempre. **F3** otra vez cierra la sesión VR sin cerrar el juego.

### Qué comprobar
1. ¿Aparece EscapeTheUAB dentro del visor? 2. ¿Cada ojo ve la habitación bien (sin doble imagen ni ojos invertidos)?
3. ¿Sigue la cabeza (girar, agacharse, inclinarse)? 4. ¿La escala y las distancias son reales (puertas ≈ 2 m, techo, altura de los ojos)?
5. ¿El modo escritorio sigue igual con VR desactivada?

### Diagnóstico (consola y panel)
Estado de la sesión (`NombreEstadoSesion`), runtime y versión, nombre del visor y seguimiento, GPU y versión de OpenGL frente al mínimo
del runtime, extensiones, espacios disponibles/elegido, formatos de swapchain (elegido), y por ojo: tamaño, máximo, nº de imágenes y muestras.

| Síntoma | Causa probable |
|---|---|
| `Sin runtime OpenXR` | Horizon Link cerrado o no marcado como runtime activo de OpenXR |
| `Sin visor` | Casco apagado o Link no activado |
| `OpenGL insuficiente` | El controlador no dio 4.x; la ventana volvió a 3.3 (ver `Diagnostico`) |
| Sesión creada pero pantalla negra en el casco | Aún no está en `READY`/`FOCUSED` (ponte el casco); mirar el estado en el panel |
| `Framebuffer del ojo incompleto` | Formato de swapchain no renderizable; enviar el `Diagnostico` |
| Imagen muy oscura/clara | Formato elegido distinto de sRGB (ver *Formato elegido* en el diagnóstico) |
