# Linterna FPS y preparación del brazo de interacción

La linterna de `assets/models/items/3dexter/flashlight.obj` se carga una vez al iniciar la aplicación mediante `COBJModel::LoadModel`. Su MTL usa `map_Kd flashlight.png` con ruta relativa. El modelo está centrado, normalizado y orientado a lo largo del eje X; la transformación local lo gira 90 grados sobre Y para apuntarlo hacia el frente de la cámara.

La malla `arms_rig2.obj` no se puede separar de forma segura con el cargador actual. Aunque el archivo contiene dos etiquetas `g`, sus caras usan el mismo material (`Arms`) y el cargador `COBJModel` separa VAO por material, no por grupo ni por una jerarquía de partes. Dibujar una selección de caras desde la aplicación requeriría modificar el cargador del profesor o alterar la geometría sin conocer los límites anatómicos; por ahora ambos brazos quedan cargados, pero no se dibujan durante el modo FPS.

Para habilitar una animación independiente del brazo derecho, preparar en Blender una malla separada del brazo y la mano derechos, preservando UV, normales, material, textura y licencia del asset. Orientar y colocar su origen en el hombro o punto de pivote de alcance, y exportar un OBJ independiente con el MTL y las texturas relativas. Con esa pieza se puede añadir después una animación de alcanzar y volver; la tecla E no se asigna hasta que exista ese modelo independiente.

El pack de linterna y llave se identifica como Horror Player Item Pack V2.0 de 3Dexter3D bajo CC0 1.0 en `assets/licenses/3dexter_horror_player_item_pack.txt`.
