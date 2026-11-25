#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace netjuego
{
	struct UltimoEndpoint
	{
		std::string host = "127.0.0.1";
		uint16_t puerto = 7777;
	};

	std::filesystem::path RutaPreferenciasRed();
	bool CargarUltimoEndpoint(UltimoEndpoint& endpoint, const std::filesystem::path& archivo = {});
	bool GuardarUltimoEndpoint(const UltimoEndpoint& endpoint, std::string& error, const std::filesystem::path& archivo = {});
}
