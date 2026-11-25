#pragma once

// Sesion de red en ejecucion: arranque, parada, bucle por fotograma, autoridad de agarre del host,
// replicas del cliente y estado de escena. No define el protocolo ni el transporte. src/app es el
// dueno de todo el estado y lo presta mediante este contexto; aqui no se puede incluir las cabeceras de estado
// (define globales).
// La API publica va en ingles; src/app enlaza cada miembro en sessionBindings().
#include "src/animation/GlbAnimatedObjects.h"
#include "src/networking/replication/Game/RemotePhysicsTimeline.h"
#include "src/networking/replication/NetAdapters.h"
#include "src/networking/replication/NetGame.h"
#include "src/physics/PhysicsScene.h"
#include "src/physics/PhysicsWorld.h"
#include "src/scene/SceneLoader.h"
#include "src/vr/OpenXRManager.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace netgame::session
{
	// Estado que solo usa la sesion; lo instancia src/app.
	struct SessionRuntimeState
	{
		bool physicsInReplicaMode = false;
		std::map<uint32_t, bool> sleepingObjects;
		// true: agarre de escritorio anclado a cabeza/mirada; false: rayo de mando VR.
		std::map<uint32_t, bool> headAnchoredRemoteGrabs;
		timeline::RemotePhysicsBuffers remotePhysics;
	};

	struct SessionContext
	{
		// Estado prestado
		std::vector<SceneObject>& sceneObjects;
		// Referencia al unique_ptr: reconstruir la fisica sustituye la escena fisica en mitad de una llamada.
		std::unique_ptr<fisica::PhysicsScene>& physicsScene;
		const fisica::PhysicsWorld& physicsWorld;
		NetworkGame& session;
		glb::AnimatedScene& animatedObjects;
		std::unique_ptr<GlbDoors>& networkDoors;
		SessionRuntimeState& state;
		const vr::OpenXrManager& xr;
		const vr::XrPose& headPose;		// pose del visor del ultimo fotograma VR
		const bool& hasHeadPose;

		// Servicios que siguen en src/app (camara, escena, entrada, diagnostico y Hirity)
		std::function<glm::vec3()> eyePosition;
		std::function<glm::vec3()> viewForward;	// punto mirado menos ojos, sin normalizar
		std::function<glm::vec3()> bodyFeetPosition;
		std::function<std::string()> sceneName;	// nombre del fichero de escena; vacio si no hay
		std::function<bool(std::string& error, bool physicsAuthority)> rebuildPhysics;
		std::function<bool()> fpsInputActive;
		std::function<void(glm::vec3& origin, glm::vec3& direction)> lookRay;
		std::function<bool()> grabDebugActive;
		std::function<void(const char* event, uint32_t id, net::PeerId peer, net::PeerId owner,
			const glm::vec3& position, const glm::vec3& point, const char* detail)> traceGrab;
		std::function<void()> onHiritiGrabbed;
		std::function<void(float charge)> onHiritiThrown;
	};

	bool CaptureSceneState(const SessionContext& context, std::vector<SceneObjectState>& output);
	void StartSession(const SessionContext& context, const NetworkConfig& config);
	void StopSession(const SessionContext& context);
	// Una vez por fotograma, en el hilo de juego. Offline: no hace nada (coste cero).
	void UpdateSession(const SessionContext& context, double now, bool playing);
}
