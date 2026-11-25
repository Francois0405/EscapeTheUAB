#pragma once

#include <algorithm>
#include <cmath>

namespace interfaz
{
	struct UiWindowRect
	{
		float x = 0.0f;
		float y = 0.0f;
		float ancho = 0.0f;
		float alto = 0.0f;
	};

	inline float ComputeUiScale(float anchoLogico, float altoLogico,
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

	inline UiWindowRect ScaleUiRect(const UiWindowRect& actual,
		float factor, float anchoDisplay, float altoDisplay)
	{
		const float ancho = std::clamp(actual.ancho * factor, 0.0f, anchoDisplay);
		const float alto = std::clamp(actual.alto * factor, 0.0f, altoDisplay);
		return {
			std::clamp(actual.x * factor, 0.0f, std::max(0.0f, anchoDisplay - ancho)),
			std::clamp(actual.y * factor, 0.0f, std::max(0.0f, altoDisplay - alto)),
			ancho,
			alto
		};
	}
}
