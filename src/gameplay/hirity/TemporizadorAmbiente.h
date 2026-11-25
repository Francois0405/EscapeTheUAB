#ifndef ENTORNVGI_TEMPORIZADOR_AMBIENTE_H
#define ENTORNVGI_TEMPORIZADOR_AMBIENTE_H

#include <cmath>

namespace juego
{
	class TemporizadorAudioHiriti
	{
	public:
		enum class Evento
		{
			Ninguno,
			Ambiente,
			Buscando
		};

		static constexpr float IntervaloAmbiente = 30.0f;
		static constexpr float IntervaloEntreEventos = 15.0f;

		Evento Actualizar(bool activo, float delta)
		{
			if (!activo)
			{
				m_tiempo = 0.0f;
				m_siguienteEvento = Evento::Ambiente;
				m_esPrimerEvento = true;
				return Evento::Ninguno;
			}
			if (!std::isfinite(delta) || delta <= 0.0f) return Evento::Ninguno;

			m_tiempo += delta;
			const float intervalo = m_esPrimerEvento ? IntervaloAmbiente : IntervaloEntreEventos;
			if (m_tiempo < intervalo) return Evento::Ninguno;

			m_tiempo = std::fmod(m_tiempo, intervalo);
			const Evento evento = m_siguienteEvento;
			m_siguienteEvento = evento == Evento::Ambiente ? Evento::Buscando : Evento::Ambiente;
			m_esPrimerEvento = false;
			return evento;
		}

	private:
		float m_tiempo = 0.0f;
		Evento m_siguienteEvento = Evento::Ambiente;
		bool m_esPrimerEvento = true;
	};
}

#endif
