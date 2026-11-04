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
