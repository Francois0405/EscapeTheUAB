#include "src/networking/session/NetworkSessionRuntime.h"

#include "src/networking/replication/Game/HostPhysicsPublication.h"
#include "src/networking/replication/Game/SceneNetworkState.h"
#include "src/networking/session/SessionRules.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <utility>

namespace netgame::session
{
	namespace
	{
		// Adaptador de entrada. SOLO LEE la camara/mandos; nunca los modifica (la pose local manda).
		bool BuildLocalPose(const SessionContext& context, net::PoseEntry& pose)
		{
			const auto& openXrManager = context.xr;
			auto& physicsWorld = context.physicsWorld;

			const glm::vec3 ojos = context.eyePosition();
			const glm::vec3 frente = context.viewForward();
			if (openXrManager.IsSessionRunning() && context.hasHeadPose)
			{
				const glm::vec3 pies = context.bodyFeetPosition();
				const vr::Anchor anclaje = vr::PlayerAnchor(pies, frente,
					openXrManager.ReferenceSpaceHandle() == vr::ReferenceSpaceKind::Suelo, physicsWorld.PlayerEyeHeight());
				vr::XrPose izq, der;
				const bool hayIzq = openXrManager.ControllerAimPose(0, izq), hayDer = openXrManager.ControllerAimPose(1, der);
				if (netgame::PoseFromVr(context.headPose, hayIzq ? &izq : nullptr, hayDer ? &der : nullptr, anclaje, pose)) return true;
			}
			return netgame::PoseFromDesktop(ojos, frente, pose);
		}

		bool FindDynamicObject(const SessionContext& context, uint32_t id, std::size_t& indice)
		{
			const auto& sceneObjects = context.sceneObjects;
			return netgame::scene_network_state::FindUniqueDynamicSceneObjectIndex(id, sceneObjects, indice);
		}

		bool ApplySceneState(const SessionContext& context, const std::vector<netgame::SceneObjectState>& estados)
		{
			auto& sceneObjects = context.sceneObjects;
			auto& networkGame = context.session;
			const auto& rebuildPhysicsAfterEditing = context.rebuildPhysics;

			std::vector<std::pair<std::size_t, const netgame::SceneObjectState*>> aplicaciones;
			std::map<uint32_t, bool> idsRecibidos;
			for (const auto& e : estados)
			{
				if (!netgame::IsValidSceneObjectState(e)) return false;
				idsRecibidos[e.id] = true; // varios cambios fiables del mismo objeto se aplican en orden
			}
			for (const auto& par : idsRecibidos)
			{
				const auto it = std::find_if(estados.rbegin(), estados.rend(), [&](const auto& e) { return e.id == par.first; });
				if (it == estados.rend()) return false;
				std::size_t indice = sceneObjects.size(), coincidencias = 0;
				for (std::size_t i = 0; i < sceneObjects.size(); ++i)
					if (netgame::HashId(sceneObjects[i].id) == par.first) { indice = i; ++coincidencias; }
				if (coincidencias != 1) return false; // desconocido o ID de escena ambiguo
				aplicaciones.push_back({ indice, &*it });
			}
			std::vector<std::pair<std::size_t, SceneObject>> anteriores;
			for (const auto& aplicacion : aplicaciones)
			{
				SceneObject& objeto = sceneObjects[aplicacion.first];
				anteriores.push_back({ aplicacion.first, objeto });
				const auto& e = *aplicacion.second;
				objeto.posicion = e.posicion; objeto.rotacion = e.rotacion; objeto.escala = e.escala;
				objeto.fisica.presente = e.colisionPresente;
				objeto.fisica.tipo = netgame::ReplicaPhysicsType(e.colisionPresente, e.tipoColision,
					objeto.fisica.tipo, fisica::BodyType::Dinamico, fisica::BodyType::Rebotador,
					fisica::BodyType::Estatico);
				objeto.fisica.mitadTamano = e.mitadTamano; objeto.fisica.desplazamiento = e.desplazamiento; objeto.fisica.masa = e.masa;
			}
			std::string error;
			if (!aplicaciones.empty() && !rebuildPhysicsAfterEditing(error, !networkGame.IsClient()))
			{
				for (const auto& anterior : anteriores) sceneObjects[anterior.first] = anterior.second;
				std::cerr << "Red: se rechazo un cambio de escena porque Bullet no pudo reconstruirlo: " << error << '\n';
				return false;
			}
			return true;
		}
	}

	bool CaptureSceneState(const SessionContext& context, std::vector<SceneObjectState>& salida)
	{
		return netgame::scene_network_state::CaptureSceneNetworkStates(context.sceneObjects, salida);
	}

	void StartSession(const SessionContext& context, const NetworkConfig& cfg)
	{
		auto& networkGame = context.session;
		auto& animatedObjects = context.animatedObjects;
		auto& networkDoors = context.networkDoors;
		bool& fisicaRedEnModoReplica = context.state.physicsInReplicaMode;
		auto& objetosDormidosAntes = context.state.sleepingObjects;
		auto& bufferFisicaRemota = context.state.remotePhysics;
		const auto& rebuildPhysicsAfterEditing = context.rebuildPhysics;

		if (cfg.rol == net::Role::Offline)
		{
			const bool eraCliente = networkGame.IsClient();
			networkGame.StopSessionGame();
			if (eraCliente || fisicaRedEnModoReplica) { std::string error; rebuildPhysicsAfterEditing(error, true); fisicaRedEnModoReplica = false; }
			return;
		}
		if (networkGame.IsOnline()) return;
		bufferFisicaRemota.clear(); objetosDormidosAntes.clear();
		networkDoors = std::make_unique<netgame::GlbDoors>(animatedObjects);
		const std::string nombreEscena = context.sceneName();
		std::vector<netgame::SceneObjectState> estadoEscena;
		if (!CaptureSceneState(context, estadoEscena) || !networkGame.SetSceneState(estadoEscena, false))
		{
			std::cerr << "Red: la escena tiene IDs duplicados o propiedades no validas; no se inicia la sesion.\n";
			return;
		}
		const std::string escena = netgame::SceneNetworkFingerprint(nombreEscena, estadoEscena);
		const glm::vec3 posicionObjetoPrueba = SharedObjectSpawnPosition(context.eyePosition(), context.viewForward());
		if (!networkGame.BeginPlayback(cfg, escena, networkDoors.get(), posicionObjetoPrueba)) return;
		std::string errorFisica;
		if (!rebuildPhysicsAfterEditing(errorFisica, cfg.rol != net::Role::Client))
		{
			std::cerr << "Red: no se pudo preparar la autoridad fisica de la escena: " << errorFisica << '\n';
			networkGame.StopSessionGame();
		}
		else fisicaRedEnModoReplica = cfg.rol == net::Role::Client;
	}

	void StopSession(const SessionContext& context)
	{
		auto& physicsScene = context.physicsScene;
		auto& networkGame = context.session;
		bool& fisicaRedEnModoReplica = context.state.physicsInReplicaMode;
		auto& objetosDormidosAntes = context.state.sleepingObjects;
		auto& agarresRemotosDesdeCabeza = context.state.headAnchoredRemoteGrabs;
		auto& bufferFisicaRemota = context.state.remotePhysics;
		const auto& rebuildPhysicsAfterEditing = context.rebuildPhysics;
		const auto& traceGrab = context.traceGrab;

		const bool eraCliente = networkGame.IsClient();
		if (networkGame.IsHosting())
		{
			for (const auto& par : networkGame.PropietariosDinamicos())
			{
				if (par.second == netgame::kSinDueno || par.second == net::kHostPeerId) continue;
				std::size_t indice = 0;
				if (FindDynamicObject(context, par.first, indice))
				{
					const bool liberado = physicsScene->ReleaseRemoteDynamic(static_cast<int>(indice), glm::vec3(0.0f));
					glm::vec3 posicion(0.0f);
					if (liberado) physicsScene->DynamicObjectPosition(static_cast<int>(indice), posicion);
					traceGrab("disconnect-release", par.first, par.second, par.second, posicion,
						glm::vec3(0.0f), liberado ? "host-session-stopped-and-cleared-peer-owner" : "host-session-stopped-constraint-not-active");
				}
				agarresRemotosDesdeCabeza.erase(par.first);
				networkGame.SetDynamicOwner(par.first, netgame::kSinDueno);
			}
		}
		networkGame.StopSessionGame();
		bufferFisicaRemota.clear(); objetosDormidosAntes.clear();
		agarresRemotosDesdeCabeza.clear();
		if (eraCliente || fisicaRedEnModoReplica)
		{
			std::string error;
			if (!rebuildPhysicsAfterEditing(error, true)) std::cerr << "Offline: no se pudo restaurar la simulacion local: " << error << '\n';
			fisicaRedEnModoReplica = false;
		}
	}

	void UpdateSession(const SessionContext& context, double ahora, bool jugando)
	{
		auto& sceneObjects = context.sceneObjects;
		auto& physicsScene = context.physicsScene;
		auto& networkGame = context.session;
		bool& fisicaRedEnModoReplica = context.state.physicsInReplicaMode;
		auto& objetosDormidosAntes = context.state.sleepingObjects;
		auto& agarresRemotosDesdeCabeza = context.state.headAnchoredRemoteGrabs;
		auto& bufferFisicaRemota = context.state.remotePhysics;
		const auto& rebuildPhysicsAfterEditing = context.rebuildPhysics;
		const auto& traceGrab = context.traceGrab;
		const auto& grabTraceEnabled = context.grabDebugActive;
		const auto& fpsInputActive = context.fpsInputActive;
		const auto& gazeOriginAndDirection = context.lookRay;

		if (!networkGame.IsOnline())
		{
			if (fisicaRedEnModoReplica)
			{
				std::string error;
				if (!rebuildPhysicsAfterEditing(error, true)) std::cerr << "Red: no se pudo restaurar Bullet local: " << error << '\n';
				fisicaRedEnModoReplica = false;
			}
			return;
		}
		net::PoseEntry pose;
		const bool hayPose = BuildLocalPose(context, pose);
		networkGame.Advance(ahora, hayPose ? &pose : nullptr);
		if (networkGame.IsHosting())
		{
			std::vector<netgame::DynamicObjectRequest> solicitudes;
			networkGame.TakePhysicsRequests(solicitudes);
			for (const auto& solicitud : solicitudes)
			{
				const net::PeerId propietario = networkGame.DynamicOwner(solicitud.id);
				std::size_t indice = 0;
				if (solicitud.agarrar)
				{
					if (!netgame::CanAcquireDynamic(propietario, solicitud.peer,
						networkGame.DynamicObjectOfPeer(solicitud.peer)) || !FindDynamicObject(context, solicitud.id, indice))
					{
						traceGrab("reject", solicitud.id, solicitud.peer, propietario, solicitud.origen,
							solicitud.direccion, "owned-busy-or-unknown-dynamic-id");
						continue;
					}
					net::PoseEntry poseRemota;
					if (!networkGame.PoseOfPeer(solicitud.peer, poseRemota))
					{
						traceGrab("reject", solicitud.id, solicitud.peer, propietario, solicitud.origen,
							solicitud.direccion, "peer-pose-unavailable");
						continue;
					}
					if (!RemoteGrabRayMatchesPose(solicitud.origen, solicitud.direccion, poseRemota))
					{
						traceGrab("reject", solicitud.id, solicitud.peer, propietario, solicitud.origen,
							solicitud.direccion, "ray-origin-or-direction-does-not-match-peer-pose");
						continue;
					}
					if (!physicsScene->HoldRemoteDynamic(static_cast<int>(indice), solicitud.origen, solicitud.direccion))
					{
						traceGrab("reject", solicitud.id, solicitud.peer, propietario, solicitud.origen,
							solicitud.direccion, "host-ray-missed-target-or-target-already-constrained");
						continue;
					}
					// En escritorio el rayo sale de la cabeza, mientras que la mano del avatar es un
					// marcador visual a 0,5 m. En VR el rayo y la pose real salen del mismo mando.
					const bool desdeCabeza = GrabAnchorsToHead(solicitud.origen, poseRemota);
					agarresRemotosDesdeCabeza[solicitud.id] = desdeCabeza;
					networkGame.SetDynamicOwner(solicitud.id, solicitud.peer);
					glm::vec3 posicionObjeto(0.0f);
					physicsScene->DynamicObjectPosition(static_cast<int>(indice), posicionObjeto);
					const net::Transform7& poseAgarre = desdeCabeza ? poseRemota.head : poseRemota.rightHand;
					const glm::vec3 puntoAgarre = fisica::PhysicsScene::GrabDestination(
						netgame::PositionOfPose(poseAgarre), netgame::RotationOfPose(poseAgarre) * glm::vec3(0, 1, 0));
					traceGrab("accepted", solicitud.id, solicitud.peer, solicitud.peer, posicionObjeto,
						puntoAgarre, desdeCabeza ? "host-dynamic-remote-constraint-head-anchor" : "host-dynamic-remote-constraint-vr-hand-anchor");
					if (sceneObjects[indice].id == "hiriti") context.onHiritiGrabbed();
				}
				else if (propietario == solicitud.peer)
				{
					const bool liberado = FindDynamicObject(context, solicitud.id, indice) &&
						physicsScene->ReleaseRemoteDynamic(static_cast<int>(indice), solicitud.velocidad);
					glm::vec3 posicionLiberada(0.0f);
					if (liberado) physicsScene->DynamicObjectPosition(static_cast<int>(indice), posicionLiberada);
					traceGrab(solicitud.desconexionPeer ? "disconnect-release" : (liberado ? "released" : "release-rejected"), solicitud.id, solicitud.peer,
						propietario, posicionLiberada, solicitud.velocidad,
						solicitud.desconexionPeer
							? (liberado ? "peer-left-and-host-restored-gravity" : "peer-left-but-constraint-not-active")
							: (liberado ? "host-restored-gravity-and-applied-throw-velocity" : "constraint-was-not-active"));
					if (liberado && sceneObjects[indice].id == "hiriti")
						context.onHiritiThrown(ThrowChargeFromReleaseVelocity(solicitud.velocidad));
					agarresRemotosDesdeCabeza.erase(solicitud.id);
					// La propiedad también se limpia si el cuerpo ya no existe/reconstruyó.
					networkGame.SetDynamicOwner(solicitud.id, netgame::kSinDueno);
				}
				else if (!solicitud.agarrar)
				{
					traceGrab("release-rejected", solicitud.id, solicitud.peer, propietario,
						solicitud.velocidad, glm::vec3(0.0f), "requester-is-not-current-owner");
				}
			}

			for (const auto& par : networkGame.PropietariosDinamicos())
			{
				if (par.second == netgame::kSinDueno) continue;
				static std::map<uint32_t, double> ultimaMuestraAgarre;
				std::size_t indice = 0; net::PoseEntry poseRemota;
				if (!FindDynamicObject(context, par.first, indice)) continue;
				if (par.second == net::kHostPeerId)
				{
					if (physicsScene->GrabbedObject() != static_cast<int>(indice))
					{
						traceGrab("auto-drop", par.first, par.second, par.second, glm::vec3(0.0f),
							glm::vec3(0.0f), "host-owner-set-but-local-constraint-missing-owner-cleared");
						agarresRemotosDesdeCabeza.erase(par.first);
						networkGame.SetDynamicOwner(par.first, netgame::kSinDueno);
					}
					else if (grabTraceEnabled())
					{
						auto& ultima = ultimaMuestraAgarre[par.first];
						if (ahora - ultima >= 0.5)
						{
							glm::vec3 posicion(0.0f);
							if (physicsScene->DynamicObjectPosition(static_cast<int>(indice), posicion))
								traceGrab("holding", par.first, net::kHostPeerId, net::kHostPeerId, posicion,
									posicion, "host-local-dynamic-constraint-active");
							ultima = ahora;
						}
					}
					continue;
				}
				if (!networkGame.PoseOfPeer(par.second, poseRemota)) continue;
				const auto anclaje = agarresRemotosDesdeCabeza.find(par.first);
				const bool desdeCabeza = anclaje != agarresRemotosDesdeCabeza.end() && anclaje->second;
				const net::Transform7& poseAnclaje = desdeCabeza ? poseRemota.head : poseRemota.rightHand;
				const glm::vec3 origenAgarre = netgame::PositionOfPose(poseAnclaje);
				const glm::vec3 direccionAgarre = netgame::RotationOfPose(poseAnclaje) * glm::vec3(0, 1, 0);
				const glm::vec3 destino = fisica::PhysicsScene::GrabDestination(origenAgarre, direccionAgarre);
				std::vector<fisica::PhysicsScene::DynamicState> actuales;
				physicsScene->CaptureDynamicStates(actuales);
				auto actual = std::find_if(actuales.begin(), actuales.end(), [&](const auto& e) { return e.indiceObjeto == static_cast<int>(indice); });
				if (actual == actuales.end())
				{
					traceGrab("auto-drop", par.first, par.second, par.second, glm::vec3(0.0f),
						glm::vec3(0.0f), "host-body-missing-position-unavailable-owner-cleared");
					agarresRemotosDesdeCabeza.erase(par.first);
					networkGame.SetDynamicOwner(par.first, netgame::kSinDueno);
				}
				else if (glm::distance(actual->pose.posicion, destino) > fisica::PhysicsScene::SoltarSiSeAleja)
				{
					const bool liberado = physicsScene->ReleaseRemoteDynamic(static_cast<int>(indice), glm::vec3(0.0f));
					traceGrab(liberado ? "auto-release" : "auto-drop", par.first, par.second, par.second, actual->pose.posicion,
						destino, liberado ? "host-dynamic-constraint-exceeded-two-meter-limit" : "distance-limit-but-constraint-not-active");
					agarresRemotosDesdeCabeza.erase(par.first);
					networkGame.SetDynamicOwner(par.first, netgame::kSinDueno);
				}
				else
				{
					physicsScene->UpdateRemoteGrab(static_cast<int>(indice), destino);
					if (grabTraceEnabled())
					{
						auto& ultima = ultimaMuestraAgarre[par.first];
						if (ahora - ultima >= 0.5)
						{
							traceGrab("holding", par.first, par.second, par.second, actual->pose.posicion,
								destino, physicsScene->HeldDynamicObject(static_cast<int>(indice))
									? "host-dynamic-constraint-active" : "owner-set-but-constraint-missing");
							ultima = ahora;
						}
					}
				}
			}

			if (jugando)
			{
				std::vector<fisica::PhysicsScene::DynamicState> fisicos;
				physicsScene->CaptureDynamicStates(fisicos);
				std::vector<net::DynamicStateEntry> estados;
				estados.reserve(fisicos.size());
				for (const auto& fisico : fisicos)
				{
					if (fisico.indiceObjeto < 0 || fisico.indiceObjeto >= static_cast<int>(sceneObjects.size())) continue;
					net::DynamicStateEntry estado;
					estado.objectId = netgame::HashId(sceneObjects[fisico.indiceObjeto].id);
					estado.tick = PhysicsSnapshotTick(ahora);
					estado.transform = netgame::ToTransform(fisico.pose.posicion, fisico.pose.rotacion);
					for (int i = 0; i < 3; ++i)
					{
						estado.linearVelocity[i] = fisico.velocidad[i];
						estado.angularVelocity[i] = fisico.velocidadAngular[i];
					}
					estado.owner = networkGame.DynamicOwner(estado.objectId);
					estado.flags = PhysicsStateFlags(fisico.activo, estado.owner);
					estados.push_back(estado);
					if (netgame::publication::ObserveHostSleepTransition(
						objetosDormidosAntes, estado.objectId, fisico.activo))
						networkGame.PublishFinalPhysicsState(estado);
				}
				networkGame.SendPhysicsStates(estados);
			}
		}
		if (networkGame.IsClient())
		{
			const uint32_t objetoPropio = networkGame.DynamicObjectOfPeer(networkGame.LocalPeerId());
			const auto hiritiPropio = std::find_if(sceneObjects.begin(), sceneObjects.end(), [objetoPropio](const SceneObject& objeto)
				{ return objetoPropio != 0 && netgame::HashId(objeto.id) == objetoPropio && objeto.id == "hiriti"; });
			if (hiritiPropio != sceneObjects.end()) context.onHiritiGrabbed();

			std::vector<netgame::SceneObjectState> estados;
			networkGame.TakeSceneStates(estados);
			if (!estados.empty() && !ApplySceneState(context, estados))
				std::cerr << "Red: actualizacion de objeto desconocido, ambiguo o incompatible descartada.\n";

			std::vector<net::DynamicStateEntry> finales;
			networkGame.TakeReliablePhysicsStates(finales);
			for (const auto& estado : finales)
				netgame::timeline::ApplyReliableFinalPhysicsState(bufferFisicaRemota, estado, ahora);
			for (const auto& par : networkGame.EstadosFisicosRemotos())
				netgame::timeline::StoreRemotePhysicsSample(bufferFisicaRemota, par.second, ahora);

			static double ultimoTiempoFisica = ahora;
			const float delta = std::clamp(static_cast<float>(ahora - ultimoTiempoFisica), 0.0f, 0.1f);
			ultimoTiempoFisica = ahora;
			for (auto& par : bufferFisicaRemota)
			{
				std::size_t indice = 0;
				if (!FindDynamicObject(context, par.first, indice) || par.second.empty()) continue;
				auto objetivo = netgame::timeline::InterpolateRemotePhysicsState(par.second, ahora);
				fisica::PhysicsScene::DynamicState aplicado;
				aplicado.indiceObjeto = static_cast<int>(indice);
				aplicado.pose.posicion = netgame::PositionOfPose(objetivo.transform);
				aplicado.pose.rotacion = netgame::RotationOfPose(objetivo.transform);
				if ((objetivo.flags & 1) != 0)
				{
					std::vector<fisica::PhysicsScene::DynamicState> locales;
					physicsScene->CaptureDynamicStates(locales);
					auto actual = std::find_if(locales.begin(), locales.end(), [&](const auto& e) { return e.indiceObjeto == static_cast<int>(indice); });
					if (actual != locales.end())
					{
						const auto mezcla = netgame::timeline::BlendRemotePhysicsPose(
							actual->pose.posicion, actual->pose.rotacion,
							aplicado.pose.posicion, aplicado.pose.rotacion, delta);
						aplicado.pose.posicion = mezcla.position;
						aplicado.pose.rotacion = mezcla.rotation;
					}
				}
				physicsScene->ApplyReplicatedState(aplicado);
				const net::PeerId propietario = networkGame.DynamicOwner(par.first);
				if (grabTraceEnabled() && propietario != netgame::kSinDueno &&
					networkGame.IsClient())
				{
					static std::map<uint32_t, double> ultimaMuestraReplica;
					auto& ultima = ultimaMuestraReplica[par.first];
					if (ahora - ultima >= 0.5)
					{
						const auto& rotacion = aplicado.pose.rotacion;
						const auto rotacionObjetivo = netgame::RotationOfPose(objetivo.transform);
						std::ostringstream detalle;
						detalle << "client-kinematic;hostSnapshotTick=" << objetivo.tick << ";position-is-interpolated-from-host;rotation=("
							<< rotacion.x << ',' << rotacion.y << ',' << rotacion.z << ',' << rotacion.w
							<< ");host-target-rotation=(" << rotacionObjetivo.x << ',' << rotacionObjetivo.y << ','
							<< rotacionObjetivo.z << ',' << rotacionObjetivo.w << ')';
						traceGrab("replica", par.first, propietario, propietario, aplicado.pose.posicion,
							netgame::PositionOfPose(objetivo.transform), detalle.str().c_str());
						ultima = ahora;
					}
				}
			}
		}
		if (jugando && networkGame.IsActive() && fpsInputActive())
		{
			glm::vec3 origen, direccion;
			gazeOriginAndDirection(origen, direccion);
			networkGame.UpdateCarry(origen, direccion, ahora);
		}
	}
}
