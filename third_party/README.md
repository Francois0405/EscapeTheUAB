# Dependencias vendorizadas

SOIL2, Dear ImGui, ImGuizmo, nlohmann/json y cgltf se compilan desde
`third_party/` en todas las plataformas. Native File Dialog con GTK3 se usa
en Linux; Windows y macOS obtienen el resto de dependencias del toolchain
configurado por CMake.

## SOIL2

- Origen: <https://github.com/SpartanJ/SOIL2>
- Autor original: Jonathan Dummer; fork mantenido por Martin Lucas Golini.
- Licencia: dominio público (`third_party/SOIL2/src/SOIL2.c` incluye la nota).
- Uso: carga de texturas mediante `loadIMA_SOIL`.

## Native File Dialog

- Origen: <https://github.com/mlabbe/nativefiledialog>
- Licencia: zlib; el texto completo está en
  `third_party/nativefiledialog/LICENSE`.
- Uso: diálogos nativos de apertura de archivos, con backend GTK3 en Linux.

## cgltf (lector de GLB / glTF 2.0)

- Origen: <https://github.com/jkuhlmann/cgltf>
- Versión: commit `de9828bc6419064c302546313ce8ff5eac6cd703` (3-dic-2025) del archivo
  `cgltf.h`. Es posterior a la última etiqueta publicada (`v1.15`, feb-2025).
- SHA-256 de `third_party/cgltf/cgltf.h`:
  `efb169dee911696b5d35fc8e3f7ea0c56d679debc529eba9ca6aa6443ba9d5e9`
- Licencia: MIT. El texto está en `third_party/cgltf/LICENSE` y también al final
  de `cgltf.h`. El archivo se conserva **sin modificar**.
- Uso: `src/rendering/GlbModel.cpp` lee mallas, jerarquía de nodos y animaciones de
  los archivos `.glb` de `assets/animations/`. Es una biblioteca de un
  solo archivo: `CGLTF_IMPLEMENTATION` se define únicamente en `GlbModel.cpp`.
- Se usa en todas las plataformas (Windows, Linux y macOS) y no depende de vcpkg.

## Dear ImGui

- Ubicación: `third_party/imgui/`; versión indicada en `third_party/imgui/Version.txt`.
- Licencia: MIT, en `third_party/imgui/LICENSE.txt` (etiqueta `v1.89.4`).

## ImGuizmo

- Ubicación: `third_party/imguizmo/`.
- Licencia: texto conservado en `third_party/imguizmo/LICENSE-ImGuizmo`.

## nlohmann/json

- Ubicación: `third_party/nlohmann/`.
- Licencia: MIT, en `third_party/nlohmann/LICENSE.MIT` y en el encabezado de `json.hpp`.

## miniaudio

- Ubicación: `third_party/miniaudio/miniaudio.h`.
- Licencia: dominio público o MIT-0, a elección; el texto está al final de la cabecera.

## stb_vorbis

- La biblioteca la aporta vcpkg o el sistema; `third_party/stb_compat/` solo contiene el adaptador propio de inclusión.

## OpenXR

- No se vendoriza. El cargador llega de vcpkg (`openxr-loader`) o de `libopenxr-dev` en Linux.

Los textos de licencia, también de los paquetes de vcpkg, se listan en [licenses/README.md](licenses/README.md).
