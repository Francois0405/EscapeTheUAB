#include "src/networking/preferences/NetworkPreferences.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

int main()
{
	namespace fs = std::filesystem;
	const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
	const fs::path dir = fs::temp_directory_path() / ("EscapeTheUAB-NetworkPreferences-" + std::to_string(unique));
	const fs::path file = dir / "multijugador.ini";
	int failures = 0;
	auto check = [&](bool condition, const char* label)
	{
		if (!condition) { std::cerr << "FAIL: " << label << '\n'; ++failures; }
	};
	std::error_code ec;
	fs::create_directories(dir, ec);
	check(!ec, "create isolated preferences directory");
	std::string error;
	const netgame::LastEndpoint saved{ "100.87.12.34", 7777 };
	check(netgame::GuardarUltimoEndpoint(saved, error, file), "save Tailscale IPv4 and port");
	netgame::LastEndpoint loaded;
	check(netgame::CargarUltimoEndpoint(loaded, file) && loaded.host == saved.host && loaded.puerto == saved.puerto,
		"load saved endpoint");
	check(netgame::GuardarUltimoEndpoint({ "100.87.12.34", 45678 }, error, file), "save custom port");
	check(netgame::CargarUltimoEndpoint(loaded, file) && loaded.puerto == 45678, "reload custom port");
	check(!netgame::GuardarUltimoEndpoint({ "100.87.12.34\n127.0.0.1", 7777 }, error, file), "reject multiline host");
	{
		std::ofstream malformed(file, std::ios::binary | std::ios::trunc);
		malformed << "1\n100.87.12.34\n70000\n";
	}
	check(!netgame::CargarUltimoEndpoint(loaded, file), "reject out-of-range persisted port");
	fs::remove(file, ec);
	fs::remove(dir, ec);
	return failures == 0 ? 0 : 1;
}
