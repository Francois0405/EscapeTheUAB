# Cambios FPS y escenas JSON

## 1. Resumen

Se incorporó giro FPS continuo con ratón, pausa, captura del cursor y compatibilidad con los controles académicos de cámara. También se añadió lectura estricta de escenas JSON versión 1 y una habitación JSON equivalente al TXT actual. Los dos formatos llegan a `SceneObject` y al renderizador existente.

## 2. Archivos modificados

| Ruta | Funciones o elementos | Cambio y motivo |
|---|---|---|
| `EntornVGI/main.cpp` | `modoJuegoFPS`, `fijarCapturaCursor`, `sincronizarCursorFPS`, `OnWindowFocus`, `OnKeyDown`, `OnMouseMove`, `OnMouseButton`, `OnMouseWheel`, `cargarYAplicarEscena`, `MostraEntornVGIWindow`, `ShowArxiusOptions`, `main` | Añade controles FPS, gestión del foco y cursor, selector de formato, carga transaccional de modelos y salida por menú. |
| `EntornVGI/SceneLoader.h` | `SceneObject`, `CargarEscenaJSON` | Añade identificador y categoría a cada instancia y declara el lector JSON estricto; conserva `CargarEscena`. |
| `EntornVGI/SceneLoader.cpp` | `CargarEscena`, `CargarEscenaJSON`, `leerVector3` | Mantiene el lector TXT y añade comprobación de versión, campos, vectores, finitud, IDs y rutas OBJ. |
| `EntornVGI/assets/scenes/habitacion_beta.json` | Datos de escena | Describe las 3 instancias que ya tenía la rama `carlos`, con las mismas transformaciones del TXT. |
| `EntornVGI/assets/scenes/habitacion_beta.txt` | Datos originales de Carlos | Se conserva sin cambios y se usa como referencia para las tres instancias del JSON. |
| `EntornVGI/third_party/nlohmann/json.hpp` | nlohmann/json 3.11.3 | Añade la distribución de encabezado único con licencia MIT; evita dependencia de descarga durante compilación. |
| `EntornVGI/EntornVGI.vcxproj`, `EntornVGI/EntornVGI.vcxproj.filters` | Configuración del proyecto | Incluye el encabezado de nlohmann/json en el proyecto de Visual Studio. |

## 3. Funcionamiento de la cámara

El modo FPS reutiliza la cámara de navegación del entorno: `modoJuegoFPS()` es verdadero cuando `camera == CAM_NAVEGA` y `projeccio == PERSPECT`. Así se conservan el cálculo de vista, el movimiento WASD y los brazos dibujados por `dibuixa_Escena()`.

El recorrido del ratón es:

1. GLFW genera movimiento y llama a `OnMouseMove`.
2. Si el modo FPS está activo, `OnMouseMove` calcula la diferencia con la posición anterior, rota el vector de mirada en el plano XY y llama a `InclinaNavega` para el ángulo vertical.
3. `Teclat_Navega` procesa las teclas WASD y aplica el desplazamiento según el tiempo del fotograma.
4. Tras `glfwPollEvents`, el bucle principal llama a `sincronizarCursorFPS` para capturar el cursor solo si el juego está activo, sin pausa y con foco.

```cpp
fpsTienePosicionAnterior = false;
glfwSetInputMode(window, GLFW_CURSOR, capturar ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
```

La captura con `GLFW_CURSOR_DISABLED` evita que el giro dependa de mantener un botón. ESC alterna pausa y reanudación; en pausa se libera el cursor. Ctrl+Shift+M alterna el estado solicitado del cursor. Al perder el foco se libera y se envían eventos de liberación para WASD. La primera posición después de cada cambio se guarda sin aplicar delta, evitando un salto.

`ImGui_ImplGlfw_InitForOpenGL(window, true)` instala los callbacks de ImGui y encadena los callbacks de la aplicación. Por eso el callback propio ya no llama también a `io.AddMouseButtonEvent`: ImGui recibe el clic una sola vez. Durante FPS, los callbacks propios de botón y rueda no ejecutan acciones antiguas de cámara.

## 4. Funcionamiento de JSON

`ShowArxiusOptions()` permite escoger el TXT o el JSON. `CargarEscenaJSON()` abre el archivo con `std::ifstream`, lo analiza con `nlohmann::json` y valida primero la raíz y los campos obligatorios. Cada entrada debe tener `id`, `model`, `position`, `rotation` y `scale`. Las listas de transformación deben tener tres números finitos; los IDs no se pueden repetir y las rutas se resuelven desde la carpeta de la escena.

```json
{
  "version": 1,
  "name": "habitacion_beta",
  "architecture": [],
  "objects": []
}
```

Los elementos validados se convierten en `SceneObject`. `cargarYAplicarEscena()` resuelve cada ruta OBJ, carga cada archivo una sola vez por ruta absoluta y construye colecciones temporales. Solo después de cargar correctamente toda la escena sustituye `objetosEscena` y `modelosHabitacion`; si hay error, conserva la escena anterior.

`dibuixa_Escena()` usa las mismas instancias para ambos formatos. Aplica traslación y escala con GLM, y las rotaciones en grados mediante `glm::radians`, manteniendo el eje Z vertical del proyecto. `architecture` y `objects` clasifican las instancias para futuras propiedades; todavía no hay colisiones ni puzles.

La dependencia se integra con el encabezado único `third_party/nlohmann/json.hpp` versión 3.11.3. Visual Studio la muestra en el proyecto y el código la incluye directamente, por lo que no se necesita instalar paquetes externos.

## 5. Código del profesor frente a código nuestro

Se reutilizaron `OnCameraNavega`, `Teclat_Navega`, `InclinaNavega`, `dibuixa_Escena`, `dibuixa_EscenaGL`, `COBJModel::LoadModel` y el renderizado de brazos existente. Los controles originales con botones siguen en su ruta cuando no está activo el modo FPS.

Son nuevas `CargarEscenaJSON`, `leerVector3`, `cargarYAplicarEscena`, `modoJuegoFPS`, `fijarCapturaCursor`, `sincronizarCursorFPS` y la lógica FPS de `OnKeyDown`, `OnMouseMove` y `OnWindowFocus`. `CargarEscena` sigue siendo el lector TXT existente, con IDs técnicos asignados al convertir cada fila.

## 6. Pruebas

### Ejecutadas y superadas

- En `dev`, la comprobación estructural Python confirmó que JSON y TXT contienen las mismas 3 rutas y transformaciones; los IDs son únicos y los valores finitos.
- En `dev`, `git diff --check` terminó sin errores.
- En la rama anterior `feature/fps-controls-json-scenes`, MSBuild compiló `Debug|x64` con 0 errores y un smoke test de `SceneLoader.cpp` verificó una escena de 12 instancias y casos inválidos. Esos resultados no corresponden a la base `carlos` de esta rama `dev`.

### Fallidas y correcciones

- No hubo pruebas fallidas en `dev`.

### No ejecutadas

- No se pudo compilar `dev`: MSBuild y `cl.exe` no están disponibles en el entorno actual. La compilación de la rama anterior no sustituye esta comprobación.
- No se realizó una sesión gráfica interactiva para comprobar ratón, WASD, pausa, foco, menús y brazos.
- Los casos de escenas inválidas y el conteo de llamadas a `LoadModel` no se repitieron sobre `dev`.
## 7. Defensa universitaria

**1. ¿Por qué se desactiva el cursor en el modo FPS?**

`GLFW_CURSOR_DISABLED` oculta el puntero y permite recibir movimiento relativo sin mantener pulsado un botón. Cuando hay pausa, se pierde el foco o el usuario alterna la captura, se vuelve al cursor normal.

**2. ¿Cómo se evita el salto al recuperar el ratón?**

`fijarCapturaCursor()` invalida `fpsTienePosicionAnterior`. El primer `OnMouseMove()` guarda la nueva posición y no rota; los deltas se aplican a partir del segundo evento.

**3. ¿Cómo puede ImGui recibir clics si también hay callbacks de la aplicación?**

El backend GLFW de ImGui se inicializa con el encadenamiento de callbacks habilitado. La aplicación conserva su callback, pero no vuelve a inyectar el botón con `AddMouseButtonEvent`, evitando duplicados.

**4. ¿Por qué no se borra la habitación actual al comenzar a leer otra?**

`CargarEscenaJSON()` construye una lista candidata y solo la publica al terminar la validación. `cargarYAplicarEscena()` carga también todos los modelos temporalmente y sustituye las colecciones globales al final. Un error devuelve antes de ese intercambio.

**5. ¿Cómo se evita cargar varias veces el mismo OBJ y cómo se aplican los grados?**

`cargarYAplicarEscena()` guarda los modelos en un mapa cuya clave es la ruta absoluta; solo llama a `LoadModel` si no existe esa clave. Cada instancia conserva su transformación y `dibuixa_Escena()` convierte los grados a radianes con `glm::radians` antes de rotar.
