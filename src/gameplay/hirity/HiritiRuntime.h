#pragma once

// Ciclo de Hiriti en partida: voz (atenuada por distancia), aparicion por la espalda, seguimiento,
// actualizacion por fotograma y audio ambiental. main.cpp es el dueno de todo el estado y lo presta
// mediante este contexto; aqui no se puede incluir main.h (define globales).
// La API publica va en ingles; main.cpp enlaza cada miembro con su global en contextoHiriti().
#include "src/gameplay/hirity/TemporizadorAmbiente.h"
#include "src/audio/AudioManager.h"
#include "src/gameplay/hirity/AnimacionHiriti.h"
#include "src/networking/replication/NetJuego.h"
#include "src/physics/EscenaFisica.h"
#include "src/physics/MundoFisico.h"
#include "src/scene/SceneLoader.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace juego::hiriti
{
	struct HiritiContext
	{
		// Estado prestado
		AudioManager& audioManager;
		AnimacionHiriti& hiritiAnimation;
		TemporizadorAudioHiriti& hiritiAudioTimer;
		const std::vector<SceneObject>& sceneObjects;
		// Referencia al unique_ptr: la carga de habitacion sustituye la escena fisica.
		std::unique_ptr<fisica::EscenaFisica>& physicsScene;
		const fisica::MundoFisico& physicsWorld;
		const netjuego::RedJuego& session;
		bool& hiritiFollowEnabled;
		const float& hiritiStopDistance;
		const float& hiritiMaxSpeed;

		// Servicios que siguen en main.cpp (camara, reloj y diagnostico de agarre)
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
