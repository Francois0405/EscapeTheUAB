#pragma once

// Atenuacion de la voz de Hiriti segun su distancia a los ojos del jugador. Cabecera pura:
// sin escena, fisica, audio ni OpenGL, para poder probarla de forma aislada.
#include <algorithm>

namespace juego::hiriti
{
	inline float AttenuateVolumeByDistance(float baseVolume, float distance)
	{
		constexpr float distanciaCompleta = 2.0f;
		constexpr float distanciaMaxima = 20.0f;
		constexpr float volumenMinimo = 0.15f;
		const float atenuacion = 1.0f - (1.0f - volumenMinimo) *
			std::clamp((distance - distanciaCompleta) / (distanciaMaxima - distanciaCompleta), 0.0f, 1.0f);
		return baseVolume * atenuacion;
	}
}
