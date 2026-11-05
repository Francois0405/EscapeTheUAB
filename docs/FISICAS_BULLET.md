# Físicas con Bullet (Fases 1, 2 y 3)

Estado: **Fase 1** (mundo físico, cubo que cae) y **Fase 2** (habitaciones y objetos animados
declarados en el JSON, con colisiones estáticas, dinámicas y cinemáticas). **Fase 3** (controlador físico del jugador FPS). Manos VR e
interacción física quedan para fases posteriores.

## Archivos

| Archivo | Para qué sirve |
|---|---|
| `EntornVGI/MundoFisico.h/.cpp` | Envoltorio pequeño de Bullet (`fisica::MundoFisico`) y la demo (`fisica::DemoCuboCaida`). Sin OpenGL. |
| `tests/PhysicsTests.cpp` | Pruebas sin ventana: gravedad, caída, paso fijo, FPS, pausa, suelo, reinicio. |
| `EntornVGI/main.cpp` | Crea la demo, la actualiza cada fotograma, la dibuja y añade el panel ImGui "Fisica (Bullet)". |
| `vcpkg.json`, `CMakeLists.txt` | Dependencia `bullet3` y target `EntornVGI_bullet`. |

## Ejes y unidades

Comprobado en `habitacion_beta.json` y `docs/ANIMACIONES_GLB.md`: **Z es vertical** y la
unidad es el **metro**. Por eso la gravedad es `(0, 0, -9.81)`. El suelo visible de la
sala tiene su cara superior en `z = -1.70`; la demo usa esa misma altura.

## Cómo funciona el mundo físico

1. `MundoFisico` crea las 5 piezas que exige Bullet (configuración, despachador,
   broadphase, solver y mundo dinámico) y las destruye en orden inverso.
2. Un **cuerpo rígido** = forma + masa + posición. `masa = 0` → estático; `masa > 0` → dinámico.
3. Bullet no libera nada por su cuenta: `MundoFisico` posee formas y cuerpos con `unique_ptr`.
4. Los tipos de Bullet no salen de `MundoFisico.cpp`; la interfaz pública usa GLM.

## Cómo se actualiza cada fotograma

En el bucle de `main.cpp`: `demoFisica.Actualizar(delta, animacionPausada)`.

* El tiempo real se **acumula**; mientras haya ≥ 1/60 s se ejecuta un paso fijo de 1/60 s.
  Con 30 FPS son 2 pasos por fotograma; con 144 FPS, a veces 0 (resultado idéntico).
* Un fotograma enorme se recorta a 0,25 s (máx. 15 pasos): el juego no se bloquea.
* **Pausa** (ESC o ventana sin foco): no se simula y el tiempo no se acumula, así que al
  reanudar no hay un salto.
* Para dibujar, `PoseDe(id)` da posición y rotación; `main.cpp` construye
  `traslación * rotación * escala` y reutiliza `test.obj` (cubo de ±1 m).

## Probar la demo

`F1` (paneles de depuración, modo juego) → ventana **Fisica (Bullet)** → *Activar demo cubo*.
El cubo cae desde 1,2 m (en `(6, -1)`, zona libre de la sala), golpea el suelo y se detiene.
*Reiniciar demo* lo vuelve a soltar; *Quitar demo* lo elimina. El panel muestra gravedad,
posición, velocidad y si Bullet ya lo ha "dormido". No se modifica ninguna instancia de
`habitacion_beta.json`.

## Crear otro objeto físico

```cpp
fisica::MundoFisico mundo;
auto suelo = mundo.CrearCajaEstatica(glm::vec3(0, 0, -1.85), glm::vec3(8, 5, 0.15));
auto caja  = mundo.CrearCajaDinamica(glm::vec3(2, 0, 1), glm::vec3(0.2f), /*kg*/ 1.0f);
// cada fotograma:
mundo.Actualizar(delta);
fisica::PoseCuerpo pose = mundo.PoseDe(caja);   // pose.posicion, pose.rotacion
```

Los tamaños son **medias dimensiones**. Para formas nuevas (esfera, cápsula), añade una
función `Crear...` junto a `CrearCaja` cambiando solo la `btCollisionShape`.

## Notas de comportamiento

* Al impactar a ~7 m/s, Bullet permite ~5 cm de penetración momentánea y lo corrige; no es un error.
* Por defecto Bullet duerme los cuerpos tras ~2 s quietos (`CuboEnReposo()`).

## Dependencias y verificación

* Windows/macOS (CMake): `bullet3` en `vcpkg.json` con el `builtin-baseline` existente (puerto 3.25#3).
* Linux: `sudo apt install libbullet-dev` (añadido al README). CMake usa el `BulletConfig.cmake` del paquete.
* Visual Studio (`EntornVGI.vcxproj`): se añadieron los archivos al proyecto, pero el proyecto usa
  librerías de `include/`/`lib/x64`. Hay que instalar Bullet con vcpkg en modo clásico
  (`vcpkg install bullet3:x64-windows` + `vcpkg integrate install`).
* **Verificado solo en macOS arm64** (CMake + vcpkg, Debug). Linux y Windows/VS **no** se han probado.


# Fase 2: físicas en las habitaciones (JSON)

## Archivos nuevos o modificados

| Archivo | Qué hace |
|---|---|
| `SceneLoader.h/.cpp` | Lee los campos opcionales `physics` (objetos OBJ) y la lista `animated` (GLB). `CargarDatosEscenaJSON` devuelve `DatosEscena`; `CargarEscenaJSON` (la antigua) sigue igual. |
| `EscenaFisica.h/.cpp` | Puente escena → Bullet: crea/retira los cuerpos de una habitación, sincroniza las hojas cinemáticas y da la matriz de dibujo. Sin OpenGL ni cámara. |
| `MundoFisico.h/.cpp` | Ahora admite `TipoCuerpo` (estático/dinámico/cinemático), `CrearCaja`, `MoverCinematico`, `EliminarCuerpo`, `CajasDeColision`, `AristasDeCaja`. La demo ya usa el mundo único. |
| `ObjetosAnimadosGLB.h/.cpp` | Añade `escala` por instancia y `SeleccionarAnimacion(nombre)`. |
| `DibujoColisionadores.h/.cpp` | Dibuja las cajas como líneas (reutiliza el shader `eixos`). |
| `main.cpp` | Ya no hay puertas escritas en C++: `cargarYAplicarEscena` monta todo desde el JSON. |
| `tools/medir_obj.py` | Mide un OBJ y escribe su bloque `physics`. |
| `assets/scenes/habitacion_beta.json` | Sigue con 12 instancias; ahora con `physics` y dos puertas en `animated`. |
| `assets/scenes/ejemplo_fisica.json` | Mesa sólida, tres cajas que caen y dos puertas (menú Arxius → "Cargar ejemplo fisica JSON"). |

## Formato JSON (todo lo nuevo es opcional)

```json
{ "id": "mesa", "model": "../models/TaulaRara.obj", "position": [5, 0, -1.7],
  "rotation": [90, 0, 0], "scale": [1, 1, 1],
  "physics": { "type": "static",            // "static" | "dynamic"
               "shape": "box",              // opcional; solo "box"
               "halfSize": [1.295, 0.5788, 1.015],   // medias dimensiones
               "offset":   [0, 0.5812, 0],            // centro de la caja (opcional)
               "mass": 2.0 } }              // solo en "dynamic" (kg)
```

```json
"animated": [ { "id": "puerta_1", "model": "../animations/Puerta.glb",
    "position": [6, -2, -1.7], "rotation": [0, 0, -90], "scale": 1,
    "animation": "NombreDeLaAnimacion",     // opcional: por defecto la primera
    "colliders": [
      { "node": "DoorFrame", "type": "static",    "halfSize": [...], "offset": [...] },
      { "node": "Door",      "type": "kinematic", "halfSize": [...], "offset": [...] } ] } ]
```

Un JSON sin `physics` ni `animated` carga como antes. Los campos mal escritos dentro de `physics`,
`animated` y `colliders` dan error (no se ignoran). El `version` sigue siendo 1.

## Espacio de las cajas (importante)

* **Objetos OBJ:** `halfSize` y `offset` se miden en el espacio del **modelo** (las coordenadas de los
  vértices del `.obj`), **antes de la escala**. Si la escala es 0,65, la caja también mide 0,65 veces. Así no
  se aplica dos veces. Resultado en el mundo (`R` = rotación del objeto, `Rz·Ry·Rx`):
  `centro = position + R·(scale·offset)`, `mitad = |scale|·halfSize`, orientación = `R`.
  `python3 tools/medir_obj.py ruta.obj [static|dynamic] [kg]` da `halfSize` y `offset` listos.
* **Objetos animados:** las cajas van en el espacio **local del nodo** del GLB (el de glTF, Y arriba), porque
  se pegan a ese nodo. La caja sigue al nodo con su matriz de mundo: `colocación · (Y-arriba→Z-arriba) · nodo`.
* **Dinámicos con offset:** Bullet mueve el centro de la caja; el dibujo deshace el offset
  (`M = trasladar(centro)·giro·trasladar(−scale·offset)·escalar`), así que el origen del modelo no salta.

## Puertas: marco estático y hoja cinemática

`Puerta.glb` tiene los nodos `DoorFrame` (marco: dos postes y un dintel, **hueco** de 0,8×2,0 m) y `Door`
(hoja, hija del marco, con el pivote en la bisagra). Por eso el marco son **3 cajas estáticas** (una caja
entera taparía el hueco) y la hoja es **1 caja cinemática** pegada al nodo `Door`. Cada fotograma:

1. `ActualizarObjetos` avanza la animación (pose de cada puerta).
2. `SincronizarCinematicos` mueve la caja de la hoja a la pose de su nodo (la bisagra queda fija, la hoja gira).
3. `MundoFisico::Actualizar` simula. El marco no se mueve nunca.

Las dos puertas comparten el mismo `ModeloGLB` pero tienen pose y reloj propios (se prueba en los tests).
Los nombres de animación se comparan tal cual están en el GLB; `Puerta.glb` guarda `Door_acci\u00f3n` con
la secuencia escapada literal, por eso las puertas del ejemplo usan la animación por defecto. Conviene usar
nombres ASCII en Blender si se quiere elegirlas por nombre.

## Cambio de habitación y cargas fallidas

`cargarYAplicarEscena` hace: (1) leer/validar JSON, (2) cargar OBJ, (3) cargar GLB, (4) crear los cuerpos
nuevos, (5) **solo si todo salió bien** vaciar la habitación anterior y sustituirla. Si algo falla se descartan
los temporales y la habitación activa queda intacta (sin cuerpos a medias: `EscenaFisica::Crear` es atómica).
Hay un único `MundoFisico`; la demo del cubo y cada habitación quitan solo sus propios cuerpos.

## Depuración: "Mostrar colisionadores"

F1 → ventana **Fisica (Bullet)** → casilla *Mostrar colisionadores*: **azul** estático, **verde** dinámico
(más oscuro si duerme), **naranja** cinemático. Se leen de Bullet (no modifican nada) y se ven a través de las paredes.

## Ejemplo: una mesa sólida y una caja que cae

En el JSON de la habitación (dentro de `"objects"`):

```json
{ "id": "mi_mesa", "model": "../models/TaulaRara.obj", "position": [5, 0, -1.7], "rotation": [90, 0, 0], "scale": [1, 1, 1],
  "physics": { "type": "static", "halfSize": [1.295, 0.5788, 1.015], "offset": [0, 0.5812, 0] } },
{ "id": "mi_caja", "model": "../models/test.obj", "position": [5, 0, 1.0], "rotation": [0, 0, 0], "scale": [0.25, 0.25, 0.25],
  "physics": { "type": "dynamic", "halfSize": [1, 1, 1], "mass": 2.0 } }
```

Nuevo objeto desde Blender: exportar el `.obj` a `assets/models/`, medirlo con `tools/medir_obj.py`, pegar el
bloque `physics` en el JSON y ejecutar. Para un GLB animado: copiarlo a `assets/animations/`, añadir un bloque
a `animated` y sus `colliders` por nombre de nodo.

## Tests (`ctest`)

`SceneLoaderTests` (formato antiguo y nuevo, errores comprensibles, carga fallida sin efectos),
`PhysicsTests` (Fase 1 + colisionadores trasladados/girados/escalados, estático que no cae, dinámico que cae y
choca, 12 OBJ + 2 puertas montadas desde `habitacion_beta.json`, hoja cinemática que gira sobre la bisagra con el
marco quieto, cambio de habitación sin residuos, carga fallida, pausa y reinicio), `GlbTests`, `ObjLoaderTests`.
Cambio justificado en tests de la Fase 1: `DemoCuboCaida` ya no tiene mundo propio, así que se construye con uno.

## Límites y pendiente

* Solo cajas. Sin mallas triangulares; el contorno redondeado (lámpara) se aproxima con una caja.
* El mundo físico es independiente de GLFW y de la cámara (listo para VR).
* Los objetos animados solo giran en Z y usan escala uniforme.
* No se ha verificado el dibujo de colisionadores ni los paneles visualmente (solo que la app arranca y carga la sala).
* **Verificado solo en macOS arm64.** Linux y Windows/Visual Studio: sin probar. En VS se añadieron los archivos nuevos
  al proyecto, pero no se ha compilado.


# Fase 3: jugador FPS con colisiones

## Cómo funcionaba antes y cómo funciona ahora

La cámara FPS son dos puntos: `opvN` (los ojos) y `n` (hacia dónde mira). Antes WASD sumaba directamente a
ambos, sin colisiones. Ahora:

```
WASD ──► moverJugadorFPS()        "quiero ir así a 3 m/s"  (velocidad deseada, no posición)
              │
              ▼
   MundoFisico (Bullet): cápsula  ── choca, DESLIZA, cae ──►  PosicionPiesJugador()
              │
              ▼
   sincronizarJugadorYCamara()    ojos = pies + 1,7 m ; el punto mirado se mueve igual
```

El ratón sigue girando solo la cámara: **Bullet nunca toca la orientación**.

## Funciones principales (`MundoFisico`)

| Función | Qué hace |
|---|---|
| `CrearJugador(pies)` | Cápsula vertical (Z) de 25 cm de radio y 1,8 m de alto con los pies en `pies`. |
| `FijarVelocidadJugador(v)` | Velocidad horizontal deseada (m/s, mundo). Se convierte en desplazamiento por paso fijo, por eso no depende de los FPS. |
| `PosicionPiesJugador()` / `JugadorEnSuelo()` | Resultado de la simulación. |
| `TeletransportarJugador(pies)` | Coloca al jugador (cambio de sala, depuración). |
| `EliminarJugador()` | Lo quita del mundo. |

Medidas en `fisica::MedidasJugador` (radio 0,25; altura 1,8; ojos 1,7; escalón 0,10).

## Conceptos de Bullet que usa

* **`btKinematicCharacterController`**: en cada paso fijo intenta mover la cápsula lo que se le pide, la frena
  contra lo que choque, la desliza por la pared (por eso en diagonal contra una pared sigue avanzando a lo largo)
  y le aplica la gravedad. No es un cuerpo rígido: no recibe empujones ni gira. Es la solución estándar de Bullet.
* **Fantasma (`btPairCachingGhostObject`)**: objeto que detecta solapamientos sin recibir fuerzas. Necesita el
  `btGhostPairCallback` del broadphase (en `MundoFisico::Impl`).
* **Eje Z**: hay que decirle al controlador que "arriba" es Z (`setUp`) y la gravedad; por defecto Bullet usa Y.
* **Pasos fijos de 1/60 s** (ya existentes): a pocos FPS se ejecutan varios pasos pequeños seguidos, así que el
  jugador no se salta paredes. Un fotograma enorme se recorta a 0,25 s (15 pasos de ≤ 5 cm).
* **`setMaxPenetrationDepth(0.02)`**: por defecto Bullet tolera 20 cm de solapamiento. Con 2 cm una puerta que se
  cierra empuja al jugador en vez de atravesarlo.

## Estado, pausa y modos de cámara

* Pausa (ESC / sin foco): el mundo no avanza y la velocidad se pone a 0; el jugador queda congelado.
* F1 (paneles de depuración): no puede caminar pero sigue bajo gravedad y colisiones.
* Cámaras que no son FPS: la cápsula sigue a la cámara (teletransporte cada fotograma) y al volver al FPS no hay salto.
* El jugador se crea tras cargar la primera habitación (pies 1,7 m bajo los ojos, +2 cm). Al cambiar de
  habitación permanece donde está.

## Puertas (comportamiento definido)

* **Cerradas:** la hoja y el marco son sólidos; no se atraviesan, tampoco a pocos FPS.
* **Abiertas:** la de `habitacion_beta` queda tumbada contra un lado del hueco: pasan 72 cm libres, cabe la cápsula (50 cm).
* **Cerrándose sobre el jugador:** la hoja (cinemática) NO se detiene; **empuja** al jugador fuera de su recorrido
  poco a poco (2-5 cm por paso). Si lo aprieta contra el marco, hay un único pico final (~11 cm, medido). Nunca queda solapado.
  Mejora futura posible: que la puerta se detenga si el jugador está en su recorrido.
* `puerta_1` de `habitacion_beta` está medio tapada por `table_right` (disposición ya existente de la escena).

## Cómo adaptarlo a VR (sin implementarlo aún)

La cápsula solo gobierna la **posición del cuerpo**; nada en ella depende de la cámara FPS ni del ratón.

1. **Posiciones separadas:** cuerpo (`PosicionPiesJugador`), visor y mandos (OpenXR) son datos distintos. La altura
   de los ojos (`AlturaOjos`) solo se usa en la cámara FPS; en VR los ojos vienen del visor.
2. **Locomoción con joystick:** igual que WASD: calcular una velocidad en el mundo con la dirección del visor/mando
   y llamar a `FijarVelocidadJugador`.
3. **Caminar de verdad (room-scale):** cada fotograma, la posición horizontal del visor respecto al cuerpo es un
   movimiento deseado: `velocidad = (cabezaXY − cuerpoXY) / dt` (limitada). Si la pared lo frena, el cuerpo se queda y
   el visor se aleja de él; entonces hay que avisar al jugador (p. ej. fundido a negro) en vez de mover la cabeza.
4. **La cabeza no se toca:** Bullet solo devuelve la posición de los pies; la pose del visor la manda el seguimiento
   real. La cámara VR = origen del jugador + pose del visor.
5. **Manos:** se añadirán como cuerpos cinemáticos aparte (`CrearCaja(TipoCuerpo::Cinematico...)`), como las puertas.

## Tests de la Fase 3 (`PhysicsTests`)

Suelo y gravedad, andar libre, choque frontal, deslizamiento en diagonal, objeto estático (no se trepa, se rodea),
independencia de FPS (144/60/15/4 FPS y un tirón de 10 s), pausa y reanudación, teletransporte, puerta cerrada
(también a 8 FPS), puerta abierta, puerta que se cierra sobre el jugador, y un recorrido real de `habitacion_beta`.
Verificado solo en macOS arm64: el movimiento en la ventana (sensación de juego, deslizamiento con el ratón) **no** se
ha probado a mano; Windows, Linux y VR tampoco.

## Fuera de alcance (a propósito)

Saltos, agacharse, escaleras, empujar cajas con el cuerpo, físicas de manos, OpenXR.


# Integración con la rama VR: vista estéreo de prueba y agarre (E)

## Vista estereoscópica de prueba (F2) — NO es VR real

`F2` divide la pantalla en dos mitades (ojo izquierdo / derecho, 6,4 cm de separación) usando la cámara FPS.
Sirve para probar el dibujo a dos vistas en un monitor. **No hay OpenXR, visor, seguimiento de cabeza ni mandos, y no
funciona en un Meta Quest 2.** Con VR real, OpenXR dará la pose y la proyección de cada ojo y `VistaEstereo.h` se sustituirá.

* Solo actúa con la cámara de navegación; las demás cámaras del profesor (esférica, geode, ortográficas) se dibujan siempre igual.
* Reutiliza `Vista_Navega` para la vista central y desplaza solo la matriz de cada ojo (`estereo::VistaDeOjo`).
* No toca la física ni al jugador: solo cambia cómo se dibuja.

## Agarrar y soltar con E

Una sola acción, `interactuar()` en `main.cpp`, con esta prioridad:

1. Con un objeto en la mano → lo suelta (con las manos ocupadas no se abren puertas).
2. Si se mira un objeto **dinámico** a menos de 2 m → lo agarra (un rayo de Bullet: si hay una pared en medio no se agarra, y
   suelos, muebles estáticos y puertas nunca son agarrables).
3. Si no → la puerta que se tiene delante, como antes.

El objeto agarrado **sigue siendo el mismo cuerpo de Bullet**: sin gravedad y con una velocidad (máx. 6 m/s) que cada paso fijo
lo lleva hacia un punto 1,1 m delante de los ojos. Por eso choca con paredes en vez de atravesarlas (penetración medida ≤ 3 cm),
lo que se dibuja es exactamente la pose de Bullet, y al soltarlo recupera la gravedad y cae. Si queda atascado a más de 2 m de la
mano, se suelta solo. Pausa/foco/F1: la mano no se actualiza y el objeto se queda donde está. Al cambiar de habitación el agarre
se elimina. En `habitacion_beta`, `box_large` (8 kg) y `box_small` (1 kg) son ahora `dynamic`, así que son los agarrables.

Funciones: `MundoFisico::CuerpoAlFinalDelRayo / SujetarCuerpo / SoltarCuerpoSujeto / FijarDestinoDeSujecion` y
`EscenaFisica::IntentarAgarrar / ActualizarAgarre / SoltarAgarre`. Reciben solo origen y dirección del mundo (en VR: mano o mando).

**Pendiente (subfase):** lanzar objetos, rotarlos con la mano, que el objeto no pueda empujar al jugador contra una pared, agarrar con
dos manos y manos físicas VR.
