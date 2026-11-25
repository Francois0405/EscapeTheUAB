# Paredes paramétricas y puertas en EscapeTheUAB

Esta guía describe el soporte de paredes con aperturas integrado en el editor. Está pensada para estudiantes que quieran seguir los datos desde el JSON hasta el render, Bullet y el historial del editor.

## Idea general

Una pared compatible sigue siendo un único objeto lógico de escena. Se guarda como una instancia de `pared.obj` con escala, transformación, material y collider estático. El campo `wall.apertures` declara los huecos que deben quedar libres.

Al dibujar y al construir la escena física, EscapeTheUAB calcula paños sólidos rectangulares que rodean cada apertura. La pared lógica no se reemplaza ni se edita en disco. No se toca el OBJ compartido, y cada instancia puede tener dimensiones y aperturas diferentes.

El generador actual está diseñado para una pared rectangular con su eje horizontal local X, grosor local Y y altura local Z. Admite rotación de la instancia en el plano XY. La apertura se expresa en metros locales desde el centro de la pared. No es todavía un generador de paredes curvas ni de polígonos con huecos arbitrarios.

## Datos JSON

La forma mínima de una pared paramétrica es:

```json
{
  "id": "pared_laboratorio_norte",
  "category": "architecture",
  "model": "../models/arquitectura/pared.obj",
  "position": [0.0, 4.0, -1.5],
  "rotation": [0.0, 0.0, 0.0],
  "scale": [3.0, 0.15, 1.5],
  "physics": {
    "type": "static",
    "halfSize": [1.0, 1.0, 1.0],
    "offset": [0.0, 0.0, 0.0]
  },
  "wall": {
    "apertures": [
      {
        "id": "puerta_lab_apertura",
        "doorId": "puerta_lab",
        "centerU": 0.0,
        "width": 1.04,
        "height": 2.1,
        "base": 0.0
      }
    ]
  }
}
```

La puerta animada enlaza de vuelta a la pared y al ID del hueco:

```json
{
  "id": "puerta_lab",
  "model": "../animations/Puerta_Lab.glb",
  "position": [0.0, 4.0, -1.5],
  "rotation": [0.0, 0.0, 0.0],
  "scale": [1.0, 1.0, 1.0],
  "wallId": "pared_laboratorio_norte",
  "openingId": "puerta_lab_apertura",
  "colliders": [
    { "node": "DoorFrame", "type": "static", "halfSize": [0.05, 1.05, 0.04], "offset": [-0.45, 1.05, -0.013] },
    { "node": "DoorFrame", "type": "static", "halfSize": [0.05, 1.05, 0.04], "offset": [0.45, 1.05, -0.013] },
    { "node": "DoorFrame", "type": "static", "halfSize": [0.4, 0.05, 0.04], "offset": [0.0, 2.05, -0.013] },
    { "node": "Door", "type": "kinematic", "halfSize": [0.418, 1.005, 0.017], "offset": [-0.418, -0.041, -0.017] }
  ]
}
```

Los campos `wall`, `wallId` y `openingId` son opcionales, por lo que las escenas antiguas que no los usan siguen cargando. Cuando una puerta se asocia, las referencias deben ser recíprocas y únicas. Un hueco conservado sin puerta tiene `doorId` vacío, pero mantiene su propio `id`.

Una pared paramétrica necesita collider estático centrado en el cubo base de la pared (`halfSize` y `offset` unitarios/cero); así la escala de la instancia es la dimensión física y visual que usa el generador. El cargador rechaza definiciones paramétricas con una forma de collider distinta para no cortar una geometría diferente de la que se renderiza.

## Cálculo de los sólidos alrededor del hueco

`GenerateWallSections` recibe las semidimensiones locales de la pared y sus aperturas. Para cada hueco comprueba valores finitos y positivos, que no invada un margen de esquina de 10 cm, que no llegue al techo y que no solape otro hueco. Ordena aperturas por `centerU`.

Para una pared de 6 m de ancho, 30 cm de grosor y 3 m de alto:

- Semidimensiones: `(3.0, 0.15, 1.5)` m.
- Hueco centrado: `centerU = 0.0` m.
- Puerta: `width = 1.0` m, `height = 2.1` m, `base = 0.0` m.
- Bordes horizontales del hueco: `left = -0.5` m y `right = +0.5` m.
- Borde inferior: `-1.5 + base = -1.5` m, al nivel del suelo de la pared.
- Borde superior: `-1.5 + base + height = +0.6` m.

El generador produce tres cajas sólidas:

1. Paño izquierdo: centro X `-1.75`, semiancho `1.25`, semigrosor `0.15`, semialtura `1.5` m.
2. Paño derecho: centro X `+1.75`, semiancho `1.25`, semigrosor `0.15`, semialtura `1.5` m.
3. Dintel: centro X `0`, centro Z `1.05`, semiancho `0.5`, semigrosor `0.15`, semialtura `0.45` m.

Las cajas laterales definen las jambas y el dintel define el borde superior del paso. En esta variante `base = 0`, por lo que no hace falta un antepecho. El cálculo se hace en coordenadas locales; el centro y la orientación de cada segmento se transforman con la rotación y posición de la instancia. El material de pared se conserva en los segmentos.

## Colliders Bullet

`PhysicsScene::Create` usa la misma lista calculada por `GenerateWallSections` para sustituir el collider de caja completa por un collider estático por segmento. Cada centro local se transforma con la rotación de la pared y cada media dimensión se entrega a `PhysicsWorld::CreateBox`.

Esto mantiene una colisión por paño sólido, en lugar de una colisión que tape el hueco. Los colliders de marco y hoja del GLB se crean además desde sus nodos definidos en JSON. La hoja conserva su collider cinemático y se sincroniza con la animación; el hueco de pared no depende del estado abierto/cerrado de la hoja.

Al cambiar una puerta instalada, el editor marca que hay que reconstruir los cuerpos de la escena para que la copia física corresponda con la geometría y los datos nuevos. Bullet no corta mallas automáticamente: recibe las cajas que ya se calcularon.

## Colocar, mover y quitar puertas

El navegador de assets detecta GLB bajo la raíz de animaciones configurada. Para el flujo actual, se selecciona el GLB, se pulsa **Add to scene**, se selecciona la instancia animada y, desde el inspector, se escoge una pared paramétrica y se pulsa la acción de instalar/mover apertura. La anchura y altura se derivan de los límites de los colliders del nodo `DoorFrame`; el hueco recibe un pequeño margen de montaje.

La instalación prepara primero una apertura propuesta y valida límites y solapes. Si falla, restaura puerta y paredes anteriores. Si funciona, actualiza la asociación de la puerta, traslada el hueco anterior si existía, sincroniza el GLB y solicita reconstrucción de Bullet. La duplicación crea nuevos IDs, una nueva apertura desplazada sobre la misma pared, y conserva el GLB original como recurso compartido.

Al eliminar una puerta asociada, el editor permite:

- **Delete + close opening**: quita puerta y apertura; la pared vuelve a ser continua.
- **Delete only; keep opening**: quita la puerta y conserva el hueco sin asociación.

La validación del editor permite que una puerta asociada coexista con la pared que contiene su apertura, pero sigue validando la puerta contra los segmentos restantes y los demás objetos. No es necesario activar la opción insegura de penetrar arquitectura.

## Undo/Redo y Save

Las operaciones estructurales registran una instantánea del estado previo de la escena en el historial de `SceneEditor`. Una operación válida de instalación o eliminación modifica puerta y pared como una sola acción de usuario; un rechazo no registra historial ni deja media apertura. Undo/Redo restaura los datos y la sincronización física se solicita de nuevo.

El serializador conserva campos desconocidos de las instancias existentes y escribe `wall.apertures` dentro del objeto de pared. Para puertas existentes actualiza posición, rotación, escala y asociación; para instancias nuevas escribe rutas relativas al directorio de escena. Save y el cargador verifican que las asociaciones sigan apuntando al mismo hueco.

## Diferencias respecto a los proyectos estudiados

### GodotLevelBuilder

[GodotLevelBuilder](https://github.com/glebasos/GodotLevelBuilder) publica licencia MIT ([LICENSE](https://github.com/glebasos/GodotLevelBuilder/blob/master/LICENSE)). Su `WallPrimitive` descompone una pared en bandas entre los bordes de aperturas, genera caras frontales, traseras y de revelado, y reutiliza la malla para crear una colisión trimesh. `OpeningPlacement` centraliza snap, límites y solapes; `AddOpeningCommand` y `RemoveOpeningCommand` encapsulan la mutación con Undo.

EscapeTheUAB usa una descomposición más pequeña para paredes rectangulares: cajas 3D que comparten representación visual y física de Bullet. La asociación puerta-pared se guarda en el JSON de la escena, y la hoja GLB aporta sus colliders animados. No depende de Godot, de su `ArrayMesh` ni de trimesh.

La propuesta de comandos de apertura es una buena referencia para separar una acción editable, pero el editor actual usa historial por instantáneas y no necesita introducir una segunda infraestructura de comandos para este sistema.

### Home-Builder

[Home-Builder](https://github.com/AdamSzukalski/Home-Builder/blob/main/Source/HomeBuilder/Building/BuildingMesh.cpp) construye paredes a lo largo de splines. `BuildWall()` muestrea la spline en sus puntos y en los límites de las aperturas, calcula offsets de esquinas, genera las caras por tramos y mantiene UV basadas en distancia para que el material no cambie de escala con cada segmento.

El repositorio consultado no declara licencia SPDX en sus metadatos de GitHub y no se encontró un archivo `LICENSE` en la raíz. Por eso se estudió únicamente para entender el enfoque spline/UV; no se copió código ni se incorporó dependencia. Ese diseño resuelve paredes curvas, uniones y esquinas de spline, problemas que quedan fuera del modelo de pared rectangular actual de EscapeTheUAB.

## Archivos de EscapeTheUAB para estudiar

- `src/scene/SceneLoader.h`: estructuras `SceneWall`, `SceneWallOpening`, `SceneWallSection` y datos de asociación de la puerta.
- `src/scene/SceneLoader.cpp`: `GenerateWallSections` y lectura/validación de `wall.apertures`, `wallId` y `openingId`.
- `src/app/`: renderizado de cada tramo generado con el material de la instancia.
- `src/physics/PhysicsScene.cpp`: reemplazo del collider completo por cajas Bullet de los tramos.
- `src/editor/SceneEditor.cpp`: `BoxesOfObject`, `ValidateAnimated`, `InstallDoorInWall`, sincronización, duplicación y eliminación.
- `src/editor/SceneSerializer.cpp`: persistencia de aperturas, asociaciones y puertas nuevas.
- `tests/SceneLoaderTests.cpp`: límites y validación de las aperturas.
- `tests/EditorPersistenceTests.cpp`: Save/Reload y apertura que se conserva sin puerta.
- `tests/PhysicsTests.cpp`: colisión en vano con puerta cerrada/abierta y reconstrucción de colliders.

## Límites conocidos

- Las paredes divididas manualmente de escenas antiguas no se migran automáticamente. Vincular una puerta con segmentos y dinteles exige conocer qué fragmentos forman el mismo vano; inferirlo solo por distancia o nombres podría borrar arquitectura válida. Se conserva íntegro el JSON antiguo y la migración debe ser una operación explícita y revisada.
- La interfaz actual usa selección de asset, **Add to scene** y el inspector para asociar una pared. Todavía no existe arrastre directo desde el Asset Browser con una previsualización de apertura en el viewport.
- La parametrización supone paredes rectangulares alineadas localmente y aperturas que empiezan en la base cuando `base = 0`. Curvas, paredes con varias aperturas apiladas y materiales con UV continuas por toda la superficie requieren una extensión posterior.
- Las pruebas automatizadas validan partición, persistencia y paso Bullet, pero no sustituyen una comprobación interactiva de colocación y aspecto en la ventana del juego.
