# Procedencia y derechos de los assets

Revisado el 2026-11-26. Esto es un registro de evidencias, no un dictamen legal.

## Verificados: mismo contenido que el publicado por el origen

Comparados byte a byte (MD5) con el fichero que descarga hoy el publicador.

| Fichero en `assets/models/escape_pack/arquitectura/` | Origen | Licencia declarada por el publicador |
|---|---|---|
| `suelo.jpg` | Poly Haven `concrete_floor_painted`, Diffuse 4K | CC0, <https://polyhaven.com/license> |
| `pared.jpg` | Poly Haven `blue_metal_plate`, Diffuse 4K | CC0 |
| `techo.jpg` | Poly Haven `ceiling_interior`, Diffuse 4K | CC0 |
| `suelo_laboratorio.jpg` | Poly Haven `metal_grate_rusty`, Diffuse 4K | CC0 |
| `metal_walkway010_color.png` | ambientCG `MetalWalkway010_2K-PNG_Color.png` | CC0 1.0, <https://docs.ambientcg.com/license/> |

## Origen documentado en el repositorio, sin comparación de ficheros

La licencia es la que muestra la página del publicador; **no se ha comprobado que cada fichero salga de ese paquete**.

| Assets | Evidencia del origen | Licencia en la página del publicador |
|---|---|---|
| Linterna y llave | `3dexter_horror_player_item_pack.txt` (3Dexter3D, aportado por el equipo) | CC0 1.0, <https://3dexter.itch.io/retrohorrorkit> |
| `escape_pack/kenney/` | Cada fichero dice "Created by Kenney (www.kenney.nl)". Los nombres corresponden a su mobiliario (se supone Furniture Kit) | CC0, <https://kenney.nl/assets/furniture-kit> |
| `escape_pack/scifi/` | `tools/instalar_assets_laboratorio_escapeuab.py` los extrae de "Modular SciFi MegaKit[Standard].zip" de Quaternius | CC0, <https://quaternius.com/packs/modularscifimegakit.html> |

## Derechos de distribución desconocidos

Sin autor, origen o licencia comprobables. **No publicar ni distribuir sin resolverlos.** No se les atribuye nada.

| Assets | Lo único que se sabe |
|---|---|
| `audio/hirity/appear/rickroll.ogg` | Ver abajo |
| `audio/hirity/appear/anotopat.ogg` | Metadato "Trimmed by HappyScribe": recorte de una grabación sin identificar |
| Resto de `audio/hirity/` | Sin autor en los metadatos. Preguntar al equipo quién los grabó |
| `models/escape_pack/lab/` (67 ficheros) | Los OBJ llevan la cabecera de la biblioteca `trimesh`, que no indica autor |
| `models/escape_pack/arquitectura/industrial_*.png` | Entraron en los commits de assets del equipo, sin fuente anotada |
| `models/Marc/` y resto de `models/` (incluidos Hiriti, Phone, puzles, `imatges/`, `.blend`) | Aportados por miembros del equipo o por la base de la asignatura, sin declaración de autoría |
| `animations/*.glb` | Exportados con Blender; el fichero no declara autor ni licencia |
| `textures/` (skybox, fondo del menú), `ui/` | Sin fuente anotada |
| `archive/legacy/vgi-samples/` (no se distribuye con el juego) | Modelos y texturas de la base VGI, de terceros |

## `rickroll.ogg`

- Añadido por Francois0405 en `d2e55e8` (2026-11-24); antes existió como `rickroll.opus` desde `d594df3`.
- 2,1 s, Vorbis estéreo 48 kHz. Metadato: "Trimmed by HappyScribe · https://www.happyscribe.com", es decir, un
  recorte hecho con una herramienta en línea. Ningún metadato identifica la obra ni su licencia.
- El nombre alude al meme "rickroll", asociado a la canción comercial *Never Gonna Give You Up* de Rick Astley.
  **El contenido no se ha escuchado ni identificado**; si es esa grabación, no hay derecho a distribuirla.
- El juego lo usa: Hirity elige al azar cualquier `.ogg` de `appear/`. No se ha cambiado. Recomendación: que el
  equipo lo escuche y, antes de publicar, lo sustituya o lo borre (basta quitar el fichero; el código no lo nombra).
