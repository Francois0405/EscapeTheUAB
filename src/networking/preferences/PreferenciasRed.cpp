#include "PreferenciasRed.h"

#include <cstdlib>
#include <fstream>
#include <utility>

namespace netjuego
{
	namespace
	{
		std::string VariableEntorno(const char* nombre)
		{
#if defined(_WIN32)
			char* valor = nullptr;
			size_t longitud = 0;
			if (_dupenv_s(&valor, &longitud, nombre) != 0 || !valor) { std::free(valor); return {}; }
			std::string resultado(valor);
			std::free(valor);
			return resultado;
#else
			const char* valor = std::getenv(nombre);
			return valor ? valor : "";
#endif
		}

		std::filesystem::path DirectorioConfig()
		{
#if defined(_WIN32)
			const std::string appData = VariableEntorno("APPDATA");
			if (!appData.empty()) return std::filesystem::path(appData) / "EscapeTheUAB";
#elif defined(__APPLE__)
			const std::string home = VariableEntorno("HOME");
			if (!home.empty()) return std::filesystem::path(home) / "Library" / "Application Support" / "EscapeTheUAB";
#else
			const std::string xdg = VariableEntorno("XDG_CONFIG_HOME");
			if (!xdg.empty()) return std::filesystem::path(xdg) / "escapetheuab";
			const std::string home = VariableEntorno("HOME");
			if (!home.empty()) return std::filesystem::path(home) / ".config" / "escapetheuab";
#endif
			return {};
		}

		std::filesystem::path ResolverArchivo(const std::filesystem::path& archivo)
		{
			return archivo.empty() ? RutaPreferenciasRed() : archivo;
		}
	}

	std::filesystem::path RutaPreferenciasRed()
	{
		const std::filesystem::path directorio = DirectorioConfig();
		return directorio.empty() ? std::filesystem::path{} : directorio / "multijugador.ini";
	}

	bool CargarUltimoEndpoint(UltimoEndpoint& endpoint, const std::filesystem::path& archivo)
	{
		const auto ruta = ResolverArchivo(archivo);
		if (ruta.empty()) return false;
		std::ifstream entrada(ruta);
		std::string version, host, textoPuerto, extra;
		if (!std::getline(entrada, version) || version != "1" || !std::getline(entrada, host) ||
			!std::getline(entrada, textoPuerto) || host.empty() || host.size() > 253 || textoPuerto.empty() || textoPuerto.size() > 5)
			return false;
		if (std::getline(entrada, extra)) return false;
		unsigned long puerto = 0;
		for (const unsigned char c : textoPuerto)
		{
			if (c < '0' || c > '9') return false;
			puerto = puerto * 10 + (c - '0');
			if (puerto > 65535) return false;
		}
		if (puerto == 0) return false;
		endpoint.host = std::move(host);
		endpoint.puerto = static_cast<uint16_t>(puerto);
		return true;
	}

	bool GuardarUltimoEndpoint(const UltimoEndpoint& endpoint, std::string& error, const std::filesystem::path& archivo)
	{
		const auto ruta = ResolverArchivo(archivo);
		if (ruta.empty()) { error = "No se encontro una carpeta de configuracion del usuario."; return false; }
		if (endpoint.host.empty() || endpoint.host.size() > 253 || endpoint.host.find_first_of("\r\n") != std::string::npos || endpoint.puerto == 0)
		{
			error = "La direccion o el puerto no son validos para guardar.";
			return false;
		}
		std::error_code ec;
		std::filesystem::create_directories(ruta.parent_path(), ec);
		if (ec) { error = "No se pudo crear la carpeta de configuracion: " + ec.message(); return false; }
		std::ofstream salida(ruta, std::ios::binary | std::ios::trunc);
		if (!salida) { error = "No se pudo escribir " + ruta.string(); return false; }
		salida << "1\n" << endpoint.host << '\n' << endpoint.puerto << '\n';
		salida.close();
		if (!salida) { error = "Fallo al guardar " + ruta.string(); return false; }
		error.clear();
		return true;
	}
}
