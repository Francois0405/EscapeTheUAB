# Multijugador LAN (PC y VR)

El juego arranca **OFFLINE** por defecto: sin hilo de red, sin socket y sin inicializar ENet.
La red solo se activa con `--host` / `--connect` o con el boton **Iniciar** del panel "Multijugador".

## Dependencia ENet
* **Windows / macOS (vcpkg)**: `enet` esta en `vcpkg.json` (exporta `unofficial-enet`, version 1.3.18 en el baseline
  `e456309…`). `cmake --preset ...` lo instala solo (manifest mode).
* **Linux**: `sudo apt install libenet-dev` (CMake busca `enet/enet.h` y `libenet`).
* Sin ENet el juego compila igual; el panel muestra "ENet no incluido en esta compilacion" y se queda offline.
* `-DENTORNVGI_ENABLE_NETWORK=OFF` desactiva la busqueda de ENet.

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

## Errores que veras (panel y consola)
| Mensaje | Causa |
|---|---|
| ENet no esta incluido en esta compilacion | build sin ENet (instalar `enet`/`libenet-dev` y reconfigurar) |
| El host rechazo la conexion: el host usa otra escena | escenas distintas |
| Conexion con el host perdida | host apagado, IP/puerto/firewall incorrectos o timeout (~5 s) |
| Puerto fuera de rango / argumentos de red ignorados | linea de comandos invalida: se queda offline |
| la escena ha cambiado; se detiene la sesion | cargar otra escena con la red activa |

## Pruebas
`ctest` incluye `NetworkTests` (protocolo/sesion, transporte en memoria), `NetJuegoTests` (adaptador, poses PC/OpenXR,
puertas, propiedad) y `ENetLoopbackTests` (ENet real por 127.0.0.1; solo si hay ENet).

## Limitaciones
* Sin cifrado ni autenticacion: solo para LAN de confianza. Maximo 4 clientes. Solo IPv4 (ENet 1.3.x).
* Sin NAT traversal ni descubrimiento automatico: hay que teclear la IP.
* VR en red solo probado con transporte en memoria/tests; no hay visor OpenXR en la maquina de integracion.
* `EntornVGI.vcxproj`/`.filters` **no** se actualizan: ya estan desfasados (no listan `AudioManager.cpp`, no enlazan
  Bullet ni vcpkg; usan libs sueltas como `glfw3.lib`), asi que no compilan el juego actual. La ruta soportada es
  CMake (README); las fuentes nuevas (`NetJuego.cpp`, `NetAvatar.cpp`, `Network/*`) estan solo en CMake.
