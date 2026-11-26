#!/usr/bin/env python3
"""Prepara cuatro texturas Poly Haven en EscapeTheUAB sin Blender.

Ejecutar desde la raíz del repositorio:
    python tools/preparar_texturas_escapeuab.py
    python tools/preparar_texturas_escapeuab.py --downloads "C:\\Users\\...\\Downloads"

No toca los JSON ni el codigo C++, y crea copias .bak de los OBJ/MTL existentes.
"""
from __future__ import annotations

import argparse
import re
import shutil
import sys
import zipfile
from pathlib import Path

ROOT_MODELS = Path('assets/models/escape_pack/arquitectura')
# (modelo OBJ/MTL, slug de Poly Haven)
MATERIALS = {
    'suelo': 'concrete_floor_painted',       # Aula informatica
    'pared': 'blue_metal_plate',
    'techo': 'ceiling_interior',
    'suelo_laboratorio': 'metal_grate_rusty', # Modelo adicional, NO cambia el JSON
}
SUFFIXES = {'.jpg', '.jpeg', '.png'}


def buscar_zip(downloads: Path, slug: str) -> Path:
    opciones = sorted((p for p in downloads.iterdir()
                       if p.is_file() and p.name.lower().startswith(slug.lower() + '_')
                       and p.name.lower().endswith('.zip')),
                      key=lambda p: p.stat().st_mtime, reverse=True)
    if not opciones:
        raise FileNotFoundError(f'No se encuentra el ZIP de {slug} en {downloads}')
    return opciones[0]


def extraer_color(archivo_zip: Path, destino_base: Path) -> Path:
    with zipfile.ZipFile(archivo_zip) as z:
        imgs = [i for i in z.infolist() if not i.is_dir()
                and Path(i.filename).suffix.lower() in SUFFIXES]
        def puntuacion(i: zipfile.ZipInfo):
            nombre = Path(i.filename).stem.lower()
            good = bool(re.search(r'(?:^|[_-])(diff|diffuse|albedo|basecolor|base_color)(?:[_-]|$)', nombre))
            bad = bool(re.search(r'(?:^|[_-])(nor|normal|rough|roughness|disp|displacement|ao|metallic|metalness|height|preview)(?:[_-]|$)', nombre))
            # No aceptar otros mapas por accidente.
            return (int(good and not bad), int('_4k' in nombre), int(Path(i.filename).suffix.lower() == '.jpg'), i.file_size)
        opciones = sorted(imgs, key=puntuacion, reverse=True)
        if not opciones or puntuacion(opciones[0])[0] == 0:
            samples = ', '.join(Path(i.filename).name for i in imgs[:12])
            raise ValueError(f'No se encontro mapa Diffuse/Albedo en {archivo_zip.name}. Imagenes: {samples}. '
                             'Descarga el mapa Diffuse JPG/PNG desde Poly Haven.')
        elegido = opciones[0]
        destino = destino_base.with_suffix(Path(elegido.filename).suffix.lower())
        with z.open(elegido) as origen, destino.open('wb') as salida:
            shutil.copyfileobj(origen, salida)
        print(f'  Color: {Path(elegido.filename).name} -> {destino}')
        return destino


def guardar_backup(path: Path) -> None:
    backup = path.with_name(path.name + '.bak')
    if path.exists() and not backup.exists():
        shutil.copy2(path, backup)


def UV_cubo_original(original: str, nombre: str) -> str:
    """Conserva vértices, normales y caras; inserta UV por cara.

    UV [0,1] por cara (MVP). El tiling en unidades de mundo requiere un ajuste
    posterior, porque las instancias cambian su escala en el JSON.
    """
    lineas = original.splitlines()
    # Reemplazar referencias para el nuevo material en caso de modelo duplicado.
    resultado = []
    contiene_vt = any(l.strip().startswith('vt ') for l in lineas)
    uv = ['vt 0.0 0.0', 'vt 1.0 0.0', 'vt 1.0 1.0', 'vt 0.0 1.0']
    for linea in lineas:
        if linea.startswith('mtllib '):
            linea = f'mtllib {nombre}.mtl'
        elif linea.startswith('o '):
            linea = f'o {nombre}'
        elif linea.startswith('usemtl '):
            linea = f'usemtl {nombre}'
        elif linea.startswith('f ') and not contiene_vt:
            tokens = linea.split()[1:]
            if len(tokens) == 4:
                convertidos = []
                for j, t in enumerate(tokens):
                    partes = t.split('/')
                    vi = partes[0]
                    normal = partes[2] if len(partes) > 2 and partes[2] else '1'
                    convertidos.append(f'{vi}/{j+1}/{normal}')
                linea = 'f ' + ' '.join(convertidos)
        resultado.append(linea)
    if not contiene_vt:
        indice = next((i for i, l in enumerate(resultado) if l.startswith('f ')), len(resultado))
        resultado[indice:indice] = uv
    return '\n'.join(resultado) + '\n'


def actualizar_mtl(contenido: str, nombre: str, textura: Path) -> str:
    lineas = [ln for ln in contenido.splitlines() if not ln.strip().startswith('map_Kd ')]
    nuevo = []
    for ln in lineas:
        if ln.startswith('newmtl '):
            ln = f'newmtl {nombre}'
        nuevo.append(ln)
    nuevo.append(f'map_Kd {textura.name}')
    return '\n'.join(nuevo) + '\n'


def main():
    ap = argparse.ArgumentParser(description='Instala imágenes y UV para la arquitectura EscapeTheUAB.')
    ap.add_argument('--downloads', type=Path, default=Path.home() / 'Downloads', help='Carpeta que contiene los .blend.zip')
    args = ap.parse_args()
    base = Path.cwd() / ROOT_MODELS
    if not base.is_dir() or not (base / 'suelo.obj').exists():
        sys.exit('ERROR: Ejecuta este script desde la raíz de EscapeTheUAB (donde está run-windows.cmd).')
    if not args.downloads.is_dir():
        sys.exit(f'ERROR: No existe el directorio de descargas: {args.downloads}')

    # Primero validar TODOS los ZIP; no modificar modelos si falta una textura.
    archivos = {nombre: buscar_zip(args.downloads, slug) for nombre, slug in MATERIALS.items()}
    print('Texturas encontradas:')
    for nombre, zip_ in archivos.items():
        print(f'  {nombre}: {zip_.name}')

    # Preparar variantes, manteniendo geometría exacta de los cubos originales.
    for nombre, archivo_zip in archivos.items():
        archivo_modelo = base / f'{nombre}.obj'
        archivo_mtl = base / f'{nombre}.mtl'
        es_variante = nombre == 'suelo_laboratorio'
        fuente_obj = base / ('suelo.obj.bak' if (base / 'suelo.obj.bak').exists() else 'suelo.obj') if es_variante else archivo_modelo
        fuente_mtl = base / ('suelo.mtl.bak' if (base / 'suelo.mtl.bak').exists() else 'suelo.mtl') if es_variante else archivo_mtl
        if not fuente_obj.exists() or not fuente_mtl.exists():
            raise FileNotFoundError(f'Falta el OBJ/MTL fuente para {nombre}')
        original_obj = fuente_obj.read_text(encoding='utf-8')
        original_mtl = fuente_mtl.read_text(encoding='utf-8')
        if not es_variante:
            guardar_backup(archivo_modelo)
            guardar_backup(archivo_mtl)
        else:
            # Evitar sobreescribir una variante personalizada existente.
            if archivo_modelo.exists(): guardar_backup(archivo_modelo)
            if archivo_mtl.exists(): guardar_backup(archivo_mtl)
        print(f'\n{nombre}:')
        imagen = extraer_color(archivo_zip, base / nombre)
        archivo_modelo.write_text(UV_cubo_original(original_obj, nombre), encoding='utf-8')
        archivo_mtl.write_text(actualizar_mtl(original_mtl, nombre, imagen), encoding='utf-8')
        print(f'  OBJ/MTL: {archivo_modelo.name} / {archivo_mtl.name}')

    print('\nLISTO. Falta: en el renderizador de la escena, dentro del bucle que dibuja los objetos de la escena,')
    print('cambia el argumento false por true inmediatamente antes de texturesID, textura_map.')
    print('Para usar el suelo metalico en el laboratorio, cambia SOLO su ruta en el JSON')
    print('a ../models/escape_pack/arquitectura/suelo_laboratorio.obj')
    print('No se ha modificado ningún JSON ni archivo C++.')


if __name__ == '__main__':
    main()
