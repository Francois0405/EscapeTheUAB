#pragma once

// Presentacion en primera persona del objeto sostenido. Solo GLM: sin OpenGL, escena ni fisica.
// Decide si el objeto admite viewmodel y calcula la matriz de dibujo local; la pose fisica,
// la red y el lanzamiento no intervienen.
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace viewmodel
{
	constexpr float kScreenX = 0.82f;       // fraccion del ancho, desde la izquierda
	constexpr float kScreenY = 0.78f;       // fraccion del alto, desde arriba
	constexpr float kMinApparent = 0.18f;   // tamano aparente como fraccion del alto
	constexpr float kMaxApparent = 0.24f;
	constexpr float kDepth = 1.0f;          // metros delante de la camara
	constexpr float kMaxWorldExtent = 0.6f; // mas grande: se usa la representacion fisica

	// Indice del objeto que sostiene ESTE jugador: Bullet en host/offline y, en un cliente,
	// el que el host le haya asignado (por hash del id). -1 si no sostiene nada.
	inline int SelectLocalHeld(int localGrab, bool isClient, std::uint32_t clientHeldHash,
		const std::vector<std::uint32_t>& objectHashes)
	{
		if (localGrab >= 0) return localGrab;
		if (!isClient || clientHeldHash == 0) return -1;
		for (std::size_t i = 0; i < objectHashes.size(); ++i)
			if (objectHashes[i] == clientHeldHash) return static_cast<int>(i);
		return -1;
	}

	struct Object
	{
		std::string id;
		glm::vec3 eulerDegrees{ 0.0f };
		glm::vec3 scale{ 1.0f };
		glm::vec3 boundsMin{ 0.0f };   // limites locales del modelo
		glm::vec3 boundsMax{ 0.0f };
	};

	// slide va de 0 (recien agarrado, algo mas abajo) a 1 (posicion final).
	// Devuelve false (dibujar el objeto fisico) para Hiriti, objetos grandes o datos no validos.
	inline bool Build(const Object& object, const glm::mat4& view, const glm::mat4& projection,
		float slide, glm::mat4& model)
	{
		if (object.id == "hiriti") return false;
		const float p00 = projection[0][0], p11 = projection[1][1];
		if (!(p00 > 0.0f) || !(p11 > 0.0f)) return false;

		const glm::vec3 mundo = glm::abs(object.scale) * (object.boundsMax - object.boundsMin);
		const float extension = std::max({ mundo.x, mundo.y, mundo.z });
		if (!std::isfinite(extension) || extension < 1.0e-4f || extension > kMaxWorldExtent) return false;

		// El juego usa Z arriba; en coordenadas de vista arriba es Y. Se conserva el giro de autor.
		glm::mat4 giro = glm::rotate(glm::mat4(1.0f), glm::radians(-25.0f), glm::vec3(0, 1, 0));
		giro = glm::rotate(giro, glm::radians(-90.0f), glm::vec3(1, 0, 0));
		giro = glm::rotate(giro, glm::radians(object.eulerDegrees.z), glm::vec3(0, 0, 1));
		giro = glm::rotate(giro, glm::radians(object.eulerDegrees.y), glm::vec3(0, 1, 0));
		giro = glm::rotate(giro, glm::radians(object.eulerDegrees.x), glm::vec3(1, 0, 0));
		const glm::mat4 orientado = glm::scale(giro, object.scale);

		glm::vec3 lo(std::numeric_limits<float>::max()), hi(-std::numeric_limits<float>::max());
		for (int i = 0; i < 8; ++i)
		{
			const glm::vec3 esquina((i & 1) ? object.boundsMax.x : object.boundsMin.x,
				(i & 2) ? object.boundsMax.y : object.boundsMin.y,
				(i & 4) ? object.boundsMax.z : object.boundsMin.z);
			const glm::vec3 p = glm::vec3(orientado * glm::vec4(esquina, 1.0f));
			lo = glm::min(lo, p);
			hi = glm::max(hi, p);
		}
		const glm::vec3 tamano = hi - lo;
		const float mayor = std::max({ tamano.x, tamano.y, 1.0e-4f });

		const float altoVisible = 2.0f * kDepth / p11;   // alto de pantalla a esa profundidad
		const float aparente = std::clamp(mayor / altoVisible, kMinApparent, kMaxApparent);
		float escala = aparente * altoVisible / mayor;
		escala = std::min(escala, 0.5f * kDepth / std::max(tamano.z, 1.0e-4f));   // lejos del plano cercano

		const float entrada = 1.0f - std::clamp(slide, 0.0f, 1.0f);
		const glm::vec3 destino((2.0f * kScreenX - 1.0f) * kDepth / p00,
			(1.0f - 2.0f * kScreenY) * kDepth / p11 - entrada * 0.12f * altoVisible, -kDepth);

		model = glm::inverse(view) * glm::translate(glm::mat4(1.0f), destino) *
			glm::scale(glm::mat4(1.0f), glm::vec3(escala)) *
			glm::translate(glm::mat4(1.0f), -0.5f * (lo + hi)) * orientado;
		return true;
	}
}
