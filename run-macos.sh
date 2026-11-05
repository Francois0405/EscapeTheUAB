#!/usr/bin/env bash
# EscapeTheUAB - compilar y ejecutar en macOS con una sola orden.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "Este lanzador es solo para macOS." >&2
  exit 1
fi
if [[ ! -f CMakePresets.json ]]; then
  echo "Situa este script junto a CMakePresets.json, en la raiz del repositorio." >&2
  exit 1
fi

configuration='debug'
run_game=1
for arg in "$@"; do
  case "$arg" in
    --release) configuration='release' ;;
    --debug) configuration='debug' ;;
    --no-run) run_game=0 ;;
    -h|--help)
      echo "Uso: ./run-macos.sh [--release|--debug] [--no-run]"
      exit 0 ;;
    *) echo "Opcion no reconocida: $arg" >&2; exit 2 ;;
  esac
done

for executable in cmake ninja; do
  if ! command -v "$executable" >/dev/null 2>&1; then
    echo "Falta $executable. Consulta README.md para instalar los requisitos." >&2
    exit 1
  fi
done

# El preset macOS exige vcpkg. No instalar dependencias automaticamente.
if [[ -z "${VCPKG_ROOT:-}" && -f "$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake" ]]; then
  export VCPKG_ROOT="$HOME/vcpkg"
fi
if [[ -z "${VCPKG_ROOT:-}" || ! -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]]; then
  echo 'No se encuentra vcpkg. Define VCPKG_ROOT (consulta README.md).' >&2
  exit 1
fi

preset='macos-'"$configuration"
echo "[EscapeTheUAB] Configurando $preset..."
cmake --preset "$preset"
echo "[EscapeTheUAB] Compilando $preset..."
cmake --build --preset "$preset" --parallel

exe="build/$preset/bin/EntornVGI"
if [[ ! -f "$exe" ]]; then
  echo "No aparece el ejecutable recien compilado: $exe" >&2
  exit 1
fi
if [[ "$run_game" -eq 1 ]]; then
  echo '[EscapeTheUAB] Iniciando juego...'
  "./$exe"
else
  echo '[EscapeTheUAB] Compilacion completada (sin ejecutar).'
fi
