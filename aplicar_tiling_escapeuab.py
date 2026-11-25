#!/usr/bin/env python3
"""Aplica mapeado UV repetible a suelo/pared/techo de EscapeTheUAB.

Ejecutar en la raiz del repo: python .\aplicar_tiling_escapeuab.py
Solo modifica EntornVGI/main.cpp y assets/shaders/phong_shdrML.vert.
Primero valida ambos parches, luego crea backups .pre-tiling.bak.
Reejecutar es seguro: detecta cambios ya aplicados.
"""
from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path

MAIN = Path('EntornVGI/main.cpp')
VERT = Path('assets/shaders/phong_shdrML.vert')

DECLS = '''// Escala fisica de la textura para los cubos arquitectonicos.
uniform bool uArchitectureTiling;
uniform float uTileSizeMeters;'''

UV_BODY = '''// Objetos arquitectonicos: una repeticion por cada uTileSizeMeters
    // unidades del mundo. Se usa la escala del modelo, no el UV [0,1]
    // original, porque el mismo OBJ se reutiliza en paredes muy distintas.
    if (uArchitectureTiling) {
        vec3 escala = vec3(
            length(modelMatrix[0].xyz),
            length(modelMatrix[1].xyz),
            length(modelMatrix[2].xyz));
        vec3 p = in_Vertex * escala;
        vec3 n = abs(in_Normal);
        float tile = max(uTileSizeMeters, 0.1);

        if (n.z >= n.x && n.z >= n.y) {
            vertexTexCoord = p.xy / tile; // suelo/techo
        } else if (n.x >= n.y) {
            vertexTexCoord = p.yz / tile; // pared normal X
        } else {
            vertexTexCoord = p.xz / tile; // pared normal Y
        }
    } else {
        vertexTexCoord = flag_invert_y
            ? vec2(in_TexCoord.x, 1.0 - in_TexCoord.y)
            : in_TexCoord;
    }'''

SETUP = '''            // Repetir la textura solo en los cubos de arquitectura base;
            // no altera las UV originales de muebles ni props importados.
            const bool arquitecturaBase =
                obj.modelo.find("escape_pack/arquitectura/suelo.obj") != std::string::npos ||
                obj.modelo.find("escape_pack/arquitectura/suelo_laboratorio.obj") != std::string::npos ||
                obj.modelo.find("escape_pack/arquitectura/pared.obj") != std::string::npos ||
                obj.modelo.find("escape_pack/arquitectura/techo.obj") != std::string::npos;
            glUseProgram(shader_programID);
            glUniform1i(glGetUniformLocation(shader_programID, "uArchitectureTiling"),
                        arquitecturaBase ? GL_TRUE : GL_FALSE);
            glUniform1f(glGetUniformLocation(shader_programID, "uTileSizeMeters"), 2.5f);
'''

RESET = '''            // No transferir el tiling al siguiente render (movil, props, etc.).
            glUniform1i(glGetUniformLocation(shader_programID, "uArchitectureTiling"), GL_FALSE);
'''

# Regex tolera algunos cambios de formato respecto al master.
OLD_UV = re.compile(
    r'(?m)^\s*if\s*\(flag_invert_y\)\s*vertexTexCoord\s*=\s*vec2\s*\(\s*in_TexCoord\.x\s*,\s*1\.0\s*-\s*in_TexCoord\.y\s*\)\s*;[^\n]*\n'
    r'\s*else\s*vertexTexCoord\s*=\s*vec2\s*\(\s*in_TexCoord\.x\s*,\s*in_TexCoord\.y\s*\)\s*;[^\n]*'
)


def patch_shader(old: str) -> str:
    if 'uniform bool uArchitectureTiling;' in old and 'if (uArchitectureTiling)' in old:
        return old
    if 'uArchitectureTiling' in old:
        raise ValueError('El shader tiene un cambio de tiling parcial. Revisar manualmente.')
    anchor = re.compile(r'(?m)^(uniform bool flag_invert_y;[^\n]*)$')
    if len(anchor.findall(old)) != 1:
        raise ValueError('No se ha encontrado el uniform flag_invert_y en phong_shdrML.vert.')
    patched = anchor.sub(lambda m: m.group(1) + '\n' + DECLS, old, count=1)
    patched, count = OLD_UV.subn(lambda m: UV_BODY, patched, count=1)
    if count != 1:
        raise ValueError('No se ha encontrado el bloque UV original del shader Phong.')
    return patched


def patch_main(old: str) -> str:
    if 'const bool arquitecturaBase =' in old and 'uTileSizeMeters' in old:
        return old
    if 'uArchitectureTiling' in old:
        raise ValueError('main.cpp tiene un cambio de tiling parcial. Revisar manualmente.')
    needle = '// Dibuixem el model amb la posició, rotació i escala de l\'objecte.'
    if old.count(needle) != 1:
        raise ValueError('No se encuentra el lugar para activar el tiling en el bucle de la escena.')
    old = old.replace(needle, SETUP + '\n\t\t\t' + needle, 1)
    close_call = re.compile(r'(\bit->second\.get\(\),\s*matriz\s*\n\s*\);)')
    old, count = close_call.subn(lambda m: m.group(1) + '\n' + RESET, old, count=1)
    if count != 1:
        raise ValueError('No se encuentra el final de dibuixa_EscenaGL para restaurar el uniform.')
    return old


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true', help='Comprueba si se puede aplicar sin escribir archivos')
    args = ap.parse_args()
    paths = [(VERT, patch_shader), (MAIN, patch_main)]
    new_files = []
    for path, patcher in paths:
        if not path.is_file():
            sys.exit(f'ERROR: Falta {path}; ejecuta desde la raiz de EscapeTheUAB.')
        old = path.read_text(encoding='utf-8-sig')
        try:
            new = patcher(old)
        except ValueError as exc:
            sys.exit(f'ERROR en {path}: {exc}\nNo se ha escrito ningun archivo.')
        new_files.append((path, old, new))
    for path, old, new in new_files:
        print(f'{path}: ' + ('YA APLICADO' if old == new else 'PREPARADO'))
    if args.check:
        print('Comprobacion terminada. No se ha escrito ningun archivo.')
        return
    for path, old, new in new_files:
        if old == new:
            continue
        backup = path.with_name(path.name + '.pre-tiling.bak')
        if not backup.exists():
            shutil.copy2(path, backup)
        path.write_text(new, encoding='utf-8')
        print(f'MODIFICADO: {path} (backup: {backup.name})')
    print('LISTO. Compila con .\\run-windows.cmd -NoRun. Solo afecta al shader Phong y a los OBJ de arquitectura base.')
    print('Si aun no tienes GL_REPEAT en LoadTexture2, configurarlo en objLoader.cpp.')


if __name__ == '__main__':
    main()
