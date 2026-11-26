#pragma once

// Sistema de cordura. Logica pura (sin OpenGL, ImGui ni estado de app): el HUD, el tinte y el temblor de
// camara solo leen de aqui. Es local al cliente: no toca Bullet, la red ni la autoridad del host.
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace sanity
{
	// Segundos que tarda la cordura en pasar de 100 % a 0 %.
	inline constexpr float SANITY_DEGRADATION_DURATION = 60.0f;

	// Mayor salto de tiempo que se acepta por fotograma (un tirón de ventana no consume cordura de golpe).
	inline constexpr float kMaxFrameDelta = 0.5f;

	struct Color { float r, g, b; };   // 0..1

	struct Settings
	{
		float degradationDuration = SANITY_DEGRADATION_DURATION;
		float tintMaxAlpha = 0.34f;          // opacidad del rojo con 0 % de cordura (0 = sin tinte)
		float shakeMaxAngle = 0.09f;         // radianes (~5 grados) con 0 % de cordura (0 = sin temblor)
		float shakeSpeed = 1.0f;             // multiplicador de la velocidad del temblor
	};

	inline float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

	inline float SmoothStep(float t)
	{
		t = Clamp01(t);
		return t * t * (3.0f - 2.0f * t);
	}

	// Rosa sano -> rosa oscuro -> magenta -> rojo profundo. Interpolacion continua entre paradas.
	inline Color BarColor(float fraction)
	{
		struct Stop { float at; Color c; };
		static constexpr Stop stops[] = {
			{ 0.00f, { 0.47f, 0.00f, 0.05f } },   // rojo profundo y dañado
			{ 0.30f, { 0.78f, 0.04f, 0.20f } },   // rojo intenso
			{ 0.55f, { 0.74f, 0.10f, 0.52f } },   // magenta
			{ 0.78f, { 0.88f, 0.36f, 0.58f } },   // rosa oscuro
			{ 1.00f, { 0.98f, 0.70f, 0.78f } },   // rosa claro, cerebro sano
		};
		const float f = Clamp01(fraction);
		for (std::size_t i = 1; i < sizeof(stops) / sizeof(stops[0]); ++i)
		{
			if (f <= stops[i].at)
			{
				const Stop& a = stops[i - 1];
				const Stop& b = stops[i];
				const float t = (f - a.at) / (b.at - a.at);
				return { a.c.r + (b.c.r - a.c.r) * t, a.c.g + (b.c.g - a.c.g) * t, a.c.b + (b.c.b - a.c.b) * t };
			}
		}
		return stops[4].c;
	}

	// Ruido suave y determinista en [-1, 1]: suma de senos de frecuencias no multiplos entre si.
	inline float SmoothNoise(float t, float seed)
	{
		return (std::sin(1.30f * t + seed) + 0.60f * std::sin(2.90f * t + 1.7f + seed * 1.3f) +
			0.35f * std::sin(5.10f * t + 0.4f + seed * 2.1f) + 0.45f * std::sin(17.3f * t + 2.2f + seed * 0.7f) +
			0.25f * std::sin(31.1f * t + seed * 3.3f)) / 2.65f;
	}

	struct Shake { float yaw = 0.0f; float pitch = 0.0f; };   // radianes

	class SanitySystem
	{
	public:
		Settings settings;

		// Avanza el tiempo. Solo debe llamarse mientras se juega; delta en segundos.
		void Update(float delta)
		{
			if (!(delta > 0.0f)) return;   // tambien descarta NaN
			delta = std::min(delta, kMaxFrameDelta);
			const float duration = std::max(settings.degradationDuration, 0.001f);
			elapsed_ = std::min(elapsed_ + delta, duration);
			shakeTime_ += delta * settings.shakeSpeed * (1.0f + 1.5f * (1.0f - Fraction()));
			// Mantiene la fase acotada sin salto visible (periodo comun grande, mismo valor tras el modulo).
			if (shakeTime_ > 1.0e5f) shakeTime_ = std::fmod(shakeTime_, 1.0e5f);
		}

		void Reset() { elapsed_ = 0.0f; shakeTime_ = 0.0f; }

		// 1 = cuerdo, 0 = roto. Siempre en [0, 1].
		float Fraction() const
		{
			const float duration = std::max(settings.degradationDuration, 0.001f);
			return Clamp01(1.0f - elapsed_ / duration);
		}
		float Percent() const { return Fraction() * 100.0f; }

		// 0 con cordura plena, 1 con cordura nula.
		float Insanity() const { return 1.0f - Fraction(); }

		// Opacidad del tinte rojo: nula al 100 %, creciente (curva suave) hasta tintMaxAlpha.
		float TintAlpha() const
		{
			const float i = Insanity();
			return Clamp01(settings.tintMaxAlpha) * i * (0.4f + 0.6f * i);
		}

		// Giro de camara (yaw y pitch) en radianes. Cero al 100 % y casi nulo con cordura alta.
		Shake CameraShake() const
		{
			const float i = Insanity();
			// Empieza pronto (curva i^1,3) y añade sacudidas puntuales que crecen con la perdida.
			const float jolt = std::pow(std::max(0.0f, std::sin(0.9f * shakeTime_) * std::sin(2.3f * shakeTime_ + 1.0f)), 4.0f);
			const float amplitude = std::max(settings.shakeMaxAngle, 0.0f) * std::pow(i, 1.3f) * (0.75f + 0.9f * jolt * i);
			if (amplitude <= 0.0f) return {};
			return { amplitude * SmoothNoise(shakeTime_, 0.0f), amplitude * SmoothNoise(shakeTime_, 11.0f) };
		}

	private:
		float elapsed_ = 0.0f;
		float shakeTime_ = 0.0f;
	};
}
