#include "src/app/AppIncludes.h"
#include "src/app/state/SceneState.h"
#include "src/app/state/GameplayState.h"
#include "src/app/state/NetworkState.h"
#include "src/app/state/VrState.h"
#include "src/app/adapters/SessionAdapters.h"
#include "src/app/adapters/HirityAdapters.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/app/adapters/SceneAdapters.h"
#include "src/app/input/CursorCapture.h"
#include "src/app/adapters/GrabTrace.h"

// State used only by this file.
namespace
{
	netgame::session::SessionRuntimeState networkSessionState;  // estado propio de src/networking/session
}

// Contexto prestado a src/networking/session: el estado sigue siendo de main.cpp.

const netgame::session::SessionContext& sessionBindings()
{
	static const netgame::session::SessionContext contexto{
		sceneObjects, physicsScene, physicsWorld, networkGame, animatedObjects, networkDoors, networkSessionState,
		openXrManager, lastVrHead, hasVrHead,
		[] { return glm::vec3(static_cast<float>(opvN.x), static_cast<float>(opvN.y), static_cast<float>(opvN.z)); },
		[] { return glm::vec3(static_cast<float>(n[0] - opvN.x), static_cast<float>(n[1] - opvN.y), static_cast<float>(n[2] - opvN.z)); },
		[] { return physicsWorld.HasPlayerBody() ? physicsWorld.PlayerFeetPosition() : feetPositionFromCamera(); },
		[] { return sceneSerializer.IsLoaded() ? sceneSerializer.Ruta().stem().string() : std::string(); },
		[](std::string& error, bool autoridadFisica) { return rebuildPhysicsAfterEditing(error, autoridadFisica); },
		[] { return fpsInputActive(); },
		[](glm::vec3& origen, glm::vec3& direccion) { gazeOriginAndDirection(origen, direccion); },
		[] { return grabTraceEnabled(); },
		[](const char* evento, uint32_t id, net::PeerId peer, net::PeerId propietario,
			const glm::vec3& posicion, const glm::vec3& punto, const char* detalle)
			{ traceGrab(evento, id, peer, propietario, posicion, punto, detalle); },
		[]
		{
			hiritiAnimator.DiscoverByInteraction();
			if (hiritiAnimator.MarkGrabbed())
				playHiritiSoundCue("assets/audio/hirity/story/lohashechobien.ogg");
		},
		[](float carga) { hiritiAnimator.MarkThrownByPlayer(carga); }
	};
	return contexto;
}

bool captureNetworkSceneState(std::vector<netgame::SceneObjectState>& salida)
{
	return netgame::session::CaptureSceneState(sessionBindings(), salida);
}

void startNetwork(const netgame::NetworkConfig& cfg)
{
	netgame::session::StartSession(sessionBindings(), cfg);
}

void stopNetwork()
{
	netgame::session::StopSession(sessionBindings());
}

// Una vez por fotograma, en el hilo de juego. Offline: no hace nada (coste cero).
void updateNetwork(double ahora, bool jugando)
{
	netgame::session::UpdateSession(sessionBindings(), ahora, jugando);
}
