#include "src/networking/replication/NetGame.h"
#include "networking/transport/ENetTransport.h"

#include <cstdio>

namespace netgame
{
	namespace
	{
		std::unique_ptr<net::ITransport> FabricaENet(const NetworkConfig& c)
		{
			net::ENetOptions o;
			o.isHost = c.rol == net::Role::Host;
			o.address = c.host; o.port = c.puerto; o.maxClients = 4;
			return net::CreateENetTransport(o);   // nullptr si ENet no esta compilado
		}

		const char* TextoRechazo(net::RejectReason r)
		{
			switch (r)
			{
			case net::RejectReason::BadVersion: return "version de protocolo distinta";
			case net::RejectReason::SceneMismatch: return "el host usa otra escena";
			case net::RejectReason::ServerFull: return "el host esta lleno";
			default: return "peticion invalida";
			}
		}
	}

	NetworkGame::NetworkGame(Fabrica f) : fabrica_(f ? std::move(f) : Fabrica(FabricaENet)) {}
	NetworkGame::~NetworkGame()
	{
		if (arranque_.joinable()) arranque_.join();
		if (servicio_) servicio_->Stop();
	}

	bool NetworkGame::IsEnetAvailable()
	{
		net::ENetOptions o;
		return net::CreateENetTransport(o) != nullptr;   // solo construye; enet_initialize ocurre en Start()
	}

	bool NetworkGame::BeginPlayback(const NetworkConfig& cfg, const std::string& escenaId, ISceneDoors* puertas, const glm::vec3& posObjeto)
	{
		if (cfg.rol == net::Role::Offline) return true;          // OFFLINE: ni servicio, ni hilo, ni ENet
		if (fase_ == NetworkPhase::Arrancando || fase_ == NetworkPhase::IsActive) return false;
		if (arranque_.joinable()) arranque_.join();
		if (servicio_) { servicio_->Stop(); servicio_.reset(); }
		if (escenaId.empty() || escenaId.size() > net::kMaxSceneIdLength) { FailSession("Identificador de escena no valido para la red"); return false; }
		if (cfg.rol == net::Role::Client)
		{
			std::string h = cfg.host, error; uint16_t p = cfg.puerto;
			if (!ParseEndpoint(h + ":" + std::to_string(p), h, p, error)) { FailSession(error); return false; }
		}

		cfg_ = cfg; escena_ = escenaId; puertas_ = puertas; mensaje_.clear();
		cfg_.nombre = cfg.nombre.substr(0, net::kMaxNameLength);
		ResetRemoteState();
		errorPuertas_.clear();
		if (puertas_ && !sincroPuertas_.RebuildScene(*puertas_, errorPuertas_)) std::fprintf(stderr, "Red: %s\n", errorPuertas_.c_str());
		if (cfg_.rol == net::Role::Host)
		{
			posObjetoInicial_ = IsValidCoordinate(posObjeto) ? posObjeto : glm::vec3(0.0f);
			arbitro_ = OwnershipArbiter{};
			arbitro_.DefineAnimation(kObjetoPrueba, posObjetoInicial_);
			vista_.conocido = true; vista_.dueno = kSinDueno; vista_.pos = posVisual_ = posObjetoInicial_;
		}

		net::SessionConfig sc;
		sc.role = cfg_.rol; sc.playerName = cfg_.nombre; sc.sceneId = escena_;
		sc.maxPeers = 4; sc.poseRateHz = 25.0;   // poses y dinámica a 25 Hz en canales independientes
		const NetworkConfig c = cfg_; const Fabrica fab = fabrica_;
		sinTransporte_ = false; arranqueOk_ = false;
		servicio_ = std::make_unique<net::NetService>(sc, [this, c, fab]() -> std::unique_ptr<net::ITransport>
		{
			auto t = fab(c);
			if (!t) sinTransporte_ = true;
			return t;
		});
		fase_ = NetworkPhase::Arrancando; pendienteDetener_ = false; arrancando_ = true;
		// Start() puede resolver DNS / abrir sockets: fuera del hilo de render.
		arranque_ = std::thread([this] { arranqueOk_.store(servicio_->Start()); arrancando_.store(false, std::memory_order_release); });
		return true;
	}

	void NetworkGame::ResetRemoteState()
	{
		interp_.ClearState(); vista_.ResetState(); hayPoseLocal_ = false; ignorados_ = 0;
		recuperacionEnviada_.clear(); estadosRecibidos_.clear(); recuperacionSucia_ = true; ultimaSubida_ = -1e9; ultimoAcarreo_ = -1e9;
		estadosFisicosHost_.clear(); estadosFisicosFiables_.clear(); solicitudesFisicas_.clear(); propietariosDinamicos_.clear();
		estadoConexion_ = net::ConnectionState::Idle;
	}

	void NetworkGame::StopSessionGame()
	{
		if (fase_ == NetworkPhase::Arrancando) { pendienteDetener_ = true; return; }   // se completa en Actualizar, sin bloquear
		if (arranque_.joinable()) arranque_.join();
		if (servicio_) { servicio_->Stop(); servicio_.reset(); }   // los pares ven la desconexion y liberan la propiedad
		ResetRemoteState();
		fase_ = NetworkPhase::Offline; mensaje_.clear(); puertas_ = nullptr;
	}

	void NetworkGame::FailSession(const std::string& msg)
	{
		if (arranque_.joinable()) arranque_.join();
		if (servicio_) { servicio_->Stop(); servicio_.reset(); }
		ResetRemoteState();
		fase_ = NetworkPhase::Error; mensaje_ = msg;
		std::fprintf(stderr, "Red: %s\n", msg.c_str());
	}

	void NetworkGame::ChangedDoors()
	{
		if (!IsOnline()) return;
		errorPuertas_.clear();
		if (puertas_) sincroPuertas_.RebuildScene(*puertas_, errorPuertas_);
		recuperacionSucia_ = true;
	}

	bool NetworkGame::Enqueue(const OutputEvent& e, net::PeerId soloPara)
	{
		return servicio_ && servicio_->QueueEvent(e.tipo, e.objetivo, e.payload, soloPara);
	}

	bool NetworkGame::SetSceneState(const std::vector<SceneObjectState>& estado, bool difundir)
	{
		if (difundir && (cfg_.rol != net::Role::Host || fase_ != NetworkPhase::IsActive || estado.size() != estadoEscena_.size())) return false;
		std::map<uint32_t, SceneObjectState> validados;
		for (const auto& objeto : estado)
			if (!IsValidSceneObjectState(objeto) || !validados.emplace(objeto.id, objeto).second) return false;
		if (difundir)
			for (const auto& objeto : estado)
				if (std::none_of(estadoEscena_.begin(), estadoEscena_.end(), [&](const SceneObjectState& x) { return x.id == objeto.id; })) return false;

		std::vector<SceneObjectState> cambios;
		for (const auto& objeto : estado)
		{
			auto anterior = std::find_if(estadoEscena_.begin(), estadoEscena_.end(), [&](const SceneObjectState& x) { return x.id == objeto.id; });
			if (anterior == estadoEscena_.end() || !SameSceneObjectState(*anterior, objeto)) cambios.push_back(objeto);
		}
		std::vector<net::EventMsg> eventos;
		if (difundir)
		{
			for (const auto& objeto : cambios)
			{
				net::EventMsg ev; ev.eventType = kEvEstadoObjetoEscena; ev.targetId = objeto.id;
				ev.payload = EncodeSceneObjectState(objeto);
				if (ev.payload.empty()) return false;
				eventos.push_back(std::move(ev));
			}
			if (!eventos.empty() && (!servicio_ || !servicio_->QueueEvents(eventos))) { ++ignorados_; return false; }
		}
		estadoEscena_ = estado;
		return true;
	}

	void NetworkGame::TakeSceneStates(std::vector<SceneObjectState>& salida)
	{
		salida.clear();
		salida.swap(estadosRecibidos_);
	}

	void NetworkGame::SendPhysicsStates(const std::vector<net::DynamicStateEntry>& estados)
	{
		if (!servicio_ || cfg_.rol != net::Role::Host) return;
		std::vector<net::DynamicStateEntry> activos;
		for (const auto& e : estados)
		{
			estadosFisicosHost_[e.objectId] = e;
			if ((e.flags & 3) != 0) activos.push_back(e);
		}
		servicio_->SubmitLocalDynamics(activos);
	}

	std::map<uint32_t, net::DynamicStateEntry> NetworkGame::EstadosFisicosRemotos() const
	{
		return servicio_ && cfg_.rol == net::Role::Client ? servicio_->DynamicStates() : std::map<uint32_t, net::DynamicStateEntry>{};
	}

	void NetworkGame::TakeReliablePhysicsStates(std::vector<net::DynamicStateEntry>& salida)
	{
		salida.clear();
		salida.swap(estadosFisicosFiables_);
	}

	bool NetworkGame::PublishFinalPhysicsState(const net::DynamicStateEntry& estado, net::PeerId soloPara)
	{
		if (!IsValidPhysicsState(estado) || cfg_.rol != net::Role::Host || !servicio_) return false;
		OutputEvent evento{ kEvEstadoFisicoFinal, estado.objectId, EncodePhysicsState(estado) };
		if (evento.payload.empty() || !Enqueue(evento, soloPara)) return false;
		estadosFisicosHost_[estado.objectId] = estado;
		return true;
	}

	void NetworkGame::TakePhysicsRequests(std::vector<DynamicObjectRequest>& salida)
	{
		salida.swap(solicitudesFisicas_);
		solicitudesFisicas_.clear();
	}

	bool NetworkGame::RequestDynamicGrab(uint32_t id, const glm::vec3& origen, const glm::vec3& direccion)
	{
		if (cfg_.rol != net::Role::Client || !IsActive() || id == 0) return false;
		auto payload = EncodeInteractionRay(origen, direccion);
		return !payload.empty() && Enqueue({ kEvSolicitarAgarreDinamico, id, std::move(payload) });
	}

	bool NetworkGame::RequestDynamicRelease(uint32_t id, const glm::vec3& velocidad)
	{
		if (cfg_.rol != net::Role::Client || !IsActive() || id == 0 || !IsValidCoordinate(velocidad)) return false;
		OutputEvent evento{ kEvSolicitarSoltarDinamico, id, EncodeVec3(velocidad) };
		return Enqueue(evento);
	}

	void NetworkGame::SetDynamicOwner(uint32_t id, net::PeerId propietario)
	{
		if (id == 0 || (propietario != kSinDueno && propietario > net::kMaxPeerId)) return;
		propietariosDinamicos_[id] = propietario;
		auto estado = estadosFisicosHost_.find(id);
		if (estado != estadosFisicosHost_.end())	{ estado->second.owner = propietario; if (propietario == kSinDueno) estado->second.flags &= ~uint8_t(2); else estado->second.flags |= 2; }
		if (cfg_.rol != net::Role::Host || !servicio_) return;
		std::vector<uint8_t> payload; payload.push_back(static_cast<uint8_t>(propietario & 0xFF));
		payload.push_back(static_cast<uint8_t>(propietario >> 8));
		Enqueue({ kEvPropiedadDinamica, id, std::move(payload) });
	}

	net::PeerId NetworkGame::DynamicOwner(uint32_t id) const
	{
		auto it = propietariosDinamicos_.find(id);
		return it == propietariosDinamicos_.end() ? kSinDueno : it->second;
	}

	uint32_t NetworkGame::DynamicObjectOfPeer(net::PeerId peer) const
	{
		for (const auto& par : propietariosDinamicos_) if (par.second == peer) return par.first;
		return 0;
	}

	bool NetworkGame::PoseOfPeer(net::PeerId peer, net::PoseEntry& salida) const
	{
		if (!servicio_) return false;
		auto poses = servicio_->RemotePoses();
		auto it = poses.find(peer);
		if (it == poses.end()) return false;
		salida = it->second; return true;
	}

	bool NetworkGame::HeadPose(net::PeerId p, const std::map<net::PeerId, net::PoseEntry>& poses, glm::vec3& out) const
	{
		if (p == net::kHostPeerId) { if (!hayPoseLocal_) return false; out = PositionOfPose(poseLocal_.head); return true; }
		const auto it = poses.find(p);
		if (it == poses.end() || !IsValidPose(it->second)) return false;
		out = PositionOfPose(it->second.head); return true;
	}

	void NetworkGame::Advance(double ahora, const net::PoseEntry* pose)
	{
		if (fase_ == NetworkPhase::Offline || fase_ == NetworkPhase::Error) return;
		const double dt = std::clamp(ahora - ultimoAhora_, 0.0, 0.25);
		ultimoAhora_ = ahora;

		if (fase_ == NetworkPhase::Arrancando)
		{
			if (arrancando_.load(std::memory_order_acquire)) return;   // el render sigue sin esperar
			if (arranque_.joinable()) arranque_.join();
			if (pendienteDetener_) { StopSessionGame(); return; }
			if (!arranqueOk_.load())
			{
				FailSession(sinTransporte_.load() ? "ENet no esta incluido en esta compilacion: sin transporte de red. Se sigue offline."
					: (cfg_.rol == net::Role::Host
						? "No se pudo escuchar UDP en todas las interfaces en el puerto " + std::to_string(cfg_.puerto) + ". Comprueba si esta en uso."
						: "No se pudo iniciar la conexion a " + cfg_.host + ":" + std::to_string(cfg_.puerto) + ". Comprueba la direccion, el puerto y que ENet pueda abrir UDP."));
				return;
			}
			fase_ = NetworkPhase::IsActive; recuperacionSucia_ = true;
		}

		// 1) Subida de la pose local (solo se LEE: la red nunca escribe en la camara ni en el jugador).
		if (pose && IsValidPose(*pose))
		{
			poseLocal_ = *pose; hayPoseLocal_ = true;
			if (ahora - ultimaSubida_ >= 1.0 / kHzSubidaPose) { servicio_->SubmitLocalPose(*pose); ultimaSubida_ = ahora; }
		}

		// 2) Mensajes recibidos: se aplican AQUI, en el hilo de juego.
		const auto poses = servicio_->RemotePoses();
		std::vector<net::NetEvent> evs;
		servicio_->PollEvents(evs);
		for (const auto& e : evs) { ProcessRequests(e, ahora, poses); if (fase_ != NetworkPhase::IsActive) return; }

		if (cfg_.rol == net::Role::Client)
		{
			const net::ConnectionState st = servicio_->State();
			if (st == net::ConnectionState::Disconnected)
			{
				FailSession("Sin respuesta de " + cfg_.host + ":" + std::to_string(cfg_.puerto) +
					". Comprueba que el host esta activo, que ambos equipos tienen acceso en la tailnet y que el firewall local permite UDP en el puerto indicado.");
				return;
			}
			if (st == net::ConnectionState::Rejected)
			{
				// NetService publica el estado y el evento por caminos separados; espera un frame si el motivo aun no llego.
				if (mensaje_.empty()) return;
				FailSession("El host rechazo la conexion: " + mensaje_);
				return;
			}
		}
		else BroadcastFromHost(ahora);

		interp_.Advance(poses, servicio_->LocalPeer(), ahora);
		const float k = 1.0f - std::exp(-15.0f * static_cast<float>(dt));
		posVisual_ = vista_.conocido ? glm::mix(posVisual_, vista_.pos, k) : posVisual_;
		if (vista_.conocido && glm::distance(posVisual_, vista_.pos) > 3.0f) posVisual_ = vista_.pos;
	}

	void NetworkGame::ProcessRequests(const net::NetEvent& e, double ahora, const std::map<net::PeerId, net::PoseEntry>& poses)
	{
		switch (e.kind)
		{
		case net::NetEvent::PeerJoined:
			if (cfg_.rol == net::Role::Host)
			{
				std::vector<net::EventMsg> eventos;
				for (const auto& objeto : estadoEscena_)
				{
					net::EventMsg ev; ev.eventType = kEvEstadoObjetoEscena; ev.targetId = objeto.id;
					ev.payload = EncodeSceneObjectState(objeto);
					if (ev.payload.empty()) { ++ignorados_; continue; }
					eventos.push_back(std::move(ev));
				}
				if (!eventos.empty() && (!servicio_ || !servicio_->QueueEvents(eventos, e.peer))) ++ignorados_;
				std::vector<net::EventMsg> fisicos;
				for (auto par : estadosFisicosHost_)
				{
					net::EventMsg ev; ev.eventType = kEvEstadoFisicoFinal; ev.targetId = par.first;
					ev.payload = EncodePhysicsState(par.second);
					if (!ev.payload.empty()) fisicos.push_back(std::move(ev));
				}
				if (!fisicos.empty() && (!servicio_ || !servicio_->QueueEvents(fisicos, e.peer))) ++ignorados_;
			}
			break;
		case net::NetEvent::PeerLeft:
			interp_.RemoveEntry(e.peer);
			if (cfg_.rol == net::Role::Host)
			{
				arbitro_.OnPeerLeft(e.peer);
				for (const auto& par : propietariosDinamicos_)
					if (par.second == e.peer)
					{
						DynamicObjectRequest solicitud{ par.first, e.peer, false, glm::vec3(0.0f) };
						solicitud.desconexionPeer = true;
						solicitudesFisicas_.push_back(solicitud);
					}
			}
			break;
		case net::NetEvent::StateChanged: estadoConexion_ = e.state; break;
		case net::NetEvent::Rejected: mensaje_ = TextoRechazo(e.rejectReason); break;
		case net::NetEvent::RecoveryReceived:
			if (cfg_.rol != net::Role::Client) { ++ignorados_; break; }
			if (puertas_) sincroPuertas_.ApplyRecovery(*puertas_, e.recovery);
			vista_.ApplyRecovery(e.recovery);
			posVisual_ = vista_.pos;
			break;
		case net::NetEvent::GameEvent:
			if (cfg_.rol == net::Role::Host) ProcessHost(e, ahora, poses); else ProcessClient(e);
			break;
		}
	}

	void NetworkGame::ProcessHost(const net::NetEvent& e, double ahora, const std::map<net::PeerId, net::PoseEntry>& poses)
	{
		const net::EventMsg& ev = e.event;
		glm::vec3 cabeza; const bool hay = HeadPose(e.peer, poses, cabeza);
		RequestResult r = RequestResult::Invalida;
		switch (ev.eventType)
		{
		case kEvPuertaSolicitud:
			if (ev.payload.empty() && puertas_) r = sincroPuertas_.HostHandleRequest(*puertas_, ev.targetId, hay ? &cabeza : nullptr);
			break;
		case kEvPropSolicitar:
			if (ev.payload.empty()) r = arbitro_.RequestToggle(ev.targetId, e.peer, hay ? &cabeza : nullptr, ahora);
			break;
		case kEvPropLiberar:
			if (ev.payload.empty()) r = arbitro_.ReleaseGpu(ev.targetId, e.peer);
			break;
		case kEvPropMover:
		{
			glm::vec3 p;
			if (DecodeVec3(ev.payload, p)) r = arbitro_.MoveView(ev.targetId, e.peer, p, hay ? &cabeza : nullptr, ahora);
			break;
		}
		case kEvSolicitarAgarreDinamico:
		{
			glm::vec3 origen, direccion;
			if (ev.targetId != 0 && DecodeInteractionRay(ev.payload, origen, direccion))
				solicitudesFisicas_.push_back({ ev.targetId, e.peer, true, glm::vec3(0.0f), origen, glm::normalize(direccion) });
			else ++ignorados_;
			break;
		}
		case kEvSolicitarSoltarDinamico:
		{
			glm::vec3 velocidad;
			if (DecodeVec3(ev.payload, velocidad) && ClampHostThrowSpeed(velocidad))
				solicitudesFisicas_.push_back({ ev.targetId, e.peer, false, velocidad });
			else ++ignorados_;
			break;
		}
		default: break;   // los clientes no dictan estados
		}
		if (r != RequestResult::Aplicada) ++ignorados_;
	}

	void NetworkGame::ProcessClient(const net::NetEvent& e)
	{
		const net::EventMsg& ev = e.event;
		bool ok = false;
		if (ev.eventType == kEvPuertaEstado) ok = puertas_ && sincroPuertas_.ApplyState(*puertas_, ev.targetId, ev.payload);
		else if (ev.eventType == kEvPropEstado && ev.targetId == kObjetoPrueba) ok = vista_.ApplyState(ev.payload);
		else if (ev.eventType == kEvEstadoObjetoEscena)
		{
			SceneObjectState estado;
			ok = DecodeSceneObjectState(ev.targetId, ev.payload, estado);
			if (ok) estadosRecibidos_.push_back(estado);
		}
		else if (ev.eventType == kEvPropiedadDinamica && ev.payload.size() == 2)
		{
			const auto propietario = static_cast<net::PeerId>(ev.payload[0] | (uint16_t(ev.payload[1]) << 8));
			if (propietario == kSinDueno || propietario <= net::kMaxPeerId)
			{ propietariosDinamicos_[ev.targetId] = propietario; ok = true; }
		}
		else if (ev.eventType == kEvEstadoFisicoFinal)
		{
			net::DynamicStateEntry estado;
			ok = DecodePhysicsState(ev.targetId, ev.payload, estado);
			if (ok)
			{
				estadosFisicosFiables_.push_back(estado);
				propietariosDinamicos_[estado.objectId] = estado.owner;
			}
		}
		if (!ok) ++ignorados_;
	}

	void NetworkGame::BroadcastFromHost(double ahora)
	{
		std::vector<OutputEvent> salida;
		if (puertas_) sincroPuertas_.HostDetectChanges(*puertas_, salida);
		arbitro_.Tick(ahora);
		for (uint32_t id : arbitro_.TakeChanges())
			if (const SharedObject* o = arbitro_.Buscar(id))
			{
				if (id == kObjetoPrueba) { vista_.conocido = true; vista_.dueno = o->dueno; vista_.pos = o->pos; }
				salida.push_back(ObjectStateEvent(*o));
			}
		if (!salida.empty()) recuperacionSucia_ = true;
		for (const auto& ev : salida) Enqueue(ev);

		if (recuperacionSucia_)
		{
			std::vector<net::StateEntry> r;
			arbitro_.RecoveryState(r);   // primero: el limite de 64 entradas nunca debe recortarlo
			if (puertas_) sincroPuertas_.RecoveryState(*puertas_, r);
			if (r.size() > net::kMaxRecoveryEntries) r.resize(net::kMaxRecoveryEntries);
			bool igual = r.size() == recuperacionEnviada_.size();
			for (size_t i = 0; igual && i < r.size(); ++i) igual = r[i].objectId == recuperacionEnviada_[i].objectId && r[i].value == recuperacionEnviada_[i].value;
			if (!igual) { servicio_->SetRecoveryState(r); recuperacionEnviada_ = r; }
			recuperacionSucia_ = false;
		}
	}

	bool NetworkGame::InterceptDoor(size_t idx)
	{
		if (!IsClient() || fase_ == NetworkPhase::Error) return false;   // offline o host: la alterna el llamante
		if (fase_ == NetworkPhase::IsActive && sincroPuertas_.IsOnline(idx)) Enqueue(DoorSync::PendingRequest(sincroPuertas_.NetworkId(idx)));
		return true;   // el cliente jamas cambia la puerta por su cuenta: espera el estado del host
	}

	glm::vec3 NetworkGame::ObjectVisualPosition() const { return OwnsObject() ? destinoAcarreo_ : posVisual_; }

	bool NetworkGame::ToggleAnimatedObject(const glm::vec3& origen, const glm::vec3& dir, double ahora)
	{
		if (fase_ != NetworkPhase::IsActive || !vista_.conocido) return false;
		const bool host = cfg_.rol == net::Role::Host;
		glm::vec3 cabeza(0.0f); const bool hay = hayPoseLocal_;
		if (hay) cabeza = PositionOfPose(poseLocal_.head);
		if (OwnsObject())
		{
			if (host) arbitro_.ReleaseGpu(kObjetoPrueba, net::kHostPeerId);
			else { OutputEvent e; e.tipo = kEvPropLiberar; e.objetivo = kObjetoPrueba; Enqueue(e); }
			return true;
		}
		if (!RayHitsSphere(origen, dir, vista_.pos, 0.35f, OwnershipArbiter::kAlcance - 0.5f)) return false;
		if (host) arbitro_.RequestToggle(kObjetoPrueba, net::kHostPeerId, hay ? &cabeza : nullptr, ahora);
		else { OutputEvent e; e.tipo = kEvPropSolicitar; e.objetivo = kObjetoPrueba; Enqueue(e); }
		return true;
	}

	void NetworkGame::UpdateCarry(const glm::vec3& origen, const glm::vec3& dir, double ahora)
	{
		if (fase_ != NetworkPhase::IsActive || !OwnsObject() || !(glm::length(dir) > 1e-6f)) return;
		const glm::vec3 destino = origen + glm::normalize(dir) * 1.5f;
		if (!IsValidCoordinate(destino)) return;
		destinoAcarreo_ = destino;
		const double desde = ahora - ultimoAcarreo_;
		if (desde < 0.05 || (glm::distance(destino, ultimoPosAcarreo_) < 0.01f && desde < 3.0)) return;   // 20 Hz max + latido cada 3 s
		ultimoAcarreo_ = ahora; ultimoPosAcarreo_ = destino;
		if (cfg_.rol == net::Role::Host)
		{
			const glm::vec3 cabeza = hayPoseLocal_ ? PositionOfPose(poseLocal_.head) : origen;
			arbitro_.MoveView(kObjetoPrueba, net::kHostPeerId, destino, &cabeza, ahora);
		}
		else { OutputEvent e; e.tipo = kEvPropMover; e.objetivo = kObjetoPrueba; e.payload = EncodeVec3(destino); Enqueue(e); }
	}

	std::vector<net::PoseEntry> NetworkGame::RemotePlayersToDraw(double ahora) const
	{
		std::vector<net::PoseEntry> v;
		if (fase_ != NetworkPhase::IsActive) return v;
		for (net::PeerId p : interp_.Peers())
		{
			net::PoseEntry pe;
			if (p != LocalPeerId() && interp_.SampleAt(p, ahora, pe)) v.push_back(pe);
		}
		return v;
	}

	Diagnostico NetworkGame::GetDiagnosticInfo() const
	{
		Diagnostico d; d.fase = fase_; d.escena = escena_; d.mensaje = mensaje_;
		d.modo = cfg_.rol == net::Role::Host ? "Host" : (cfg_.rol == net::Role::Client ? "Cliente" : "Offline");
		d.endpoint = cfg_.rol == net::Role::Host ? "0.0.0.0" : cfg_.host;
		d.puerto = cfg_.puerto;
		if (fase_ == NetworkPhase::Offline) { d.modo = "Offline"; d.estado = "Sin red"; return d; }
		switch (fase_)
		{
		case NetworkPhase::Arrancando: d.estado = "Iniciando..."; break;
		case NetworkPhase::Error: d.estado = "Error (offline)"; break;
		default:
			switch (servicio_ ? servicio_->State() : net::ConnectionState::Idle)
			{
			case net::ConnectionState::Connecting: d.estado = "Conectando..."; break;
			case net::ConnectionState::Connected: d.estado = cfg_.rol == net::Role::Host ? "Escuchando" : "Conectado"; break;
			case net::ConnectionState::Disconnected: d.estado = "Desconectado"; break;
			case net::ConnectionState::Rejected: d.estado = "Rechazado"; break;
			default: d.estado = "Inactivo"; break;
			}
		}
		if (servicio_) { d.peerLocal = servicio_->LocalPeer(); d.eventosDescartados = servicio_->DroppedEvents(); }
		d.remotos = interp_.Count(); d.eventosIgnorados = ignorados_; d.puertasEnRed = sincroPuertas_.Count();
		return d;
	}
}
