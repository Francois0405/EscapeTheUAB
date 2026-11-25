#!/usr/bin/env python3
"""EscapeTheUAB: suelo mate + rejilla técnica localizada, sin tocar físicas ni shaders.

Colocar en la raiz del repositorio y ejecutar:
    python preparar_sector_laboratorio.py --check
    python preparar_sector_laboratorio.py --apply

Antes de escribir, valida escena y assets. Genera backup del JSON fuera del repositorio.
No coloca lámparas ni mobiliario (se hará desde EDIT para conservar su diseño).
"""
from __future__ import annotations

import argparse
import json
import shutil
import sys
from datetime import datetime
from pathlib import Path

FLOOR_MODEL = "../models/escape_pack/arquitectura/suelo_tecnico.obj"
GRATE_MODEL = "../models/escape_pack/arquitectura/rejilla_sector.obj"
GRATE_ID = "rejilla_sector_laboratorio"

FLOOR_MTL = """# EscapeTheUAB - pavimento tecnico mate, sin textura repetitiva.
newmtl suelo_tecnico
Ka 0.38 0.42 0.44
Kd 0.40 0.44 0.47
Ks 0.015 0.015 0.015
Ns 4.0
illum 2
"""

GRATE_MTL = """# Rejilla metalica discreta. Barra geometrica sobre el pavimento.
newmtl rejilla_metal
Ka 0.36 0.40 0.43
Kd 0.47 0.53 0.56
Ks 0.025 0.025 0.025
Ns 6.0
illum 2
"""


def box_vertices(x0, x1, y0, y1, z0=0.74, z1=1.0):
    return [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
            (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]


def make_grate_obj():
    """Malla 4x8 m una vez escalada [2,4,.1]; huecos ~40x50 cm."""
    nx, ny = 10, 16
    dx, dy = 2.0 / nx, 2.0 / ny
    # Barra aprox. 4 cm en el mundo, mas legible a 960x540.
    half_x = 0.010   # 0.04 metros de grosor con escala X=2
    half_y = 0.005   # 0.04 metros de grosor con escala Y=4
    boxes = []
    # Barras longitudinales continuas
    for i in range(nx + 1):
        x = -1 + i * dx
        boxes.append((max(-1, x-half_x), min(1, x+half_x), -1, 1))
    # Travesanos unicamente entre barras, evitando caras superpuestas
    for j in range(ny + 1):
        y = -1 + j * dy
        for i in range(nx):
            x0 = -1 + i * dx + half_x
            x1 = -1 + (i+1) * dx - half_x
            boxes.append((x0, x1, max(-1, y-half_y), min(1, y+half_y)))

    lines = ["# Rejilla localizada 4 x 8 m; Z local 0.74-1.0", 
             "# Colocar en pos [-10,0,-3.13] y escala [2,4,0.1]", 
             "mtllib rejilla_sector.mtl", "o rejilla_sector", "usemtl rejilla_metal"]
    quad_indices = [
        (4, 5, 6, 7), (3, 2, 1, 0),
        (0, 1, 5, 4), (1, 2, 6, 5),
        (2, 3, 7, 6), (3, 0, 4, 7)
    ]
    vnum = 0
    for x0, x1, y0, y1 in boxes:
        vs = box_vertices(x0, x1, y0, y1)
        for x, y, z in vs:
            lines.append(f"v {x:.7f} {y:.7f} {z:.7f}")
        for face in quad_indices:
            lines.append("f " + " ".join(str(vnum+k+1) for k in face))
        vnum += 8
    return "\n".join(lines) + "\n", len(boxes), len(boxes)*12


def original_floor_as_matte(source):
    if "mtllib suelo.mtl" not in source or "usemtl suelo" not in source:
        raise ValueError("suelo.obj no contiene los identificadores esperados")
    s = source.replace("mtllib suelo.mtl", "mtllib suelo_tecnico.mtl", 1)
    s = s.replace("usemtl suelo", "usemtl suelo_tecnico", 1)
    return s.replace("o suelo\n", "o suelo_tecnico\n", 1)


def main():
    pa = argparse.ArgumentParser(description=__doc__)
    action = pa.add_mutually_exclusive_group()
    action.add_argument("--check", action="store_true", help="Solo verificar (valor predeterminado)")
    action.add_argument("--apply", action="store_true", help="Aplicar con backup")
    args = pa.parse_args()

    root = Path(__file__).resolve().parent
    model_dir = root / "assets/models/escape_pack/arquitectura"
    scene_file = root / "assets/scenes/aula_informatica_calibrada.json"
    source_file = model_dir / "suelo.obj"
    for p in (scene_file, source_file):
        if not p.is_file():
            raise FileNotFoundError(f"Falta {p}; coloca este script en la raiz del repositorio")

    old_raw = scene_file.read_text(encoding="utf-8-sig")
    data = json.loads(old_raw)
    architecture = data.get("architecture")
    objects = data.get("objects")
    if not isinstance(architecture, list) or not isinstance(objects, list):
        raise ValueError("JSON sin arrays 'architecture' y 'objects'")
    floor = [o for o in architecture if o.get("id") == "suelo"]
    if len(floor) != 1:
        raise ValueError("Debe existir un unico objeto de arquitectura id=suelo")
    existing_ids = [o.get("id") for o in architecture + objects]
    if len(existing_ids) != len(set(existing_ids)):
        raise ValueError("Hay IDs duplicados en la escena; no se puede aplicar con seguridad")
    expected_scale = [18.0, 23.0, 0.15]
    scale = floor[0].get("scale", [])
    if len(scale) != 3 or any(abs(float(v)-t)>0.1 for v,t in zip(scale, expected_scale)):
        raise ValueError(f"El suelo no tiene la escala esperada {expected_scale}: {scale}")
    old_model = floor[0].get("model", "")
    if not old_model.startswith("../models/escape_pack/arquitectura/"):
        raise ValueError(f"Modelo de suelo inesperado: {old_model}")

    matte_model = original_floor_as_matte(source_file.read_text(encoding="utf-8-sig"))
    grate_model, bar_count, triangle_count = make_grate_obj()
    new_files = {
        model_dir / "suelo_tecnico.obj": matte_model,
        model_dir / "suelo_tecnico.mtl": FLOOR_MTL,
        model_dir / "rejilla_sector.obj": grate_model,
        model_dir / "rejilla_sector.mtl": GRATE_MTL,
    }
    for dest, content in new_files.items():
        if dest.exists() and dest.read_text(encoding="utf-8-sig") != content:
            raise FileExistsError(f"Archivo existente distinto: {dest}. Por seguridad no se sobrescribe")

    floor[0]["model"] = FLOOR_MODEL
    grate = {
        "id": GRATE_ID,
        "model": GRATE_MODEL,
        "position": [-10.0, 0.0, -3.13],
        "rotation": [0.0, 0.0, 0.0],
        "scale": [2.0, 4.0, 0.10],
        # Sin colision: la caja Bullet del suelo base se mantiene intacta.
    }
    existing = next((x for x in objects if x.get("id") == GRATE_ID), None)
    if existing is None:
        objects.append(grate)
    elif existing.get("model") != GRATE_MODEL:
        raise ValueError("El ID de la rejilla ya existe, con otro modelo: no se sobrescribe")

    print("--- EscapeTheUAB / sector de laboratorio ---")
    print(f"Escena: {scene_file}")
    print(f"Suelo: {old_model} -> {FLOOR_MODEL}")
    print("Nueva zona de rejilla: 4 x 8 unidades, centrada en X=-10, Y=0")
    print("Altura visible: barras aprox. 2 cm sobre el suelo base")
    print(f"Rejilla: {bar_count} barras, {triangle_count} triangulos, sin colision nueva")
    print("Archivos de modelo:")
    for p in new_files:
        print(f"  {'YA EXISTE' if p.is_file() else 'CREAR'}: {p.relative_to(root)}")
    if not args.apply:
        print("\nCOMPROBACION CORRECTA. No se ha modificado ningun archivo.")
        print("Para aplicar: python .\\preparar_sector_laboratorio.py --apply")
        return

    # Backup fuera del repositorio para que `git add .` no lo incluya.
    when = datetime.now().strftime("%Y%m%d_%H%M%S")
    backups = Path.home() / "EscapeTheUAB_Backups" / when
    backups.mkdir(parents=True, exist_ok=False)
    shutil.copy2(scene_file, backups / scene_file.name)
    for p, content in new_files.items():
        if not p.is_file():
            p.write_text(content, encoding="utf-8", newline="\n")
    new_raw = json.dumps(data, ensure_ascii=False, indent=2) + "\n"
    scene_file.write_text(new_raw, encoding="utf-8", newline="\n")
    print("\nAPLICADO. JSON fuente guardado; nuevas mallas instaladas.")
    print(f"Backup del JSON anterior: {backups / scene_file.name}")
    print("No se han modificado fisicas, otras instancias, lamparas, shaders ni C++.")
    print("Compilar: .\\run-windows.cmd -NoRun")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, json.JSONDecodeError) as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)
