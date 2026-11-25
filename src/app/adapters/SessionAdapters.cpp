#include "src/app/AppState.h"
#include "src/app/adapters/SessionAdapters.h"
#include "src/app/adapters/HirityAdapters.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/app/adapters/SceneAdapters.h"
#include "src/app/input/CursorCapture.h"
#include "src/diagnostics/GrabTrace.h"

// Contexto prestado a src/networking/session: el estado sigue siendo de main.cpp.

const netgame::session::SessionContext& contextoSesionRed()
{
	static const netgame::session::SessionContext contexto{
		objetosEscena, escenaFisica, mundoFisico, redJuego, objetosAnimados, puertasRed, estadoSesionRed,
		gestorOpenXR, ultimaCabezaVR, hayCabezaVR,
		[] { return glm::vec3(static_cast<float>(opvN.x), static_cast<float>(opvN.y), static_cast<float>(opvN.z)); },
		[] { return glm::vec3(static_cast<float>(n[0] - opvN.x), static_cast<float>(n[1] - opvN.y), static_cast<float>(n[2] - opvN.z)); },
		[] { return mundoFisico.HayJugador() ? mundoFisico.PosicionPiesJugador() : posicionPiesDesdeCamara(); },
		[] { return serializadorEscena.Cargado() ? serializadorEscena.Ruta().stem().string() : std::string(); },
		[](std::string& error, bool autoridadFisica) { return reconstruirFisicaTrasEdicion(error, autoridadFisica); },
		[] { return entradaFPSActiva(); },
		[](glm::vec3& origen, glm::vec3& direccion) { origenYDireccionDeLaMirada(origen, direccion); },
		[] { return diagnosticoAgarreActivo(); },
		[](const char* evento, uint32_t id, net::PeerId peer, net::PeerId propietario,
			const glm::vec3& posicion, const glm::vec3& punto, const char* detalle)
			{ trazarAgarre(evento, id, peer, propietario, posicion, punto, detalle); },
		[]
		{
			animacionHiriti.DescubrirPorInteraccion();
			if (animacionHiriti.MarcarAgarrado())
				reproducirSonidoHiriti("assets/audio/hirity/story/lohashechobien.ogg");
		},
		[](float carga) { animacionHiriti.MarcarLanzadoPorJugador(carga); }
	};
	return contexto;
}

bool capturarEstadoEscenaRed(std::vector<netgame::SceneObjectState>& salida)
{
	return netgame::session::CaptureSceneState(contextoSesionRed(), salida);
}

void iniciarRed(const netgame::NetworkConfig& cfg)
{
	netgame::session::StartSession(contextoSesionRed(), cfg);
}

void detenerRed()
{
	netgame::session::StopSession(contextoSesionRed());
}

// Una vez por fotograma, en el hilo de juego. Offline: no hace nada (coste cero).
void actualizarRed(double ahora, bool jugando)
{
	netgame::session::UpdateSession(contextoSesionRed(), ahora, jugando);
}
