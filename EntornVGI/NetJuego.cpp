#include "NetJuego.h"
#include "Network/ENetTransport.h"

#include <cstdio>

namespace netjuego
{
	namespace
	{
		std::unique_ptr<net::ITransport> FabricaENet(const ConfigRed& c)
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

	RedJuego::RedJuego(Fabrica f) : fabrica_(f ? std::move(f) : Fabrica(FabricaENet)) {}
	RedJuego::~RedJuego()
	{
		if (arranque_.joinable()) arranque_.join();
		if (servicio_) servicio_->Stop();
	}

	bool RedJuego::EnetDisponible()
	{
		net::ENetOptions o;
		return net::CreateENetTransport(o) != nullptr;   // solo construye; enet_initialize ocurre en Start()
	}

	bool RedJuego::Iniciar(const ConfigRed& cfg, const std::string& escenaId, IPuertasEscena* puertas, const glm::vec3& posObjeto)
	{
		if (cfg.rol == net::Role::Offline) return true;          // OFFLINE: ni servicio, ni hilo, ni ENet
		if (fase_ == FaseRed::Arrancando || fase_ == FaseRed::Activa) return false;
		if (arranque_.joinable()) arranque_.join();
		if (servicio_) { servicio_->Stop(); servicio_.reset(); }
		if (escenaId.empty() || escenaId.size() > net::kMaxSceneIdLength) { Fallar("Identificador de escena no valido para la red"); return false; }
		if (cfg.rol == net::Role::Client)
		{
			std::string h = cfg.host, error; uint16_t p = cfg.puerto;
			if (!ParsearEndpoint(h + ":" + std::to_string(p), h, p, error)) { Fallar(error); return false; }
		}

		cfg_ = cfg; escena_ = escenaId; puertas_ = puertas; mensaje_.clear();
		cfg_.nombre = cfg.nombre.substr(0, net::kMaxNameLength);
		ReiniciarEstadoRemoto();
		errorPuertas_.clear();
		if (puertas_ && !sincroPuertas_.Reconstruir(*puertas_, errorPuertas_)) std::fprintf(stderr, "Red: %s\n", errorPuertas_.c_str());
		if (cfg_.rol == net::Role::Host)
		{
			posObjetoInicial_ = CoordenadaValida(posObjeto) ? posObjeto : glm::vec3(0.0f);
			arbitro_ = ArbitroPropiedad{};
			arbitro_.Definir(kObjetoPrueba, posObjetoInicial_);
			vista_.conocido = true; vista_.dueno = kSinDueno; vista_.pos = posVisual_ = posObjetoInicial_;
		}

		net::SessionConfig sc;
		sc.role = cfg_.rol; sc.playerName = cfg_.nombre; sc.sceneId = escena_;
		sc.maxPeers = 4; sc.poseRateHz = 20.0;   // snapshots del host a 20 Hz
		const ConfigRed c = cfg_; const Fabrica fab = fabrica_;
		sinTransporte_ = false; arranqueOk_ = false;
		servicio_ = std::make_unique<net::NetService>(sc, [this, c, fab]() -> std::unique_ptr<net::ITransport>
		{
			auto t = fab(c);
			if (!t) sinTransporte_ = true;
			return t;
		});
		fase_ = FaseRed::Arrancando; pendienteDetener_ = false; arrancando_ = true;
		// Start() puede resolver DNS / abrir sockets: fuera del hilo de render.
		arranque_ = std::thread([this] { arranqueOk_.store(servicio_->Start()); arrancando_.store(false, std::memory_order_release); });
		return true;
	}

	void RedJuego::ReiniciarEstadoRemoto()
	{
		interp_.Limpiar(); vista_.Reiniciar(); hayPoseLocal_ = false; ignorados_ = 0;
		recuperacionEnviada_.clear(); recuperacionSucia_ = true; ultimaSubida_ = -1e9; ultimoAcarreo_ = -1e9;
		estadoConexion_ = net::ConnectionState::Idle;
	}

	void RedJuego::Detener()
	{
		if (fase_ == FaseRed::Arrancando) { pendienteDetener_ = true; return; }   // se completa en Actualizar, sin bloquear
		if (arranque_.joinable()) arranque_.join();
		if (servicio_) { servicio_->Stop(); servicio_.reset(); }   // los pares ven la desconexion y liberan la propiedad
		ReiniciarEstadoRemoto();
		fase_ = FaseRed::Offline; mensaje_.clear(); puertas_ = nullptr;
	}

	void RedJuego::Fallar(const std::string& msg)
	{
		if (arranque_.joinable()) arranque_.join();
		if (servicio_) { servicio_->Stop(); servicio_.reset(); }
		interp_.Limpiar(); vista_.Reiniciar();
		fase_ = FaseRed::Error; mensaje_ = msg;
		std::fprintf(stderr, "Red: %s\n", msg.c_str());
	}

	void RedJuego::PuertasCambiadas()
	{
		if (!EnRed()) return;
		errorPuertas_.clear();
		if (puertas_) sincroPuertas_.Reconstruir(*puertas_, errorPuertas_);
		recuperacionSucia_ = true;
	}

	bool RedJuego::Encolar(const EventoSalida& e)
	{
		return servicio_ && servicio_->QueueEvent(e.tipo, e.objetivo, e.payload);
	}

	bool RedJuego::Cabeza(net::PeerId p, const std::map<net::PeerId, net::PoseEntry>& poses, glm::vec3& out) const
	{
		if (p == net::kHostPeerId) { if (!hayPoseLocal_) return false; out = PosDe(poseLocal_.head); return true; }
		const auto it = poses.find(p);
		if (it == poses.end() || !PoseValida(it->second)) return false;
		out = PosDe(it->second.head); return true;
	}

	void RedJuego::Actualizar(double ahora, const net::PoseEntry* pose)
	{
		if (fase_ == FaseRed::Offline || fase_ == FaseRed::Error) return;
		const double dt = std::clamp(ahora - ultimoAhora_, 0.0, 0.25);
		ultimoAhora_ = ahora;

		if (fase_ == FaseRed::Arrancando)
		{
			if (arrancando_.load(std::memory_order_acquire)) return;   // el render sigue sin esperar
			if (arranque_.joinable()) arranque_.join();
			if (pendienteDetener_) { Detener(); return; }
			if (!arranqueOk_.load())
			{
				Fallar(sinTransporte_.load() ? "ENet no esta incluido en esta compilacion: sin transporte de red. Se sigue offline."
					: (cfg_.rol == net::Role::Host ? "No se pudo abrir el puerto del host (¿en uso?)." : "No se pudo iniciar la conexion con el host."));
				return;
			}
			fase_ = FaseRed::Activa; recuperacionSucia_ = true;
		}

		// 1) Subida de la pose local (solo se LEE: la red nunca escribe en la camara ni en el jugador).
		if (pose && PoseValida(*pose))
		{
			poseLocal_ = *pose; hayPoseLocal_ = true;
			if (ahora - ultimaSubida_ >= 1.0 / kHzSubidaPose) { servicio_->SubmitLocalPose(*pose); ultimaSubida_ = ahora; }
		}

		// 2) Mensajes recibidos: se aplican AQUI, en el hilo de juego.
		const auto poses = servicio_->RemotePoses();
		std::vector<net::NetEvent> evs;
		servicio_->PollEvents(evs);
		for (const auto& e : evs) { Procesar(e, ahora, poses); if (fase_ != FaseRed::Activa) return; }

		if (cfg_.rol == net::Role::Client)
		{
			const net::ConnectionState st = servicio_->State();
			if (st == net::ConnectionState::Disconnected) { Fallar("Conexion con el host perdida."); return; }
			if (st == net::ConnectionState::Rejected) { Fallar("El host rechazo la conexion: " + (mensaje_.empty() ? std::string("sin motivo") : mensaje_)); return; }
		}
		else DifundirHost(ahora);

		interp_.Actualizar(poses, servicio_->LocalPeer(), ahora);
		const float k = 1.0f - std::exp(-15.0f * static_cast<float>(dt));
		posVisual_ = vista_.conocido ? glm::mix(posVisual_, vista_.pos, k) : posVisual_;
		if (vista_.conocido && glm::distance(posVisual_, vista_.pos) > 3.0f) posVisual_ = vista_.pos;
	}

	void RedJuego::Procesar(const net::NetEvent& e, double ahora, const std::map<net::PeerId, net::PoseEntry>& poses)
	{
		switch (e.kind)
		{
		case net::NetEvent::PeerJoined: break;   // el cliente pide la recuperacion al recibir el Welcome
		case net::NetEvent::PeerLeft:
			interp_.Quitar(e.peer);
			if (cfg_.rol == net::Role::Host) arbitro_.PeerDesconectado(e.peer);   // libera la propiedad (se difunde en DifundirHost)
			break;
		case net::NetEvent::StateChanged: estadoConexion_ = e.state; break;
		case net::NetEvent::Rejected: mensaje_ = TextoRechazo(e.rejectReason); break;
		case net::NetEvent::RecoveryReceived:
			if (cfg_.rol != net::Role::Client) { ++ignorados_; break; }
			if (puertas_) sincroPuertas_.AplicarRecuperacion(*puertas_, e.recovery);
			vista_.AplicarRecuperacion(e.recovery);
			posVisual_ = vista_.pos;
			break;
		case net::NetEvent::GameEvent:
			if (cfg_.rol == net::Role::Host) ProcesarHost(e, ahora, poses); else ProcesarCliente(e);
			break;
		}
	}

	void RedJuego::ProcesarHost(const net::NetEvent& e, double ahora, const std::map<net::PeerId, net::PoseEntry>& poses)
	{
		const net::EventMsg& ev = e.event;
		glm::vec3 cabeza; const bool hay = Cabeza(e.peer, poses, cabeza);
		ResultadoSolicitud r = ResultadoSolicitud::Invalida;
		switch (ev.eventType)
		{
		case kEvPuertaSolicitud:
			if (ev.payload.empty() && puertas_) r = sincroPuertas_.HostSolicitud(*puertas_, ev.targetId, hay ? &cabeza : nullptr);
			break;
		case kEvPropSolicitar:
			if (ev.payload.empty()) r = arbitro_.Solicitar(ev.targetId, e.peer, hay ? &cabeza : nullptr, ahora);
			break;
		case kEvPropLiberar:
			if (ev.payload.empty()) r = arbitro_.Liberar(ev.targetId, e.peer);
			break;
		case kEvPropMover:
		{
			glm::vec3 p;
			if (DecodificarVec3(ev.payload, p)) r = arbitro_.Mover(ev.targetId, e.peer, p, hay ? &cabeza : nullptr, ahora);
			break;
		}
		default: break;   // los clientes no dictan estados
		}
		if (r != ResultadoSolicitud::Aplicada) ++ignorados_;
	}

	void RedJuego::ProcesarCliente(const net::NetEvent& e)
	{
		const net::EventMsg& ev = e.event;
		bool ok = false;
		if (ev.eventType == kEvPuertaEstado) ok = puertas_ && sincroPuertas_.AplicarEstado(*puertas_, ev.targetId, ev.payload);
		else if (ev.eventType == kEvPropEstado && ev.targetId == kObjetoPrueba) ok = vista_.AplicarEstado(ev.payload);
		if (!ok) ++ignorados_;
	}

	void RedJuego::DifundirHost(double ahora)
	{
		std::vector<EventoSalida> salida;
		if (puertas_) sincroPuertas_.HostDetectarCambios(*puertas_, salida);
		arbitro_.Tick(ahora);
		for (uint32_t id : arbitro_.TomarCambios())
			if (const ObjetoCompartido* o = arbitro_.Buscar(id))
			{
				if (id == kObjetoPrueba) { vista_.conocido = true; vista_.dueno = o->dueno; vista_.pos = o->pos; }
				salida.push_back(EventoEstadoObjeto(*o));
			}
		if (!salida.empty()) recuperacionSucia_ = true;
		for (const auto& ev : salida) Encolar(ev);

		if (recuperacionSucia_)
		{
			std::vector<net::StateEntry> r;
			arbitro_.EstadoRecuperacion(r);   // primero: el limite de 64 entradas nunca debe recortarlo
			if (puertas_) sincroPuertas_.EstadoRecuperacion(*puertas_, r);
			if (r.size() > net::kMaxRecoveryEntries) r.resize(net::kMaxRecoveryEntries);
			bool igual = r.size() == recuperacionEnviada_.size();
			for (size_t i = 0; igual && i < r.size(); ++i) igual = r[i].objectId == recuperacionEnviada_[i].objectId && r[i].value == recuperacionEnviada_[i].value;
			if (!igual) { servicio_->SetRecoveryState(r); recuperacionEnviada_ = r; }
			recuperacionSucia_ = false;
		}
	}

	bool RedJuego::InterceptarPuerta(size_t idx)
	{
		if (!EsCliente() || fase_ == FaseRed::Error) return false;   // offline o host: la alterna el llamante
		if (fase_ == FaseRed::Activa && sincroPuertas_.EnRed(idx)) Encolar(SincroPuertas::Solicitud(sincroPuertas_.IdRed(idx)));
		return true;   // el cliente jamas cambia la puerta por su cuenta: espera el estado del host
	}

	glm::vec3 RedJuego::PosicionVisualObjeto() const { return SoyDuenoDelObjeto() ? destinoAcarreo_ : posVisual_; }

	bool RedJuego::AlternarObjeto(const glm::vec3& origen, const glm::vec3& dir, double ahora)
	{
		if (fase_ != FaseRed::Activa || !vista_.conocido) return false;
		const bool host = cfg_.rol == net::Role::Host;
		glm::vec3 cabeza(0.0f); const bool hay = hayPoseLocal_;
		if (hay) cabeza = PosDe(poseLocal_.head);
		if (SoyDuenoDelObjeto())
		{
			if (host) arbitro_.Liberar(kObjetoPrueba, net::kHostPeerId);
			else { EventoSalida e; e.tipo = kEvPropLiberar; e.objetivo = kObjetoPrueba; Encolar(e); }
			return true;
		}
		if (!RayoTocaEsfera(origen, dir, vista_.pos, 0.35f, ArbitroPropiedad::kAlcance - 0.5f)) return false;
		if (host) arbitro_.Solicitar(kObjetoPrueba, net::kHostPeerId, hay ? &cabeza : nullptr, ahora);
		else { EventoSalida e; e.tipo = kEvPropSolicitar; e.objetivo = kObjetoPrueba; Encolar(e); }
		return true;
	}

	void RedJuego::ActualizarAcarreo(const glm::vec3& origen, const glm::vec3& dir, double ahora)
	{
		if (fase_ != FaseRed::Activa || !SoyDuenoDelObjeto() || !(glm::length(dir) > 1e-6f)) return;
		const glm::vec3 destino = origen + glm::normalize(dir) * 1.5f;
		if (!CoordenadaValida(destino)) return;
		destinoAcarreo_ = destino;
		const double desde = ahora - ultimoAcarreo_;
		if (desde < 0.05 || (glm::distance(destino, ultimoPosAcarreo_) < 0.01f && desde < 3.0)) return;   // 20 Hz max + latido cada 3 s
		ultimoAcarreo_ = ahora; ultimoPosAcarreo_ = destino;
		if (cfg_.rol == net::Role::Host)
		{
			const glm::vec3 cabeza = hayPoseLocal_ ? PosDe(poseLocal_.head) : origen;
			arbitro_.Mover(kObjetoPrueba, net::kHostPeerId, destino, &cabeza, ahora);
		}
		else { EventoSalida e; e.tipo = kEvPropMover; e.objetivo = kObjetoPrueba; e.payload = CodificarVec3(destino); Encolar(e); }
	}

	std::vector<net::PoseEntry> RedJuego::RemotosParaDibujar(double ahora) const
	{
		std::vector<net::PoseEntry> v;
		if (fase_ != FaseRed::Activa) return v;
		for (net::PeerId p : interp_.Peers())
		{
			net::PoseEntry pe;
			if (p != PeerLocal() && interp_.Muestrear(p, ahora, pe)) v.push_back(pe);
		}
		return v;
	}

	Diagnostico RedJuego::ObtenerDiagnostico() const
	{
		Diagnostico d; d.fase = fase_; d.escena = escena_; d.mensaje = mensaje_;
		d.modo = cfg_.rol == net::Role::Host ? "Host" : (cfg_.rol == net::Role::Client ? "Cliente" : "Offline");
		if (fase_ == FaseRed::Offline) { d.modo = "Offline"; d.estado = "Sin red"; return d; }
		switch (fase_)
		{
		case FaseRed::Arrancando: d.estado = "Iniciando..."; break;
		case FaseRed::Error: d.estado = "Error (offline)"; break;
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
		d.remotos = interp_.Cantidad(); d.eventosIgnorados = ignorados_; d.puertasEnRed = sincroPuertas_.Cantidad();
		return d;
	}
}
