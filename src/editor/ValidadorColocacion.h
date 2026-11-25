#pragma once

#include <glm/glm.hpp>

#include <array>
#include <string>
#include <vector>

namespace editor
{
	struct CajaOrientada
	{
		glm::vec3 centro{ 0.0f };
		std::array<glm::vec3, 3> ejes{ glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1) };
		glm::vec3 mitad{ 0.5f };
		bool valida = false;
	};

	struct ObstaculoColocacion
	{
		CajaOrientada caja;
		std::string id;
		std::string nombre;
		bool arquitectura = false;
	};

	struct ResultadoColocacion
	{
		bool valida = true;
		std::string bloqueador;
	};

	// Transforma una caja local respetando el giro y la escala de una instancia.
	CajaOrientada TransformarCaja(const glm::mat4& modelo, const glm::vec3& centroLocal, const glm::vec3& mitadLocal);

	// Devuelve la profundidad mínima SAT en metros; cero significa separación o contacto permitido.
	float PenetracionOBB(const CajaOrientada& a, const CajaOrientada& b, float toleranciaContacto = 0.01f);

	// Las penetraciones preexistentes se pueden reducir sin bloquear la edición de escenas antiguas.
	// La excepción omite únicamente obstáculos marcados como arquitectura y solo es opt-in.
	ResultadoColocacion ValidarColocacion(const std::vector<CajaOrientada>& propuesta,
		const std::vector<CajaOrientada>& anterior, const std::vector<ObstaculoColocacion>& obstaculos,
		bool permitirArquitectura, float toleranciaContacto = 0.01f);
}
