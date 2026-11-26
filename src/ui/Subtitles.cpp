#include "src/ui/Subtitles.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>

namespace subtitles
{
	namespace
	{
		// En Windows las rutas enumeradas del disco llegan con '\'.
		std::string Normalize(std::string path)
		{
			std::replace(path.begin(), path.end(), '\\', '/');
			if (path.rfind("./", 0) == 0) path.erase(0, 2);
			return path;
		}
	}

	bool Manager::LoadJson(const std::string& json, std::string& error)
	{
		try
		{
			const nlohmann::json raiz = nlohmann::json::parse(json);
			std::unordered_map<std::string, Line> nuevas;
			for (const auto& e : raiz.at("lines"))
			{
				Line l;
				l.id = e.at("id").get<std::string>();
				l.audio = Normalize(e.at("audio").get<std::string>());
				l.text = e.at("text").get<std::string>();
				l.speaker = e.value("speaker", std::string());
				l.language = e.value("language", std::string("es"));
				l.priority = e.value("priority", 0);
				l.caption = e.value("category", std::string("dialogue")) == "caption";
				l.validated = e.value("validated", false);
				const std::string clave = l.audio;
				if (!nuevas.emplace(clave, std::move(l)).second)
				{
					error = "audio repetido: " + clave;
					return false;
				}
			}
			lines = std::move(nuevas);
			active.clear();
			return true;
		}
		catch (const nlohmann::json::exception& e)
		{
			error = e.what();
			return false;
		}
	}

	bool Manager::LoadFile(const std::string& path, std::string& error)
	{
		std::ifstream archivo(path, std::ios::binary);
		if (!archivo)
		{
			error = "no se puede abrir " + path;
			return false;
		}
		return LoadJson(std::string((std::istreambuf_iterator<char>(archivo)), std::istreambuf_iterator<char>()), error);
	}

	void Manager::OnPlay(const std::string& audioPath, unsigned int playback)
	{
		const std::string clave = Normalize(audioPath);
		if (playback == 0 || lines.find(clave) == lines.end()) return;
		active.push_back({ clave, playback });
	}

	const Line* Manager::Current(const std::function<bool(unsigned int)>& isPlaying, bool includeCaptions)
	{
		active.erase(std::remove_if(active.begin(), active.end(),
			[&](const Active& a) { return !isPlaying(a.playback); }), active.end());
		const Line* mejor = nullptr;
		for (const Active& a : active)
		{
			const auto it = lines.find(a.key);
			if (it == lines.end() || (it->second.caption && !includeCaptions)) continue;
			if (!mejor || it->second.priority >= mejor->priority) mejor = &it->second;
		}
		return mejor;
	}
}
