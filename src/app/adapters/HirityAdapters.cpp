#include "src/app/AppIncludes.h"
#include "src/app/state/SceneState.h"
#include "src/app/state/GameplayState.h"
#include "src/app/state/NetworkState.h"
#include "src/app/adapters/HirityAdapters.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/app/adapters/GrabTrace.h"

// State used only by this file.
namespace
{
	game::HiritiAudioTimer hiritiAudioTimer;
}

// Contexto prestado a src/gameplay/hirity: el estado sigue siendo de src/app.
const game::hiriti::HiritiContext& hiritiBindings()
{
	static const game::hiriti::HiritiContext contexto{
		audioManager, hiritiAnimator, hiritiAudioTimer, sceneObjects, physicsScene, physicsWorld,
		networkGame, hiritiFollowPlayerEnabled, hiritiFollowStopDistance, hiritiFollowMaxSpeed,
		[](glm::vec3& origen, glm::vec3& direccion) { gazeOriginAndDirection(origen, direccion); },
		[] { return glm::vec2(static_cast<float>(n[0] - opvN.x), static_cast<float>(n[1] - opvN.y)); },
		[] { return glfwGetTime(); },
		[] { return grabTraceEnabled(); },
		[](const std::string& registro) { emitGrabTrace(registro); },
		[](const char* evento, uint32_t id, net::PeerId peer, net::PeerId propietario,
			const glm::vec3& position, const glm::vec3& punto, const char* detalle)
			{ traceGrab(evento, id, peer, propietario, position, punto, detalle); }
	};
	return contexto;
}

void playHiritiSoundCue(const std::string& ruta, float volumenBase)
{
	game::hiriti::PlayHiritiSound(hiritiBindings(), ruta, volumenBase);
}

void updateHiritiBehaviour(bool puedeDescubrir, bool puedeProcesarColisiones, float delta)
{
	game::hiriti::UpdateHiriti(hiritiBindings(), puedeDescubrir, puedeProcesarColisiones, delta);
}

void updateAmbientSound(bool activo, float delta)
{
	game::hiriti::UpdateAmbientAudio(hiritiBindings(), activo, delta);
}
