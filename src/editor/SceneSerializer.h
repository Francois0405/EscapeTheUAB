#pragma once

#include "src/scene/SceneLoader.h"

#include <filesystem>
#include <string>
#include <vector>

namespace editor
{
	class SceneSerializer
	{
	public:
		bool LoadScene(const std::filesystem::path& ruta, std::string& error);
		// 'animados' (puertas GLB): se actualizan posicion, giro y escala de las instancias que ya existen en el JSON;
		// los colisionadores, animaciones y demas campos se conservan tal cual. Una lista vacia deja la seccion intacta.
		bool SaveScene(const std::vector<SceneObject>& objetos, const std::vector<SceneAnimatedObject>& animados, std::string& error);
		bool SaveScene(const std::vector<SceneObject>& objetos, std::string& error);
		const std::filesystem::path& Ruta() const { return ruta; }
		const std::filesystem::path& RutaFuente() const { return rutaFuente; }
		bool IsLoaded() const { return cargado; }

	private:
		std::filesystem::path ruta;
		std::filesystem::path rutaFuente;
		std::string documentoOriginal;
		bool cargado = false;
		bool llamadaSinListaAnimados = false;
	};
}
