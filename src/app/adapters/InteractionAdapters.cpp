#include "src/app/AppIncludes.h"
#include "src/app/state/SceneState.h"
#include "src/app/state/PlayerState.h"
#include "src/app/state/GameplayState.h"
#include "src/app/state/NetworkState.h"
#include "src/app/state/VrState.h"
#include "src/app/adapters/InteractionAdapters.h"
#include "src/app/adapters/HirityAdapters.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/app/input/CursorCapture.h"
#include "src/app/adapters/GrabTrace.h"

// Contexto prestado a src/gameplay/interaction: el estado sigue siendo de main.cpp.
const game::interaction::InteractionContext& interactionBindings()
{
	static const game::interaction::InteractionContext contexto{
		sceneObjects, physicsScene, physicsWorld, networkGame, animatedObjects, networkDoors,
		currentInteractionTarget, throwChargeActive, throwChargeTime, showDebugPanels,
		[] { return fpsInputActive(); },
		[] { return sceneEditor.IsEnabled(); },
		[] { return openXrManager.IsSessionRunning(); },
		[](glm::vec3& origen, glm::vec3& direccion) { gazeOriginAndDirection(origen, direccion); },
		[] { return glfwGetTime(); },
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

bool playerHoldsObject()
{
	return game::interaction::PlayerHasGrab(interactionBindings());
}

InteractionTarget findCurrentInteractionTarget()
{
	return game::interaction::FindInteractionTarget(interactionBindings());
}

void updateCurrentInteractionTarget(bool permitido)
{
	game::interaction::UpdateInteractionTarget(interactionBindings(), permitido);
}

void beginThrowCharge()
{
	game::interaction::StartThrowCharge(interactionBindings());
}

void performInteraction()
{
	game::interaction::Interact(interactionBindings());
}

void updateHeldObject(bool puedeJugar)
{
	game::interaction::UpdatePlayerGrab(interactionBindings(), puedeJugar);
}

void cancelThrowChargeInput()
{
	game::interaction::CancelThrowCharge(interactionBindings());
}

void throwHeldObject()
{
	game::interaction::ThrowChargedObject(interactionBindings());
}

void updateThrowChargeInput(GLFWwindow* window, float delta)
{
	game::interaction::UpdateThrowCharge(interactionBindings(), delta, [window]
		{ return glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS || openXrManager.GatilloPulsado(); });
}
