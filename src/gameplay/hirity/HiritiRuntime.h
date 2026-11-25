#pragma once

// Ciclo de Hiriti en partida: voz (atenuada por distancia), aparicion por la espalda, seguimiento,
// actualizacion por fotograma y audio ambiental. src/app es el dueno de todo el estado y lo presta
// mediante este contexto; aqui no se puede incluir las cabeceras de estado de la aplicacion.
// La API publica va en ingles; src/app enlaza cada miembro con su global en hiritiBindings().
#include "src/gameplay/hirity/HiritiAudioTimer.h"
#include "src/audio/AudioManager.h"
#include "src/gameplay/hirity/HiritiAnimation.h"
#include "src/networking/replication/NetGame.h"
#include "src/physics/PhysicsScene.h"
#include "src/physics/PhysicsWorld.h"
#include "src/scene/SceneLoader.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace game::hiriti
{
	struct HiritiContext
	{
		// Estado prestado
		AudioManager& audioManager;
		HiritiAnimator& hiritiAnimation;
		HiritiAudioTimer& hiritiAudioTimer;
		const std::vector<SceneObject>& sceneObjects;
		// Referencia al unique_ptr: la carga de habitacion sustituye la escena fisica.
		std::unique_ptr<fisica::PhysicsScene>& physicsScene;
		const fisica::PhysicsWorld& physicsWorld;
		const netgame::NetworkGame& session;
		bool& hiritiFollowEnabled;
		const float& hiritiStopDistance;
		const float& hiritiMaxSpeed;

		// Servicios que siguen en src/app (camara, reloj y diagnostico de agarre)
		std::function<void(glm::vec3& origin, glm::vec3& direction)> lookRay;
		std::function<glm::vec2()> cameraHorizontalForward;	// sin normalizar
		std::function<double()> clockSeconds;
		std::function<bool()> grabDiagnosticsEnabled;
		std::function<void(const std::string& record)> emitGrabDiagnostic;
		std::function<void(const char* event, uint32_t id, net::PeerId peer, net::PeerId owner,
			const glm::vec3& position, const glm::vec3& point, const char* detail)> traceGrab;
	};

	void PlayHiritiSound(const HiritiContext& context, const std::string& path,
		float baseVolume = 1.0f);
	void UpdateHiriti(const HiritiContext& context, bool canDiscover,
		bool canProcessCollisions, float delta);
	void UpdateAmbientAudio(const HiritiContext& context, bool active, float delta);
}
