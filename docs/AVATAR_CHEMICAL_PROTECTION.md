# Auditoría del avatar de protección química

## Estado

La integración del personaje está pendiente. El original `man-in-chemical-protection/source/персонаж.blend` y la textura entregada se conservan sin cambios. No se generó GLB ni se añadió el modelo al motor: Blender no llegó a cargar el archivo y el cargador actual no implementa skinning esquelético.

## Asset y licencia

El ZIP proporcionado contiene `source/персонаж.blend` y `textures/персполитра.png`. No se encontró un archivo de licencia o atribución en el paquete ni en la carpeta entregada. La procedencia y los derechos de redistribución quedan sin verificar; no se debe distribuir el asset en una build hasta obtener esa información.

La cabecera del `.blend` identifica formato Blender 2.93. Esto no verifica desde Blender los rigs `metarig`/`rig`, huesos `DEF-*`, pesos, materiales, UV, restricciones, proporciones ni acciones. Tampoco confirma que la textura esté enlazada correctamente. La estructura binaria preliminar no reemplaza esa inspección.

## Intento de inspección en Blender

Se encontró `/Applications/Blender.app` (Blender 5.2.2). El intento de cargar el archivo en background terminó con código 139 antes de ejecutar el script de inspección; la traza cae durante la selección del backend Metal (`GPU_backend_type_selection_detect` / `WM_init`). Forzar `--gpu-backend opengl` tampoco permitió iniciar esta build, que solo acepta Metal. Por tanto, geometría, conteos, rig, deformación y exportación siguen sin verificarse directamente en Blender.

## Compatibilidad del motor encontrada en código

- `ModeloGLB.cpp::LeerPrimitiva` lee posiciones, normales, índices y color base; no lee `JOINTS_0`, `WEIGHTS_0`, UV ni texturas.
- `LeerNodos` detecta nodos con `skin` y advierte que no hay soporte para animación esquelética.
- `LeerAnimaciones` admite canales de traslación, rotación y escala de nodos; eso no equivale a deformar una malla con joints.
- `NetAvatar` representa jugadores con geometría de líneas y almacena las poses de cabeza/manos existentes. No crea instancias del modelo ni estados de animación por avatar.

Por tanto, exportar el modelo como GLB hoy no lo convertiría en un personaje animado utilizable. Antes de integrar hacen falta inspección visual del original, licencia confirmada y soporte de skinning en el cargador/renderizador. Mantener el avatar de líneas como fallback cuando se emprenda esa fase.

## Siguiente validación reproducible

1. Abrir el original con Blender 2.93 o una versión compatible en un entorno gráfico funcional; comprobar rig, modificador Armature, grupos de vértices, UV, material y textura.
2. Confirmar licencia y atribución antes de incluir el asset.
3. Exportar un GLB de prueba conservando malla, material y jerarquía de deformación; abrirlo de forma independiente y comprobar poses extremas.
4. Implementar y probar `JOINTS_0`, `WEIGHTS_0`, inverse bind matrices y skinning antes de sustituir el avatar de líneas.
5. Integrar una instancia que comparta mallas/texturas y mantenga pose/interpolación por jugador; validar primero escritorio y después PC + Quest 3 con hardware real.
