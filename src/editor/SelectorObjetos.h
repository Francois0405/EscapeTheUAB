// SelectorObjetos.h
// -----------------------------------------------------------------------------
// Selección por rayo en el editor. Se basa en las cajas envolventes de los MODELOS (no en colisionadores Bullet),
// así que funciona con OBJ con y sin física, arquitectura, objetos pequeños y puertas GLB animadas.
// Sin ImGui ni OpenGL.
// -----------------------------------------------------------------------------
#pragma once

#include "LimitesModelo.h"

#include <glm/glm.hpp>

#include <vector>

namespace editor
{
	struct Rayo
	{
		glm::vec3 origen{ 0.0f };
		glm::vec3 direccion{ 0.0f, 0.0f, -1.0f };   // unitaria
		bool valido = false;
	};

	// Rayo que atraviesa el píxel 'cursor' del rectángulo del viewport {x, y, ancho, alto} (coordenadas de ventana,
	// Y hacia abajo). Sirve para proyección en perspectiva y ortográfica (cerca/lejos del frustum).
	Rayo RayoDesdePantalla(const glm::mat4& vista, const glm::mat4& proyeccion, const glm::vec2& cursor,
		const glm::vec4& rectangulo);

	// Un elemento seleccionable: 'modelo' lleva la caja local al mundo (posición, giro y escala del objeto).
	struct CandidatoSeleccion
	{
		int indice = -1;
		glm::mat4 modelo{ 1.0f };
		CajaLocal caja;
	};

	// Devuelve el 'indice' del candidato más cercano al origen del rayo, o -1. Si dos están a menos de 2 cm entre sí
	// gana el de menor volumen (un objeto sobre una mesa se elige antes que la mesa).
	int ElegirMasCercano(const Rayo& rayo, const std::vector<CandidatoSeleccion>& candidatos, float* distancia = nullptr);
}
