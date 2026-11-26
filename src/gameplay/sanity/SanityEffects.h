#pragma once

// Eventos visuales de baja cordura: mensajes, ojos, fallos de "matrix", rayos de escaneo y lluvia de digitos.
// Logica pura (sin ImGui ni OpenGL): decide cuando aparece cada evento y cuanto dura. El dibujo vive en
// src/ui/SanityOverlay.cpp. Todo depende del tiempo (delta), no de los fotogramas.
#include "src/gameplay/sanity/Sanity.h"

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace sanity
{
	// Hash determinista a [0, 1) para el ruido por fotograma del dibujo.
	inline float Hash01(std::uint32_t x)
	{
		x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
		return static_cast<float>(x & 0xFFFFFFu) / 16777216.0f;
	}

	inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

	// 0 por debajo de a, 1 por encima de b, curva suave entre ambos.
	inline float Ramp(float x, float a, float b) { return SmoothStep((x - a) / (b - a)); }

	// Solo ASCII: la fuente por defecto de ImGui no tiene todos los glifos y el archivo no depende de la codificacion.
	inline constexpr const char* kMessages[] = {
		"NO ESTAS SOLO", "TE ESTAN MIRANDO", "LA PUERTA NO ES UNA PUERTA", "YA HAS ESTADO AQUI",
		"NO MIRES ATRAS", "LAS PAREDES RESPIRAN", "ELLOS SABEN TU NOMBRE", "CORRE", "NO HAY SALIDA",
		"TU NO ERES TU", "EL AULA TE RECUERDA", "HIRITI TE VE", "DETRAS DE TI", "NADIE SALIO NUNCA",
		"EL SILENCIO TIENE OJOS", "ESTO NO ESTA PASANDO", "ESTA PASANDO", "ERROR: REALIDAD NO ENCONTRADA",
		"0xDEAD: MENTE CORRUPTA", "SYSTEM FAILURE: CORDURA", "ABRE LOS OJOS. TODOS", "NO CUENTES HASTA TRES",
		"LA UAB NUNCA CIERRA", "SIGUE LA LUZ QUE PARPADEA", "ALGO RESPIRA EN EL PASILLO", "YA ES TARDE",
		"TU SOMBRA LLEGO ANTES", "DESPIERTA. NO. DUERME", "NO ES TU REFLEJO", "SE ESTAN BORRANDO LOS RECUERDOS",
	};
	inline constexpr int kMessageCount = static_cast<int>(sizeof(kMessages) / sizeof(kMessages[0]));

	struct Message { int text = 0; float x = 0.5f, y = 0.5f, age = 0.0f, life = 1.0f, scale = 2.0f, seed = 0.0f; };
	struct Eye { float x = 0.5f, y = 0.5f, age = 0.0f, life = 1.0f, size = 0.1f, seed = 0.0f; };
	// kind 0: barra de color desplazada, 1: rayo fino de escaneo, 2: bloques de pixeles.
	struct Glitch { int kind = 0; float y = 0.5f, height = 0.02f, x = 0.0f, width = 1.0f, age = 0.0f, life = 0.1f, seed = 0.0f; };
	struct Rain { float x = 0.5f, y0 = 0.0f, speed = 0.5f, age = 0.0f, life = 1.0f, seed = 0.0f; int length = 8; };

	class SanityEffects
	{
	public:
		SanityEffects() : rng_(std::random_device{}()) {}

		void Reset()
		{
			messages_.clear(); eyes_.clear(); glitches_.clear(); rain_.clear();
			accMessage_ = accEye_ = accGlitch_ = accRain_ = 0.0f;
			time_ = 0.0f;
		}

		// insanity: 0 = cuerdo, 1 = roto. delta en segundos.
		void Update(float delta, float insanity)
		{
			if (!(delta > 0.0f)) return;
			delta = std::min(delta, kMaxFrameDelta);
			insanity_ = Clamp01(insanity);
			time_ += delta;
			if (time_ > 1.0e4f) time_ = std::fmod(time_, 1.0e4f);
			Age(messages_, delta); Age(eyes_, delta); Age(glitches_, delta); Age(rain_, delta);

			const float i = insanity_;
			// Mensajes: desde la mitad de la cordura. Pocos y largos al principio, muchos y breves al final.
			const float tm = Ramp(i, 0.5f, 1.0f);
			if (i >= 0.5f)
				Spawn(accMessage_, delta, 0.12f + 18.0f * std::pow(tm, 1.5f), messages_, 2 + static_cast<int>(12.0f * tm),
					[&] {
						Message m;
						m.text = RandInt(kMessageCount);
						m.x = Rand(0.15f, 0.85f); m.y = Rand(0.12f, 0.85f);
						m.life = Lerp(3.5f, 0.3f, std::pow(tm, 0.6f)) * Rand(0.8f, 1.25f);
						m.scale = Rand(1.6f, 2.6f) + 1.4f * tm * Rand(0.0f, 1.0f);
						m.seed = Rand(0.0f, 1000.0f);
						return m;
					});
			// Ojos: desde el 20 % de perdida.
			const float te = Ramp(i, 0.2f, 1.0f);
			if (i >= 0.2f)
				Spawn(accEye_, delta, 0.08f + 2.2f * te, eyes_, 1 + static_cast<int>(10.0f * te),
					[&] {
						Eye e;
						for (int tries = 0; tries < 6; ++tries)
						{
							e.x = Rand(0.04f, 0.96f); e.y = Rand(0.06f, 0.94f);
							if (std::abs(e.x - 0.5f) > 0.16f || std::abs(e.y - 0.5f) > 0.16f) break;   // deja libre la reticula
						}
						e.life = Lerp(4.0f, 1.0f, te) * Rand(0.7f, 1.3f);
						e.size = Rand(0.05f, 0.11f) * (0.8f + 0.7f * te);
						e.seed = Rand(0.0f, 1000.0f);
						return e;
					});
			// Fallos de matrix: barras, rayos y bloques.
			const float tg = Ramp(i, 0.3f, 1.0f);
			if (i >= 0.3f)
				Spawn(accGlitch_, delta, 0.4f + 50.0f * std::pow(tg, 1.3f), glitches_, 4 + static_cast<int>(30.0f * tg),
					[&] {
						Glitch g;
						const float r = Rand(0.0f, 1.0f);
						g.kind = r < 0.5f ? 0 : (r < 0.8f ? 1 : 2);
						g.y = Rand(0.0f, 1.0f); g.x = Rand(0.0f, 0.9f);
						g.width = g.kind == 2 ? Rand(0.04f, 0.2f) : Rand(0.2f, 1.0f);
						g.height = g.kind == 1 ? 0.004f : Rand(0.01f, 0.01f + 0.06f * tg);
						g.life = Lerp(0.3f, 0.1f, tg) * Rand(0.6f, 1.4f);
						g.seed = Rand(0.0f, 1000.0f);
						return g;
					});
			// Lluvia de digitos.
			const float tr = Ramp(i, 0.55f, 1.0f);
			if (i >= 0.55f)
				Spawn(accRain_, delta, 0.3f + 14.0f * tr, rain_, 3 + static_cast<int>(20.0f * tr),
					[&] {
						Rain r;
						r.x = Rand(0.0f, 1.0f); r.y0 = Rand(-0.2f, 0.6f); r.speed = Rand(0.5f, 1.2f);
						r.life = Rand(0.6f, 1.6f); r.length = 5 + RandInt(10); r.seed = Rand(0.0f, 1000.0f);
						return r;
					});
		}

		const std::vector<Message>& Messages() const { return messages_; }
		const std::vector<Eye>& Eyes() const { return eyes_; }
		const std::vector<Glitch>& Glitches() const { return glitches_; }
		const std::vector<Rain>& RainColumns() const { return rain_; }
		float Time() const { return time_; }
		float Insanity() const { return insanity_; }

	private:
		template <class T> static void Age(std::vector<T>& v, float delta)
		{
			for (T& e : v) e.age += delta;
			v.erase(std::remove_if(v.begin(), v.end(), [](const T& e) { return e.age >= e.life; }), v.end());
		}

		template <class T, class Make>
		void Spawn(float& acc, float delta, float rate, std::vector<T>& v, int cap, Make make)
		{
			acc += rate * delta;
			for (int guard = 0; acc >= 1.0f && guard < 3; ++guard)
			{
				acc -= 1.0f;
				if (static_cast<int>(v.size()) < cap) v.push_back(make());
			}
			acc = std::min(acc, 1.0f);
		}

		float Rand(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng_); }
		int RandInt(int n) { return std::uniform_int_distribution<int>(0, n - 1)(rng_); }

		std::mt19937 rng_;
		std::vector<Message> messages_;
		std::vector<Eye> eyes_;
		std::vector<Glitch> glitches_;
		std::vector<Rain> rain_;
		float accMessage_ = 0.0f, accEye_ = 0.0f, accGlitch_ = 0.0f, accRain_ = 0.0f;
		float time_ = 0.0f;
		float insanity_ = 0.0f;
	};
}
