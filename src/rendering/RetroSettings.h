#pragma once

// Ajustes del estilo retro. Cabecera pura (sin OpenGL ni estado de la aplicacion) para poder
// probar presets, limites, dithering de referencia y persistencia de forma aislada.
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

namespace retro
{
	// Original: el blit 960x540 anterior, sin shader ni cambios de filtrado.
	enum class Preset { Original = 0, Cinematic, PsxClassic, Custom };
	enum class DitherMode { None = 0, Bayer4, Bayer8, Hash };

	constexpr int kOriginalWidth = 960;
	constexpr int kOriginalHeight = 540;
	constexpr int kMinHeight = 180;
	constexpr int kMaxHeight = 1080;
	constexpr int kMaxWidth = 4096;

	struct Settings
	{
		Preset preset = Preset::Cinematic;

		// Parametros de imagen (ignorados en Original).
		int internalHeight = 540;
		int bitsPerChannel = 5;
		DitherMode dither = DitherMode::Bayer4;
		float ditherStrength = 0.8f;
		float saturation = 0.90f;
		float contrast = 1.10f;
		float exposure = 1.0f;
		float grain = 0.0f;       // 0..0.03 sobre RGB normalizado
		float vignette = 0.0f;    // 0..0.10
		float grading = 0.04f;    // sombras frias, luces calidas
		bool nearestTextures = true;

		// Independientes del preset.
		bool subtitles = true;
		bool soundCaptions = true;
		bool heldViewModel = true;
	};

	// Cambia solo los parametros de imagen; subtitulos y viewmodel se conservan.
	inline void ApplyPreset(Settings& s, Preset preset)
	{
		const Settings anterior = s;
		s = Settings{};
		s.subtitles = anterior.subtitles;
		s.soundCaptions = anterior.soundCaptions;
		s.heldViewModel = anterior.heldViewModel;
		s.preset = preset;
		if (preset == Preset::Original) s.nearestTextures = false;
		else if (preset == Preset::PsxClassic)
		{
			s.internalHeight = 360;
			s.bitsPerChannel = 4;
			s.ditherStrength = 1.0f;
			s.saturation = 1.0f;
			s.contrast = 1.05f;
			s.vignette = 0.0f;
			s.grading = 0.0f;
		}
		else if (preset == Preset::Custom)
		{
			s = anterior;
			s.preset = Preset::Custom;
		}
	}

	inline float SafeValue(float v, float minimo, float maximo, float porDefecto)
	{
		return std::isfinite(v) ? std::clamp(v, minimo, maximo) : porDefecto;
	}

	inline void Sanitize(Settings& s)
	{
		const Settings d;
		if (static_cast<int>(s.preset) < 0 || static_cast<int>(s.preset) > static_cast<int>(Preset::Custom))
			s.preset = d.preset;
		if (static_cast<int>(s.dither) < 0 || static_cast<int>(s.dither) > static_cast<int>(DitherMode::Hash))
			s.dither = d.dither;
		s.internalHeight = std::clamp(s.internalHeight, kMinHeight, kMaxHeight);
		s.bitsPerChannel = std::clamp(s.bitsPerChannel, 2, 8);
		s.ditherStrength = SafeValue(s.ditherStrength, 0.0f, 1.0f, d.ditherStrength);
		s.saturation = SafeValue(s.saturation, 0.0f, 2.0f, d.saturation);
		s.contrast = SafeValue(s.contrast, 0.5f, 1.5f, d.contrast);
		s.exposure = SafeValue(s.exposure, 0.5f, 2.0f, d.exposure);
		s.grain = SafeValue(s.grain, 0.0f, 0.03f, d.grain);
		s.vignette = SafeValue(s.vignette, 0.0f, 0.10f, d.vignette);
		s.grading = SafeValue(s.grading, 0.0f, 0.15f, d.grading);
	}

	struct Size { int width; int height; };

	// Resolucion interna de la escena. Original conserva 960x540; el resto sigue la proporcion
	// de la ventana para no deformar la imagen al escalar.
	inline Size InternalSize(const Settings& s, int windowWidth, int windowHeight)
	{
		if (s.preset == Preset::Original || windowWidth <= 0 || windowHeight <= 0)
			return { kOriginalWidth, kOriginalHeight };
		const int alto = std::clamp(s.internalHeight, kMinHeight, kMaxHeight);
		const int ancho = static_cast<int>(std::lround(static_cast<double>(alto) * windowWidth / windowHeight));
		return { std::clamp(ancho, 1, kMaxWidth), alto };
	}

	// Referencia en CPU del shader retro_post.frag (misma formula), usada por las pruebas.
	inline int Bayer4Value(int x, int y)
	{
		constexpr int m[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
		return m[(y & 3) * 4 + (x & 3)];
	}

	inline int Bayer8Value(int x, int y)
	{
		constexpr int m2[4] = { 0, 2, 3, 1 };
		return 4 * Bayer4Value(x, y) + m2[((y >> 2) & 1) * 2 + ((x >> 2) & 1)];
	}

	// Umbral en (-0.5, 0.5): con el, negro y blanco puros nunca cambian.
	inline float Bayer4Offset(int x, int y) { return (Bayer4Value(x, y) + 0.5f) / 16.0f - 0.5f; }
	inline float Bayer8Offset(int x, int y) { return (Bayer8Value(x, y) + 0.5f) / 64.0f - 0.5f; }

	inline float Quantize(float value, int bits, float offset)
	{
		const float niveles = static_cast<float>((1 << bits) - 1);
		return std::clamp(std::floor(std::clamp(value, 0.0f, 1.0f) * niveles + 0.5f + offset), 0.0f, niveles) / niveles;
	}

	inline std::string Serialize(const Settings& s)
	{
		std::ostringstream out;
		out.imbue(std::locale::classic());
		out << "preset=" << static_cast<int>(s.preset) << '\n'
			<< "internalHeight=" << s.internalHeight << '\n'
			<< "bitsPerChannel=" << s.bitsPerChannel << '\n'
			<< "dither=" << static_cast<int>(s.dither) << '\n'
			<< "ditherStrength=" << s.ditherStrength << '\n'
			<< "saturation=" << s.saturation << '\n'
			<< "contrast=" << s.contrast << '\n'
			<< "exposure=" << s.exposure << '\n'
			<< "grain=" << s.grain << '\n'
			<< "vignette=" << s.vignette << '\n'
			<< "grading=" << s.grading << '\n'
			<< "nearestTextures=" << s.nearestTextures << '\n'
			<< "subtitles=" << s.subtitles << '\n'
			<< "soundCaptions=" << s.soundCaptions << '\n'
			<< "heldViewModel=" << s.heldViewModel << '\n';
		return out.str();
	}

	// Las claves desconocidas o los valores ilegibles se ignoran y dejan el valor por defecto.
	inline Settings Parse(const std::string& text)
	{
		Settings s;
		std::istringstream lineas(text);
		std::string linea;
		while (std::getline(lineas, linea))
		{
			const auto igual = linea.find('=');
			if (igual == std::string::npos) continue;
			const std::string clave = linea.substr(0, igual);
			std::istringstream valor(linea.substr(igual + 1));
			valor.imbue(std::locale::classic());
			double v = 0.0;
			if (!(valor >> v) || !std::isfinite(v) || std::abs(v) > 1.0e6) continue;
			const int entero = static_cast<int>(v);
			const float real = static_cast<float>(v);
			if (clave == "preset") s.preset = static_cast<Preset>(entero);
			else if (clave == "internalHeight") s.internalHeight = entero;
			else if (clave == "bitsPerChannel") s.bitsPerChannel = entero;
			else if (clave == "dither") s.dither = static_cast<DitherMode>(entero);
			else if (clave == "ditherStrength") s.ditherStrength = real;
			else if (clave == "saturation") s.saturation = real;
			else if (clave == "contrast") s.contrast = real;
			else if (clave == "exposure") s.exposure = real;
			else if (clave == "grain") s.grain = real;
			else if (clave == "vignette") s.vignette = real;
			else if (clave == "grading") s.grading = real;
			else if (clave == "nearestTextures") s.nearestTextures = entero != 0;
			else if (clave == "subtitles") s.subtitles = entero != 0;
			else if (clave == "soundCaptions") s.soundCaptions = entero != 0;
			else if (clave == "heldViewModel") s.heldViewModel = entero != 0;
		}
		Sanitize(s);
		return s;
	}
}
