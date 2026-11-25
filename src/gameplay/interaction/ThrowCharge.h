#pragma once

// Aritmetica pura de la carga del lanzamiento (sin ventana, red ni fisica).
#include <algorithm>

namespace juego::interaction
{
	inline constexpr float kMaxThrowChargeSeconds = 1.25f;

	inline float ThrowChargeFraction(float chargeSeconds)
	{
		return std::clamp(chargeSeconds / kMaxThrowChargeSeconds, 0.0f, 1.0f);
	}

	// De 3 m/s (sin carga) a 15 m/s (carga completa): el maximo que admite el host.
	inline float ThrowSpeed(float chargeFraction)
	{
		return 3.0f + 12.0f * chargeFraction;
	}

	inline float AdvanceThrowCharge(float chargeSeconds, float delta)
	{
		return std::min(chargeSeconds + std::max(delta, 0.0f), kMaxThrowChargeSeconds);
	}
}
