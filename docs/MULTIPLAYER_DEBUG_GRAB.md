# Multiplayer grab diagnostics

The existing `ENTORNVGI_DEBUG_GRAB=1` diagnostics continue to print to the console. To also persist those same records locally, set `ENTORNVGI_DEBUG_GRAB_PERSIST=1` in the process environment. Persistence remains disabled unless both variables are enabled.

On Windows, files are written under `%LOCALAPPDATA%\EscapeTheUAB\logs`; on macOS they use `~/Library/Logs/EscapeTheUAB`; on Linux they use `$XDG_STATE_HOME/EscapeTheUAB/logs`, falling back to `~/.local/state/EscapeTheUAB/logs`. File names are `grab-host-<pid>.log`, `grab-client-<pid>.log`, or `grab-offline-<pid>.log`, keeping simultaneous host and client processes separate.

Each process truncates its own role/PID file when it starts and writes at most 1 MiB during that run. The log stops accepting records at the limit. Records remain event or sampled-state lines, not per-frame traces: request input, accepted and rejected requests, local grab, release, host holding, client replica/interpolation, and sampled Hiriti owner/body/held/tracking/position/velocity state. The existing holding, replica, and Hiriti samples are throttled to one per object/state each 0.5 seconds.

These logs are local diagnostics. They do not alter network messages, ownership decisions, or simulation authority.

## F4 dynamic scene view

While the F4 debug panels are open in FPS mode, `Objetos dinamicos | F4` shows up to eight scene objects with dynamic physics. Rows are selected deterministically: grabbed/owned objects first, then active bodies, then persistent scene ID in lexical order. Each row shows the persistent ID and its network hash, current host/client/offline role, owner peer, and whether the local body is dynamic or a client kinematic replica. For Hiriti, host/offline shows local follow eligibility; a client says the host-authoritative follow state is unknown because that state is not included in the physical snapshots. `SUJETO` marks an active grab/owner.

The panel compares the latest host-authoritative transform with the locally applied transform. On host/offline they are the current physics pose; on clients, the authoritative pose is the latest received host snapshot and the applied pose is the local replica body. Until a client snapshot arrives, the authoritative field says it is waiting. Poses show position and quaternion. The panel refreshes every 500 ms while visible and captures Bullet dynamic state once per refresh; it performs no per-frame logging and does not change protocol, ownership, or physics.
