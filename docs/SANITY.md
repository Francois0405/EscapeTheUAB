# Sanity: funcionamiento

Documento del comportamiento implementado. Si difiere del codigo o de los ajustes actuales,
prevalecen el codigo y los valores de `Sanity.h`.

## Resumen

Sanity es un sistema local al cliente que reduce gradualmente la cordura mientras se juega.
El valor resultante alimenta la barra del HUD, un tinte rojo, un temblor visual de camara,
efectos en la escena dibujada con Phong y efectos superpuestos de interfaz. No controla
el estado de menu/partida ni altera la simulacion fisica.

La cordura baja de 100 % a 0 % en 60 segundos de tiempo actualizado por el bucle. La pausa
no la consume; al empezar una partida nueva se reinician tanto la cordura como los eventos
visuales. No hay un efecto visual de ojos: el overlay actual dibuja mensajes, glitches, rayos
de escaneo y lluvia de caracteres.

## Implementacion

| Archivo | Responsabilidad |
|---|---|
| `src/gameplay/sanity/Sanity.h` | Logica pura: degradacion, porcentaje, tinte, color de barra y temblor |
| `src/gameplay/sanity/SanityEffects.h` | Generacion y temporizacion de mensajes, glitches y lluvia |
| `src/ui/SanityOverlay.cpp` y `.h` | Dibujo de los efectos superpuestos con listas de dibujo de ImGui |
| `src/ui/RuntimeUi.cpp` | Barra, tinte y condiciones para mostrar el overlay durante el juego |
| `src/app/MainLoop.cpp` | Actualizacion por delta y reinicio al comenzar una partida |
| `src/app/adapters/PlayerAdapters.cpp` | Aplica el temblor como desplazamiento visual del punto mirado |
| `src/app/adapters/RenderAdapters.cpp` | Limita Sanity a la escena de juego compatible y pasa sus valores al renderer |
| `src/rendering/LegacyVgiSceneRenderer.cpp` | Envia los uniforms de cordura al shader Phong |
| `assets/shaders/phong_shdrML.vert` | Deformacion temporal de vertices |
| `assets/shaders/phong_shdrML.frag` | Pixelacion, cuantizacion de color y variacion cromatica |

El estado `sanitySystem` y `sanityEffects` se define en `src/app/AppState.cpp` y se declara
en `src/app/state/PlayerState.h`. La UI recibe referencias mediante `HudContext`; el gameplay
no incluye cabeceras de estado de `app`.

## Ciclo y calculo de cordura

1. En cada fotograma en `GAME_STATE_PLAYING`, si el editor de escena no esta activo,
   `MainLoop` llama a `SanitySystem::Update(delta)` y luego a
   `SanityEffects::Update(delta, Insanity())`.
2. `delta` no positivo (incluido NaN) se descarta y cada actualizacion acepta como maximo
   0,5 s para evitar que un tiron de ventana consuma varios segundos de golpe.
3. El tiempo acumulado se limita a la duracion configurada. `Fraction()` devuelve
   `clamp(1 - elapsed / duration, 0, 1)`: 1 es cordura plena y 0 es cordura agotada.
   `Percent()` lo expresa de 0 a 100 y `Insanity()` invierte la fraccion.
4. Si la duracion configurada es cero o negativa, los calculos usan un minimo de 0,001 s.
   La configuracion predeterminada es `SANITY_DEGRADATION_DURATION = 60.0f`.
5. Al pasar del menu principal a PLAYING se llama a `Reset()` en ambos sistemas. Al salir
   de PLAYING (incluida la pausa) dejan de avanzar.

`Settings` contiene la duracion, la opacidad maxima del tinte rojo, el angulo maximo del
temblor y su velocidad. Son parametros de la logica; no existe un panel de depuracion de
Sanity ni persistencia propia de estos ajustes.

## Efectos

### HUD y pantalla

- **Barra de cordura:** aparece abajo a la derecha, se vacia de derecha a izquierda y cambia
  continuamente de rosa claro a magenta y rojo profundo segun `BarColor()`.
- **Tinte:** un rectangulo rojo de pantalla completa se dibuja detras del HUD con ImGui.
  Su alfa crece con la cordura perdida hasta `tintMaxAlpha` (0,34 por defecto).
- **Temblor:** `CameraShake()` combina ruido determinista de varias senoidales y pequeños
  impulsos. La amplitud crece con la cordura perdida y llega como maximo a 0,09 radianes
  (aprox. 5 grados). El adaptador solo desplaza el punto mirado para la imagen; no modifica
  la posicion del jugador, la camara usada por la fisica, la interaccion ni la direccion
  enviada a otras partes del juego.
- **Overlay:** se dibuja en la lista de primer plano de ImGui cuando la cordura perdida llega
  al 20 %. Incluye mensajes de terror con parpadeo y aberracion cromatica, barras/rayos/
  bloques de glitch, lineas y barrido de escaneo y columnas de caracteres. La frecuencia y
  presencia de estos elementos aumentan con la perdida de cordura.

Los eventos se generan con un `std::mt19937` sembrado al construir `SanityEffects`; sus
posiciones, variantes y duraciones son aleatorias. El ruido visual por fotograma del overlay
usa un hash determinista de la semilla del evento y del paso de tiempo, para que el glitch
cambie a saltos en vez de depender de la frecuencia de refresco. Los eventos caducan por
tiempo y sus vectores tienen limites de cantidad.

| Evento | Comienza con perdida de cordura | Comportamiento |
|---|---:|---|
| Rayos y barrido de escaneo | 25 % | Lineas finas y banda verde que recorre la pantalla |
| Glitches | 30 % | Barras de color, rayos horizontales y bloques pixelados |
| Mensajes | 50 % | Textos breves, aleatorios, con parpadeo y segmentos desplazados |
| Lluvia de caracteres | 55 % | Columnas de simbolos que caen por la pantalla |

Los porcentajes indican el umbral de generacion; la intensidad crece progresivamente hacia
0 % de cordura. El tinte y el temblor no usan estos umbrales de eventos.

### Escena 3D

El renderer pasa `uInsanity` y `uInsanityTime` al shader Phong. En el vertex shader, la
posicion de los vertices recibe ondas y desplazamientos por franjas, con cuantizacion
creciente a medida que baja la cordura. En el fragment shader, las coordenadas de textura se
agrupan en bloques, se reducen los niveles de color y algunas celdas intercambian canales
cromaticos. El tiempo del shader procede del reloj de GLFW, limitado al intervalo 0-1000 s
para conservar una fase numerica manejable.

El renderer pasa cordura cero fuera del juego de escritorio: no aplica esta deformacion en
el editor, la prueba estereo F2 ni OpenXR. El temblor de camara tambien se excluye del editor
y OpenXR. La simulacion de cordura, en cambio, se actualiza durante PLAYING siempre que el
editor no este habilitado.

## Tecnologias, origen y limites

- **C++20:** funciones inline y logica matematica sin dependencias de OpenGL o ImGui en
  `Sanity.h` y `SanityEffects.h`.
- **Dear ImGui:** dibuja HUD y overlay mediante `ImDrawList`; no crea ventanas ni captura
  entrada.
- **GLSL 330 core / OpenGL:** integra los cambios de geometria y color en los shaders Phong
  existentes del proyecto.
- **No usa Bullet, audio, ENet ni recursos multimedia externos** para los efectos de Sanity.
  No hay un repositorio externo especifico del que dependa su algoritmo; emplea formulas
  matematicas, ruido senoidal y hash implementados en el propio codigo.
- Es un efecto de cliente. No cambia la autoridad del host, la fisica, el protocolo de red,
  los identificadores de objetos ni el estado de otros jugadores.

## Pruebas y validacion

No hay pruebas automatizadas especificas de Sanity identificadas. Una compilacion confirma
integracion, pero no sustituye la comprobacion manual del comportamiento visual ni de sus
limites en editor, F2 y OpenXR.
