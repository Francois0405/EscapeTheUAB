# Hirity: funcionamiento, audio y fisicas

Documento del comportamiento implementado. Si difiere del codigo o de la escena cargada,
prevalecen el codigo y los datos actuales.

## Resumen

Hirity es una instancia de escena (`id: "hiriti"`) con un modelo OBJ y un cuerpo rigido
dinamico de tipo rebotador. Puede descubrirse al mirarlo, hablar, recibir impactos, seguir al
jugador durante ciertos eventos de busqueda y reaparecer detras del jugador si se cumplen las
condiciones de distancia e inactividad.

La implementacion se reparte entre:

| Archivo | Responsabilidad |
|---|---|
| `src/gameplay/hirity/HiritiRuntime.cpp` | Descubrimiento, voz, impactos, seguimiento, reaparicion y eventos ambientales |
| `src/gameplay/hirity/HiritiAnimation.h` | Estado y calculos puros: mirada, temporizadores, suspension del seguimiento y direccion/aceleracion |
| `src/gameplay/hirity/HiritiAudioTimer.h` | Alternancia y temporizacion de eventos ambientales |
| `src/gameplay/hirity/HiritiSoundVolume.h` | Atenuacion de volumen por distancia |
| `src/app/adapters/HirityAdapters.cpp` y `.h` | Presta el estado de `app` al modulo de gameplay |
| `src/physics/PhysicsScene.cpp` | Convierte la configuracion JSON de la instancia en un cuerpo de Bullet |
| `src/physics/PhysicsWorld.cpp` | Mundo, colisiones, rebote, impulsos y pasos de simulacion |
| `src/audio/AudioManager.cpp` | Inicializacion del motor y reproduccion one-shot |
| `src/audio/SubtitleManager.cpp` | Relaciona reproducciones de audio con las entradas de subtitulos |

El bucle de juego llama al comportamiento mediante el adaptador mientras la partida esta
activa. El modulo recibe referencias y callbacks; no lee directamente las cabeceras de estado
de `src/app`.

## Escena y modelo

La escena de inicio es `assets/scenes/aula_informatica_calibrada.json`. En ella, Hirity tiene
la posicion inicial `(-5.5, -5.65, -3.0)`, el modelo `../models/Hiriti.obj`, escala uniforme
0.2 y esta configurado como:

```json
{
  "type": "bouncy",
  "shape": "sphere",
  "halfSize": [1.0, 1.0, 1.0],
  "offset": [0.0, 0.0, 0.0],
  "mass": 0.25
}
```

Para una esfera, `halfSize` se interpreta como radio antes de aplicar la escala; por tanto,
el radio fisico en esta instancia es aproximadamente 0.2 m. El centro coincide con el origen
del modelo porque el offset es cero. El JSON enlaza la representacion visual y su colisionador:
Bullet proporciona la pose del cuerpo dinamico para dibujar el OBJ.

La escena usa metros y eje Z vertical. El cuerpo puede recibir gravedad, colisionar con el
entorno, ser agarrado y lanzado. Al caminar contra un cuerpo `bouncy`, el mundo puede darle un
impulso horizontal si esta solapado con el jugador y este avanza hacia el objeto. Esa ayuda
del jugador se limita a 7 m/s en el plano horizontal.

## Fisica y rebote

`PhysicsScene::Create` transforma `type: "bouncy"` en un cuerpo dinamico de Bullet con forma
esferica. En `PhysicsWorld`:

- La gravedad es `(0, 0, -9.81) m/s^2`.
- La simulacion avanza en pasos fijos de 1/60 s; el acumulador limita el tiempo procesado por
  fotograma para evitar bloqueos despues de una pausa o tiron.
- El tipo rebotador usa restitucion 0.82 y friccion 0.2; tambien asigna friccion de rodadura y
  giro de 0.02. La callback de contacto fija restitucion y friccion combinadas a esos valores
  cuando participa un cuerpo rebotador.
- Los impulsos y aceleraciones de gameplay llegan a Bullet a traves de `PhysicsScene`, no
  modificando directamente el JSON ni la transformacion visual.

El rebote proviene de la restitucion de las colisiones. No es una animacion ni una trayectoria
programada: gravedad, forma, masa, friccion, velocidad de impacto y geometria circundante
determinan el resultado. Una vez quieto, Hirity puede dormirse como cualquier cuerpo dinamico.

## Estados y comportamiento

### Descubrimiento y dialogo

El descubrimiento requiere que el rayo de mirada apunte al cuerpo, que este delante del
jugador y a no mas de 10 m. La tolerancia se calcula con el radio derivado de la escala visual.
La primera mirada valida marca Hirity como descubierto y emite un impulso vertical de
`(0, 0, 0.75)`.

Al descubrirlo se reproduce una de dos presentaciones: saludo en castellano con probabilidad
4/5 o saludo en gallego con probabilidad 1/5. Cinco segundos despues, si la espera sigue
activa, se reproduce `story/agarrame.ogg` una sola vez. Las funciones de animacion y
temporizacion estan separadas de la reproduccion y son cubiertas por pruebas de
`tests/AnimacionHiritiTests.cpp`.

### Agarre, lanzamiento e impactos

El agarre y el lanzamiento fisico se gestionan por la interaccion generica de objetos
dinamicos. El estado de Hirity suspende su seguimiento mientras el jugador lo manipula; si se
lanza con potencia superior a 0.5 y luego se detecta un nuevo contacto, se emite uno de los
dos sonidos de dano. Las variantes corta y larga se alternan; un contacto sostenido no dispara
repeticiones continuas.

### Busqueda y seguimiento

`HiritiAudioTimer` espera 30 s para el primer evento activo y despues alterna eventos cada
15 s:

1. Evento ambiental: elige tos o pasos fuertes y desactiva el seguimiento.
2. Evento de busqueda: elige una de las voces de busqueda y activa el seguimiento.

Al detenerse el sistema ambiental, el seguimiento se desactiva y el temporizador vuelve a su
estado inicial. Durante el seguimiento, Hirity busca al jugador en el plano horizontal. Cinco
rayos de anticipacion, separados entre si hasta +/-90 grados, estiman el espacio libre. Se
elige una direccion que equilibra alineacion con el jugador y espacio; una aceleracion limitada
a 3.2 m/s^2 la aproxima a la velocidad deseada, que respeta la distancia de parada y velocidad
maxima configuradas.

El seguimiento se suspende si Hirity esta agarrado, tiene propietario de red o se ha lanzado.
Tras una suspension por lanzamiento, espera 0.75 s estable por debajo de 0.15 m/s antes de
reanudar. Los clientes de red reciben el cuerpo replicado; el procesamiento de colisiones que
modifica la autoridad fisica queda en host u offline.

### Reaparicion

Si el jugador tiene cuerpo fisico, Hirity esta a mas de 10 m, y no esta siendo manipulado ni
puede seguir, corre un temporizador de 30 s. Al cumplirse, prueba posiciones detras del jugador
a distancias de 2.4, 2.9 y 3.4 m, con cinco angulos. Un rayo comprueba obstaculos y
`CanPlaceDynamicObject` descarta posiciones ocupadas. Solo se teletransporta si encuentra una
posicion valida; despues reproduce aleatoriamente un sonido de `assets/audio/hirity/appear/`.

## Audio y subtitulos

Los recursos del juego son ficheros `.ogg` bajo `assets/audio/hirity/`. `AudioManager` usa
[miniaudio](https://github.com/mackron/miniaudio) para el motor de audio y `stb_vorbis` para
la decodificacion Vorbis. Los eventos se lanzan como reproducciones one-shot. El volumen de
Hirity se atenúa manualmente segun la distancia entre el cuerpo y los ojos del jugador: volumen
completo hasta 2 m, descenso lineal entre 2 y 20 m y un minimo del 15 % desde 20 m. Algunos
eventos, como la voz de aparicion, pasan un volumen base de 1.6 antes de esa atenuacion. La
reproduccion one-shot no activa espacializacion 3D automatica; el juego aplica esta atenuacion
de distancia al volumen.

| Grupo | Recursos actuales | Cuando suenan |
|---|---|---|
| `story/` | `holahirity.ogg`, `holahiritygallego.ogg`, `agarrame.ogg` | Descubrimiento y espera posterior |
| `searching/` | `heydondeestashirity.ogg`, `venaquihirity.ogg` | Evento ambiental de busqueda |
| `damage/` | `damagehirity.ogg`, `damegehiritylong.ogg` | Impacto tras un lanzamiento suficientemente fuerte |
| `ambient/` | `cough.ogg`, `pasosfuertes.ogg` | Evento ambiental que no activa seguimiento |
| `appear/` | `anotopat.ogg`, `rickroll.ogg` | Reaparicion; seleccion aleatoria entre los `.ogg` disponibles |

La seleccion de aparicion enumera el directorio en ejecucion, ordena los nombres y escoge
con probabilidad equivalente; si no hay archivos, usa `anotopat.ogg` como ruta alternativa. Por ello, agregar
un `.ogg` a ese directorio puede cambiar los sonidos que se eligen sin modificar el codigo.

`assets/subtitles/hirity.json` asocia rutas de audio con texto, idioma, hablante, prioridad y,
opcionalmente, segmentos temporizados. El gestor solo activa una linea cuando empieza la
reproduccion correspondiente y la consulta junto al cursor del audio. La opcion de subtitulos
puede ocultarlas; el sistema tambien permite filtrar captions y textos no verificados.

### Procedencia de audio y modelo

Los repositorios de miniaudio y stb son dependencias de software, no el origen de las voces ni
de los efectos. El registro del proyecto no atribuye los audios de Hirity a una biblioteca
externa: los metadatos disponibles no identifican a los autores del resto de clips. La nota de
`assets/licenses/README.md` pide confirmar con el equipo quien los grabo antes de distribuirlos.

El mismo registro senala que `rickroll.ogg` no tiene autor ni licencia identificables, y que
su contenido no ha sido escuchado ni identificado en esa revision. Se recomienda verificarlo
y resolver sus derechos antes de publicar o distribuir el juego. El modelo `Hiriti.obj` tambien
figura entre los modelos aportados por el equipo o la base de la asignatura sin declaracion de
autoria o licencia comprobable.

## Tecnologias y repositorios externos

| Tecnologia | Uso en Hirity | Referencia |
|---|---|---|
| C++20 y CMake | Runtime, composicion de dependencias y compilacion | `src/gameplay/hirity/`, `src/app/adapters/`, `src/CMakeLists.txt` |
| Bullet 3 | Cuerpos rigidos, contactos, gravedad, simulacion y raycasts | [bulletphysics/bullet3](https://github.com/bulletphysics/bullet3); `bullet3` en `vcpkg.json` |
| GLM | Vectores, distancias, direcciones y aceleraciones | [g-truc/glm](https://github.com/g-truc/glm); dependencia `glm` |
| miniaudio | Motor y reproduccion de audio | [mackron/miniaudio](https://github.com/mackron/miniaudio); copia en `third_party/miniaudio/` |
| stb_vorbis / stb | Decodificacion de audio Vorbis | [nothings/stb](https://github.com/nothings/stb); llega desde vcpkg o el sistema |
| JSON | Definicion de la instancia fisica en la escena | `assets/scenes/aula_informatica_calibrada.json`; parser del proyecto |
| OBJ | Malla visual de Hirity | `assets/models/Hiriti.obj`; el modelo no define la colision de Bullet |
| ENet | Transporte de red; la simulacion de dinamicos se mantiene autoritativa | [lsalzman/enet](https://github.com/lsalzman/enet); `enet` en `vcpkg.json` |

Bullet esta distribuido bajo zlib; miniaudio bajo dominio publico o MIT-0; GLM usa MIT;
stb ofrece sus propias condiciones de licencia. Consultar las copias y avisos del proyecto en
`third_party/licenses/` antes de redistribuir. Los derechos de esos repositorios no aclaran los
derechos de los audios o del modelo del juego.

## Pruebas y limites conocidos

- `tests/AnimacionHiritiTests.cpp` cubre mirada, espera de dialogo, suspension, contactos,
  seguimiento, direccion y atenuacion de voz.
- `tests/PhysicsTests.cpp` cubre el mundo de Bullet de forma automatizada; eso no sustituye una
  prueba manual del recorrido completo de Hirity en una partida.
- Los textos y las fuentes de sonido no se deben dar por verificados solo porque existan en el
  JSON; la procedencia de audio y modelo sigue pendiente de confirmacion en el registro de
  assets.
