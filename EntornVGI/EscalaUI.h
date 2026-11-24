#pragma once

#include <algorithm>
#include <cmath>

namespace interfaz
{
	inline float CalcularEscalaUI(float anchoLogico, float altoLogico,
		float escalaDpiX, float escalaDpiY)
	{
		constexpr float anchoReferencia = 1280.0f;
		constexpr float altoReferencia = 720.0f;
		const float resolucionLogica = std::clamp(
			std::min(anchoLogico / anchoReferencia, altoLogico / altoReferencia),
			0.75f, 1.5f);
		const float dpi = std::clamp(
			std::sqrt(std::max(0.01f, escalaDpiX * escalaDpiY)),
			0.75f, 2.0f);
		return std::clamp(std::sqrt(resolucionLogica * dpi), 0.75f, 1.75f);
	}
}
