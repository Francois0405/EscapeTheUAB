#ifndef ENTORNVGI_ANIMACION_HIRITI_H
#define ENTORNVGI_ANIMACION_HIRITI_H

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/glm.hpp>
#include <limits>

namespace juego
{
	class AnimacionHiriti
	{
	public:
		static constexpr int NumeroBotes = 7;
		static constexpr float AlcanceDescubrimiento = 10.0f;
		static constexpr float TiempoHastaAgarrame = 5.0f;
		static constexpr float PotenciaMinimaSonidoDanio = 0.5f;
		static constexpr float TiempoEstableParaReanudarSeguimiento = 0.75f;
		static constexpr float VelocidadEstableParaReanudarSeguimiento = 0.15f;
		static constexpr float AlcanceAnticipacionObstaculos = 2.2f;
		static constexpr float CooldownReaparicion = 30.0f;
		static constexpr float DistanciaParaReaparecer = 10.0f;

		bool ActualizarTemporizadorReaparicion(bool estaLejos, bool interaccionJugador, float delta)
		{
			if (std::isfinite(delta) && delta > 0.0f)
				m_tiempoDesdeReaparicion += delta;
			return estaLejos && !interaccionJugador &&
				m_tiempoDesdeReaparicion >= CooldownReaparicion;
		}

		void RegistrarReaparicion()
		{
			m_tiempoDesdeReaparicion = 0.0f;
		}

		bool SeguimientoSuspendido() const { return m_seguimientoSuspendido; }
		float TiempoDesdeReaparicion() const { return m_tiempoDesdeReaparicion; }

		static glm::vec3 CalcularDireccionSeguimiento(const glm::vec3& posicionHiriti,
			const glm::vec3& posicionJugador, const std::array<float, 5>& distanciasObstaculos)
		{
			const glm::vec2 haciaJugador(posicionJugador.x - posicionHiriti.x,
				posicionJugador.y - posicionHiriti.y);
			const float distancia = glm::length(haciaJugador);
			if (!std::isfinite(distancia) || distancia < 1.0e-4f) return glm::vec3(0.0f);

			const glm::vec2 objetivo = haciaJugador / distancia;
			constexpr float angulos[] = { 0.0f, 0.7853981634f, -0.7853981634f, 1.5707963268f, -1.5707963268f };
			float mejorPuntuacion = -std::numeric_limits<float>::infinity();
			glm::vec2 mejorDireccion = objetivo;
			for (std::size_t i = 0; i < distanciasObstaculos.size(); ++i)
			{
				const float distanciaObstaculo = distanciasObstaculos[i];
				if (!std::isfinite(distanciaObstaculo)) continue;
				const float angulo = angulos[i];
				const float coseno = std::cos(angulo);
				const float seno = std::sin(angulo);
				const glm::vec2 candidata(
					objetivo.x * coseno - objetivo.y * seno,
					objetivo.x * seno + objetivo.y * coseno);
				const float alineacion = glm::dot(candidata, objetivo);
				const float espacioLibre = std::clamp(
					distanciaObstaculo / AlcanceAnticipacionObstaculos, 0.0f, 1.0f);
				const float puntuacion = alineacion * 0.65f + espacioLibre * 0.35f;
				if (puntuacion > mejorPuntuacion)
				{
					mejorPuntuacion = puntuacion;
					mejorDireccion = candidata;
				}
			}
			return glm::vec3(mejorDireccion.x, mejorDireccion.y, 0.0f);
		}

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

		// Un agarre físico validado por el host también cuenta como descubrimiento, incluso si
		// la detección visual local y la hitbox no coinciden exactamente entre dos réplicas.
		bool DescubrirPorInteraccion()
		{
			if (m_descubierto) return false;
			m_descubierto = true;
			m_esperandoAgarrame = false;
			m_tiempoDesdeDescubrimiento = 0.0f;
			return true;
		}

		void MarcarLanzadoPorJugador(float potencia)
		{
			m_sonidoDanioPendiente = std::isfinite(potencia) && potencia > PotenciaMinimaSonidoDanio;
			m_seguimientoSuspendido = true;
			m_tiempoEstable = 0.0f;
		}

		int ActualizarContacto(bool hayContacto)
		{
			const bool nuevoImpacto = m_sonidoDanioPendiente && hayContacto && !m_habiaContacto;
			m_habiaContacto = hayContacto;
			if (!nuevoImpacto) return -1;

			m_sonidoDanioPendiente = false;
			const int variante = m_siguienteSonidoDanio;
			m_siguienteSonidoDanio = 1 - m_siguienteSonidoDanio;
			return variante;
		}

		bool ActualizarSuspensionSeguimiento(bool jugadorManipula, const glm::vec3& velocidad, float delta)
		{
			if (jugadorManipula)
			{
				m_seguimientoSuspendido = true;
				m_tiempoEstable = 0.0f;
				return false;
			}
			if (!m_seguimientoSuspendido) return true;

			if (std::isfinite(delta) && delta > 0.0f &&
				std::isfinite(velocidad.x) && std::isfinite(velocidad.y) && std::isfinite(velocidad.z) &&
				glm::length(velocidad) <= VelocidadEstableParaReanudarSeguimiento)
			{
				m_tiempoEstable += delta;
				if (m_tiempoEstable >= TiempoEstableParaReanudarSeguimiento)
				{
					m_seguimientoSuspendido = false;
					m_tiempoEstable = 0.0f;
					m_tiempoDesdeReaparicion = 0.0f;
					return true;
				}
			}
			else
				m_tiempoEstable = 0.0f;
			return false;
		}

		static glm::vec3 CalcularAceleracionSeguimiento(const glm::vec3& posicionHiriti,
			const glm::vec3& posicionJugador, const glm::vec3& velocidad,
			const glm::vec3& direccionSeguimiento, float distanciaParada,
			float velocidadMaxima, float aceleracionMaxima)
		{
			const glm::vec2 haciaJugador(posicionJugador.x - posicionHiriti.x,
				posicionJugador.y - posicionHiriti.y);
			const glm::vec2 velocidadHorizontal(velocidad.x, velocidad.y);
			const float distancia = glm::length(haciaJugador);
			if (!std::isfinite(distancia) || !std::isfinite(velocidadHorizontal.x) ||
				!std::isfinite(velocidadHorizontal.y) || !std::isfinite(distanciaParada) ||
				!std::isfinite(velocidadMaxima) || !std::isfinite(aceleracionMaxima) ||
				distanciaParada < 0.0f ||
				velocidadMaxima <= 0.0f || aceleracionMaxima <= 0.0f)
				return glm::vec3(0.0f);

			const glm::vec2 direccionHorizontal(direccionSeguimiento.x, direccionSeguimiento.y);
			const float longitudDireccion = glm::length(direccionHorizontal);
			const glm::vec2 direccion = longitudDireccion > 1.0e-4f
				? direccionHorizontal / longitudDireccion : glm::vec2(0.0f);
			const float distanciaDeFrenado = std::max(0.0f, distancia - distanciaParada);
			const float rapidezDeseada = std::min(velocidadMaxima,
				std::sqrt(2.0f * aceleracionMaxima * distanciaDeFrenado));
			const glm::vec2 velocidadDeseada = direccion * rapidezDeseada;
			glm::vec2 aceleracion = (velocidadDeseada - velocidadHorizontal) * 2.5f;
			const float magnitud = glm::length(aceleracion);
			if (magnitud > aceleracionMaxima)
				aceleracion *= aceleracionMaxima / magnitud;
			return glm::vec3(aceleracion.x, aceleracion.y, 0.0f);
		}

		void Reiniciar()
		{
			m_descubierto = false;
			m_esperandoAgarrame = false;
			m_agarrado = false;
			m_tiempoDesdeDescubrimiento = 0.0f;
			m_tiempoDesdeReaparicion = 0.0f;
			m_sonidoDanioPendiente = false;
			m_habiaContacto = false;
			m_siguienteSonidoDanio = 0;
			m_seguimientoSuspendido = false;
			m_tiempoEstable = 0.0f;
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
		bool m_sonidoDanioPendiente = false;
		bool m_habiaContacto = false;
		int m_siguienteSonidoDanio = 0;
		bool m_seguimientoSuspendido = false;
		float m_tiempoEstable = 0.0f;
		float m_tiempoDesdeReaparicion = 0.0f;
	};
}

#endif
