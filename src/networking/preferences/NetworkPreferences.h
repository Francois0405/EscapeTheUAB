#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace netgame
{
	struct LastEndpoint
	{
		std::string host = "127.0.0.1";
		uint16_t puerto = 7777;
	};

	std::filesystem::path RutaPreferenciasRed();
	bool CargarUltimoEndpoint(LastEndpoint& endpoint, const std::filesystem::path& archivo = {});
	bool GuardarUltimoEndpoint(const LastEndpoint& endpoint, std::string& error, const std::filesystem::path& archivo = {});
}
