// NetAvatar.h
// Dibujo de los jugadores remotos y del objeto compartido de prueba: cajas de lineas con el shader de ejes
// (eixos.vert/frag) que ya usa DibujoColisionadores. NO toca shaders. El jugador local NO se dibuja.
#pragma once

#include "src/networking/replication/NetJuegoLogica.h"

namespace netjuego
{
	// 'programaEjes' = shader eixos ya compilado; matrices de la camara actual (escritorio o un ojo VR).
	void DibujarAvataresRemotos(unsigned int programaEjes, const glm::mat4& proyeccion, const glm::mat4& vista,
		const std::vector<net::PoseEntry>& remotos, bool dibujarObjeto, const glm::vec3& posObjeto, bool objetoMio, bool objetoOcupado);
	void LiberarDibujoAvatares();   // con el contexto GL activo, antes de cerrar la ventana
}
