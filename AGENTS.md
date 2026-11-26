# EscapeTheUAB — instrucciones comunes para agentes

- C++20, CMake, OpenGL, GLFW, Dear ImGui, Bullet, ENet y OpenXR opcional.
- Compilar en Windows: `.\run-windows.cmd -Configuration Debug -NoRun` (o `Release`). El `.cmd` espera una tecla si falla;
  en sesiones no interactivas usa `pwsh -NoProfile -File .\run-windows.ps1 -Configuration Debug -NoRun`.
- Pruebas: `ctest --test-dir build/windows-debug --output-on-failure` (el `ctest.exe` que acompaña al CMake de Visual Studio).
- `src/ui` no incluye cabeceras de estado de `src/app`; el estado se presta mediante los contextos de `RuntimeUi.h`.
- Los cambios visuales son locales al cliente: no tocan Bullet, el protocolo ENet ni la autoridad multijugador.
- El editor y OpenXR no usan el framebuffer retro; no añadir efectos a esas rutas sin validarlos.
- No hacer commits, push, merges ni borrar ramas o recursos sin autorización expresa.

Estado del trabajo visual: `docs/midnight-hoops/TASKS.md` y `docs/midnight-hoops/VALIDATION.md`.
