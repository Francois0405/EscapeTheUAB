#include "src/app/AppState.h"
#include "src/rendering/MaterialTextures.h"

GLuint materialTexture(const SceneMaterial& material)
{
	if (!material.presente || material.textura.empty() || !sceneSerializer.Cargado()) return 0;
	std::error_code ec;
	const auto absoluta = std::filesystem::absolute(sceneSerializer.Ruta().parent_path() / material.textura, ec).lexically_normal();
	if (ec) return 0;
	const std::string clave = absoluta.string();
	if (const auto it = sceneMaterialTextures.find(clave); it != sceneMaterialTextures.end()) return it->second;
	const GLint id = loadIMA_SOIL(clave.c_str());
	if (id <= 0) return 0;
	sceneMaterialTextures.emplace(clave, static_cast<GLuint>(id));
	return static_cast<GLuint>(id);
}

unsigned int editorTexture(const std::filesystem::path& ruta)
{
	std::error_code ec;
	const auto absoluta = std::filesystem::absolute(ruta, ec).lexically_normal();
	if (ec || !std::filesystem::is_regular_file(absoluta, ec)) return 0;
	const std::string clave = absoluta.string();
	if (const auto it = sceneMaterialTextures.find(clave); it != sceneMaterialTextures.end()) return it->second;
	const GLint id = loadIMA_SOIL(clave.c_str());
	if (id <= 0) return 0;
	sceneMaterialTextures.emplace(clave, static_cast<GLuint>(id));
	return static_cast<unsigned int>(id);
}
