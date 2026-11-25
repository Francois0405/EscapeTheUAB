# Mapas de renombrado del refactor

Registro de los renombrados mecanicos aplicados sobre `0d727a8`, en orden. Sirven para portar a la
estructura actual el trabajo de una rama antigua: no son un script para ejecutar a ciegas, porque el
mismo nombre antiguo puede tener otro significado en codigo nuevo. Los enumeradores se renombraron enum
por enum; ver el commit "Finish English enumerators". La tabla de funciones esta en `docs/PORTING_MAP.md`.

Comprobaciones reutilizables:

- `python tools/refactor/literals.py . <base> <rev>`: compara todos los literales de cadena entre dos revisiones.
- `python tools/refactor/includecase.py . <rev>`: includes cuyo uso de mayusculas no coincide con el fichero.
