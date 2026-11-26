# Codex

Las instrucciones del proyecto están en [AGENTS.md](AGENTS.md). Aquí solo lo específico de ejecutar Codex en
Windows:

- Lanza PowerShell con `-NoProfile` para evitar ruido y errores de escritura del perfil del usuario.
- Usa `run-windows.ps1`, no `run-windows.cmd`: el `.cmd` se queda esperando una tecla tras un error.
- Si el sandbox impide escribir en la caché de vcpkg o descargar herramientas, informa del error exacto. No
  cambies permisos ni protecciones.
- Los ficheros de `docs/` y `assets/subtitles/` están en UTF-8; léelos con esa codificación antes de reportar
  caracteres corruptos.
- Las auditorías son de solo lectura.
