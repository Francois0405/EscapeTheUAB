# EntornVGI

EntornVGI es un entorno grafico 3D docente basado en C++20, OpenGL, GLFW,
GLEW, GLM, SOIL2, Dear ImGui, Native File Dialog y un cargador OBJ/MTL propio.
El codigo fuente y los recursos son comunes a Windows, Linux y macOS. El
proyecto Visual Studio existente se conserva; CMake es el sistema de
compilacion comun recomendado.

## Requisitos

- CMake 3.25 o posterior y Ninja.
- Compilador C++20: MSVC (Visual Studio 2026), GCC/Clang o Apple Clang.
- OpenGL y un driver que soporte al menos OpenGL 3.3; macOS usa OpenGL 4.1
  Core, el maximo disponible en esa plataforma.
- vcpkg en Windows/macOS. Linux usa paquetes de la distribucion para GLFW,
  GLEW y GLM.

SOIL2 y Dear ImGui se compilan desde fuentes conservadas en el repositorio.
GLFW, GLEW, GLM y Native File Dialog Extended se resuelven mediante el
manifiesto `vcpkg.json` en Windows/macOS. El preset Linux usa los paquetes del
sistema y el backend GTK3 de Native File Dialog incluido en `third_party/`.
No se enlazan las bibliotecas `.lib` antiguas fuera de Windows.

## Linux (Ubuntu)

Instala el toolchain, Ninja, OpenGL, GLFW, GLEW, GLM y GTK3:

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build pkg-config \
    libgl1-mesa-dev libglfw3-dev libglew-dev libglm-dev libgtk-3-dev
```

Desde la raiz del repositorio:
   
```bash
cmake --preset linux-debug
cmake --build --preset linux-debug --parallel
./build/linux-debug/bin/EntornVGI
```

Para Release:

```bash
cmake --preset linux-release
cmake --build --preset linux-release --parallel
./build/linux-release/bin/EntornVGI
```

El backend Linux de dialogos necesita GTK3 y una sesion grafica activa. CMake
copia `assets/`, `shaders/` y `textures/` al directorio del ejecutable; al
arrancar, el programa localiza la raiz de recursos aun si el directorio actual
es otro.

## Windows 10/11 (Visual Studio Community 2026)

1. Instala Visual Studio 2026 con **Desktop development with C++**, CMake y
   Ninja.
2. Clona e inicializa vcpkg siguiendo su guia oficial; define `VCPKG_ROOT` en
   el entorno antes de abrir Visual Studio.
3. Abre la carpeta raiz del repositorio en Visual Studio. Selecciona
   `windows-debug` o `windows-release` en los presets de CMake y compila el
   destino `EntornVGI`.

Los presets usan MSVC y el manifiesto instala GLFW, GLEW, GLM y Native File
Dialog Extended. Para construir desde Developer PowerShell de Visual Studio,
define las rutas que usan los presets y compila:

```powershell
$env:VCPKG_ROOT = 'D:\vcpkg'
$env:NINJA = 'D:\Visual Studio\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
cmake --preset windows-debug
cmake --build --preset windows-debug --parallel
.\build\windows-debug\bin\EntornVGI.exe
```

Sustituye `D:\vcpkg` por la ruta donde inicializaste vcpkg. Si CMake no está
en `PATH`, invoca `cmake.exe` desde la instalación de Visual Studio.

El proyecto anterior `EntornVGI.slnx` y `EntornVGI/EntornVGI.vcxproj` se
mantiene disponible para usuarios que necesiten el flujo heredado.

## macOS (Apple Silicon e Intel)

Instala Xcode Command Line Tools, Homebrew, CMake y Ninja. Inicializa vcpkg una
vez y define `VCPKG_ROOT` en la terminal desde la que vayas a configurar el
proyecto:

```bash
xcode-select --install
brew install cmake ninja
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh"
export VCPKG_ROOT="$HOME/vcpkg"
```

Desde la raíz del repositorio, configura, compila y ejecuta:

```bash
cmake --preset macos-debug
cmake --build --preset macos-debug --parallel
./build/macos-debug/bin/EntornVGI
```

Para Release, sustituye `macos-debug` por `macos-release` en los comandos de
configuración y compilación; el ejecutable queda en
`build/macos-release/bin/EntornVGI`. En VS Code instala las extensiones
recomendadas **CMake Tools** y **C/C++**, abre VS Code desde la terminal donde
definiste `VCPKG_ROOT`, selecciona el preset macOS y usa los controles de
configurar, compilar y ejecutar.

macOS solicita un contexto OpenGL 4.1 Core con compatibilidad forward, y los
shaders GLSL 330 del proyecto son compatibles con ese contexto. Apple ha
declarado OpenGL obsoleto; no se puede usar un contexto de compatibilidad
heredado en macOS.

## Dependencias y estructura

```text
CMakeLists.txt                 Configuracion CMake comun
CMakePresets.json              Presets Debug/Release de cada plataforma
vcpkg.json                     Dependencias multiplataforma de vcpkg
EntornVGI/                     Aplicacion, cargadores y shaders
EntornVGI/assets/              Modelos OBJ/MTL, escenas y texturas
include/ImGui/                 Fuentes y backends Dear ImGui
third_party/SOIL2/             Fuentes SOIL2
third_party/nativefiledialog/  Backend GTK3 usado por Linux
lib/                           Bibliotecas heredadas de Windows
```

`ModelLoader.cpp` no se incluye en el ejecutable: tampoco forma parte de la
lista de compilacion original de Visual Studio y depende de Assimp. El flujo de
carga de escenas usado por la aplicacion es `SceneLoader` junto con el cargador
OBJ/MTL propio.

## Validacion y limitaciones

En Ubuntu con GCC y una NVIDIA GTX 1650/OpenGL 3.3 se configuraron y compilaron
los presets `linux-debug` y `linux-release`. Se iniciaron ambos ejecutables:
los shaders de iluminacion, ejes y skybox compilaron y enlazaron, se cargo
`arms_rig.obj` y `SceneLoader` leyo los 12 objetos de la escena. La validacion
fue de arranque; no se verifico visualmente ni de forma interactiva toda la
camara, navegacion, texturas, ImGui o los dialogos. Los builds siguen
produciendo avisos heredados, principalmente inicializadores incompletos y
variables no usadas en la geometria antigua.

Windows se configuró y compiló con MSVC, Ninja y vcpkg; el ejecutable inició en
esta sesión, pero no se hizo una validación visual o interactiva completa.
macOS no se pudo compilar desde este entorno Windows y debe validarse con Apple
Clang/vcpkg en macOS. Un build Linux no certifica macOS. La carga de los
diálogos nativos requiere un entorno de escritorio compatible.

El codigo contiene rutinas GLUT antiguas junto con rutas de dibujo modernas
basadas en VAO/EBO. Las rutas heredadas que aun dependan de OpenGL inmediato no
son compatibles con un perfil Core; las opciones activas en macOS deben
validarse visualmente y migrarse si se ejercitan esas rutinas.

## Archivos generados

`.gitignore` excluye directorios CMake, artefactos de compilacion, ejecutables,
bibliotecas generadas y estado local del IDE. Los modelos `*.obj`, materiales
`*.mtl`, texturas, shaders y escenas **no** se ignoran. `.vscode/` contiene
solo presets y configuracion compartida, sin rutas personales.
