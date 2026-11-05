# exportar_glb.py
# -----------------------------------------------------------------------------
# Exporta un .blend a .glb con los ajustes que espera EscapeTheUAB.
# Así todos los compañeros exportan igual y no hay que recordar las casillas.
#
# Uso (desde la raíz del repositorio; ajusta la ruta de Blender si hace falta):
#   macOS:   /Applications/Blender.app/Contents/MacOS/Blender -b MiEscena.blend \
#              --python tools/exportar_glb.py -- EntornVGI/assets/animations/MiEscena.glb
#   Windows: "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b MiEscena.blend ^
#              --python tools/exportar_glb.py -- EntornVGI\assets\animations\MiEscena.glb
#
# También puedes hacerlo a mano: File > Export > glTF 2.0 con los mismos ajustes
# (ver docs/ANIMACIONES_GLB.md).
# -----------------------------------------------------------------------------
import sys
import bpy

# Lo que viene después de "--" en la línea de comandos es la ruta de salida.
argumentos = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(argumentos) != 1:
    sys.exit("Falta la ruta de salida: ... --python tools/exportar_glb.py -- ruta/Salida.glb")

bpy.ops.export_scene.gltf(
    filepath=argumentos[0],
    export_format='GLB',               # un solo archivo binario
    use_renderable=True,               # solo lo visible en render (se omiten los CTRL_*, que están ocultos)
    export_yup=True,                   # glTF estándar: Y arriba (la conversión a Z arriba la hace el C++)
    export_apply=False,                # no aplicar modificadores: se conservan pivotes y jerarquía
    export_animations=True,
    export_animation_mode='ACTIONS',   # una animación por cada Action de Blender
    export_force_sampling=True,        # convierte TODAS las curvas a muestras lineales (el C++ solo lee lineal/escalón)
    export_cameras=False,              # cámaras y luces: fuera de la primera versión
    export_lights=False,
    export_materials='EXPORT',
)
print("Exportado:", argumentos[0])
