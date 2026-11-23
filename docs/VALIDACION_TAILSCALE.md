# Guía de validación Tailscale para EscapeTheUAB

Esta guía describe cómo validar una sesión ENet entre dos ordenadores de la misma tailnet. `ENetLoopbackTests` solo verifica tráfico local y no demuestra conectividad entre dispositivos.

## Preparar la tailnet

1. Instala el cliente apropiado desde [tailscale.com/download](https://tailscale.com/download) en los dos equipos: Windows, macOS o la distribución Linux correspondiente.
2. Abre Tailscale e inicia sesión en la misma tailnet en ambos equipos.
3. Si está activada la aprobación de dispositivos, pide a una persona administradora que apruebe ambos desde la consola de administración.
4. Verifica que ambos aparecen conectados y que la política de acceso permite tráfico UDP del cliente al host, en el puerto de juego. Por defecto es UDP 7777.
5. Tailscale suele atravesar NAT sin cambios en el router; el firewall local del sistema aún puede bloquear el socket del juego. No publiques ni redirijas UDP 7777 en el router para esta prueba.

Tailscale asigna IPv4 de `100.64.0.0/10` de forma predeterminada, aunque una tailnet puede usar un pool personalizado. Usa la IP que reporte Tailscale en ese equipo, no inventes una dirección del rango. Las IPv4 de Tailscale permanecen asociadas al dispositivo mientras cambia de red.

## Comprobar cada equipo

En cada ordenador confirma que Tailscale está conectado y obtén la dirección:

```text
tailscale status
tailscale ip -4
```

Desde el cliente comprueba la conectividad con el host:

```text
tailscale ping <IP-TAILSCALE-DEL-HOST>
```

El comando debe identificar al otro nodo. Esto valida Tailscale entre pares, pero todavía no valida el puerto ENet del juego.

## Firewall local del host

El host ENet se enlaza a `0.0.0.0:<puerto>`; por eso escucha en todas las interfaces IPv4, incluida Tailscale. Al probar otro puerto, sustituye 7777 en los comandos y la regla.

### Windows

Ejecuta PowerShell como administrador solo si hace falta crear la regla. Limita el origen al rango IPv4 predeterminado de Tailscale:

```powershell
New-NetFirewallRule -DisplayName "EscapeTheUAB ENet Tailscale" -Direction Inbound -Action Allow -Protocol UDP -LocalPort 7777 -RemoteAddress 100.64.0.0/10 -Profile Any
```

Si la tailnet usa un pool IPv4 personalizado, sustituye `100.64.0.0/10` por el rango autorizado. Con el host en ejecución, comprueba el socket:

```powershell
Get-NetUDPEndpoint -LocalPort 7777 | Select-Object LocalAddress,LocalPort,OwningProcess
```

Se espera que aparezca `0.0.0.0` y el puerto indicado. La regla puede eliminarse al terminar la prueba:

```powershell
Remove-NetFirewallRule -DisplayName "EscapeTheUAB ENet Tailscale"
```

### Linux

Revisa el firewall que use esa distribución. Con UFW, una regla acotada al rango IPv4 predeterminado es:

```bash
sudo ufw allow from 100.64.0.0/10 to any port 7777 proto udp
sudo ufw status
```

Con firewalld, revisa la zona de la interfaz Tailscale y añade una regla equivalente limitada al origen de la tailnet. Si se usa otro gestor, permite entrada UDP al puerto del juego desde la interfaz/rango Tailscale. No ejecutes comandos de instalación desde el juego.

Con el host activo, inspecciona el socket:

```bash
ss -lunp | grep ':7777'
```

Se espera `0.0.0.0:7777` o `*:7777`, no solo `127.0.0.1:7777`.

### macOS

En **Ajustes del Sistema → Red → Firewall → Opciones**, permite conexiones entrantes para el ejecutable `EntornVGI` cuando macOS lo solicite. Si se usa un firewall de terceros, permite UDP del rango/IP de Tailscale hacia el puerto del juego.

Con el host activo, inspecciona el socket:

```bash
lsof -nP -iUDP:7777
```

Confirma que el socket no está limitado a `127.0.0.1`.

## Ejecutar la prueba entre dos equipos

Mantén los lanzadores habituales (`run-windows.cmd`, `run-linux.sh`, `run-macos.sh`) para compilar y arrancar; Tailscale es una capa de red del sistema y no modifica esos lanzadores. Ejecuta el juego desde su menú **Multijugador** o usa el binario ya compilado.

1. En el host, selecciona **Host** y el puerto UDP 7777. Verifica el socket con el comando de su sistema anterior.
2. Anota la IPv4 de Tailscale del host (`tailscale ip -4`).
3. En el cliente, selecciona **Cliente**, introduce la IP del host y puerto 7777. El juego recuerda el último destino. Alternativamente ejecuta `EntornVGI --connect <IP-TAILSCALE>:7777`.
4. Espera el estado activo y verifica que ambos juegos muestran un jugador remoto. Mueve ambos jugadores y comprueba que las poses se actualizan.
5. Comprueba un evento fiable disponible en el juego, como abrir/cerrar una puerta o tomar/liberar el objeto compartido.
6. Desconecta un equipo, confirma que desaparece el remoto y vuelve a conectar. Repite con el host ausente o con un puerto equivocado: el cliente debe mostrar un mensaje de falta de respuesta y regresar al estado de error sin bloquear el juego.
7. Guarda sistema operativo, versiones, IPs anonimizadas (`100.x.x.x`), puerto, estado de firewall, resultado de cada paso y si Tailscale indicó conexión directa o relay. No registres claves de autenticación.

## Registro de ejecución

Completa esta tabla después de ejecutar los dos equipos; no marques una plataforma por haber compilado o pasado loopback:

| Host | Cliente | Tailscale ping | Socket host | Sesión ENet | Poses/eventos/desconexión |
|---|---|---|---|---|---|
| Pendiente | Pendiente | Pendiente | Pendiente | Pendiente | Pendiente |

## Referencias oficiales

- [Descargar Tailscale](https://tailscale.com/download)
- [Añadir dispositivos a una tailnet y aprobarlos](https://tailscale.com/docs/features/access-control/device-management/how-to/set-up)
- [Direcciones IPv4 de Tailscale](https://tailscale.com/docs/concepts/tailscale-ip-addresses)
- [Puertos y firewall de Tailscale](https://tailscale.com/docs/reference/faq/firewall-ports)
