#!/usr/bin/env python3
"""Importa suelo MetalWalkway010 y assets seleccionados de Quaternius a EscapeTheUAB.

Requiere solo Python 3.8+ (sin paquetes externos). No modifica JSON, shaders ni C++.

Desde la raiz del repositorio:
    python instalar_assets_laboratorio_escapeuab.py --downloads "E:\\Descargas - Brave" --check
    python instalar_assets_laboratorio_escapeuab.py --downloads "E:\\Descargas - Brave"

- Recupera el color PNG 2K sin extraer mapas PBR no soportados.
- Crea suelo_rejilla.obj/.mtl como copia del suelo_laboratorio ya existente,
  conservando su geometria, UV, escalas y colisiones del JSON.
- Extrae un conjunto de OBJ/MTL Quaternius y corrige las rutas absolutas de
  los mapas diffuse para que funcionen en cualquier Windows/Mac/Linux.
- No sobreescribe archivos existentes diferentes salvo con --force (con .bak).
"""
from __future__ import annotations
import argparse
import re
import shutil
import sys
import zipfile
from pathlib import Path

ARCH = Path('assets/models/escape_pack/arquitectura')
SCI = Path('assets/models/escape_pack/scifi')
WALKWAY_NAME = 'MetalWalkway010_2K-PNG.zip'
MEGAKIT_NAME = 'Modular SciFi MegaKit[Standard].zip'
MEGAKIT_ROOT = 'Modular SciFi MegaKit[Standard]/'
# Seleccionados para la primera prueba: luminarias, elementos de techo y decoracion.
PICKS = [
    ('Props', 'Prop_Light_Wide'),
    ('Props', 'Prop_Light_Small'),
    ('Props', 'Prop_Vent_Wide'),
    ('Props', 'Prop_Cable_1'),
    ('Props', 'Prop_Cable_3'),
    ('Props', 'Prop_Crate3'),
    ('Props', 'Prop_Computer'),
    ('Props', 'Prop_Fan_Small'),
    ('Platforms', 'Platform_DarkPlates'),
]


def check_member(z: zipfile.ZipFile, member: str) -> bytes:
    try:
        return z.read(member)
    except KeyError:
        raise ValueError('El ZIP no contiene: ' + member)


def write_safe(target: Path, data: bytes, force: bool, check: bool):
    if target.is_file():
        if target.read_bytes() == data:
            print('  YA EXISTE (igual):', target)
            return
        if not force:
            raise RuntimeError('Existe otro archivo: ' + str(target) + '\nUsa --force para sobreescribir (genera .bak), o revisalo manualmente.')
    if check:
        print('  PREPARADO:', target)
        return
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.is_file():
        bak = target.with_name(target.name + '.bak')
        if not bak.exists():
            shutil.copy2(target, bak)
    target.write_bytes(data)
    print('  INSTALADO:', target)


def scene_floor(src_obj: bytes, src_mtl: bytes):
    obj = src_obj.decode('utf-8-sig', 'replace')
    mtl = src_mtl.decode('utf-8-sig', 'replace')
    if not re.search(r'(?m)^mtllib\s+', obj):
        raise ValueError('suelo_laboratorio.obj no contiene mtllib; revisa el modelo antes de instalar.')
    if not re.search(r'(?m)^newmtl\s+', mtl):
        raise ValueError('suelo_laboratorio.mtl no contiene newmtl.')
    obj = re.sub(r'(?m)^mtllib\s+[^\r\n]+', 'mtllib suelo_rejilla.mtl', obj)
    obj = re.sub(r'(?m)^usemtl\s+[^\r\n]+', 'usemtl suelo_rejilla', obj)
    mtl = re.sub(r'(?m)^newmtl\s+[^\r\n]+', 'newmtl suelo_rejilla', mtl)
    mtl = re.sub(r'(?m)^\s*map_Kd\s+[^\r\n]+\r?\n?', '', mtl)
    mtl = mtl.rstrip() + '\nmap_Kd metal_walkway010_color.png\n'
    return obj.encode('utf-8'), mtl.encode('utf-8')


def fix_mtl(raw: bytes, assets: zipfile.ZipFile, texture_data: dict) -> bytes:
    text = raw.decode('utf-8-sig', 'replace')
    new_lines = []
    for line in text.splitlines():
        clean = line.strip()
        if clean.startswith(('map_Bump ', 'bump ', 'map_bump ')):
            # El shader actual no usa normal map/tangent space.
            continue
        if clean.startswith('map_Kd '):
            source = clean[len('map_Kd '):].strip()
            file = re.split(r'[\\/]+', source)[-1]
            member = MEGAKIT_ROOT + 'Textures/' + file
            png = check_member(assets, member)
            if file in texture_data and texture_data[file] != png:
                raise ValueError('Dos texturas distintas tienen el mismo nombre: ' + file)
            texture_data[file] = png
            new_lines.append('map_Kd textures/' + file)
        else:
            new_lines.append(line)
    return ('\n'.join(new_lines) + '\n').encode('utf-8')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--downloads', type=Path, default=Path.home() / 'Downloads')
    p.add_argument('--walkway', type=Path, default=None, help='ZIP de ambientCG si no esta en --downloads')
    p.add_argument('--megakit', type=Path, default=None, help='ZIP de Quaternius si no esta en --downloads')
    p.add_argument('--check', action='store_true', help='Validar sin escribir')
    p.add_argument('--force', action='store_true', help='Permitir cambiar archivos existentes creando .bak')
    args=p.parse_args()
    root=Path.cwd()
    arch=root/ARCH
    sci=root/SCI
    if not (root/'run-windows.cmd').exists() or not (arch/'suelo_laboratorio.obj').exists() or not (arch/'suelo_laboratorio.mtl').exists():
        sys.exit('ERROR: Ejecuta desde la raiz del repo, con suelo_laboratorio.obj/.mtl ya preparado.')
    walk=args.walkway or args.downloads/WALKWAY_NAME
    mega=args.megakit or args.downloads/MEGAKIT_NAME
    for zip_path in (walk,mega):
        if not zip_path.exists():
            sys.exit('ERROR: No existe '+str(zip_path))
    print('Ambos ZIP encontrados. Modo:', 'VERIFICACION' if args.check else 'INSTALACION')
    pending=[]
    with zipfile.ZipFile(walk) as wz, zipfile.ZipFile(mega) as mz:
        image=check_member(wz,'MetalWalkway010_2K-PNG_Color.png')
        floor_obj,floor_mtl=scene_floor((arch/'suelo_laboratorio.obj').read_bytes(),(arch/'suelo_laboratorio.mtl').read_bytes())
        pending.extend([(arch/'metal_walkway010_color.png',image),(arch/'suelo_rejilla.obj',floor_obj),(arch/'suelo_rejilla.mtl',floor_mtl)])
        pngs={}
        for category,name in PICKS:
            base=MEGAKIT_ROOT+'OBJ/'+category+'/'+name
            obj=check_member(mz,base+'.obj')
            mtl=fix_mtl(check_member(mz,base+'.mtl'),mz,pngs)
            # Un solo directorio para los OBJ/MTL, todas las imagenes en textures/.
            pending.extend([(sci/(name+'.obj'),obj),(sci/(name+'.mtl'),mtl)])
        pending.extend((sci/'textures'/name,data) for name,data in sorted(pngs.items()))
    # Evita dejar un proyecto a medias si hay conflictos.
    conflicts=[]
    for path,data in pending:
        if path.exists() and path.read_bytes()!=data and not args.force:
            conflicts.append(str(path))
    if conflicts:
        sys.exit('ERROR: hay archivos existentes diferentes. No se ha escrito nada:\n  '+'\n  '.join(conflicts)+'\nRevisa o usa --force (creara .bak).')
    for path,data in pending:
        write_safe(path,data,args.force,args.check)
    print('\nLISTO.' if not args.check else '\nCOMPROBACION COMPLETA.')
    print('A continuacion:')
    print('1) En main.cpp, agrega suelo_rejilla.obj a la lista arquitecturaBase de uArchitectureTiling.')
    print('2) En el JSON del laboratorio, cambia solo model de suelo_laboratorio.obj a suelo_rejilla.obj.')
    print('3) Compila y prueba el suelo. No se ha cambiado el JSON ni el codigo.')
    print('4) En EDIT, busca Prop_Light_Wide en Asset Browser; anadelo sin collider.')
    print('5) El modelo de la lampara no emite luz fisica: eso se configura aparte.')

if __name__=='__main__':
    main()
