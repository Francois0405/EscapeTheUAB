#!/usr/bin/env python3
"""Mide un archivo OBJ y escribe el bloque "physics" listo para pegar en el JSON.

Uso:  python3 tools/medir_obj.py EntornVGI/assets/models/TaulaRara.obj [static|dynamic] [masa_kg]

halfSize y offset salen en el espacio del MODELO (las coordenadas de los vertices del
archivo, antes de aplicar la escala del objeto), que es exactamente lo que espera el JSON.
"""
import json
import sys


def medir(ruta):
    minimo = [float("inf")] * 3
    maximo = [float("-inf")] * 3
    with open(ruta, encoding="utf-8", errors="replace") as archivo:
        for linea in archivo:
            if linea.startswith("v "):
                coordenadas = [float(valor) for valor in linea.split()[1:4]]
                for eje in range(3):
                    minimo[eje] = min(minimo[eje], coordenadas[eje])
                    maximo[eje] = max(maximo[eje], coordenadas[eje])
    if minimo[0] == float("inf"):
        sys.exit("El OBJ no contiene vertices ('v x y z').")
    return minimo, maximo


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    tipo = sys.argv[2] if len(sys.argv) > 2 else "static"
    minimo, maximo = medir(sys.argv[1])
    centro = [round((a + b) / 2, 4) for a, b in zip(minimo, maximo)]
    mitad = [round((b - a) / 2, 4) for a, b in zip(minimo, maximo)]
    fisica = {"type": tipo, "shape": "box", "halfSize": mitad, "offset": centro}
    if tipo == "dynamic":
        fisica["mass"] = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
    print(json.dumps({"physics": fisica}))


main()
