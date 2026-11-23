#ifndef ENTORNVGI_ANIMACION_HIRITI_H
#define ENTORNVGI_ANIMACION_HIRITI_H

#include <algorithm>
#include <glm/glm.hpp>

namespace juego
{
	class AnimacionHiriti
	{
	public:
		static constexpr int NumeroBotes = 7;
		static constexpr float AlcanceDescubrimiento = 10.0f;

		bool Actualizar(bool puedeDescubrir, const glm::vec3& origen,
			const glm::vec3& direccion, const glm::vec3& centro, float radio)
		{
			if (!m_descubierto && puedeDescubrir && EstaMirando(origen, direccion, centro, radio))
			{
				m_descubierto = true;
				return true;
			}
			return false;
		}

		void Reiniciar()
		{
			m_descubierto = false;
		}

	private:
		static bool EstaMirando(const glm::vec3& origen, const glm::vec3& direccion,
			const glm::vec3& centro, float radio)
		{
			const float longitudDireccion = glm::length(direccion);
			if (longitudDireccion < 1.0e-5f) return false;

			const glm::vec3 rayo = direccion / longitudDireccion;
			const glm::vec3 haciaCentro = centro - origen;
			const float distanciaSobreRayo = glm::dot(haciaCentro, rayo);
			if (distanciaSobreRayo <= 0.0f || distanciaSobreRayo > AlcanceDescubrimiento) return false;

			const glm::vec3 diferencia = haciaCentro - rayo * distanciaSobreRayo;
			const float radioDeteccion = std::max(radio * 1.5f, 0.1f);
			return glm::dot(diferencia, diferencia) <= radioDeteccion * radioDeteccion;
		}

		bool m_descubierto = false;
	};
}

#endif
