// DibujoColisionadores.h
// -----------------------------------------------------------------------------
// Opción de depuración "Mostrar colisionadores": dibuja las cajas del mundo físico como
// líneas de colores. Solo LEE el mundo (no modifica ningún cuerpo).
//   azul = estático      verde = dinámico (verde oscuro si está dormido)      naranja = cinemático
// Usa el shader de ejes (eixos.vert/frag), que ya pinta líneas con color por vértice en
// coordenadas del mundo.
// -----------------------------------------------------------------------------
#pragma once

#include "MundoFisico.h"

namespace fisica
{
	// 'programaEjes' es el shader de eixos ya compilado; las matrices son las de la cámara.
	void DibujarColisionadores(unsigned int programaEjes, const glm::mat4& proyeccion, const glm::mat4& vista,
		const std::vector<CajaColision>& cajas);

	// Libera los buffers de OpenGL (llamar con el contexto activo, antes de cerrar la ventana).
	void LiberarDibujoColisionadores();
}
