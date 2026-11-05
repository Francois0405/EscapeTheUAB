# EscapeTheUAB: controles FPS y escenas JSON

## Formato de escena

Las habitaciones se definen únicamente en `EntornVGI/assets/scenes/*.json`. La escena inicial `habitacion_beta.json` contiene 12 instancias: seis en `architecture` (suelo, techo y cuatro paredes) y seis en `objects` (armario, dos mesas, lámpara y dos cajas). Cada instancia especifica `id`, `model`, `position`, `rotation` y `scale`. Las rotaciones están en grados.

`SceneObject` es el formato interno sencillo que conecta el archivo con el renderizador: guarda el identificador, la categoría, el modelo y tres vectores GLM. El lector no cambia el sistema de coordenadas ni las matrices OpenGL existentes.

## Módulos principales

| Archivo y líneas | Función o elemento | Responsabilidad |
|---|---|---|
| `EntornVGI/SceneLoader.h:8-25` | `SceneObject`, `CargarEscenaJSON` | Define los datos de una instancia y la única interfaz de lectura de escenas. |
| `EntornVGI/SceneLoader.cpp:13-38` | `leerVector3` | Comprueba que cada transformación tenga tres números finitos dentro del rango de `float`. |
| `EntornVGI/SceneLoader.cpp:42-164` | `CargarEscenaJSON` | Valida versión, nombre, colecciones, campos, IDs, transformaciones y existencia de cada OBJ antes de entregar la escena completa. |
| `EntornVGI/main.cpp:138-200` | `cargarYAplicarEscena` | Carga exclusivamente JSON y prepara las instancias y modelos antes de sustituir la habitación activa. |
| `EntornVGI/main.cpp:56-90` | `fijarCapturaCursor`, `sincronizarCursorFPS` | Captura el cursor durante el juego y lo libera al pausar, mostrar paneles o perder el foco. |
| `EntornVGI/main.cpp:93-135` | `inclinarJugadorFPS`, `moverJugadorFPS` | Controla la inclinación y el desplazamiento FPS ligados al movimiento del ratón y al tiempo del fotograma. |
| `EntornVGI/main.cpp:919` | `dibuixa_Escena` | Dibuja las instancias con las transformaciones y el renderizado OpenGL del proyecto, y conserva los brazos FPS. |
| `EntornVGI/main.cpp:1218` | `draw_Menu_ImGui` | Mantiene los paneles académicos fuera del modo juego; en juego muestra solo la interfaz prevista para ese modo. |
| `EntornVGI/main.cpp:4036` | `OnKeyDown` | F1 alterna los paneles VGI y sincroniza el cursor; ESC pausa y reanuda. |
| `EntornVGI/main.cpp:6914` | `main` | Crea la ventana, fija ImGui a escala 1.0 y carga al inicio `habitacion_beta.json`. |
| `EntornVGI/EntornVGI.vcxproj:74,203-205` | Configuración de Visual Studio | Usa la carpeta del proyecto como directorio de trabajo y copia `glfw3.dll` y `glew32.dll` junto al ejecutable x64. |

### Lectura y renderizado

El documento tiene versión `1`, un nombre y dos listas independientes, `architecture` y `objects`. `CargarEscenaJSON` recorre ambas, asigna la categoría según la lista y rechaza el documento completo si encuentra un error. Las rutas de modelos se resuelven desde la carpeta del JSON. `cargarYAplicarEscena` carga los modelos y solo publica la nueva habitación cuando los datos necesarios están disponibles.

El renderizador sigue usando `SceneObject`, GLM y OpenGL. Las rotaciones del JSON se convierten de grados a radianes en el punto de dibujo. No se cambiaron las escalas de los modelos OBJ ni las matrices GLM para corregir la interfaz.

### Juego e interfaz

El modo de juego inicia en perspectiva con la habitación completa y los brazos. WASD mueve al jugador y el ratón cambia la dirección de la cámara. ESC pausa o reanuda el juego. F1 muestra u oculta los paneles de depuración VGI; al mostrarlos se libera el cursor y al ocultarlos se recupera la captura si el juego está activo.

ImGui empieza en escala 1.0 (`FontGlobalScale`) y no se llama a `ScaleAllSizes`, evitando escalar dos veces. `imgui.ini` se dejó vacío para que ImGui genere posiciones y tamaños iniciales; la copia `imgui.ini.before-scale-reset` conserva el estado anterior.

### Modelos y DLL

Los OBJ y MTL de la habitación y de los brazos, además de `arms_01.png`, están en `EntornVGI/assets/models`. Las dependencias de enlace x64 están en `lib/x64`; las DLL de GLFW y GLEW también se guardan ahí y el proyecto las copia al directorio del ejecutable después de compilar. La configuración no contiene rutas absolutas ligadas a una cuenta de usuario. Visual Studio usa `EntornVGI` como directorio de trabajo para resolver las rutas relativas de escenas, modelos, texturas y shaders.

## Validación

### Materiales de color y texturas de imagen

Un material OBJ/MTL puede definir propiedades de iluminación y color (por ejemplo `Kd`) sin asignar una imagen. `test.mtl` tiene un material de este tipo y no declara `map_Kd`; por tanto, el modelo debe dibujarse usando el color/material y no una muestra de imagen. La linterna FPS usa `flashlight.mtl` y `flashlight.png`; los brazos siguen disponibles como asset, pero permanecen ocultos durante la exploración FPS.

El booleano `textur` enviado a `dibuixa_EscenaGL()` controla el uniform del shader: cuando vale `true`, los fragment shaders muestrean `texture0`; cuando vale `false`, entregan el color calculado por el material. La bandera global `textura` se activa al cargar el JSON y no indica que cada OBJ de la habitación tenga una imagen. Si se deja activa para todos, un modelo sin mapa puede muestrear la textura que haya quedado enlazada y teñirse. Por eso las instancias de la habitación se dibujan con `false`, el objeto OBJ seleccionado conserva su ajuste y la linterna fuerza temporalmente `true` durante su dibujo.

Una mejora futura, si hacen falta imágenes distintas por OBJ, es guardar por modelo/material la presencia e identificador de su mapa y seleccionar y enlazar esa textura al dibujar cada material. Esta corrección no introduce ese sistema.

### Ejecutada en esta integración

- MSBuild 18.10.1 compiló `EntornVGI.slnx` en `Debug|x64`; el ejecutable se generó correctamente.
- Tras compilar, `x64/Debug` contiene `glfw3.dll` y `glew32.dll`.
- La comprobación estructural de `habitacion_beta.json` encontró 6 elementos `architecture`, 6 `objects`, 12 IDs únicos y ningún modelo OBJ ausente.
- La búsqueda en el código de llamadas `CargarEscena(`, `habitacion_beta.txt`, `formatoJSON` y opciones de menú TXT no encontró coincidencias.

### Pendiente

- No se abrió la aplicación para comprobar visualmente habitación, materiales, brazos, tamaño de ImGui ni comportamiento interactivo de ratón, WASD, F1 y ESC.
- No se probaron configuraciones Release o Win32 ni la ejecución desde otra carpeta de trabajo.

La revisión de código confirma que F1 alterna `mostrarPanelesDebug`, hace visible la ventana académica principal al abrirlos y llama a `sincronizarCursorFPS()`. El menú omite los paneles académicos del modo juego cuando la bandera está desactivada; al cerrarlos, la captura del cursor se recupera si sigue en modo juego activo. La interacción real de F1/cursor sigue pendiente de prueba en ejecución.

La compilación y las comprobaciones estructurales no sustituyen una prueba gráfica en ejecución.
