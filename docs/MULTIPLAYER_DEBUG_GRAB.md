# Multiplayer grab diagnostics

The existing `ENTORNVGI_DEBUG_GRAB=1` diagnostics continue to print to the console. To also persist those same records locally, set `ENTORNVGI_DEBUG_GRAB_PERSIST=1` in the process environment. Persistence remains disabled unless both variables are enabled.

On Windows, files are written under `%LOCALAPPDATA%\EscapeTheUAB\logs`; on macOS they use `~/Library/Logs/EscapeTheUAB`; on Linux they use `$XDG_STATE_HOME/EscapeTheUAB/logs`, falling back to `~/.local/state/EscapeTheUAB/logs`. File names are `grab-host-<pid>.log`, `grab-client-<pid>.log`, or `grab-offline-<pid>.log`, keeping simultaneous host and client processes separate.

Each process truncates its own role/PID file when it starts and writes at most 1 MiB during that run. The log stops accepting records at the limit. Records remain event or sampled-state lines, not per-frame traces: request input, accepted and rejected requests, local grab, release, host holding, client replica/interpolation, and sampled Hiriti owner/body/held/tracking/position/velocity state. The existing holding, replica, and Hiriti samples are throttled to one per object/state each 0.5 seconds.

These logs are local diagnostics. They do not alter network messages, ownership decisions, or simulation authority.
