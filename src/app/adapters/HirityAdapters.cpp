#include "src/app/AppState.h"
#include "src/app/adapters/HirityAdapters.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/diagnostics/GrabTrace.h"

// Contexto prestado a src/gameplay/hirity: el estado sigue siendo de main.cpp.
const juego::hiriti::HiritiContext& contextoHiriti()
{
	static const juego::hiriti::HiritiContext contexto{
		audioManager, animacionHiriti, temporizadorAudioHiriti, objetosEscena, escenaFisica, mundoFisico,
		redJuego, hiritiFollowPlayerEnabled, hiritiFollowStopDistance, hiritiFollowMaxSpeed,
		[](glm::vec3& origen, glm::vec3& direccion) { origenYDireccionDeLaMirada(origen, direccion); },
		[] { return glm::vec2(static_cast<float>(n[0] - opvN.x), static_cast<float>(n[1] - opvN.y)); },
		[] { return glfwGetTime(); },
		[] { return diagnosticoAgarreActivo(); },
		[](const std::string& registro) { emitirDiagnosticoAgarre(registro); },
		[](const char* evento, uint32_t id, net::PeerId peer, net::PeerId propietario,
			const glm::vec3& posicion, const glm::vec3& punto, const char* detalle)
			{ trazarAgarre(evento, id, peer, propietario, posicion, punto, detalle); }
	};
	return contexto;
}

void reproducirSonidoHiriti(const std::string& ruta, float volumenBase)
{
	juego::hiriti::PlayHiritiSound(contextoHiriti(), ruta, volumenBase);
}

void actualizarHiriti(bool puedeDescubrir, bool puedeProcesarColisiones, float delta)
{
	juego::hiriti::UpdateHiriti(contextoHiriti(), puedeDescubrir, puedeProcesarColisiones, delta);
}

void actualizarAudioAmbiente(bool activo, float delta)
{
	juego::hiriti::UpdateAmbientAudio(contextoHiriti(), activo, delta);
}
