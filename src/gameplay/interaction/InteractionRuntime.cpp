#include "src/gameplay/interaction/InteractionRuntime.h"

#include <algorithm>

namespace game::interaction
{
	bool PlayerHasGrab(const InteractionContext& context)
	{
		auto& escenaFisica = context.physicsScene;
		auto& redJuego = context.session;

		return escenaFisica->HayAgarre() ||
			(redJuego.EsCliente() && redJuego.ObjetoDinamicoDePeer(redJuego.PeerLocal()) != 0);
	}

	InteractionTarget FindInteractionTarget(const InteractionContext& context)
	{
		auto& escenaFisica = context.physicsScene;
		auto& mundoFisico = context.physicsWorld;
		auto& objetosAnimados = context.animatedObjects;

		if (PlayerHasGrab(context)) return {};

		glm::vec3 origen, direccion;
		context.lookRay(origen, direccion);
		const float alcance = std::max(fisica::PhysicsScene::AlcanceDeAgarre, glb::RADIO_INTERACCION);
		float distancia = std::numeric_limits<float>::infinity();
		const fisica::IdCuerpo cuerpoMirado = mundoFisico.CuerpoAlFinalDelRayo(origen, direccion, alcance, &distancia);
		if (cuerpoMirado == fisica::CuerpoInvalido) return {};

		const int indiceDinamico = escenaFisica->ObjetoDinamicoDeCuerpo(cuerpoMirado);
		if (indiceDinamico >= 0)
			return { InteractionAction::Recoger, indiceDinamico, nullptr, distancia, "[E] Coger" };

		const int indiceAnimado = escenaFisica->ObjetoAnimadoDeCuerpo(cuerpoMirado);
		if (indiceAnimado < 0 || indiceAnimado >= static_cast<int>(objetosAnimados.objetos.size()))
			return {};

		glb::AnimatedObject* puerta = glb::BuscarObjetoInteractuable(objetosAnimados, origen, direccion);
		if (puerta == nullptr || puerta != &objetosAnimados.objetos[indiceAnimado])
			return {};

		const InteractionAction accion =
			(puerta->estado == glb::DoorState::Cerrada || puerta->estado == glb::DoorState::Cerrando)
			? InteractionAction::Abrir : InteractionAction::Cerrar;
		return { accion, -1, puerta, distancia, accion == InteractionAction::Abrir ? "[E] Abrir" : "[E] Cerrar" };
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
		auto& objetosEscena = context.sceneObjects;
		auto& escenaFisica = context.physicsScene;
		auto& redJuego = context.session;
		auto& puertasRed = context.networkDoors;

		const InteractionTarget objetivo = context.currentTarget;
		// Objeto compartido de prueba (solo con red): si se apunta a el, pedirlo/soltarlo tiene prioridad.
		if (redJuego.Activa())
		{
			glm::vec3 origenRed, direccionRed;
			context.lookRay(origenRed, direccionRed);
			if (redJuego.AlternarObjeto(origenRed, direccionRed, context.clockSeconds())) return;
		}
		if (objetivo.accion == InteractionAction::Recoger)
		{
			if (objetivo.indiceDinamico < 0 || objetivo.indiceDinamico >= static_cast<int>(objetosEscena.size())) return;
			glm::vec3 origen, direccion;
			context.lookRay(origen, direccion);
			const uint32_t id = netgame::HashId(objetosEscena[objetivo.indiceDinamico].id);
			bool agarrado = false;
			if (redJuego.Activa() && redJuego.EsCliente())
			{
				const bool encolada = redJuego.SolicitarAgarreDinamico(id, origen, direccion);
				context.traceGrab("request", id, redJuego.PeerLocal(), redJuego.PropietarioDinamico(id),
					origen, direccion, encolada ? "client-kinematic-request-queued" : "client-request-queue-failed");
			}
			else if (redJuego.Activa() && redJuego.EsHost())
			{
				const net::PeerId propietario = redJuego.PropietarioDinamico(id);
				if (propietario == netgame::kSinDueno &&
					escenaFisica->SujetarObjetoDinamico(objetivo.indiceDinamico, origen, direccion))
				{
					agarrado = true;
					redJuego.FijarPropietarioDinamico(id, net::kHostPeerId);
				}
				else context.traceGrab("reject", id, redJuego.PeerLocal(), propietario, origen, direccion,
					propietario == netgame::kSinDueno ? "host-local-physics-rejected-grab" : "host-local-object-owned-by-peer");
			}
			else
			{
				agarrado = escenaFisica->SujetarObjetoDinamico(objetivo.indiceDinamico, origen, direccion);
				if (!agarrado) context.traceGrab("reject", id, net::kUnassignedPeerId, netgame::kSinDueno,
					origen, direccion, "offline-local-physics-rejected-grab");
			}
			if (agarrado && objetosEscena[objetivo.indiceDinamico].id == "hiriti")
				context.onHiritiGrabbed();
			if (agarrado)
			{
				glm::vec3 posicion(0.0f);
				escenaFisica->PosicionObjetoDinamico(objetivo.indiceDinamico, posicion);
				const net::PeerId peer = redJuego.Activa() ? redJuego.PeerLocal() : net::kUnassignedPeerId;
				context.traceGrab("local-grab", id, peer, peer, posicion,
					fisica::PhysicsScene::DestinoDeAgarre(origen, direccion),
					redJuego.EsHost() ? "authoritative-host-dynamic" : "offline-dynamic");
			}
		}
		else if ((objetivo.accion == InteractionAction::Abrir || objetivo.accion == InteractionAction::Cerrar) &&
			objetivo.objetoAnimado != nullptr)
		{
			// Cliente: la puerta la decide el host (se envia la solicitud y se espera su estado).
			size_t indicePuerta = 0;
			if (puertasRed && puertasRed->IndiceDe(objetivo.objetoAnimado, indicePuerta) && redJuego.InterceptarPuerta(indicePuerta)) return;
			glb::AlternarPuerta(*objetivo.objetoAnimado);
		}
	}

	// Cada fotograma de juego: la "mano" sigue a la mirada. Solo si se puede jugar (sin pausa, sin
	// paneles F1 y con foco); si no, el objeto se queda donde estaba.
	void UpdatePlayerGrab(const InteractionContext& context, bool canPlay)
	{
		if (!canPlay) return;
		glm::vec3 origen, direccion;
		context.lookRay(origen, direccion);
		context.physicsScene->ActualizarAgarre(origen, direccion);
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
		auto& objetosEscena = context.sceneObjects;
		auto& escenaFisica = context.physicsScene;
		auto& redJuego = context.session;

		if (!context.throwChargeActive) return;
		const float carga = ThrowChargeFraction(context.throwChargeSeconds);
		CancelThrowCharge(context);
		if (!PlayerHasGrab(context)) return;

		glm::vec3 origen, direccion;
		context.lookRay(origen, direccion);
		const float rapidez = ThrowSpeed(carga);
		const glm::vec3 velocidad = glm::normalize(direccion) * rapidez;
		if (redJuego.Activa() && redJuego.EsCliente())
		{
			const uint32_t id = redJuego.ObjetoDinamicoDePeer(redJuego.PeerLocal());
			const auto it = std::find_if(objetosEscena.begin(), objetosEscena.end(), [id](const SceneObject& objeto)
				{ return id != 0 && netgame::HashId(objeto.id) == id; });
			if (id != 0)
			{
				const bool encolada = redJuego.SolicitarSoltarDinamico(id, velocidad);
				context.traceGrab(encolada ? "release-request" : "release-rejected", id, redJuego.PeerLocal(),
					redJuego.PropietarioDinamico(id), glm::vec3(0.0f), velocidad,
					encolada ? "client-release-request-queued" : "client-release-request-queue-failed");
				if (encolada && it != objetosEscena.end() && it->id == "hiriti")
					context.onHiritiThrown(carga);
			}
		}
		else if (escenaFisica->HayAgarre())
		{
			const int indice = escenaFisica->ObjetoAgarrado();
			const uint32_t id = indice >= 0 && indice < static_cast<int>(objetosEscena.size())
				? netgame::HashId(objetosEscena[indice].id) : 0;
			if (escenaFisica->LanzarAgarre(velocidad))
			{
				if (redJuego.Activa() && redJuego.EsHost() && id != 0)
					redJuego.FijarPropietarioDinamico(id, netgame::kSinDueno);
				if (indice >= 0 && indice < static_cast<int>(objetosEscena.size()) && objetosEscena[indice].id == "hiriti")
					context.onHiritiThrown(carga);
				glm::vec3 posicion(0.0f);
				if (indice >= 0) escenaFisica->PosicionObjetoDinamico(indice, posicion);
				context.traceGrab("released", id, redJuego.Activa() ? redJuego.PeerLocal() : net::kUnassignedPeerId,
					netgame::kSinDueno, posicion, velocidad, "local-launch-applied-throw-velocity");
			}
		}
	}

	void UpdateThrowCharge(const InteractionContext& context, float delta,
		const std::function<bool()>& throwButtonHeld)
	{
		auto& escenaFisica = context.physicsScene;
		auto& redJuego = context.session;

		if (!context.throwChargeActive) return;
		const uint32_t objetoPropio = redJuego.Activa() && redJuego.EsCliente()
			? redJuego.ObjetoDinamicoDePeer(redJuego.PeerLocal()) : 0;
		if (!context.fpsInputActive() || !netgame::PuedeCargarLanzamiento(escenaFisica->HayAgarre(),
			redJuego.EsCliente() ? net::Role::Client : net::Role::Offline, objetoPropio))
		{
			CancelThrowCharge(context);
			return;
		}
		if (throwButtonHeld())
			context.throwChargeSeconds = AdvanceThrowCharge(context.throwChargeSeconds, delta);
	}
}
