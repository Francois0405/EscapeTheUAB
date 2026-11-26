#pragma once

// Subtitulos ligados a las reproducciones reales de AudioManager. Logica pura: sin ImGui,
// audio ni OpenGL; quien dibuja le pregunta que linea sigue sonando.
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace subtitles
{
	struct Line
	{
		std::string id;
		std::string speaker;   // vacio si no se puede atribuir
		std::string audio;     // ruta relativa "assets/audio/..."
		std::string text;
		std::string language;
		int priority = 0;
		bool caption = false;     // descripcion de un sonido no verbal
		bool validated = false;   // false hasta que una persona la compare con el audio
	};

	class Manager
	{
	public:
		// Sustituye el catalogo. Si el JSON no es valido devuelve false y no cambia nada.
		bool LoadJson(const std::string& json, std::string& error);
		bool LoadFile(const std::string& path, std::string& error);

		// Aviso de que playback (id distinto de 0) ha empezado a reproducir ese archivo.
		void OnPlay(const std::string& audioPath, unsigned int playback);

		// Olvida lo que ya no suena y devuelve la linea de mayor prioridad (a igualdad, la mas
		// reciente). nullptr si no hay nada que mostrar.
		const Line* Current(const std::function<bool(unsigned int)>& isPlaying, bool includeCaptions);

		void Clear() { active.clear(); }
		std::size_t LineCount() const { return lines.size(); }

	private:
		struct Active { std::string key; unsigned int playback; };
		std::unordered_map<std::string, Line> lines;   // por ruta de audio normalizada
		std::vector<Active> active;
	};
}
