// VistaEstereo.h
// -----------------------------------------------------------------------------
// MODO ESTEREOSCÓPICO DE PRUEBA (tecla F2): la pantalla se divide en dos mitades, una por ojo.
// Sirve para probar el dibujo a dos vistas en un monitor. NO es VR real: no hay OpenXR, ni visor, ni
// seguimiento de cabeza, ni mandos; no funciona en un Meta Quest 2 ni en ningún otro casco.
//
// Con VR de verdad, OpenXR entregará la pose de cada ojo (posición y orientación) y su propia
// proyección; esta función desaparecerá y esas matrices ocuparán su lugar.
//
// Solo usa GLM (sin OpenGL ni Bullet), así que se puede probar sin ventana.
// -----------------------------------------------------------------------------
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace estereo
{
	// Distancia entre las pupilas: 6,4 cm de media.
	constexpr float MitadSeparacionOjos = 0.032f;

	enum class Ojo { Izquierdo, Derecho };

	// Matriz de vista de un ojo a partir de la vista CENTRAL (la de la cámara FPS).
	// Se desplaza el MUNDO en el espacio de la cámara: para el ojo izquierdo el mundo se mueve hacia
	// la derecha (+X de cámara), es decir, el ojo queda a la izquierda del centro. La orientación no cambia.
	inline glm::mat4 VistaDeOjo(const glm::mat4& vistaCentral, Ojo ojo, float mitadSeparacion = MitadSeparacionOjos)
	{
		const float desplazamiento = (ojo == Ojo::Izquierdo) ? mitadSeparacion : -mitadSeparacion;
		return glm::translate(glm::mat4(1.0f), glm::vec3(desplazamiento, 0.0f, 0.0f)) * vistaCentral;
	}

	// Rectángulo de pantalla de cada ojo: mitad izquierda / mitad derecha.
	struct Rectangulo { int x, ancho; };
	inline Rectangulo MitadDePantalla(Ojo ojo, int anchoTotal)
	{
		return { ojo == Ojo::Izquierdo ? 0 : anchoTotal / 2, anchoTotal / 2 };
	}
}
