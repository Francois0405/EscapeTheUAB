#include "src/app/AppState.h"
#include "src/diagnostics/GrabTrace.h"

bool grabTraceEnabled()
{
	static const bool activo = []
	{
		const char* valor = std::getenv("ENTORNVGI_DEBUG_GRAB");
		return valor != nullptr && valor[0] != '\0' && valor[0] != '0';
	}();
	return activo;
}

net::GrabDiagnosticRole grabTraceRole()
{
	return networkGame.IsHosting() ? net::GrabDiagnosticRole::Host :
		networkGame.IsClient() ? net::GrabDiagnosticRole::Client : net::GrabDiagnosticRole::Offline;
}

void persistGrabTrace(const std::string& registro)
{
	if (!net::GrabPersistenceEnabledFromEnvironment()) return;
	static std::array<std::unique_ptr<net::GrabDiagnostics>, 3> sinks;
	const auto rol = grabTraceRole();
	const auto indice = static_cast<std::size_t>(rol);
	auto& sink = sinks[indice];
	if (!sink)
		sink = std::make_unique<net::GrabDiagnostics>(net::DefaultGrabDiagnosticsDirectory(),
			rol, net::CurrentProcessId());
	sink->Write(registro);
}

void emitGrabTrace(const std::string& registro)
{
	std::clog << registro << '\n';
	persistGrabTrace(registro);
}

void traceGrab(const char* evento, uint32_t id, net::PeerId peer, net::PeerId propietario,
	const glm::vec3& posicion, const glm::vec3& punto, const char* detalle)
{
	if (!grabTraceEnabled()) return;
	std::ostringstream registro;
	registro << "[grab] " << evento << " id=" << id << " peer=" << peer << " owner=" << propietario
		<< " detail=" << detalle << " pos=(" << posicion.x << ',' << posicion.y << ',' << posicion.z
		<< ") point=(" << punto.x << ',' << punto.y << ',' << punto.z << ')';
	emitGrabTrace(registro.str());
}
