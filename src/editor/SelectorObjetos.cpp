#include "SelectorObjetos.h"

#include <cmath>
#include <limits>

namespace editor
{
	Rayo RayoDesdePantalla(const glm::mat4& vista, const glm::mat4& proyeccion, const glm::vec2& cursor,
		const glm::vec4& rectangulo)
	{
		Rayo rayo;
		if (rectangulo.z <= 0.0f || rectangulo.w <= 0.0f) return rayo;
		const float ndcX = ((cursor.x - rectangulo.x) / rectangulo.z) * 2.0f - 1.0f;
		const float ndcY = 1.0f - ((cursor.y - rectangulo.y) / rectangulo.w) * 2.0f;
		const glm::mat4 aMundo = glm::inverse(proyeccion * vista);
		const glm::vec4 cerca = aMundo * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
		const glm::vec4 lejos = aMundo * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
		if (std::abs(cerca.w) < 1.0e-9f || std::abs(lejos.w) < 1.0e-9f) return rayo;
		rayo.origen = glm::vec3(cerca) / cerca.w;
		const glm::vec3 fin = glm::vec3(lejos) / lejos.w;
		const glm::vec3 d = fin - rayo.origen;
		if (glm::length(d) < 1.0e-9f) return rayo;
		rayo.direccion = glm::normalize(d);
		rayo.valido = true;
		return rayo;
	}

	int ElegirMasCercano(const Rayo& rayo, const std::vector<CandidatoSeleccion>& candidatos, float* distancia)
	{
		if (!rayo.valido) return -1;
		int elegido = -1;
		float mejorT = std::numeric_limits<float>::max();
		float mejorVolumen = std::numeric_limits<float>::max();
		for (const CandidatoSeleccion& candidato : candidatos)
		{
			if (!candidato.caja.valida) continue;
			float t = 0.0f;
			if (!InterseccionRayoCaja(rayo.origen, rayo.direccion, candidato.modelo, candidato.caja, t)) continue;
			const float volumen = std::abs(glm::determinant(glm::mat3(candidato.modelo))) * candidato.caja.Volumen();
			const bool masCerca = t < mejorT - 0.02f;
			const bool empate = std::abs(t - mejorT) <= 0.02f && volumen < mejorVolumen;
			if (elegido < 0 || masCerca || empate)
			{
				elegido = candidato.indice;
				mejorT = std::min(mejorT, t);
				mejorVolumen = volumen;
			}
		}
		if (elegido >= 0 && distancia != nullptr) *distancia = mejorT;
		return elegido;
	}
}
