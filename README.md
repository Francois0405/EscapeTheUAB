# EscapeTheUAB — EntornVGI

Proyecto académico de visualización gráfica 3D basado en el entorno del profesor.
Utiliza **C++20, OpenGL, GLFW, GLEW, GLM, SOIL2, Dear ImGui**, carga de
modelos **OBJ/MTL** y escenas en **JSON**. Se conserva la solución original de
Visual Studio y se utiliza **CMake** para las compilaciones multiplataforma.

## Inicio rápido — una sola orden

Después de instalar los requisitos de tu sistema **una vez**, desde la raíz
del repositorio (la carpeta que contiene `CMakePresets.json`):

| Sistema | Compilar y ejecutar | Compilar sin abrir | Release |
| --- | --- | --- | --- |
| **Windows** | Doble clic en `run-windows.cmd` o `./run-windows.cmd` en PowerShell | `./run-windows.cmd -NoRun` | `./run-windows.cmd -Configuration Release` |
| **Linux** | `./run-linux.sh` | `./run-linux.sh --no-run` | `./run-linux.sh --release` |
| **macOS** | `./run-macos.sh` | `./run-macos.sh --no-run` | `./run-macos.sh --release` |

En Windows, el lanzador localiza Visual Studio y vcpkg, inicializa **MSVC x64**,
configura el preset, compila y abre `build/windows-debug/bin/EntornVGI.exe`.
**Si la compilación falla, no ejecuta un EXE antiguo.** No es necesario
configurar `$env:INCLUDE`, `$env:NINJA` o `$env:VCPKG_ROOT` manualmente en cada
terminal, siempre que las herramientas estén instaladas en una ruta detectable.
Se respeta `VCPKG_ROOT` si ya está definido. CMake reutiliza los resultados de
compilación: los cambios posteriores se compilan incrementalmente.

En Linux y macOS se usa el compilador de CMake, Ninja y los presets del sistema.
Los scripts **no instalan paquetes, no actualizan Git, no cambian de rama y no
hacen commits ni merges**. Ejecutan lo que tengas actualmente en tu copia local.

Si los lanzadores `.sh` no son ejecutables tras descomprimir, usa
`bash run-linux.sh` o `bash run-macos.sh`, según el sistema.

## Requisitos por plataforma

### Windows 10/11 — Visual Studio con C++

Instala Visual Studio 2026 con los componentes **Desarrollo de escritorio con
C++**, **MSVC x64**, **Windows SDK**, **CMake** y **Ninja**. El proyecto utiliza
`vcpkg.json` para gestionar GLFW, GLEW, GLM y Native File Dialog Extended.

En el equipo donde se preparó el lanzador se detecta Visual Studio en
`D:\Visual Studio` y vcpkg en `D:\Visual Studio\VC\vcpkg`. También se
intenta localizar Visual Studio mediante `vswhere` y vcpkg con `VCPKG_ROOT`
o en `~/vcpkg`. No es obligatorio utilizar esas rutas concretas.

Desde PowerShell:

```powershell
cd F:\EscapeTheUAB\EscapeTheUAB  # cambia la ruta si clonaste el repo en otro sitio
.\run-windows.cmd
```

Para compilar únicamente:

```powershell
.\run-windows.cmd -NoRun
```

En Visual Studio también puedes abrir la raíz como proyecto CMake y seleccionar
`windows-debug` o `windows-release`. Se conserva `EntornVGI.slnx` y el proyecto
`.vcxproj` original, pero **no debes confundir su salida con la de CMake**:
son compilaciones distintas.

### Linux (Ubuntu / derivados)

Instala las dependencias una sola vez:

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build pkg-config \
    libgl1-mesa-dev libglfw3-dev libglew-dev libglm-dev libgtk-3-dev
```

Después, desde la raíz del repositorio:

```bash
./run-linux.sh
# o: ./run-linux.sh --release
```

El preset Linux usa GCC/Clang y las bibliotecas de la distribución, incluido
GTK3 para los diálogos de archivos. Necesitas una sesión gráfica activa y un
driver que soporte OpenGL 3.3 o superior. No hace falta vcpkg para este preset.

### macOS (Apple Silicon o Intel)

Instala una sola vez las herramientas de Xcode, Homebrew, CMake, Ninja y vcpkg:

```bash
xcode-select --install
brew install cmake ninja
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh"
```

Después:

```bash
./run-macos.sh
# o: ./run-macos.sh --release
```

El lanzador detecta `~/vcpkg` automáticamente. Si lo has instalado en otra
carpeta, define `export VCPKG_ROOT="/ruta/a/vcpkg"` una sola vez en tu perfil de
shell o antes de ejecutar el script. El contexto OpenGL solicitado en macOS es
**4.1 Core (forward-compatible)** y se usan shaders GLSL 330. Algunas rutas
antiguas del código del profesor no admiten OpenGL Core y deben probarse.
OpenGL está deprecado por Apple: el soporte macOS requiere validación real.

## Estructura del proyecto

```text
CMakeLists.txt                Compilación común
CMakePresets.json             Presets para Windows, Linux y macOS
vcpkg.json                    Dependencias Windows/macOS
run-windows.cmd/.ps1          Lanzador Windows: MSVC + CMake + ejecutar
run-linux.sh                  Lanzador Linux
run-macos.sh                  Lanzador macOS
EntornVGI/main.cpp            Entrada, renderizado y controles
EntornVGI/SceneLoader.*       Lectura de escenas JSON
EntornVGI/objLoader.*         Importador OBJ/MTL del profesor
EntornVGI/assets/models/     Modelos y materiales
EntornVGI/assets/scenes/     Escenas JSON
EntornVGI/shaders/           Shaders GLSL
include/ImGui/                Dear ImGui
third_party/SOIL2/           Carga de texturas
third_party/nativefiledialog/ Backend GTK3 para Linux
lib/                          Bibliotecas Windows heredadas
build/<preset>/bin/           Ejecutables generados por CMake
```

Los recursos (`assets/`, `shaders/` y `textures/`) se copian junto al ejecutable
**tras compilar**. El programa busca automáticamente la raíz de recursos. Al
importar un OBJ con `.mtl`, las texturas deben referenciarse con nombres o
rutas **relativas** (por ejemplo `map_Kd Phone_BaseColor.png`), nunca con una
ruta `C:\...` o `D:\...`: Linux y macOS no conocen esas rutas.

La habitación de prueba está en `EntornVGI/assets/scenes/habitacion_beta.json`,
con sus instancias y transformaciones. El modelo visible en primera persona
se configura en el código de la versión local que estés ejecutando; un modelo
OBJ no aporta por sí solo animaciones esqueléticas ni emite luz. No confundas
una sustitución de modelo con una integración ya comprobada de la iluminación.

## Qué hacer cuando algo no funciona

**Windows: `fatal error C1083: Cannot open include file: float.h`.**
Ocurre si `cl.exe` se ejecuta fuera del entorno MSVC o falta el Windows SDK.
Usa `run-windows.cmd`: inicializa automáticamente
`Launch-VsDevShell.ps1`. El hecho de que `float.h` no aparezca dentro de la
carpeta de MSVC no implica un error; suele estar en la UCRT del Windows SDK.
Si persiste, verifica la instalación de **Desktop development with C++** y SDK.

**Los cambios no aparecen y se siguen viendo modelos antiguos.**
Comprueba que el lanzador terminó con éxito y no ejecutaste un EXE antiguo.
El ejecutable CMake Debug es `build/windows-debug/bin/EntornVGI.exe` en Windows;
el de Linux/macOS está bajo el directorio de su preset. La solución `.slnx`
puede producir ejecutables en otra ubicación. Si has modificado un modelo o
una textura, recompila mediante el lanzador para volver a copiar recursos.

**No carga una textura / un OBJ.** Confirma que el `.obj` referencia un
`mtllib` válido, que `.mtl` tiene `map_Kd` con ruta relativa y que los archivos
están presentes en `assets/models/` (o en sus subcarpetas). El cargador OBJ
actual utiliza `map_Kd` para color; un mapa PBR `map_Ns` de rugosidad **no es
una garantía de soporte de roughness**. Revisa los mensajes de consola.

**Linux/macOS no compilan o no arrancan.** Guarda el registro completo con:

```bash
bash run-linux.sh --no-run 2>&1 | tee build-linux.log
# macOS: bash run-macos.sh --no-run 2>&1 | tee build-macos.log
```

Adjunta el primer `error:` o `fatal error:` de la compilación, no solo los
warnings heredados. Para problemas gráficos, incluye la salida de ejecución,
GPU, driver y versión de OpenGL.

## Validación y limitaciones

- **Windows:** en noviembre de 2026 se verificó que el preset `windows-debug`
  configura y compila con MSVC, Ninja y vcpkg y el programa arranca con la
  habitación de 12 instancias. El nuevo lanzador debe probarse en un Windows
  real después de incorporarlo al repositorio.
- **Linux:** la documentación anterior registra configuración, compilación y
  arranque de Debug/Release en Ubuntu con GCC y NVIDIA GTX 1650/OpenGL 3.3;
  no todas las interacciones, texturas o diálogos quedaron validados.
- **macOS:** está preparado el preset, pero requiere compilación y prueba
  reales con Apple Clang/vcpkg; no se puede declarar verificado a partir de
  una compilación de Windows o Linux.
- El código académico conserva funciones OpenGL heredadas: algunas pueden
  fallar bajo un contexto Core. No se ha implementado aquí Bullet Physics ni
  OpenXR; son posibles ampliaciones posteriores.

`ModelLoader.cpp` no participa en la compilación actual: necesita Assimp y
no estaba incluido en el proyecto original. `SceneLoader` y el cargador propio
OBJ/MTL son la ruta actualmente utilizada. El repositorio ignora salidas CMake,
artefactos temporales, ImGui y archivos generados. Las rutas de assets y las
escenas deben conservarse en control de versiones cuando sus licencias lo
permitan.

## Contribuciones del equipo

Antes de incorporar los cambios, ejecuta `git status -sb` y confirma qué rama
estás utilizando. Respeta el trabajo de tus compañeros; no uses `git reset
--hard`, `git clean` ni copies un `main.cpp` antiguo sobre cambios nuevos.
Mantén `master` para versiones comprobadas y desarrolla funcionalidades en la
rama de trabajo acordada con el grupo; solicita revisión antes del merge.
Los scripts de ejecución no hacen ninguna operación Git.
