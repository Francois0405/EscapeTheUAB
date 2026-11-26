# Textos de licencia de las dependencias

Copias literales; no editar.

## Paquetes de vcpkg (`vcpkg/`)

Fichero `copyright` que vcpkg instala con cada paquete, en la versión usada en Windows el 2026-11-26 con la
línea base de `vcpkg.json`: bullet3 3.25, enet 1.3.18, glew 2.3.1, glfw3 3.5.1, glm 1.0.3, jsoncpp 1.9.6
(lo arrastra OpenXR), meshoptimizer 1.3, nativefiledialog-extended 1.4.1, openxr-loader 1.1.63 y
stb 2026-08-02 (de ahí sale stb_vorbis). Si cambia la línea base, hay que volver a copiarlos. En Linux las
dependencias vienen de la distribución, con sus propias versiones y avisos.

El fichero de meshoptimizer es grande porque vcpkg le añade completas las cabeceras `cgltf.h` y `fast_obj.h`, que usa su
herramienta `gltfpack` (el juego no la usa).

## Código vendorizado

| Dependencia | Versión en el repositorio | Texto | De dónde sale |
|---|---|---|---|
| Dear ImGui | 1.89.4 WIP | `../imgui/LICENSE.txt` | Repositorio oficial, etiqueta `v1.89.4` |
| nlohmann/json | 3.11.3 | `../nlohmann/LICENSE.MIT` | Repositorio oficial, etiqueta `v3.11.3` |
| miniaudio | 0.11.26 | `../miniaudio/LICENSE` | Extraído del final de `miniaudio.h` |
| SOIL2 | 1.3.0 (`SOIL2.h`) | `../SOIL2/NOTICES.txt` | Avisos extraídos de sus fuentes; varios auxiliares no llevan aviso propio |
| cgltf | commit `de9828bc` | `../cgltf/LICENSE` | Ya estaba |
| ImGuizmo | sin etiqueta anotada | `../imguizmo/LICENSE-ImGuizmo` | Ya estaba |
| Native File Dialog (Linux) | sin etiqueta anotada | `../nativefiledialog/LICENSE` | Ya estaba |

Los ejecutables no incluyen estos textos automáticamente: al distribuir el juego hay que adjuntar esta carpeta
y los ficheros de la tabla.
