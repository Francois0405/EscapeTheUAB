# Sincronización multijugador de escena y física

## Autoridad y flujo

El host es la única autoridad que simula los cuerpos dinámicos compartidos. Los OBJ con colisión dinámica se crean como cuerpos dinámicos Bullet en el host y cinemáticos en clientes. El cliente no predice ni integra gravedad para esos cuerpos. Las poses que llegan se aplican al mismo `btRigidBody` cinemático que usa la escena para colisión y render; no se reconstruye el cuerpo al llegar cada snapshot.

Los IDs de red son `HashId(SceneObject::id)`, a partir del ID persistente del JSON. La negociación de escena incluye nombre y conjunto ordenado de IDs OBJ. Se rechazan IDs duplicados o desconocidos. No se usan direcciones, índices de vector ni IDs internos de Bullet.

Las modificaciones editoriales, cambios de propietario, agarres, estados finales, puertas y configuración inicial usan eventos fiables por el canal ENet de eventos. El host valida y aplica una solicitud de agarre mediante raycast contra el objeto esperado. Mientras el objeto está sostenido, el host limita su movimiento con el solver Bullet existente y lo dirige hacia la pose remota del controlador. Al liberar, solo el host elimina la sujeción, restaura la gravedad y aplica la velocidad de lanzamiento validada.

Para habilitar el registro local opt-in del diagnóstico de agarre, consulta [Multiplayer grab diagnostics](MULTIPLAYER_DEBUG_GRAB.md).

Los cuerpos dinámicos despiertos o agarrados se transmiten en `DynamicSnapshot` no fiable secuenciado a 25 Hz. Se mandan hasta 16 entradas por paquete, con rotación circular si hay más cuerpos activos. Cada entrada incluye ID, tick en milisegundos monotónicos del host, transform, velocidades lineal y angular, actividad y propietario. Los clientes descartan ticks obsoletos y reconstruyen los intervalos entre muestras desde esos ticks (no desde su tiempo de llegada), conservando hasta ocho muestras por objeto. Renderizan con 120 ms de retraso mediante mezcla lineal de posición y slerp de rotación. No extrapolan a través de posibles rebotes o colisiones: si falta una muestra futura, mantienen la última pose. Al pasar a reposo el host manda un `kEvEstadoFisicoFinal` fiable. El estado fiable de todos los objetos se conserva para clientes tardíos.

Pose, física y eventos tienen canales ENet independientes. Así, un snapshot físico secuenciado no puede desplazar un snapshot de avatares. El orden solo se garantiza dentro de cada canal; los ticks de entidad y la comprobación de antigüedad del estado final resuelven el cruce entre el canal fiable y el canal dinámico no fiable.

Las funciones `TomarEstadosEscena()` y `TomarEstadosFisicosFiables()` consumen las colas: limpian el vector de salida reutilizado y lo intercambian con la cola interna. Así, una salida de la iteración anterior no vuelve a la cola ni se entrega dos veces. Los bucles de prueba esperan el evento concreto en vez de asumir que una pausa fija basta para que llegue.

Los cuerpos dinámicos de escena que se replican hoy son OBJ configurados como dinámicos en el JSON. Los GLB animados actuales se sincronizan como puertas mediante su sistema de eventos y colisionadores; no hay GLB agarrables en el flujo `EscenaFisica::dinamicos`. Agregar un GLB físico jugable requiere integrarlo deliberadamente en ese registro, no solo asignarle una malla GLB.

## Comparación técnica de referencias

| Referencia | Patrón observado | Uso en EscapeTheUAB |
|---|---|---|
| [Oculus Networked Physics Sample](https://github.com/fbsamples/oculus-networked-physics-sample) | Interacción VR, autoridad/propiedad por objeto y estado físico remoto; muestra de Unity/Rift archivada. | Reutilizamos la separación entre eventos de agarre/liberación y estado continuo, además de representar el objeto remoto con una sola autoridad. No se copia su stack Unity ni su topología de autoridad distribuida. |
| [valverl/NetworkedPhysics](https://github.com/valverl/NetworkedPhysics) | Implementación de referencia de la serie Network Physics de Glenn Fiedler. | Sirve como contraste de buffering, orden y autoridad. Conservamos ENet, host autoritativo y el protocolo existente; no añadimos una dependencia ni una capa genérica nueva. |
| [Bullet Physics](https://github.com/bulletphysics/bullet3) | Para mover un objeto cinemático se actualizan sus transforms del rigid body y motion state y se refresca el AABB del broadphase. | `FijarPoseCinematica` actualiza world/interpolation transforms y motion state, y llama `updateSingleAabb`. El cliente usa el mismo collider como representación visible; evita cuerpos duplicados y reconstrucciones por snapshot. |
| [Snapshot Interpolation](https://www.gafferongames.com/post/snapshot_interpolation/) | Buffer de snapshots retrasados, secuencia para descartar datos antiguos y reconstrucción interpolada; extrapolar cuerpos con colisiones puede divergir. | Buffer de ocho muestras, ticks por entidad, 120 ms, mezcla de posición y slerp. Se eliminó extrapolación porque este juego depende de choques y rebotes; se prefiere una pausa breve ante pérdida a atravesar paredes. |
| [Networked Physics in VR](https://www.gafferongames.com/post/networked_physics_in_virtual_reality/) | Estado de propiedad para arbitrar agarres, estado físico suficiente para reconstrucción remota y poses de manos para interacción VR. | El host arbitra agarres PC/VR con IDs de peer, y reutiliza las poses de cabeza/manos ya transmitidas. El cliente pide la acción; el host decide, simula y replica. No se transmiten matrices de huesos ni se adopta transferencia de autoridad por objeto, que añadiría conflictos y reconciliación innecesarios aquí. |

## Deficiencias encontradas y correcciones de esta revisión

- Poses y física compartían el mismo canal no fiable. Se añadió un canal ENet dinámico independiente; pruebas verifican la asignación de los tres streams.
- El cliente confirmaba que había agarrado un objeto, pero el código de carga de lanzamiento consultaba solo el agarre local de Bullet. Como su cuerpo es cinemático, el lanzamiento se cancelaba. La carga ahora acepta la propiedad dinámica confirmada y una prueba cubre cliente, cliente sin objeto y modo offline.
- Una liberación por desconexión solo quitaba la propiedad si Bullet encontraba aún el cuerpo sujeto. Ahora se limpia la propiedad confirmada incluso si el cuerpo no está disponible; evita objetos bloqueados después de reconstrucciones.
- Un evento fiable final podía cruzarse con un snapshot más reciente del canal dinámico. El cliente ahora compara ticks antes de vaciar el buffer, para que el evento viejo no haga retroceder la representación.
- Antes se extrapolaba la velocidad hasta 100 ms ante pérdida. Se quitó por la divergencia visual que puede causar en colisiones y rebotes.

## Conexión tardía, desconexión y reconexión

Al entrar un peer, el host manda de forma fiable el estado de configuración de escena y el último estado físico cacheado, con propietario. Al salir un peer, se encola la liberación de cada objeto que tenía asignado; el host elimina la propiedad aunque el cuerpo ya no esté sujeto. El cliente reconstruye sus cuerpos en modo dinámico al desconectarse o volver a offline. La sesión no admite alta/baja arbitraria de objetos ni recarga de escena durante una partida; cerrar la sesión antes de esas operaciones.

## Límites conocidos

- Snapshots llevan hasta 16 objetos activos por paquete. Más de 16 objetos simultáneos se reparten de forma circular; la frecuencia efectiva por objeto baja con el número de objetos. Medir en la escena real antes de aumentar tráfico o cambiar el límite.
- Al reposar, la posición final viaja fiable. Los objetos estáticos sin movimiento no consumen snapshots continuos.
- Los clientes no son autoridades físicas: su agarre y lanzamiento se solicitan por eventos fiables; el host realiza el raycast, resuelve el conflicto y simula Bullet.
- No se incorporó extrapolación, predicción local ni autoridad distribuida. Si una actualización se pierde, puede observarse una pausa de hasta que llegue otra muestra.
- El protocolo es v2; builds anteriores son incompatibles y el handshake debe rechazarlos.
- Se soportan cuerpos OBJ dinámicos y puertas GLB. La física de GLB dinámicos, puzles adicionales, LAN/Tailscale entre dos máquinas y PC + Quest requieren validación de contenido y hardware.

## Añadir un objeto físico replicable

1. Declarar un ID JSON único y persistente. Evitar nombres visibles repetibles.
2. Configurar una caja física dinámica válida y confirmar que el objeto entra en `EscenaFisica::dinamicos`.
3. Mantener al host como único simulador y aplicar en el cliente el cuerpo cinemático correspondiente.
4. Si agrega propiedades compartidas, codificarlas y validarlas en los eventos fiables de escena, sin poner selección/editor/cámara en el protocolo.
5. Añadir pruebas de serialización, rechazo de IDs inválidos, actualización Bullet, agarre/liberación, estado final y conexión tardía.

Para Puzle 1 y Puzle 2, los clientes deben solicitar acciones discretas y el host confirmarlas. Agregar estados iniciales al estado fiable de entrada tardía. Los eventos de puerta actuales conservan su camino separado.

## Validación automatizada

Desde la raíz del repositorio:

```sh
cmake --build build/macos-debug -j 4
ctest --test-dir build/macos-debug --output-on-failure
```

`NetworkTests` comprueba codec, límites, stream dinámico separado, snapshot y rechazo de tick antiguo. `NetJuegoTests` comprueba evento fiable final, solicitud cliente de agarre/lanzamiento, autoridad/propietario y estado para late join. `PhysicsTests` comprueba varios agarres Bullet, representación cinemática, AABB y coherencia de pose visual. Estos tests usan transporte en memoria excepto `ENetLoopbackTests`; no equivalen por sí solos a dos sesiones gráficas.

## Prueba manual con dos procesos

En el host:

```sh
./build/macos-debug/bin/EntornVGI --host --port 7777 --name Host
```

En el cliente:

```sh
./build/macos-debug/bin/EntornVGI --connect 127.0.0.1:7777 --name Cliente
```

Durante PLAY, tomar, transportar, lanzar y dejar rebotar un objeto OBJ dinámico; comprobar trayectoria, colisiones y reposo en ambas ventanas. Repetir con el cliente como actor, una solicitud simultánea, desconexión del propietario y reconexión. Conectar tarde después de mover un objeto. Comprobar puertas, poses, menú F4, offline y guardado del host. Para dos equipos LAN, usar como endpoint la IP LAN del host y permitir UDP 7777 en firewall; para Tailscale, usar la IP de la tailnet del host y permitir UDP 7777. Quest 3 debe validar posteriormente pose de controlador/cabeza y agarre con hardware real.

## Validación integrada de Windows (2026-11-25)

| Escenario | Estado | Evidencia |
|---|---|---|
| Codec de snapshots, límites MTU, IDs duplicados y ticks antiguos | PASS | `NetworkTests` |
| Canales de pose/física/eventos separados | PASS | `NetworkTests` |
| Solicitud y liberación de cliente, propiedad, final fiable y late join | PASS | `NetJuegoTests` con transporte en memoria |
| Representación cinemática Bullet, actualización de AABB y collider/visual | PASS | `PhysicsTests` |
| CTest completo Debug y Release | PASS | 19/19 en cada configuración. |
| `NetworkTests`, `NetJuegoTests`, `PhysicsTests` y tests del editor | PASS | Incluidos en los dos conjuntos completos. |
| `ObjLoaderTests` y `SceneLoaderTests` | PASS | Incluidos en los dos conjuntos completos; valida el bloque std140 y el slot 16. |
| `NetJuegoTests` repetido | PASS | 20/20 Debug y 20/20 Release, secuencialmente. |
| `ENetLoopbackTests` repetido | PASS | 10/10 Debug y 10/10 Release, secuencialmente. |
| Host/cliente gráfico por loopback | PARTIAL | Los dos procesos arrancaron; el host enlazó UDP `0.0.0.0:7777` y el cliente abrió un socket UDP. El handshake y la interacción dentro del juego no se verificaron. |
| LAN entre equipos y Tailscale | NOT RUN | Requiere dos equipos y comprobación manual del firewall/red. |
| PC + Quest con OpenXR | NOT RUN | `OpenXRTests` pasó, pero no había sesión de runtime/visor que validar. |

No marcar como PASS una validación gráfica, LAN, Tailscale o Quest si no se ejecutó realmente. Los tests en memoria no reemplazan evidencia de red real.
