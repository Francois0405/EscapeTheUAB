# OpenXR (VR real) en EscapeTheUAB

> **Estado honesto:** hay una base OpenXR compilable (Etapas A y B). **Nada de esto se ha probado con un visor ni en
> Windows.** No se afirma compatibilidad con Meta Quest 2.

## Lo que hay y lo que no

| Etapa | Qué | Estado |
|---|---|---|
| A | Instancia OpenXR, runtime, visor, extensión OpenGL, versión de OpenGL | Hecha. **Probada en macOS sin runtime** (rutas de fallo). Con visor: sin probar |
| B | Sesión enlazada al contexto OpenGL (WGL) y eventos de estado | Escrita **solo para Windows**; **no compilada ni probada** |
| C | Espacios de referencia, swapchains, `xrWaitFrame/BeginFrame/EndFrame`, proyección por ojo, dibujo en el visor | **No implementada** |
| – | Manos, mandos, agarre con mandos, locomoción VR | No implementado |

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

## Probar un Meta Quest 2 con Quest Link (Windows) cuando haya visor

1. Instala la app Meta Quest Link, conecta el casco por USB-C (o Air Link) y activa Quest Link en el casco.
2. En Meta Quest Link → Ajustes → General, marca **Meta Quest Link como runtime activo de OpenXR**.
3. `.\run-windows.cmd` (compila con vcpkg → `openxr-loader`). Debe aparecer `OpenXR: ON` en la configuración de CMake.
4. Entra en el juego, pulsa **F3** (o el botón del panel). Esperado en consola: `[OpenXR] Sesion creada: ...`.
   Si no, el mensaje indica la causa. Después `F3` otra vez para cerrar.
5. Solo comprobarás creación de sesión y eventos: **aún no se dibuja nada en el visor** (falta la Etapa C).

## Próximo paso: Etapa C

`xrCreateReferenceSpace` (LOCAL/STAGE), `xrEnumerateViewConfigurationViews`, un swapchain OpenGL por ojo, `xrBeginSession` en `READY`,
y por fotograma `xrWaitFrame → xrBeginFrame → xrLocateViews → dibujar con la pose y el FOV de cada ojo → xrEndFrame`.
La pose del visor irá aparte de la cápsula del jugador (`MundoFisico`); Bullet no moverá la cabeza.
