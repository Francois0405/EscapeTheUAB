#pragma once

// Interaccion del jugador local: objetivo mirado, accion "interactuar", seguimiento del agarre y
// carga/lanzamiento. main.cpp es el dueno de todo el estado y lo presta mediante este contexto;
// aqui no se puede incluir main.h (define globales).
// La API publica va en ingles; main.cpp enlaza cada miembro con su global en contextoInteraccion().
#include "src/animation/ObjetosAnimadosGLB.h"
#include "src/gameplay/interaction/ThrowCharge.h"
#include "src/networking/replication/NetAdaptadores.h"
#include "src/networking/replication/NetJuego.h"
#include "src/physics/EscenaFisica.h"
#include "src/physics/MundoFisico.h"
#include "src/scene/SceneLoader.h"

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

namespace juego::interaction
{
	// El valor numerico aparece en los registros de diagnostico de agarre: no reordenar.
	enum class AccionInteraccion { Ninguna, Recoger, Abrir, Cerrar };

	struct ObjetivoInteraccion
	{
		AccionInteraccion accion = AccionInteraccion::Ninguna;
		int indiceDinamico = -1;
		glb::ObjetoAnimado* objetoAnimado = nullptr;
		float distancia = std::numeric_limits<float>::infinity();
		const char* texto = "";
	};

	struct InteractionContext
	{
		// Estado prestado
		const std::vector<SceneObject>& sceneObjects;
		// Referencias al unique_ptr: la carga de habitacion sustituye la escena fisica y las puertas.
		std::unique_ptr<fisica::EscenaFisica>& physicsScene;
		const fisica::MundoFisico& physicsWorld;
		netjuego::RedJuego& session;
		glb::EscenaAnimada& animatedObjects;
		std::unique_ptr<netjuego::PuertasGLB>& networkDoors;
		ObjetivoInteraccion& currentTarget;
		bool& throwChargeActive;
		float& throwChargeSeconds;
		const bool& debugPanelsVisible;

		// Servicios que siguen en main.cpp (estado de entrada, camara, reloj, diagnostico y Hirity)
		std::function<bool()> fpsInputActive;
		std::function<bool()> editorActive;
		std::function<bool()> xrSessionRunning;
		std::function<void(glm::vec3& origin, glm::vec3& direction)> lookRay;
		std::function<double()> clockSeconds;
		std::function<void(const char* event, uint32_t id, net::PeerId peer, net::PeerId owner,
			const glm::vec3& position, const glm::vec3& point, const char* detail)> traceGrab;
		std::function<void()> onHiritiGrabbed;
		std::function<void(float charge)> onHiritiThrown;
	};

	bool PlayerHasGrab(const InteractionContext& context);
	ObjetivoInteraccion FindInteractionTarget(const InteractionContext& context);
	void UpdateInteractionTarget(const InteractionContext& context, bool allowed);
	// Quien la llame debe comprobar antes la pausa, el foco y el teclado de ImGui.
	void Interact(const InteractionContext& context);
	void UpdatePlayerGrab(const InteractionContext& context, bool canPlay);
	void StartThrowCharge(const InteractionContext& context);
	void CancelThrowCharge(const InteractionContext& context);
	void ThrowChargedObject(const InteractionContext& context);
	// throwButtonHeld solo se consulta si la carga sigue siendo valida.
	void UpdateThrowCharge(const InteractionContext& context, float delta,
		const std::function<bool()>& throwButtonHeld);
}
