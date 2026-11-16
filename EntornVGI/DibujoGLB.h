// DibujoGLB.h
// -----------------------------------------------------------------------------
// Dibujo con OpenGL de un ModeloGLB (ver ModeloGLB.h para los datos).
//
// Se usa el MISMO shader que el resto de la escena (gouraud_shdrML), que espera:
//   atributo 0 = posición, atributo 2 = normal, uniform modelMatrix y material.*
// -----------------------------------------------------------------------------
#ifndef DIBUJO_GLB_H
#define DIBUJO_GLB_H

#include <glm/glm.hpp>
#include "ModeloGLB.h"
#include "ObjetosAnimadosGLB.h"

namespace glb
{
	// Copia las mallas a la tarjeta gráfica. Necesita un contexto OpenGL activo.
	void SubirModeloAGPU(ModeloGLB& modelo);

	// Libera lo que reservó SubirModeloAGPU. Llamar antes de cerrar la ventana.
	void LiberarModeloDeGPU(ModeloGLB& modelo);

	// Dibuja el modelo con una pose dada.
	//   programa    : shader ya activo (glUseProgram) con la cámara ya enviada.
	//   matrizBase  : dónde se coloca el modelo en el mundo del juego (ya en Z arriba).
	void DibujarModeloGLB(const ModeloGLB& modelo, const PoseGLB& pose, unsigned int programa, const glm::mat4& matrizBase);

	// Lo mismo para toda la lista de objetos animados: sube cada modelo una sola vez,
	// y dibuja cada instancia con su propia pose y su propia colocación.
	void SubirEscenaAnimadaAGPU(EscenaAnimada& escena);
	void LiberarEscenaAnimadaDeGPU(EscenaAnimada& escena);
	void DibujarObjetosAnimados(const EscenaAnimada& escena, unsigned int programa);
}

#endif
