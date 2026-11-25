#ifndef ENTORNVGI_TEMPORIZADOR_AMBIENTE_H
#define ENTORNVGI_TEMPORIZADOR_AMBIENTE_H

#include <cmath>

namespace game
{
	class HiritiAudioTimer
	{
	public:
		enum class Evento
		{
			None,
			Ambient,
			Searching
		};

		static constexpr float IntervaloAmbiente = 30.0f;
		static constexpr float IntervaloEntreEventos = 15.0f;

		Evento Update(bool activo, float delta)
		{
			if (!activo)
			{
				m_tiempo = 0.0f;
				m_siguienteEvento = Evento::Ambient;
				m_esPrimerEvento = true;
				return Evento::None;
			}
			if (!std::isfinite(delta) || delta <= 0.0f) return Evento::None;

			m_tiempo += delta;
			const float intervalo = m_esPrimerEvento ? IntervaloAmbiente : IntervaloEntreEventos;
			if (m_tiempo < intervalo) return Evento::None;

			m_tiempo = std::fmod(m_tiempo, intervalo);
			const Evento evento = m_siguienteEvento;
			m_siguienteEvento = evento == Evento::Ambient ? Evento::Searching : Evento::Ambient;
			m_esPrimerEvento = false;
			return evento;
		}

	private:
		float m_tiempo = 0.0f;
		Evento m_siguienteEvento = Evento::Ambient;
		bool m_esPrimerEvento = true;
	};
}

#endif
