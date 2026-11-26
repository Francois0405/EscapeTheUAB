#include "src/audio/SubtitleManager.h"

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
				l.audio = Normalize(e.at("audio").get<std::string>());
				l.text = e.at("text").get<std::string>();
				l.id = e.value("id", l.audio);
				l.speaker = e.value("speaker", std::string());
				l.language = e.value("language", std::string("es"));
				l.priority = e.value("priority", 0);
				l.caption = e.value("category", std::string("dialogue")) == "caption";
				l.verified = e.value("verified", false);
				if (e.contains("segments"))
					for (const auto& s : e.at("segments"))
					{
						Segment segmento{ s.at("start").get<float>(), s.at("end").get<float>(), s.at("text").get<std::string>() };
						if (!(segmento.end > segmento.start) || segmento.start < 0.0f)
						{
							error = "segmento no valido en " + l.audio;
							return false;
						}
						l.segments.push_back(std::move(segmento));
					}
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

	bool Manager::Current(const std::function<bool(unsigned int)>& isPlaying,
		const std::function<float(unsigned int)>& cursorSeconds, Filter filter, Visible& out)
	{
		active.erase(std::remove_if(active.begin(), active.end(),
			[&](const Active& a) { return !isPlaying(a.playback); }), active.end());
		const Line* mejor = nullptr;
		const std::string* texto = nullptr;
		for (const Active& a : active)
		{
			const auto it = lines.find(a.key);
			if (it == lines.end()) continue;
			const Line& linea = it->second;
			if ((linea.caption && !filter.captions) || (!linea.verified && !filter.unverified)) continue;
			if (mejor && linea.priority < mejor->priority) continue;
			const std::string* candidato = &linea.text;
			if (!linea.segments.empty())
			{
				candidato = nullptr;
				const float t = cursorSeconds(a.playback);
				for (const Segment& s : linea.segments)
					if (t >= s.start && t < s.end) { candidato = &s.text; break; }
			}
			if (!candidato || candidato->empty()) continue;
			mejor = &linea;
			texto = candidato;
		}
		if (!mejor) return false;
		out = { mejor->speaker, *texto, mejor->verified };
		return true;
	}
}
