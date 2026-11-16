# Animaciones desde Blender (GLB)

Idea: **se diseña en Blender, se exporta a `.glb` y el juego reproduce lo que trae el archivo.**
No se escriben keyframes en C++.

```
Blender (.blend)  --exportar_glb.py-->  EntornVGI/assets/animations/Objeto.glb  --ModeloGLB.cpp-->  juego
```

## Cómo se reproduce (resumen del código)

| Archivo | Para qué sirve |
|---|---|
| `third_party/cgltf/cgltf.h` | Biblioteca que lee el `.glb` (MIT, sin modificar). |
| `EntornVGI/AnimacionGLB.h` | Matemática de keyframes: interpolación (`glm::mix` / `glm::slerp`) y `ReproductorAnimacion`. |
| `EntornVGI/ModeloGLB.h/.cpp` | Carga el `.glb` (nodos, mallas, animaciones), aplica la pose de un instante y calcula `mundo(hijo) = mundo(padre) * local(hijo)`. |
| `EntornVGI/ObjetosAnimadosGLB.h/.cpp` | Lista de instancias: cada una con su posición, su pose y su reloj; los modelos se comparten. |
| `EntornVGI/DibujoGLB.h/.cpp` | Sube las mallas a OpenGL y las dibuja con el shader de la escena. |
| `tests/GlbTests.cpp` | Pruebas: keyframes, interpolación, jerarquía, pivote y carga (`ctest`). |
| `tools/exportar_glb.py` | Exporta desde Blender con los ajustes correctos. |

Función que actualiza los keyframes: `glb::AplicarAnimacion(pose, animacion, tiempo)`.
Función que avanza el tiempo de todas las instancias: `glb::ActualizarObjetos(escena, delta)`.

## Receta para un cajón

### 1. En Blender

1. Modela el cajón. **Una unidad = 1 metro** y **Z es arriba** (como en el juego).
2. **Pivote**: el origen del objeto es lo que se mueve o gira. En un cajón, ponlo donde quieras que
   "empiece" el movimiento (por ejemplo en el centro de la cara frontal).
   `Object > Set Origin > Origin to 3D Cursor` (antes coloca el cursor 3D con `Shift+C`/`Shift+S`).
3. **Jerarquía**: si el cajón lleva tirador, haz que el tirador sea hijo del cajón (`Ctrl+P`).
   Los hijos siguen al padre sin código adicional. El mueble puede ser el padre del cajón.
4. **Animación**: selecciona SOLO el objeto que se mueve (el cajón), ve al fotograma 1, pulsa `I`
   (posición), ve al fotograma final, desplázalo y pulsa `I` otra vez.
   - 24 fotogramas por segundo. Un cajón que sale en medio segundo son 12 fotogramas.
   - Pon un nombre claro a la *Action* (por ejemplo `AbrirCajon`).
5. **Objetos auxiliares** que no deben verse en el juego (huecos, guías): ponles el prefijo `CTRL_`
   y desactiva *Hide in Render* (el icono de la cámara en el Outliner). El exportador los omite.
6. **No uses `Apply > Scale/Rotation`** en objetos animados: perderías el pivote.

### 2. Exportar

Desde la raíz del repositorio (ajusta la ruta de Blender en Windows/Linux):

```bash
/Applications/Blender.app/Contents/MacOS/Blender -b Cajon.blend \
  --python tools/exportar_glb.py -- EntornVGI/assets/animations/Cajon.glb
```

Si prefieres hacerlo a mano: `File > Export > glTF 2.0`, formato **glTF Binary (.glb)**,
**+Y Up** activado, *Renderable Objects* activado, *Animation* activado con modo *Actions*
y **Always Sample Animations activado**. Esta última casilla es importante: convierte las curvas
Bezier de Blender en muestras lineales que el juego sabe leer. Sin ella el cargador responde con
un error que lo explica.

### 3. En el juego

Cada puerta o cajón es una **instancia** declarada en la lista `"animated"` del JSON de la
habitación (ver `docs/FISICAS_BULLET.md`). Añadir uno es **un bloque JSON**, sin código nuevo:

```json
{ "id": "cajon_1", "model": "../animations/Cajon.glb", "position": [2, 1, -1.7], "rotation": [0, 0, 0] }
//   id único        archivo .glb                        posición en la habitación   giro en Z (grados)
```

- Si dos instancias usan el mismo `.glb` (dos puertas), el archivo se carga **una sola vez**, pero cada
  instancia tiene su propia pose y su propio reloj: se abren de forma independiente.
- Con **F1** aparece un botón "Abrir" por cada instancia (solo para probar).
- No hay que tocar `main.cpp`, `ModeloGLB`, `AnimacionGLB`, `DibujoGLB` ni `ObjetosAnimadosGLB`.

## Abrir y cerrar con E

Cada instancia tiene un estado (`glb::EstadoPuerta`) y se mueve con su propio reloj:

```
Cerrada --E--> Abriendo --(llega al final)--> Abierta --E--> Cerrando --(llega al principio)--> Cerrada
```

- **Convención al diseñar en Blender:** el PRIMER fotograma de la animación es la puerta CERRADA y el
  ÚLTIMO es la puerta ABIERTA. Cerrar es reproducir esa misma animación hacia atrás: no hace falta
  una segunda animación.
- **E mientras se mueve** invierte el sentido desde el instante actual, sin saltos.
- **Qué puerta se activa:** la que está a menos de 2 m (`RADIO_INTERACCION`) y delante de la mirada
  (a menos de unos 45 grados, `COSENO_MINIMO_INTERACCION`). Si hay varias, gana la más centrada en
  la mirada y solo se activa esa.
- **No depende de la cámara:** `InteractuarConObjeto(escena, posicion, direccion)` solo recibe una
  posición y una dirección del mundo. En `main.cpp`, `interactuarConPuertaDelJugador()` es el único
  sitio que las saca de la cámara FPS; con VR se llamaría con las del mando.
- **E se ignora** si el juego está en pausa, la ventana no tiene el foco o se escribe en un cuadro de
  texto de ImGui (`PuedeInteractuar`).
- Con **F1** siguen los botones de depuración: "Abrir" reinicia la apertura y "Alternar" hace lo mismo
  que E sobre esa puerta.

## Coordenadas (por qué sale bien)

- Blender y nuestro juego: **Z arriba**, metros.
- glTF estándar: **Y arriba**. Blender convierte al exportar: `(x, y, z)` pasa a `(x, z, -y)`.
- El juego deshace el cambio con **una** matriz, `glb::MatrizYArribaAZArriba()` (giro de +90° en X):
  `(X, Y, Z)` pasa a `(X, -Z, Y)`. Se aplica una vez al dibujar, no a cada nodo.
- `matrizBase` es la posición y orientación del modelo dentro de la habitación (ya en Z arriba).

## Límites de esta primera versión

Se admiten posición, rotación y escala animadas con interpolación lineal o escalón. No hay
esqueletos, *morph targets*, cámaras ni luces animadas, ni texturas (solo el color base del material).
Si un `.glb` trae algo de esto, `ModeloGLB::avisos` lo indica en la consola.

## Ejercicio: tu primer cajón

1. En Blender, un cubo escalado a (0.4, 0.5, 0.2) m con el origen en su cara trasera.
2. Keyframes de posición: en el fotograma 1 en (0, 0, 0) y en el 12 en (0, -0.4, 0).
3. Exporta con `tools/exportar_glb.py` a `EntornVGI/assets/animations/Cajon.glb`.
4. Añade un bloque a la lista `"animated"` del JSON de la habitación con su `.glb`, posición y giro.
5. Comprueba con `ctest` y abriendo el cajón desde el botón.
6. Reto: duplica la animación para que, en el fotograma 24, el cajón vuelva a cerrarse.
   ¿Qué cambia en C++? (Respuesta: nada. La duración viene del archivo.)
