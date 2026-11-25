#include "src/app/AppState.h"
#include "src/rendering/MaterialTextures.h"

GLuint texturaDeMaterial(const SceneMaterial& material)
{
	if (!material.presente || material.textura.empty() || !serializadorEscena.Cargado()) return 0;
	std::error_code ec;
	const auto absoluta = std::filesystem::absolute(serializadorEscena.Ruta().parent_path() / material.textura, ec).lexically_normal();
	if (ec) return 0;
	const std::string clave = absoluta.string();
	if (const auto it = texturasMaterialEscena.find(clave); it != texturasMaterialEscena.end()) return it->second;
	const GLint id = loadIMA_SOIL(clave.c_str());
	if (id <= 0) return 0;
	texturasMaterialEscena.emplace(clave, static_cast<GLuint>(id));
	return static_cast<GLuint>(id);
}

unsigned int texturaParaEditor(const std::filesystem::path& ruta)
{
	std::error_code ec;
	const auto absoluta = std::filesystem::absolute(ruta, ec).lexically_normal();
	if (ec || !std::filesystem::is_regular_file(absoluta, ec)) return 0;
	const std::string clave = absoluta.string();
	if (const auto it = texturasMaterialEscena.find(clave); it != texturasMaterialEscena.end()) return it->second;
	const GLint id = loadIMA_SOIL(clave.c_str());
	if (id <= 0) return 0;
	texturasMaterialEscena.emplace(clave, static_cast<GLuint>(id));
	return static_cast<unsigned int>(id);
}
