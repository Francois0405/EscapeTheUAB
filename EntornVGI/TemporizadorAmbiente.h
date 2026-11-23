#ifndef ENTORNVGI_TEMPORIZADOR_AMBIENTE_H
#define ENTORNVGI_TEMPORIZADOR_AMBIENTE_H

#include <cmath>

namespace juego
{
	class TemporizadorAmbiente
	{
	public:
		static constexpr float Intervalo = 30.0f;

		bool Actualizar(bool activo, float delta)
		{
			if (!activo)
			{
				m_tiempo = 0.0f;
				return false;
			}
			if (!std::isfinite(delta) || delta <= 0.0f) return false;

			m_tiempo += delta;
			if (m_tiempo < Intervalo) return false;

			m_tiempo = std::fmod(m_tiempo, Intervalo);
			return true;
		}

	private:
		float m_tiempo = 0.0f;
	};
}

#endif
