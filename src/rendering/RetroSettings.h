#pragma once

// Ajustes del estilo retro. Cabecera pura (sin OpenGL ni estado de la aplicacion) para poder
// probar presets, limites, dithering de referencia y persistencia de forma aislada.
#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>
#include <string>

namespace retro
{
	// Original: el blit 960x540 anterior, sin shader ni cambios de filtrado.
	enum class Preset { Original = 0, Cinematic, PsxClassic };
	enum class DitherMode { Off = 0, Mono, Chromatic };
	enum class Pattern { Bayer4 = 0, Bayer8, Hash };

	constexpr int kOriginalWidth = 960;
	constexpr int kOriginalHeight = 540;
	constexpr int kMinHeight = 180;
	constexpr int kMaxHeight = 1080;
	constexpr int kMaxWidth = 4096;

	// Version del formato de retro_settings.ini. 2: PSX Classic pasa a ser el aspecto oficial.
	constexpr int kSettingsVersion = 2;

	// Los valores iniciales son el aspecto oficial aprobado por el equipo (PSX Classic).
	// ApplyPreset define TODOS los parametros de imagen de cada preset: cambiar estos
	// valores iniciales no altera Original ni Retro Cinematic.
	struct Settings
	{
		Preset preset = Preset::PsxClassic;
		bool modified = false;   // algun parametro de imagen se ha cambiado respecto al preset

		// Parametros de imagen (ignorados en Original).
		int internalHeight = 360;
		int bitsPerChannel = 4;
		DitherMode dither = DitherMode::Chromatic;
		Pattern pattern = Pattern::Bayer8;
		float ditherStrength = 1.0f;
		float saturation = 0.90f;
		float contrast = 1.12f;
		float exposureEV = 0.0f;
		float grain = 0.0f;           // 0..0.03 sobre RGB normalizado
		float vignette = 0.10f;       // 0..0.10
		float grading = 0.0f;         // sombras frias, luces calidas
		float fogDensity = 0.040f;    // 0..0.08 por metro a partir de 3 m
		bool nearestTextures = true;

		// Independientes del preset.
		bool subtitles = true;
		bool soundCaptions = true;
		bool unverifiedSubtitles = true;   // marcados en pantalla como "sin verificar"
		bool heldViewModel = false;        // experimental hasta validarlo jugando
		bool pickupNotice = true;
	};

	// Restaura los parametros de imagen del preset; subtitulos y viewmodel se conservan.
	inline void ApplyPreset(Settings& s, Preset preset)
	{
		const auto imagen = [&s](int alto, int bits, Pattern patron, float intensidad, float saturacion,
			float contraste, float vineta, float niebla, bool nearest)
		{
			s.modified = false;
			s.internalHeight = alto;
			s.bitsPerChannel = bits;
			s.dither = DitherMode::Chromatic;
			s.pattern = patron;
			s.ditherStrength = intensidad;
			s.saturation = saturacion;
			s.contrast = contraste;
			s.exposureEV = 0.0f;
			s.grain = 0.0f;
			s.vignette = vineta;
			s.grading = 0.0f;
			s.fogDensity = niebla;
			s.nearestTextures = nearest;
		};
		s.preset = preset;
		switch (preset)
		{
		case Preset::Original:     // alternativa de depuracion: presentacion anterior
			imagen(540, 5, Pattern::Bayer4, 0.85f, 0.92f, 1.08f, 0.0f, 0.0f, false);
			break;
		case Preset::Cinematic:    // alternativa de depuracion
			imagen(540, 5, Pattern::Bayer4, 0.85f, 0.92f, 1.08f, 0.0f, 0.0f, true);
			break;
		case Preset::PsxClassic:   // aspecto oficial
			imagen(360, 4, Pattern::Bayer8, 1.0f, 0.90f, 1.12f, 0.10f, 0.040f, true);
			break;
		}
	}

	inline float SafeValue(float v, float minimo, float maximo, float porDefecto)
	{
		return std::isfinite(v) ? std::clamp(v, minimo, maximo) : porDefecto;
	}

	inline void Sanitize(Settings& s)
	{
		const Settings d;
		if (static_cast<int>(s.preset) < 0 || static_cast<int>(s.preset) > static_cast<int>(Preset::PsxClassic))
			s.preset = d.preset;
		if (static_cast<int>(s.dither) < 0 || static_cast<int>(s.dither) > static_cast<int>(DitherMode::Chromatic))
			s.dither = d.dither;
		if (static_cast<int>(s.pattern) < 0 || static_cast<int>(s.pattern) > static_cast<int>(Pattern::Hash))
			s.pattern = d.pattern;
		s.internalHeight = std::clamp(s.internalHeight, kMinHeight, kMaxHeight);
		s.bitsPerChannel = std::clamp(s.bitsPerChannel, 3, 8);
		s.ditherStrength = SafeValue(s.ditherStrength, 0.0f, 1.5f, d.ditherStrength);
		s.saturation = SafeValue(s.saturation, 0.5f, 1.5f, d.saturation);
		s.contrast = SafeValue(s.contrast, 0.8f, 1.3f, d.contrast);
		s.exposureEV = SafeValue(s.exposureEV, -1.0f, 1.0f, d.exposureEV);
		s.grain = SafeValue(s.grain, 0.0f, 0.03f, d.grain);
		s.vignette = SafeValue(s.vignette, 0.0f, 0.10f, d.vignette);
		s.grading = SafeValue(s.grading, 0.0f, 0.15f, d.grading);
		s.fogDensity = SafeValue(s.fogDensity, 0.0f, 0.08f, d.fogDensity);
	}

	// Los efectos retro solo se aplican a la vista de escritorio jugable. El editor, la vista
	// estereo (F2) y OpenXR conservan la presentacion anterior.
	inline bool IsActive(const Settings& s, bool editor, bool stereoPreview, bool xrRunning)
	{
		return s.preset != Preset::Original && !editor && !stereoPreview && !xrRunning;
	}

	struct Size { int width; int height; };

	// Resolucion interna de la escena. Inactivo conserva 960x540; activo sigue la proporcion
	// de la ventana para no deformar la imagen al escalar.
	inline Size InternalSize(const Settings& s, bool active, int windowWidth, int windowHeight)
	{
		if (!active || windowWidth <= 0 || windowHeight <= 0)
			return { kOriginalWidth, kOriginalHeight };
		const int alto = std::clamp(s.internalHeight, kMinHeight, kMaxHeight);
		const int ancho = static_cast<int>(std::lround(static_cast<double>(alto) * windowWidth / windowHeight));
		return { std::clamp(ancho, 1, kMaxWidth), alto };
	}

	// Referencia en CPU del shader retro_post.frag (misma formula), usada por las pruebas.
	struct Texel { int x; int y; };

	// Desfase del patron por canal en modo cromatico: R(0,0), G(1,2), B(2,1).
	inline Texel ChannelShift(int channel)
	{
		constexpr Texel shift[3] = { { 0, 0 }, { 1, 2 }, { 2, 1 } };
		return shift[channel];
	}

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

	// Umbral en (-0.5, 0.5).
	inline float Bayer4Offset(int x, int y) { return (Bayer4Value(x, y) + 0.5f) / 16.0f - 0.5f; }
	inline float Bayer8Offset(int x, int y) { return (Bayer8Value(x, y) + 0.5f) / 64.0f - 0.5f; }

	// Umbral (ya con intensidad) del canal en el texel interno. El patron Hash solo existe en GPU.
	inline float DitherOffset(const Settings& s, int x, int y, int channel)
	{
		if (s.dither == DitherMode::Off || s.pattern == Pattern::Hash) return 0.0f;
		const Texel shift = ChannelShift(s.dither == DitherMode::Chromatic ? channel : 0);
		const float umbral = s.pattern == Pattern::Bayer4 ? Bayer4Offset(x + shift.x, y + shift.y)
			: Bayer8Offset(x + shift.x, y + shift.y);
		return umbral * s.ditherStrength;
	}

	inline float Quantize(float value, int bits, float offset)
	{
		const float niveles = static_cast<float>((1 << bits) - 1);
		return std::clamp(std::floor(std::clamp(value, 0.0f, 1.0f) * niveles + 0.5f + offset), 0.0f, niveles) / niveles;
	}

	inline std::string Serialize(const Settings& s)
	{
		std::ostringstream out;
		out.imbue(std::locale::classic());
		out << "version=" << kSettingsVersion << '\n'
			<< "preset=" << static_cast<int>(s.preset) << '\n'
			<< "modified=" << s.modified << '\n'
			<< "internalHeight=" << s.internalHeight << '\n'
			<< "bitsPerChannel=" << s.bitsPerChannel << '\n'
			<< "dither=" << static_cast<int>(s.dither) << '\n'
			<< "pattern=" << static_cast<int>(s.pattern) << '\n'
			<< "ditherStrength=" << s.ditherStrength << '\n'
			<< "saturation=" << s.saturation << '\n'
			<< "contrast=" << s.contrast << '\n'
			<< "exposureEV=" << s.exposureEV << '\n'
			<< "grain=" << s.grain << '\n'
			<< "vignette=" << s.vignette << '\n'
			<< "grading=" << s.grading << '\n'
			<< "fogDensity=" << s.fogDensity << '\n'
			<< "nearestTextures=" << s.nearestTextures << '\n'
			<< "subtitles=" << s.subtitles << '\n'
			<< "soundCaptions=" << s.soundCaptions << '\n'
			<< "unverifiedSubtitles=" << s.unverifiedSubtitles << '\n'
			<< "heldViewModel=" << s.heldViewModel << '\n'
			<< "pickupNotice=" << s.pickupNotice << '\n';
		return out.str();
	}

	// Las claves desconocidas o los valores ilegibles se ignoran. Lo que falte se completa con el
	// preset guardado en el propio fichero, no con el oficial, para no mezclar dos aspectos.
	// Los ajustes retocados (modified=1) y el preset elegido se respetan siempre. Un fichero anterior a
	// la version 2 que no se hubiera retocado adopta la definicion actual de su preset.
	inline Settings Parse(const std::string& text)
	{
		Settings s;
		std::istringstream busqueda(text);
		for (std::string linea; std::getline(busqueda, linea);)
		{
			if (linea.rfind("preset=", 0) != 0) continue;
			std::istringstream valor(linea.substr(7));
			int preset = -1;
			if (valor >> preset && preset >= static_cast<int>(Preset::Original) &&
				preset <= static_cast<int>(Preset::PsxClassic))
				ApplyPreset(s, static_cast<Preset>(preset));
		}
		std::istringstream lineas(text);
		std::string linea;
		int version = 1;
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
			if (clave == "version") version = entero;
			else if (clave == "preset") s.preset = static_cast<Preset>(entero);
			else if (clave == "modified") s.modified = entero != 0;
			else if (clave == "internalHeight") s.internalHeight = entero;
			else if (clave == "bitsPerChannel") s.bitsPerChannel = entero;
			else if (clave == "dither") s.dither = static_cast<DitherMode>(entero);
			else if (clave == "pattern") s.pattern = static_cast<Pattern>(entero);
			else if (clave == "ditherStrength") s.ditherStrength = real;
			else if (clave == "saturation") s.saturation = real;
			else if (clave == "contrast") s.contrast = real;
			else if (clave == "exposureEV") s.exposureEV = real;
			else if (clave == "grain") s.grain = real;
			else if (clave == "vignette") s.vignette = real;
			else if (clave == "grading") s.grading = real;
			else if (clave == "fogDensity") s.fogDensity = real;
			else if (clave == "nearestTextures") s.nearestTextures = entero != 0;
			else if (clave == "subtitles") s.subtitles = entero != 0;
			else if (clave == "soundCaptions") s.soundCaptions = entero != 0;
			else if (clave == "unverifiedSubtitles") s.unverifiedSubtitles = entero != 0;
			else if (clave == "heldViewModel") s.heldViewModel = entero != 0;
			else if (clave == "pickupNotice") s.pickupNotice = entero != 0;
		}
		Sanitize(s);
		if (version < kSettingsVersion && !s.modified) ApplyPreset(s, s.preset);
		return s;
	}
}
