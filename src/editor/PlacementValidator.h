#pragma once

#include <glm/glm.hpp>

#include <array>
#include <string>
#include <vector>

namespace editor
{
	struct OrientedBox
	{
		glm::vec3 centro{ 0.0f };
		std::array<glm::vec3, 3> ejes{ glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1) };
		glm::vec3 mitad{ 0.5f };
		bool valida = false;
	};

	struct PlacementObstacle
	{
		OrientedBox caja;
		std::string id;
		std::string nombre;
		bool arquitectura = false;
	};

	struct PlacementResult
	{
		bool valida = true;
		std::string bloqueador;
	};

	// Transforma una caja local respetando el giro y la escala de una instancia.
	OrientedBox TransformBox(const glm::mat4& modelo, const glm::vec3& centroLocal, const glm::vec3& mitadLocal);

	// Devuelve la profundidad mínima SAT en metros; cero significa separación o contacto permitido.
	float ObbPenetration(const OrientedBox& a, const OrientedBox& b, float toleranciaContacto = 0.01f);

	// Calcula cuánto desplazar las cajas propuestas hacia +X para dejarlas a la derecha
	// de todos los obstáculos, con un margen. Devuelve false si no hay cajas válidas.
	bool CalculateStagingOffsetX(const std::vector<OrientedBox>& propuesta,
		const std::vector<OrientedBox>& obstaculos, float margen, float& desplazamiento);

	// Las penetraciones preexistentes se pueden reducir sin bloquear la edición de escenas antiguas.
	// La excepción omite únicamente obstáculos marcados como arquitectura y solo es opt-in.
	PlacementResult ValidatePlacement(const std::vector<OrientedBox>& propuesta,
		const std::vector<OrientedBox>& anterior, const std::vector<PlacementObstacle>& obstaculos,
		bool permitirArquitectura, float toleranciaContacto = 0.01f);
}
