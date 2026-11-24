#ifndef ENTORNVGI_ANIMACION_HIRITI_H
#define ENTORNVGI_ANIMACION_HIRITI_H

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace juego
{
	class AnimacionHiriti
	{
	public:
		static constexpr int NumeroBotes = 7;
		static constexpr float AlcanceDescubrimiento = 10.0f;
		static constexpr float TiempoHastaAgarrame = 5.0f;
		static constexpr float PotenciaMinimaSonidoDanio = 0.5f;

		bool Actualizar(bool puedeDescubrir, const glm::vec3& origen,
			const glm::vec3& direccion, const glm::vec3& centro, float radio)
		{
			if (!m_descubierto && puedeDescubrir && EstaMirando(origen, direccion, centro, radio))
			{
				m_descubierto = true;
				m_esperandoAgarrame = true;
				m_tiempoDesdeDescubrimiento = 0.0f;
				return true;
			}
			return false;
		}

		bool ActualizarAgarrame(float delta)
		{
			if (!m_esperandoAgarrame || !std::isfinite(delta) || delta <= 0.0f) return false;

			m_tiempoDesdeDescubrimiento += delta;
			if (m_tiempoDesdeDescubrimiento < TiempoHastaAgarrame) return false;

			m_esperandoAgarrame = false;
			return true;
		}

		bool MarcarAgarrado()
		{
			if (!m_descubierto || m_agarrado) return false;
			m_agarrado = true;
			m_esperandoAgarrame = false;
			return true;
		}

		void MarcarLanzadoPorJugador(float potencia)
		{
			m_lanzadoPorJugador = true;
			m_sonidoDanioHabilitado = std::isfinite(potencia) && potencia >= PotenciaMinimaSonidoDanio;
		}

		int ActualizarContacto(bool hayContacto)
		{
			const bool nuevoImpacto = m_lanzadoPorJugador && m_sonidoDanioHabilitado &&
				hayContacto && !m_habiaContacto;
			m_habiaContacto = hayContacto;
			if (!nuevoImpacto) return -1;

			const int variante = m_siguienteSonidoDanio;
			m_siguienteSonidoDanio = 1 - m_siguienteSonidoDanio;
			return variante;
		}

		void Reiniciar()
		{
			m_descubierto = false;
			m_esperandoAgarrame = false;
			m_agarrado = false;
			m_tiempoDesdeDescubrimiento = 0.0f;
			m_lanzadoPorJugador = false;
			m_sonidoDanioHabilitado = false;
			m_habiaContacto = false;
			m_siguienteSonidoDanio = 0;
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
		bool m_esperandoAgarrame = false;
		bool m_agarrado = false;
		float m_tiempoDesdeDescubrimiento = 0.0f;
		bool m_lanzadoPorJugador = false;
		bool m_sonidoDanioHabilitado = false;
		bool m_habiaContacto = false;
		int m_siguienteSonidoDanio = 0;
	};
}

#endif
