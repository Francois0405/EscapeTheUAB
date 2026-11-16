# Dependencias vendorizadas (Linux)

Estas copias se usan únicamente en la compilación Linux con CMake. Windows
sigue usando las cabeceras y bibliotecas ya incluidas en `include/` y `lib/x64/`.

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
- Uso: `EntornVGI/ModeloGLB.cpp` lee mallas, jerarquía de nodos y animaciones de
  los archivos `.glb` de `EntornVGI/assets/animations/`. Es una biblioteca de un
  solo archivo: `CGLTF_IMPLEMENTATION` se define únicamente en `ModeloGLB.cpp`.
- Se usa en todas las plataformas (Windows, Linux y macOS) y no depende de vcpkg.
