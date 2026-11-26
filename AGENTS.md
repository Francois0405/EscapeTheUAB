# EscapeTheUAB — instrucciones comunes para agentes

Juego académico 3D (asignatura VGI, UAB): C++20, CMake, OpenGL, GLFW, Dear ImGui, Bullet, ENet y OpenXR
opcional. Entrega: 18-12-2026.

## Antes de tocar nada

1. `git status -sb` y rama. No descartes cambios ajenos.
2. Lee [docs/architecture/CODE_MAP.md](docs/architecture/CODE_MAP.md): módulos, bucle, estado y pipeline.
3. Estado del trabajo visual: [docs/midnight-hoops/TASKS.md](docs/midnight-hoops/TASKS.md) y
   [VALIDATION.md](docs/midnight-hoops/VALIDATION.md). Paneles: [docs/architecture/UI_PANELS.md](docs/architecture/UI_PANELS.md).

## Compilar y probar

- `.\run-windows.cmd -Configuration Debug -NoRun` (o `Release`). El `.cmd` espera una tecla si falla; en sesiones
  no interactivas usa `pwsh -NoProfile -File .\run-windows.ps1 -Configuration Debug -NoRun`.
- `ctest --test-dir build/windows-debug --output-on-failure` (el `ctest.exe` del CMake de Visual Studio).
- Tras añadir o quitar fuentes: `src/CMakeLists.txt` y `python tools/generate_vs_project.py`.
- Evidencia visual: `ENTORNVGI_RETRO_CAPTURE=<carpeta>` recorre fases, guarda capturas y métricas y cierra.
- Un arranque normal reescribe `retro_settings.ini` al salir. No borres ni sobrescribas el de otra persona.

## Reglas de arquitectura

- `src/ui`, `src/rendering`, `src/gameplay` y demás módulos no incluyen cabeceras de estado de `src/app`; el
  estado se presta por contextos construidos en `src/app/adapters/` o `src/app/ui/UiFrame.cpp`.
- Los cambios visuales son locales al cliente: no tocan Bullet, el protocolo ENet ni la autoridad del host.
- Editor, estéreo F2 y OpenXR no usan los efectos retro; no añadirlos sin validarlos.
- Contratos que no cambian sin acuerdo: claves JSON, `id` de objetos, nombres de recursos, mensajes de red,
  nombres de uniforms y las 17 luces.
- Aspecto oficial: preset `PSX Classic` en `src/rendering/RetroSettings.h`. `Original` y `Retro Cinematic` son
  alternativas de depuración. Cambiar valores oficiales requiere aprobación del equipo.
- No se incorporan recursos de otros videojuegos.

## Disciplina

- No hacer commits, push, merges ni borrar ramas o recursos sin autorización expresa.
- Distingue implementado, compilado, probado automáticamente y validado a mano. No declares validado lo que no
  se ha ejecutado.
- Los subtítulos solo pasan a `verified: true` después de que una persona escuche el audio.
- Sin rutas personales absolutas en el código ni en la documentación.
