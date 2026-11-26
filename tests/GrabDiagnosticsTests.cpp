#include "src/diagnostics/GrabDiagnostics.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

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
	std::vector<unsigned long long> marcasMonotonicas(const std::string& contenido)
	{
		std::vector<unsigned long long> marcas;
		std::size_t inicio = 0;
		while (inicio < contenido.size())
		{
			const std::size_t fin = contenido.find('\n', inicio);
			if (fin == std::string::npos) break;
			const std::string linea = contenido.substr(inicio, fin - inicio);
			const std::string prefijo = "mono_ns=";
			if (linea.compare(0, prefijo.size(), prefijo) != 0) return {};
			const std::size_t separador = linea.find(' ');
			if (separador == std::string::npos) return {};
			marcas.push_back(std::stoull(linea.substr(prefijo.size(), separador - prefijo.size())));
			inicio = fin + 1;
		}
		return marcas;
	}
}

int main()
{
	const auto root = std::filesystem::temp_directory_path() /
		("EscapeTheUAB-GrabDiagnosticsTests-" + std::to_string(net::CurrentProcessId()));
	std::error_code error;
	std::filesystem::remove_all(root, error);
	{
		net::GrabDiagnostics host(root, net::GrabDiagnosticRole::Host, 101);
		net::GrabDiagnostics client(root, net::GrabDiagnosticRole::Client, 101);
		comprobar(host.Enabled() && client.Enabled(), "crea los archivos de log");
		comprobar(host.Path() != client.Path(), "separa host y cliente aunque compartan PID");
		host.Write("[grab] accepted id=7 pos=(1,2,3) point=(4,5,6)");
		host.Write("[grab] released id=7");
		client.Write("[grab] replica id=7 pos=(1,2,3) target=(4,5,6)");
		const std::string hostContent = leer(host.Path());
		const std::string clientContent = leer(client.Path());
		const auto hostMarks = marcasMonotonicas(hostContent);
		comprobar(hostContent.find("[grab] accepted id=7 pos=(1,2,3) point=(4,5,6)") != std::string::npos &&
			hostContent.find("[grab] released id=7") != std::string::npos,
			"persiste escrituras repetidas con el contenido de evento intacto");
		comprobar(hostMarks.size() == 2 && hostMarks[0] <= hostMarks[1],
			"cada registro persistido lleva una marca monotónica no decreciente");
		const std::size_t primeraLinea = hostContent.find('\n');
		const std::size_t segundoEspacio = hostContent.find(' ', primeraLinea + 1);
		const std::size_t prefijoPrimero = hostContent.find(' ') + 1;
		const std::size_t prefijoSegundo = segundoEspacio - (primeraLinea + 1) + 1;
		const std::string primerEvento = "[grab] accepted id=7 pos=(1,2,3) point=(4,5,6)";
		const std::string segundoEvento = "[grab] released id=7";
		comprobar(!hostContent.empty() && hostContent.back() == '\n' && hostContent.size() ==
			prefijoPrimero + primerEvento.size() + 1 + prefijoSegundo + segundoEvento.size() + 1,
			"bytes persistidos coinciden exactamente con timestamps, eventos y saltos de linea");
		comprobar(clientContent.find("mono_ns=") == 0 && clientContent.find("[grab] replica id=7") != std::string::npos,
			"persiste replicas con marca monotónica");
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
		comprobar(content.find("[grab] persistence stopped: 1 MiB per-process limit reached\n") != std::string::npos &&
			marcasMonotonicas(content).size() == 1,
			"persiste el aviso de limite con timestamp dentro del presupuesto de bytes");
		comprobar(content.find("this record must not be written") == std::string::npos,
			"detiene los registros al alcanzar el limite");
	}
	std::filesystem::remove_all(root, error);
	return fallos == 0 ? 0 : 1;
}
