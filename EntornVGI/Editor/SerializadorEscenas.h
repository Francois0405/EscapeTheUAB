#pragma once

#include "../SceneLoader.h"

#include <filesystem>
#include <string>
#include <vector>

namespace editor
{
	class SerializadorEscenas
	{
	public:
		bool Cargar(const std::filesystem::path& ruta, std::string& error);
		// 'animados' (puertas GLB): se actualizan posicion, giro y escala de las instancias que ya existen en el JSON;
		// los colisionadores, animaciones y demas campos se conservan tal cual. Una lista vacia deja la seccion intacta.
		bool Guardar(const std::vector<SceneObject>& objetos, const std::vector<SceneObjetoAnimado>& animados, std::string& error);
		bool Guardar(const std::vector<SceneObject>& objetos, std::string& error);
		const std::filesystem::path& Ruta() const { return ruta; }
		bool Cargado() const { return cargado; }

	private:
		std::filesystem::path ruta;
		std::string documentoOriginal;
		bool cargado = false;
	};
}
