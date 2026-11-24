# Multijugador LAN (PC y VR)

El juego arranca **OFFLINE** por defecto: sin hilo de red, sin socket y sin inicializar ENet.
La red solo se activa con `--host` / `--connect` o con el boton **Iniciar** del panel "Multijugador".

## Dependencia ENet
* **Windows / macOS**: `enet` esta declarado en `vcpkg.json`; los presets usan vcpkg en modo manifiesto e instala las dependencias que falten. En Windows se enlazan tambien `ws2_32` y `winmm`.
* **Linux**: instala el paquete de desarrollo ENet de tu distribucion (por ejemplo `libenet-dev` en Debian/Ubuntu). CMake busca `enet/enet.h` y `libenet`; no instala paquetes ni invoca `sudo`.
* Los presets de plataforma activan `ENTORNVGI_REQUIRE_ENET=ON` y fallan durante la configuracion si no pueden encontrar ENet.
* Para una compilacion deliberadamente offline, configura `-DENTORNVGI_REQUIRE_ENET=OFF`. La capa de red sigue siendo opcional con `-DENTORNVGI_ENABLE_NETWORK=OFF`; si ENet no esta disponible, CMake lo indica y el panel permanece offline.
* Para una compilacion multijugador manual fuera de los presets, usa `-DENTORNVGI_REQUIRE_ENET=ON`. En Windows/macOS resuelve `enet` mediante vcpkg; en Linux, comprueba que tu gestor de paquetes instalo cabeceras y biblioteca de desarrollo.

## Como jugar (host + cliente)
Ambos deben ejecutar **la misma escena** (si no, el host rechaza: "el host usa otra escena").

1. **Host**: desde la raiz del repo ejecuta `build/windows-debug/bin/EntornVGI.exe --host --port 7777 --name Ana`
   en Windows, o `./build/macos-debug/bin/EntornVGI --host --port 7777 --name Ana` en macOS
   (o panel: Host -> Iniciar).
   Abre el **puerto UDP 7777** en el firewall (Windows: permitir EntornVGI en red privada).
2. Averigua la IP LAN del host (`ipconfig` / `ifconfig`, p. ej. `192.168.1.20`).
3. **Cliente**: `build/windows-debug/bin/EntornVGI.exe --connect 192.168.1.20:7777 --name Luis`
   en Windows, o `./build/macos-debug/bin/EntornVGI --connect 192.168.1.20:7777 --name Luis` en macOS
   (o panel: Cliente, IP, puerto -> Iniciar).
4. Los jugadores remotos se ven como avatares (cabeza + manos en VR); el cubo amarillo se coge/suelta con
   **E / gatillo**. Las puertas las decide el host (el cliente envia la solicitud).
Opciones: `--host`, `--connect HOST[:PUERTO]`, `--port N` (1-65535), `--name N`.

El panel y los argumentos `--connect` guardan el ultimo destino en la carpeta de configuracion del usuario (`%APPDATA%/EscapeTheUAB`, `~/.config/escapetheuab` o `~/Library/Application Support/EscapeTheUAB`). El puerto por defecto es UDP 7777; el campo Puerto y `--port` permiten cambiarlo.

## Jugar entre equipos con Tailscale

ENet usa IPv4 y el host se enlaza a `0.0.0.0:<puerto>` (todas las interfaces IPv4 locales), incluida la interfaz Tailscale. El cliente acepta la IPv4 `100.x.y.z` de Tailscale igual que una IP LAN. No se añade el SDK de Tailscale y Tailscale no es necesario para jugar por LAN.

1. Instala Tailscale en ambos equipos, inicia sesión en la **misma tailnet** y completa la aprobación del dispositivo si el administrador la exige.
2. Comprueba que la política de acceso de la tailnet permite UDP desde el equipo cliente al puerto del host (7777 por defecto).
3. En el host inicia el juego y elige Host, o ejecuta `EntornVGI --host --port 7777`. Obtén su IPv4 de Tailscale con `tailscale ip -4`.
4. Permite en el firewall local del host el tráfico entrante UDP al puerto de juego. No hace falta abrir ni redirigir el puerto en el router.
5. En el otro equipo elige Cliente y escribe esa IP y puerto, o ejecuta `EntornVGI --connect 100.x.y.z:7777`. El último destino usado se recordará para conexiones posteriores.

La guía con instrucciones y comprobaciones por plataforma está en [`VALIDACION_TAILSCALE.md`](VALIDACION_TAILSCALE.md). Una prueba loopback confirma ENet local; solo una sesión entre dos equipos confirma Tailscale real.

## Errores que veras (panel y consola)
| Mensaje | Causa |
|---|---|
| ENet no esta incluido en esta compilacion | compilacion offline configurada con `ENTORNVGI_REQUIRE_ENET=OFF`; instala ENet y reconfigura para tener transporte real |
| El host rechazo la conexion: el host usa otra escena | escenas distintas |
| Sin respuesta de `IP:puerto` | host apagado, tailnet o política de acceso, IP/puerto o firewall UDP local; comprueba ambos equipos |
| Puerto fuera de rango | usa un valor entre 1 y 65535; UDP 7777 es el predeterminado |
| No se pudo iniciar la conexion a `IP:puerto` | revisa formato de dirección, puerto y disponibilidad de ENet |
| No se pudo escuchar UDP en todas las interfaces | puerto ocupado o no se pudo abrir el socket del host |
| Argumentos de red ignorados | linea de comandos invalida: se queda offline |
| la escena ha cambiado; se detiene la sesion | cargar otra escena con la red activa |

## Pruebas
`ctest --test-dir build/<preset> --output-on-failure` incluye `NetworkTests` (protocolo/sesion, transporte en memoria), `NetJuegoTests` (adaptador, poses PC/OpenXR,
puertas, propiedad) y `ENetLoopbackTests` (ENet real por 127.0.0.1). Con los presets normales, la ausencia de ENet detiene CMake en vez de omitir silenciosamente esta prueba.

El estado actual de la replicación de edición y sus límites está en [`MULTIPLAYER_SYNC.md`](MULTIPLAYER_SYNC.md).

## Limitaciones
* El juego no cifra ni autentica por sí mismo; úsalo en una LAN o tailnet cuyos miembros y permisos sean de confianza. Máximo 4 clientes. Solo IPv4 (ENet 1.3.x).
* Sin NAT traversal ni descubrimiento automatico: hay que teclear la IP.
* VR en red no se valida en esta guía; las pruebas de transporte no sustituyen a una prueba con visor OpenXR.
* `EntornVGI.vcxproj`/`.filters` **no** se actualizan: ya estan desfasados (no listan `AudioManager.cpp`, no enlazan
  Bullet ni vcpkg; usan libs sueltas como `glfw3.lib`), asi que no compilan el juego actual. La ruta soportada es
  CMake (README); las fuentes nuevas (`NetJuego.cpp`, `NetAvatar.cpp`, `Network/*`) estan solo en CMake.
