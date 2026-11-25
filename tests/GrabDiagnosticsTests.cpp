#include "GrabDiagnostics.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace
{
	int fallos = 0;
	void comprobar(bool condicion, const char* mensaje)
	{
		if (!condicion) { std::cerr << "FAIL: " << mensaje << '\n'; ++fallos; }
	}
	std::string leer(const std::filesystem::path& path)
	{
		std::ifstream input(path, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(input), {});
	}
}

int main()
{
	const auto root = std::filesystem::temp_directory_path() / "EscapeTheUAB-GrabDiagnosticsTests";
	std::error_code error;
	std::filesystem::remove_all(root, error);
	{
		net::GrabDiagnostics host(root, net::GrabDiagnosticRole::Host, 101);
		net::GrabDiagnostics client(root, net::GrabDiagnosticRole::Client, 101);
		comprobar(host.Enabled() && client.Enabled(), "crea los archivos de log");
		comprobar(host.Path() != client.Path(), "separa host y cliente aunque compartan PID");
		host.Write("[grab] accepted id=7 pos=(1,2,3) point=(4,5,6)");
		client.Write("[grab] replica id=7 pos=(1,2,3) target=(4,5,6)");
		comprobar(leer(host.Path()).find("accepted id=7") != std::string::npos,
			"persiste evento accepted y transformaciones");
		comprobar(leer(client.Path()).find("replica id=7") != std::string::npos,
			"persiste replicas en el log del cliente");
	}
	{
		net::GrabDiagnostics restarted(root, net::GrabDiagnosticRole::Host, 101);
		restarted.Write("[grab] request id=9");
		const std::string content = leer(restarted.Path());
		comprobar(content.find("accepted id=7") == std::string::npos &&
			content.find("request id=9") != std::string::npos,
			"cada ejecucion trunca su archivo previo");
	}
	{
		net::GrabDiagnostics bounded(root, net::GrabDiagnosticRole::Offline, 202);
		bounded.Write(std::string(net::GrabDiagnostics::MaxBytesPerRun, 'x'));
		bounded.Write("[grab] this record must not be written");
		const std::string content = leer(bounded.Path());
		comprobar(content.size() <= net::GrabDiagnostics::MaxBytesPerRun,
			"limita el archivo a 1 MiB por ejecucion");
		comprobar(content.find("this record must not be written") == std::string::npos,
			"detiene los registros al alcanzar el limite");
	}
	std::filesystem::remove_all(root, error);
	return fallos == 0 ? 0 : 1;
}
