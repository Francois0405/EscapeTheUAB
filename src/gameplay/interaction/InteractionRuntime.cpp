#include "src/gameplay/interaction/InteractionRuntime.h"

#include <algorithm>

namespace game::interaction
{
	bool PlayerHasGrab(const InteractionContext& context)
	{
		auto& physicsScene = context.physicsScene;
		auto& networkGame = context.session;

		return physicsScene->HasGrab() ||
			(networkGame.IsClient() && networkGame.DynamicObjectOfPeer(networkGame.LocalPeerId()) != 0);
	}

	InteractionTarget FindInteractionTarget(const InteractionContext& context)
	{
		auto& physicsScene = context.physicsScene;
		auto& physicsWorld = context.physicsWorld;
		auto& animatedObjects = context.animatedObjects;

		if (PlayerHasGrab(context)) return {};

		glm::vec3 origen, direccion;
		context.lookRay(origen, direccion);
		const float alcance = std::max(physics::PhysicsScene::AlcanceDeAgarre, glb::RADIO_INTERACCION);
		float distancia = std::numeric_limits<float>::infinity();
		const physics::IdCuerpo cuerpoMirado = physicsWorld.BodyAtRayEnd(origen, direccion, alcance, &distancia);
		if (cuerpoMirado == physics::CuerpoInvalido) return {};

		const int indiceDinamico = physicsScene->DynamicObjectOfBody(cuerpoMirado);
		if (indiceDinamico >= 0)
			return { InteractionAction::PickUp, indiceDinamico, nullptr, distancia, "[E] Coger" };

		const int indiceAnimado = physicsScene->AnimatedObjectOfBody(cuerpoMirado);
		if (indiceAnimado < 0 || indiceAnimado >= static_cast<int>(animatedObjects.objetos.size()))
			return {};

		glb::AnimatedObject* puerta = glb::FindInteractableObject(animatedObjects, origen, direccion);
		if (puerta == nullptr || puerta != &animatedObjects.objetos[indiceAnimado])
			return {};

		const InteractionAction accion =
			(puerta->estado == glb::DoorState::Closed || puerta->estado == glb::DoorState::Closing)
			? InteractionAction::Open : InteractionAction::Close;
		return { accion, -1, puerta, distancia, accion == InteractionAction::Open ? "[E] Abrir" : "[E] Cerrar" };
	}

	void UpdateInteractionTarget(const InteractionContext& context, bool allowed)
	{
		context.currentTarget = {};
		if (!allowed || !context.fpsInputActive() || context.editorActive() ||
			context.debugPanelsVisible || context.xrSessionRunning() || PlayerHasGrab(context)) return;
		context.currentTarget = FindInteractionTarget(context);
	}

	// Accion "interactuar" (tecla E). Prioridad:
	//   1) Si se mira un objeto agarrable (dinamico) al alcance, se agarra.
	//   2) Si no, se intenta con la puerta que se tiene delante.
	void Interact(const InteractionContext& context)
	{
		auto& sceneObjects = context.sceneObjects;
		auto& physicsScene = context.physicsScene;
		auto& networkGame = context.session;
		auto& networkDoors = context.networkDoors;

		const InteractionTarget objetivo = context.currentTarget;
		// Objeto compartido de prueba (solo con red): si se apunta a el, pedirlo/soltarlo tiene prioridad.
		if (networkGame.IsActive())
		{
			glm::vec3 origenRed, direccionRed;
			context.lookRay(origenRed, direccionRed);
			if (networkGame.ToggleAnimatedObject(origenRed, direccionRed, context.clockSeconds())) return;
		}
		if (objetivo.accion == InteractionAction::PickUp)
		{
			if (objetivo.indiceDinamico < 0 || objetivo.indiceDinamico >= static_cast<int>(sceneObjects.size())) return;
			glm::vec3 origen, direccion;
			context.lookRay(origen, direccion);
			const uint32_t id = netgame::HashId(sceneObjects[objetivo.indiceDinamico].id);
			bool agarrado = false;
			if (networkGame.IsActive() && networkGame.IsClient())
			{
				const bool encolada = networkGame.RequestDynamicGrab(id, origen, direccion);
				context.traceGrab("request", id, networkGame.LocalPeerId(), networkGame.DynamicOwner(id),
					origen, direccion, encolada ? "client-kinematic-request-queued" : "client-request-queue-failed");
			}
			else if (networkGame.IsActive() && networkGame.IsHosting())
			{
				const net::PeerId propietario = networkGame.DynamicOwner(id);
				if (propietario == netgame::kSinDueno &&
					physicsScene->HoldDynamicObject(objetivo.indiceDinamico, origen, direccion))
				{
					agarrado = true;
					networkGame.SetDynamicOwner(id, net::kHostPeerId);
				}
				else context.traceGrab("reject", id, networkGame.LocalPeerId(), propietario, origen, direccion,
					propietario == netgame::kSinDueno ? "host-local-physics-rejected-grab" : "host-local-object-owned-by-peer");
			}
			else
			{
				agarrado = physicsScene->HoldDynamicObject(objetivo.indiceDinamico, origen, direccion);
				if (!agarrado) context.traceGrab("reject", id, net::kUnassignedPeerId, netgame::kSinDueno,
					origen, direccion, "offline-local-physics-rejected-grab");
			}
			if (agarrado && sceneObjects[objetivo.indiceDinamico].id == "hiriti")
				context.onHiritiGrabbed();
			if (agarrado)
			{
				glm::vec3 position(0.0f);
				physicsScene->DynamicObjectPosition(objetivo.indiceDinamico, position);
				const net::PeerId peer = networkGame.IsActive() ? networkGame.LocalPeerId() : net::kUnassignedPeerId;
				context.traceGrab("local-grab", id, peer, peer, position,
					physics::PhysicsScene::GrabDestination(origen, direccion),
					networkGame.IsHosting() ? "authoritative-host-dynamic" : "offline-dynamic");
			}
		}
		else if ((objetivo.accion == InteractionAction::Open || objetivo.accion == InteractionAction::Close) &&
			objetivo.objetoAnimado != nullptr)
		{
			// Cliente: la puerta la decide el host (se envia la solicitud y se espera su estado).
			size_t indicePuerta = 0;
			if (networkDoors && networkDoors->IndexOf(objetivo.objetoAnimado, indicePuerta) && networkGame.InterceptDoor(indicePuerta)) return;
			glb::ToggleDoor(*objetivo.objetoAnimado);
		}
	}

	// Cada fotograma de juego: la "mano" sigue a la mirada. Solo si se puede jugar (sin pausa, sin
	// paneles F1 y con foco); si no, el objeto se queda donde estaba.
	void UpdatePlayerGrab(const InteractionContext& context, bool canPlay)
	{
		if (!canPlay) return;
		glm::vec3 origen, direccion;
		context.lookRay(origen, direccion);
		context.physicsScene->UpdateGrab(origen, direccion);
	}

	void StartThrowCharge(const InteractionContext& context)
	{
		if (!PlayerHasGrab(context)) return;
		context.throwChargeActive = true;
		context.throwChargeSeconds = 0.0f;
	}

	void CancelThrowCharge(const InteractionContext& context)
	{
		context.throwChargeActive = false;
		context.throwChargeSeconds = 0.0f;
	}

	void ThrowChargedObject(const InteractionContext& context)
	{
		auto& sceneObjects = context.sceneObjects;
		auto& physicsScene = context.physicsScene;
		auto& networkGame = context.session;

		if (!context.throwChargeActive) return;
		const float carga = ThrowChargeFraction(context.throwChargeSeconds);
		CancelThrowCharge(context);
		if (!PlayerHasGrab(context)) return;

		glm::vec3 origen, direccion;
		context.lookRay(origen, direccion);
		const float rapidez = ThrowSpeed(carga);
		const glm::vec3 speed = glm::normalize(direccion) * rapidez;
		if (networkGame.IsActive() && networkGame.IsClient())
		{
			const uint32_t id = networkGame.DynamicObjectOfPeer(networkGame.LocalPeerId());
			const auto it = std::find_if(sceneObjects.begin(), sceneObjects.end(), [id](const SceneObject& objeto)
				{ return id != 0 && netgame::HashId(objeto.id) == id; });
			if (id != 0)
			{
				const bool encolada = networkGame.RequestDynamicRelease(id, speed);
				context.traceGrab(encolada ? "release-request" : "release-rejected", id, networkGame.LocalPeerId(),
					networkGame.DynamicOwner(id), glm::vec3(0.0f), speed,
					encolada ? "client-release-request-queued" : "client-release-request-queue-failed");
				if (encolada && it != sceneObjects.end() && it->id == "hiriti")
					context.onHiritiThrown(carga);
			}
		}
		else if (physicsScene->HasGrab())
		{
			const int indice = physicsScene->GrabbedObject();
			const uint32_t id = indice >= 0 && indice < static_cast<int>(sceneObjects.size())
				? netgame::HashId(sceneObjects[indice].id) : 0;
			if (physicsScene->ThrowGrab(speed))
			{
				if (networkGame.IsActive() && networkGame.IsHosting() && id != 0)
					networkGame.SetDynamicOwner(id, netgame::kSinDueno);
				if (indice >= 0 && indice < static_cast<int>(sceneObjects.size()) && sceneObjects[indice].id == "hiriti")
					context.onHiritiThrown(carga);
				glm::vec3 position(0.0f);
				if (indice >= 0) physicsScene->DynamicObjectPosition(indice, position);
				context.traceGrab("released", id, networkGame.IsActive() ? networkGame.LocalPeerId() : net::kUnassignedPeerId,
					netgame::kSinDueno, position, speed, "local-launch-applied-throw-velocity");
			}
		}
	}

	void UpdateThrowCharge(const InteractionContext& context, float delta,
		const std::function<bool()>& throwButtonHeld)
	{
		auto& physicsScene = context.physicsScene;
		auto& networkGame = context.session;

		if (!context.throwChargeActive) return;
		const uint32_t objetoPropio = networkGame.IsActive() && networkGame.IsClient()
			? networkGame.DynamicObjectOfPeer(networkGame.LocalPeerId()) : 0;
		if (!context.fpsInputActive() || !netgame::CanChargeThrow(physicsScene->HasGrab(),
			networkGame.IsClient() ? net::Role::Client : net::Role::Offline, objetoPropio))
		{
			CancelThrowCharge(context);
			return;
		}
		if (throwButtonHeld())
			context.throwChargeSeconds = AdvanceThrowCharge(context.throwChargeSeconds, delta);
	}
}
