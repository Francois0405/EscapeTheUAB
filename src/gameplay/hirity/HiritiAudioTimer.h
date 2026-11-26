#ifndef ENTORNVGI_TEMPORIZADOR_AMBIENTE_H
#define ENTORNVGI_TEMPORIZADOR_AMBIENTE_H

#include <cmath>

namespace game
{
	class HiritiAudioTimer
	{
	public:
		enum class AudioEvent
		{
			None,
			Ambient,
			Searching
		};

		static constexpr float IntervaloAmbiente = 30.0f;
		static constexpr float IntervaloEntreEventos = 15.0f;

		AudioEvent Update(bool activo, float delta)
		{
			if (!activo)
			{
				m_tiempo = 0.0f;
				m_siguienteEvento = AudioEvent::Ambient;
				m_esPrimerEvento = true;
				return AudioEvent::None;
			}
			if (!std::isfinite(delta) || delta <= 0.0f) return AudioEvent::None;

			m_tiempo += delta;
			const float intervalo = m_esPrimerEvento ? IntervaloAmbiente : IntervaloEntreEventos;
			if (m_tiempo < intervalo) return AudioEvent::None;

			m_tiempo = std::fmod(m_tiempo, intervalo);
			const AudioEvent evento = m_siguienteEvento;
			m_siguienteEvento = evento == AudioEvent::Ambient ? AudioEvent::Searching : AudioEvent::Ambient;
			m_esPrimerEvento = false;
			return evento;
		}

	private:
		float m_tiempo = 0.0f;
		AudioEvent m_siguienteEvento = AudioEvent::Ambient;
		bool m_esPrimerEvento = true;
	};
}

#endif
