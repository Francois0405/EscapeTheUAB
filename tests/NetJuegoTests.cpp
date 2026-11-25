// NetJuegoTests.cpp
// Adaptador de red del juego (EntornVGI/NetJuego*.h): configuracion, poses PC/OpenXR (Z arriba), interpolacion,
// puertas con el host como autoridad, propiedad temporal del objeto compartido, offline sin ENet ni hilos.
// Usa un transporte en memoria: sin sockets, sin ENet, sin OpenGL, sin Bullet, sin visor.
#include "src/networking/replication/NetGame.h"
#include "src/gameplay/interaction/ThrowCharge.h"
#include "src/physics/PhysicsWorld.h"
#include "src/networking/replication/Game/RemotePhysicsTimeline.h"
#include "src/networking/replication/Game/HostPhysicsPublication.h"
#include "src/networking/replication/Game/SceneNetworkState.h"
#include "src/networking/session/SessionRules.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <thread>

using namespace netgame;

namespace
{
	int fallos = 0;
	void ok(bool c, const char* caso) { if (!c) { std::cerr << "FALLO: " << caso << '\n'; ++fallos; } }
	bool cerca(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }
	bool cerca(const glm::vec3& a, const glm::vec3& b, float eps = 1e-3f) { return glm::distance(a, b) <= eps; }

	using namespace net;
	// ---- Transporte en memoria (thread-safe) ----
	struct Inbox { std::mutex m; std::deque<TransportEvent> q; };
	struct Hub
	{
		Inbox host; std::map<LinkId, std::shared_ptr<Inbox>> clients;
		std::mutex m; LinkId next = 1;
	};
	void Push(Inbox& i, TransportEvent e) { std::lock_guard<std::mutex> l(i.m); i.q.push_back(std::move(e)); }
	void Drain(Inbox& i, std::vector<TransportEvent>& out, int timeoutMs)
	{
		for (int t = 0; t <= timeoutMs; ++t)
		{
			{ std::lock_guard<std::mutex> l(i.m); if (!i.q.empty()) { for (auto& e : i.q) out.push_back(std::move(e)); i.q.clear(); return; } }
			if (timeoutMs == 0) return;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
	class HostEnd : public ITransport
	{
	public:
		explicit HostEnd(std::shared_ptr<Hub> h) : hub(h) {}
		bool Start() override { return true; }
		void Stop() override {}
		void Poll(std::vector<TransportEvent>& o, int t) override { Drain(hub->host, o, t); }
		void Send(LinkId l, uint8_t ch, bool, const std::vector<uint8_t>& d) override
		{
			std::shared_ptr<Inbox> c; { std::lock_guard<std::mutex> g(hub->m); auto it = hub->clients.find(l); if (it == hub->clients.end()) return; c = it->second; }
			TransportEvent e; e.kind = TransportEvent::Received; e.link = l; e.channel = ch; e.data = d; Push(*c, std::move(e));
		}
		void Broadcast(uint8_t ch, bool r, const std::vector<uint8_t>& d) override { std::vector<LinkId> ids; { std::lock_guard<std::mutex> g(hub->m); for (auto& kv : hub->clients) ids.push_back(kv.first); } for (auto id : ids) Send(id, ch, r, d); }
		void Disconnect(LinkId l) override
		{
			std::shared_ptr<Inbox> c; { std::lock_guard<std::mutex> g(hub->m); auto it = hub->clients.find(l); if (it == hub->clients.end()) return; c = it->second; hub->clients.erase(it); }
			TransportEvent e; e.kind = TransportEvent::Disconnected; e.link = l; Push(*c, e);
		}
		std::shared_ptr<Hub> hub;
	};
	class ClientEnd : public ITransport
	{
	public:
		explicit ClientEnd(std::shared_ptr<Hub> h) : hub(h), inbox(std::make_shared<Inbox>()) {}
		bool Start() override
		{
			{ std::lock_guard<std::mutex> g(hub->m); link = hub->next++; hub->clients[link] = inbox; }
			TransportEvent e; e.kind = TransportEvent::Connected; e.link = link;
			Push(*inbox, e); Push(hub->host, e);
			return true;
		}
		void Stop() override { Disconnect(link); }
		void Poll(std::vector<TransportEvent>& o, int t) override { Drain(*inbox, o, t); }
		void Send(LinkId, uint8_t ch, bool, const std::vector<uint8_t>& d) override
		{ TransportEvent e; e.kind = TransportEvent::Received; e.link = link; e.channel = ch; e.data = d; Push(hub->host, std::move(e)); }
		void Broadcast(uint8_t ch, bool r, const std::vector<uint8_t>& d) override { Send(link, ch, r, d); }
		void Disconnect(LinkId) override
		{
			bool had; { std::lock_guard<std::mutex> g(hub->m); had = hub->clients.erase(link) > 0; }
			if (had) { TransportEvent e; e.kind = TransportEvent::Disconnected; e.link = link; Push(hub->host, e); Push(*inbox, e); }
		}
		std::shared_ptr<Hub> hub; std::shared_ptr<Inbox> inbox; LinkId link = 0;
	};

	// Como ENet: al parar, el host desconecta a todos sus clientes.
	class HostEndRealista : public HostEnd
	{
	public:
		using HostEnd::HostEnd;
		void Stop() override
		{
			std::vector<LinkId> ids; { std::lock_guard<std::mutex> g(hub->m); for (auto& kv : hub->clients) ids.push_back(kv.first); }
			for (auto id : ids) Disconnect(id);
		}
	};
	// ---- puertas simuladas ----
	struct PuertaFalsa { std::string id; bool abierta = false; glm::vec3 punto{ 0 }; };
	class PuertasFalsas : public ISceneDoors
	{
	public:
		std::vector<PuertaFalsa> v;
		size_t Cantidad() const override { return v.size(); }
		std::string Id(size_t i) const override { return v[i].id; }
		bool Abierta(size_t i) const override { return v[i].abierta; }
		void FijarAbierta(size_t i, bool a) override { v[i].abierta = a; }
		glm::vec3 Punto(size_t i) const override { return v[i].punto; }
	};
	std::unique_ptr<PuertasFalsas> NuevasPuertas()
	{
		auto p = std::make_unique<PuertasFalsas>();
		p->v.push_back({ "puerta_entrada", false, glm::vec3(0, 2, 1) });
		p->v.push_back({ "puerta_lab", false, glm::vec3(30, 2, 1) });
		return p;
	}

	PoseEntry PoseEn(const glm::vec3& cabeza, const glm::vec3& adelante = glm::vec3(0, 1, 0))
	{
		PoseEntry p; PoseDesdeEscritorio(cabeza, adelante, p); return p;
	}

	double Reloj() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

	struct Nodo
	{
		std::unique_ptr<NetworkGame> red; std::unique_ptr<PuertasFalsas> puertas; PoseEntry pose;
		void Paso() { red->Actualizar(Reloj(), &pose); }
	};
	template <class F> bool Esperar(std::vector<Nodo*> nodos, F cond, double seg = 5.0)
	{
		const double fin = Reloj() + seg;
		while (Reloj() < fin)
		{
			for (auto* n : nodos) n->Paso();
			if (cond()) return true;
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		return false;
	}

	std::unique_ptr<Nodo> Crear(std::shared_ptr<Hub> hub, Role rol, const std::string& escena, const glm::vec3& cabeza, int* llamadasFabrica = nullptr)
	{
		auto n = std::make_unique<Nodo>();
		n->puertas = NuevasPuertas(); n->pose = PoseEn(cabeza);
		n->red = std::make_unique<NetworkGame>([hub, llamadasFabrica](const NetworkConfig& c) -> std::unique_ptr<ITransport>
		{
			if (llamadasFabrica) ++*llamadasFabrica;
			if (c.rol == Role::Host) return std::make_unique<HostEndRealista>(hub);
			return std::make_unique<ClientEnd>(hub);
		});
		NetworkConfig cfg; cfg.rol = rol; cfg.nombre = rol == Role::Host ? "host" : "cliente";
		ok(n->red->Iniciar(cfg, escena, n->puertas.get(), glm::vec3(0, 1, 1)), "Iniciar acepta la configuracion");
		return n;
	}
}

int main()
{
	// ---------------- configuracion ----------------
	{
		NetworkConfig c; std::string err;
		std::string host; uint16_t puerto = 1;
		char a0[] = "x";
		char* argv0[] = { a0 };
		ok(ParsearArgumentos(1, argv0, c, err) && c.rol == Role::Offline, "sin argumentos = offline por defecto");
		char h[] = "--host", p[] = "--port", pv[] = "9000";
		char* argvH[] = { a0, h, p, pv };
		ok(ParsearArgumentos(4, argvH, c, err) && c.rol == Role::Host && c.puerto == 9000, "--host --port");
		char cn[] = "--connect", ep[] = "192.168.1.20:7001";
		char* argvC[] = { a0, cn, ep };
		ok(ParsearArgumentos(3, argvC, c, err) && c.rol == Role::Client && c.host == "192.168.1.20" && c.puerto == 7001, "--connect ip:puerto");
		ok(ParsearEndpoint("100.64.23.7:7777", host, puerto, err) && host == "100.64.23.7" && puerto == 7777, "IPv4 Tailscale y puerto por defecto");
		ok(ParsearEndpoint("100.127.255.254:65535", host, puerto, err) && puerto == 65535, "IPv4 tailnet en limite superior");
		ok(!ParsearEndpoint("100.300.1.2:7777", host, puerto, err) && !ParsearEndpoint("100.1.2:7777", host, puerto, err), "IPv4 literal invalida rechazada");
		char mal[] = "--connect", ep2[] = "host malo:99999";
		char* argvM[] = { a0, mal, ep2 };
		ok(!ParsearArgumentos(3, argvM, c, err), "endpoint invalido rechazado");
		char pm[] = "70000"; char* argvP[] = { a0, h, p, pm };
		ok(!ParsearArgumentos(4, argvP, c, err), "puerto fuera de rango rechazado");
		puerto = 1;
		ok(ParsearEndpoint("10.0.0.5", host, puerto, err) && puerto == 1 && host == "10.0.0.5", "endpoint sin puerto conserva el actual");
		ok(!ParsearEndpoint("", host, puerto, err) && !ParsearEndpoint("a:0", host, puerto, err) && !ParsearEndpoint("a:x", host, puerto, err), "endpoint vacio/puerto 0/no numerico");
	}

	// ---------------- buffer e interpolacion de fisica remota ----------------
	{
		using namespace netgame::timeline;
		const glm::vec3 poseActual(1.0f, 2.0f, 3.0f);
		const glm::quat rotacionActual = glm::angleAxis(0.4f, glm::vec3(0, 0, 1));
		const glm::vec3 poseObjetivo(5.0f, 6.0f, 7.0f);
		const glm::quat rotacionObjetivo = glm::angleAxis(1.0f, glm::vec3(0, 0, 1));
		const auto sinTiempo = BlendRemotePhysicsPose(poseActual, rotacionActual,
			poseObjetivo, rotacionObjetivo, 0.0f);
		ok(cerca(sinTiempo.position, poseActual),
			"delta cero conserva la posicion actual durante la mezcla");
		ok(cerca(sinTiempo.rotation.z, rotacionActual.z) && cerca(sinTiempo.rotation.w, rotacionActual.w),
			"delta cero conserva la rotacion actual durante la mezcla");

		constexpr float mezclaEsperada = 0.834701f;
		ok(cerca(RemotePhysicsBlendFactor(0.1f), mezclaEsperada, 2e-6f),
			"delta 0.1 conserva el factor de mezcla aproximado 0.834701");
		const auto mezclada = BlendRemotePhysicsPose(poseActual, rotacionActual,
			poseObjetivo, rotacionObjetivo, 0.1f);
		const glm::vec3 posicionEsperada = glm::mix(poseActual, poseObjetivo, mezclaEsperada);
		const glm::quat rotacionEsperada = glm::slerp(rotacionActual, rotacionObjetivo, mezclaEsperada);
		ok(cerca(mezclada.position, posicionEsperada),
			"delta 0.1 aplica la posicion interpolada esperada");
		ok(cerca(mezclada.rotation.z, rotacionEsperada.z) && cerca(mezclada.rotation.w, rotacionEsperada.w),
			"delta 0.1 aplica la rotacion interpolada esperada");

		std::map<uint32_t, bool> dormidosAntes;
		ok(!netgame::publication::ObserveHostSleepTransition(dormidosAntes, 42, false),
			"primera observacion dormida no publica estado final");
		ok(!netgame::publication::ObserveHostSleepTransition(dormidosAntes, 7, true),
			"primera observacion despierta no publica estado final");
		ok(!netgame::publication::ObserveHostSleepTransition(dormidosAntes, 42, false),
			"estado dormido repetido no publica estado final");
		ok(!netgame::publication::ObserveHostSleepTransition(dormidosAntes, 42, true),
			"transicion dormido-a-despierto no publica estado final");
		ok(!netgame::publication::ObserveHostSleepTransition(dormidosAntes, 42, true),
			"estado despierto repetido no publica estado final");
		ok(netgame::publication::ObserveHostSleepTransition(dormidosAntes, 42, false),
			"transicion despierto-a-dormido publica estado final");
		ok(!netgame::publication::ObserveHostSleepTransition(dormidosAntes, 42, false),
			"estado dormido posterior a transicion no repite publicacion");
		ok(!netgame::publication::ObserveHostSleepTransition(dormidosAntes, 99, false),
			"ID independiente no hereda estado previo y su primera observacion no publica");
		ok(netgame::publication::ObserveHostSleepTransition(dormidosAntes, 7, false),
			"ID independiente publica su propia transicion despierto-a-dormido");

		const auto estado = [](uint32_t tick, float x, uint8_t flags = 0,
			net::PeerId owner = net::kUnassignedPeerId)
		{
			net::DynamicStateEntry e;
			e.objectId = HashId("timeline_test"); e.tick = tick;
			e.transform.pos[0] = x; e.flags = flags; e.owner = owner;
			return e;
		};

		RemotePhysicsBuffers buffer;
		auto invalido = estado(1, 0); invalido.objectId = 0;
		StoreRemotePhysicsSample(buffer, invalido, 1.0);
		StoreRemotePhysicsSample(buffer, estado(100, 0), 10.0);
		StoreRemotePhysicsSample(buffer, estado(100, 9), 99.0);
		StoreRemotePhysicsSample(buffer, estado(99, 8), 99.0);
		StoreRemotePhysicsSample(buffer, estado(100u + 0x80000000u, 7), 99.0);
		StoreRemotePhysicsSample(buffer, estado(120, 2), 20.0);
		StoreRemotePhysicsSample(buffer, estado(140, 4), 30.0);
		const auto& reconstruidas = buffer.at(HashId("timeline_test"));
		ok(buffer.size() == 1 && reconstruidas.size() == 3 &&
			cerca(static_cast<float>(reconstruidas[0].time), 10.0f) &&
			cerca(static_cast<float>(reconstruidas[1].time), 10.02f) &&
			cerca(static_cast<float>(reconstruidas[2].time), 10.04f) &&
			reconstruidas[1].state.transform.pos[0] == 2.0f,
			"buffer ignora estado invalido, tick igual/antiguo/medio-rango y reconstruye tiempo desde tick pese al jitter de llegada");

		RemotePhysicsBuffers envuelto;
		StoreRemotePhysicsSample(envuelto, estado(0xFFFFFFF0u, 1), 5.0);
		StoreRemotePhysicsSample(envuelto, estado(0x18u, 2), 100.0);
		ok(envuelto.at(HashId("timeline_test")).size() == 2 &&
			cerca(static_cast<float>(envuelto.at(HashId("timeline_test"))[1].time), 5.04f),
			"buffer acepta wraparound y reconstruye el intervalo de 40 ms");

		RemotePhysicsBuffers limitado;
		for (uint32_t i = 1; i <= 9; ++i)
			StoreRemotePhysicsSample(limitado, estado(i, static_cast<float>(i)), 50.0 + i);
		const auto& ultimos = limitado.at(HashId("timeline_test"));
		ok(ultimos.size() == 8 && ultimos.front().state.tick == 2 && ultimos.back().state.tick == 9,
			"el noveno snapshot expulsa el primero y conserva ocho muestras");

		std::deque<RemotePhysicsSample> muestras;
		auto primero = estado(1, 0, 1, 3); primero.linearVelocity[0] = 1;
		auto ultimo = estado(2, 10, 3, 7); ultimo.linearVelocity[0] = 9;
		const float medio = std::sqrt(0.5f);
		ultimo.transform.rot[2] = medio; ultimo.transform.rot[3] = medio;
		muestras.push_back({ 10.0, primero }); muestras.push_back({ 10.04, ultimo });
		const auto antes = InterpolateRemotePhysicsState(muestras, 9.9);
		const auto entre = InterpolateRemotePhysicsState(muestras, 10.14);
		const auto despues = InterpolateRemotePhysicsState(muestras, 10.5);
		ok(cerca(antes.transform.pos[0], 0.0f) && cerca(entre.transform.pos[0], 5.0f) &&
			cerca(despues.transform.pos[0], 10.0f) && entre.tick == 2 && entre.flags == 3 &&
			entre.owner == 7 && entre.linearVelocity[0] == 9 &&
			cerca(entre.transform.rot[2], std::sin(3.14159265f / 8.0f), 1e-3f),
			"interpolacion usa el objetivo de 120 ms, interpola pose sin extrapolar y conserva metadata de la muestra nueva");

		std::deque<RemotePhysicsSample> tresMuestras = {
			{ 10.0, primero }, { 10.04, ultimo }, { 10.08, estado(3, 40) }
		};
		ok(cerca(InterpolateRemotePhysicsState(tresMuestras, 10.14).transform.pos[0], 5.0f),
			"interpolacion con tres muestras conserva el segmento inicial");
		ok(cerca(InterpolateRemotePhysicsState(tresMuestras, 10.18).transform.pos[0], 25.0f),
			"interpolacion selecciona el segmento posterior con tres muestras");

		const std::deque<RemotePhysicsSample> unaMuestra = { { 10.0, estado(1, 6) } };
		ok(cerca(InterpolateRemotePhysicsState(unaMuestra, 100.0).transform.pos[0], 6.0f),
			"una muestra conserva su pose aunque el objetivo quede mucho mas adelante");

		std::deque<RemotePhysicsSample> duracionCero = {
			{ 1.0, primero }, { 1.0, ultimo }
		};
		ok(cerca(InterpolateRemotePhysicsState(duracionCero, 0.0).transform.pos[0], 10.0f),
			"segmento de duracion cero selecciona el extremo posterior");

		RemotePhysicsBuffers finales;
		StoreRemotePhysicsSample(finales, estado(0xFFFFFFFEu, 1), 2.0);
		StoreRemotePhysicsSample(finales, estado(0xFFFFFFFFu, 2), 3.0);
		ApplyReliableFinalPhysicsState(finales, estado(0xFFFFFFFFu, 8), 7.0);
		ApplyReliableFinalPhysicsState(finales, estado(0xFFFFFFFEu, 9), 7.0);
		const auto sinCambio = finales.at(HashId("timeline_test"));
		ApplyReliableFinalPhysicsState(finales, estado(0u, 4), 7.25);
		const auto& finalNuevo = finales.at(HashId("timeline_test"));
		ok(sinCambio.size() == 2 && sinCambio.back().state.transform.pos[0] == 2.0f &&
			finalNuevo.size() == 1 && finalNuevo.front().state.tick == 0 &&
			finalNuevo.front().state.transform.pos[0] == 4.0f &&
			cerca(static_cast<float>(finalNuevo.front().time), 7.25f),
			"final igual o antiguo se ignora y el final nuevo con wraparound colapsa y reancla el buffer a su llegada");

		RemotePhysicsBuffers sinBufferPrevio;
		ApplyReliableFinalPhysicsState(sinBufferPrevio, estado(0, 11), 8.5);
		const auto& primerFinal = sinBufferPrevio.at(HashId("timeline_test"));
		ok(primerFinal.size() == 1 && primerFinal.front().state.tick == 0 &&
			primerFinal.front().state.transform.pos[0] == 11.0f &&
			cerca(static_cast<float>(primerFinal.front().time), 8.5f),
			"estado final fiable sin buffer previo se acepta y reancla a su llegada");
	}

	// ---------------- estado replicable de objetos JSON ----------------
	{
		struct ObjetoConFisica
		{
			std::string id;
			bool colision = true;
			fisica::BodyType tipo = fisica::BodyType::Estatico;
		};
		const std::vector<ObjetoConFisica> objetos = {
			{ "mueble", true, fisica::BodyType::Estatico },
			{ "hiriti", true, fisica::BodyType::Rebotador },
			{ "caja", true, fisica::BodyType::Dinamico }
		};
		const auto hashObjeto = [](const ObjetoConFisica& objeto) { return HashId(objeto.id); };
		const auto objetoReplicable = [](const ObjetoConFisica& objeto)
		{
			return objeto.colision && fisica::EsCuerpoDinamico(objeto.tipo);
		};
		const auto& hiriti = objetos[1];
		const uint32_t hiritiId = HashId("hiriti");
		const uint8_t tipoHiriti = TipoColisionEscena(hiriti.colision, fisica::EsCuerpoDinamico(hiriti.tipo));
		ok(tipoHiriti == 2, "Rebotador de Hiriti se anuncia como cuerpo dynamic replicable");
		SceneObjectState estadoHiriti;
		estadoHiriti.id = hiritiId; estadoHiriti.colisionPresente = true;
		estadoHiriti.tipoColision = tipoHiriti; estadoHiriti.masa = 0.25f;
		ok(EstadoObjetoEscenaValido(estadoHiriti) &&
			CodificarEstadoObjetoEscena(estadoHiriti).size() == 66,
			"estado inicial de Hiriti conserva su ID persistente y acepta clase replicable");
		std::size_t indiceHiriti = objetos.size();
		ok(IndiceUnicoObjetoRed(hiritiId, objetos, hashObjeto, objetoReplicable, indiceHiriti) && indiceHiriti == 1,
			"filtro de snapshots encuentra el índice único de Hiriti Rebotador");
		auto ambiguos = objetos; ambiguos.push_back(hiriti);
		ok(!IndiceUnicoObjetoRed(hiritiId, ambiguos, hashObjeto, objetoReplicable, indiceHiriti),
			"filtro de snapshots rechaza IDs persistentes ambiguos");
		ok(!IndiceUnicoObjetoRed(HashId("mueble"), objetos, hashObjeto, objetoReplicable, indiceHiriti),
			"filtro de snapshots sigue rechazando objetos estáticos");
		ok(TipoFisicoReplicaEscena(true, 2, fisica::BodyType::Rebotador,
			fisica::BodyType::Dinamico, fisica::BodyType::Rebotador, fisica::BodyType::Estatico) == fisica::BodyType::Rebotador &&
			TipoFisicoReplicaEscena(true, 2, fisica::BodyType::Estatico,
				fisica::BodyType::Dinamico, fisica::BodyType::Rebotador, fisica::BodyType::Estatico) == fisica::BodyType::Dinamico &&
			TipoFisicoReplicaEscena(true, 1, fisica::BodyType::Rebotador,
				fisica::BodyType::Dinamico, fisica::BodyType::Rebotador, fisica::BodyType::Estatico) == fisica::BodyType::Estatico &&
			TipoFisicoReplicaEscena(false, 2, fisica::BodyType::Rebotador,
				fisica::BodyType::Dinamico, fisica::BodyType::Rebotador, fisica::BodyType::Estatico) == fisica::BodyType::Estatico,
			"el estado recibido conserva Rebotador local solo con dynamic; static/none del host manda y dynamic discrepante cae a Dinamico");
		ok(!PausarMundoJuego(true, false, false, false, false, true) &&
			PausarMundoJuego(true, false, false, false, false, false) &&
			PausarMundoJuego(true, true, false, false, false, true) &&
			PausarMundoJuego(true, false, true, false, false, true) &&
			PausarMundoJuego(false, false, false, true, false, true) &&
			!PausarMundoJuego(true, false, false, false, true, false),
			"host en PLAY sin foco avanza; cliente/offline sin foco, editor, pausa y menú congelan; XR conserva su excepción");
		glm::vec3 lanzamientoMaximo = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)) * kRapidezMaximaLanzamiento;
		ok(LimitarRapidezLanzamientoHost(lanzamientoMaximo) && glm::length(lanzamientoMaximo) <= kRapidezMaximaLanzamiento,
			"la velocidad máxima normalizada respeta el límite estricto de 15 m/s");
		glm::vec3 redondeoLanzamiento(kRapidezMaximaLanzamiento + 5e-5f, 0.0f, 0.0f);
		ok(LimitarRapidezLanzamientoHost(redondeoLanzamiento) &&
			cerca(glm::length(redondeoLanzamiento), kRapidezMaximaLanzamiento, 1e-6f),
			"el host ajusta solo el pequeño exceso de redondeo float32 a 15 m/s");
		glm::vec3 lanzamientoExcesivo(kRapidezMaximaLanzamiento + 0.01f, 0.0f, 0.0f);
		ok(!LimitarRapidezLanzamientoHost(lanzamientoExcesivo),
			"el host rechaza lanzamientos que exceden el margen de redondeo acotado");

		SceneObjectState original;
		original.id = HashId("caja_sobre_mesa");
		original.posicion = glm::vec3(1.25f, -2.0f, 3.5f);
		original.rotacion = glm::vec3(0.0f, 15.0f, 90.0f);
		original.escala = glm::vec3(0.5f, 0.75f, 1.0f);
		original.colisionPresente = true; original.tipoColision = 2;
		original.mitadTamano = glm::vec3(0.4f, 0.5f, 0.6f);
		original.desplazamiento = glm::vec3(0.1f, 0.0f, -0.2f); original.masa = 4.0f;
		const auto payload = CodificarEstadoObjetoEscena(original);
		SceneObjectState recibido;
		ok(payload.size() == 66 && DecodificarEstadoObjetoEscena(original.id, payload, recibido) &&
			IgualEstadoObjetoEscena(original, recibido), "estado de objeto JSON ida y vuelta");
		auto truncado = payload; truncado.pop_back();
		ok(!DecodificarEstadoObjetoEscena(original.id, truncado, recibido), "estado de objeto truncado rechazado");
		original.escala.x = 0.0f;
		ok(CodificarEstadoObjetoEscena(original).empty(), "estado de objeto con escala cero rechazado");
	}

	// ---------------- captura y lookup de estado de escena ----------------
	{
		auto makeObject = [](const std::string& id, bool collider, fisica::BodyType bodyType, float mass)
		{
			SceneObject object;
			object.id = id;
			object.posicion = glm::vec3(1.0f, 2.0f, 3.0f);
			object.rotacion = glm::vec3(0.0f, 10.0f, 20.0f);
			object.escala = glm::vec3(1.0f);
			object.fisica.presente = collider;
			object.fisica.tipo = bodyType;
			object.fisica.mitadTamano = glm::vec3(0.7f, 0.8f, 0.9f);
			object.fisica.desplazamiento = glm::vec3(0.11f, -0.22f, 0.33f);
			object.fisica.masa = mass;
			return object;
		};
		std::vector<SceneObjectState> states;
		const std::vector<SceneObject> objects = {
			makeObject("sin_collider", false, fisica::BodyType::Estatico, 0.0f),
			makeObject("estatico", true, fisica::BodyType::Estatico, 0.0f),
			makeObject("dinamico", true, fisica::BodyType::Dinamico, 2.0f),
			makeObject("hiriti", true, fisica::BodyType::Rebotador, 0.25f),
			makeObject("dinamico_sin_collider", false, fisica::BodyType::Dinamico, 2.0f)
		};
		ok(netgame::scene_network_state::CaptureSceneNetworkStates(objects, states) && states.size() == 5,
			"captura conserva los cinco objetos de escena");
		ok(states.size() == 5, "captura devuelve el numero esperado de estados antes de consultar indices");
		if (states.size() >= 5)
		{
			ok(states[0].id == HashId("sin_collider") && states[1].id == HashId("estatico") &&
				states[2].id == HashId("dinamico") && states[3].id == HashId("hiriti"),
				"captura preserva el orden de objetos de escena");
			ok(states[0].tipoColision == 0 && !states[0].colisionPresente &&
				states[1].tipoColision == 1 && states[1].colisionPresente &&
				states[2].tipoColision == 2 && states[3].tipoColision == 2 &&
				states[4].tipoColision == 0 && !states[4].colisionPresente,
				"captura conserva none, static, dynamic y Rebotador como clases de colision de red");
			ok(cerca(states[2].posicion, glm::vec3(1.0f, 2.0f, 3.0f)) &&
				cerca(states[2].rotacion, glm::vec3(0.0f, 10.0f, 20.0f)) &&
				cerca(states[2].mitadTamano, glm::vec3(0.7f, 0.8f, 0.9f)) &&
				cerca(states[2].desplazamiento, glm::vec3(0.11f, -0.22f, 0.33f)),
				"captura copia posicion, rotacion, mitad de tamano y desplazamiento de la fisica");
		}

		const std::vector<SceneObject> duplicateObjects = { objects[0], objects[0] };
		ok(!netgame::scene_network_state::CaptureSceneNetworkStates(duplicateObjects, states) && states.size() == 1,
			"ID persistente duplicado falla y conserva solo la salida parcial anterior");
		const std::vector<SceneObject> hashCollisionObjects = { objects[0], objects[1] };
		const auto collidingHash = [](const std::string&) { return 123u; };
		ok(!netgame::scene_network_state::CaptureSceneNetworkStates(hashCollisionObjects, states, collidingHash) && states.size() == 1,
			"colision de hash falla antes de agregar el segundo estado");
		const auto zeroHash = [](const std::string&) { return 0u; };
		ok(!netgame::scene_network_state::CaptureSceneNetworkStates(std::vector<SceneObject>{ objects[0] }, states, zeroHash) && states.empty(),
			"hash cero se rechaza antes de emitir un estado");
		const auto reservedHash = [](const std::string&) { return kRangoReservadoIni; };
		ok(!netgame::scene_network_state::CaptureSceneNetworkStates(std::vector<SceneObject>{ objects[0] }, states, reservedHash) && states.empty(),
			"hash reservado se rechaza por el validador del estado");

		auto invalidScaleObject = objects[0]; invalidScaleObject.escala.y = 0.0f;
		ok(!netgame::scene_network_state::CaptureSceneNetworkStates(std::vector<SceneObject>{ objects[1], invalidScaleObject }, states) &&
			states.size() == 1 && states[0].id == HashId("estatico"),
			"escala invalida falla despues del mapeo y mantiene la salida parcial");
		auto invalidMassObject = objects[2]; invalidMassObject.fisica.masa = 0.0f;
		ok(!netgame::scene_network_state::CaptureSceneNetworkStates(std::vector<SceneObject>{ objects[1], invalidMassObject }, states) &&
			states.size() == 1 && states[0].id == HashId("estatico"),
			"masa dinamica invalida falla despues del mapeo y mantiene la salida parcial");

		std::size_t index = 77;
		ok(netgame::scene_network_state::FindUniqueDynamicSceneObjectIndex(HashId("dinamico"), objects, index) && index == 2,
			"lookup devuelve el unico objeto dinamico por hash persistente");
		ok(!netgame::scene_network_state::FindUniqueDynamicSceneObjectIndex(HashId("desconocido"), objects, index),
			"lookup rechaza IDs desconocidos");
		ok(netgame::scene_network_state::FindUniqueDynamicSceneObjectIndex(HashId("hiriti"), objects, index) && index == 3,
			"lookup acepta el objeto Rebotador de Hiriti");
		ok(!netgame::scene_network_state::FindUniqueDynamicSceneObjectIndex(HashId("dinamico_sin_collider"), objects, index),
			"lookup rechaza un objeto dinamico sin collider");
		const std::vector<SceneObject> duplicateLookupObjects = { objects[2], objects[3] };
		const auto ambiguousHash = [](const std::string&) { return 321u; };
		ok(!netgame::scene_network_state::FindUniqueDynamicSceneObjectIndex(321u, duplicateLookupObjects,
			ambiguousHash, index),
			"lookup rechaza exactamente dos coincidencias con el mismo hash inyectado 321");
		ok(!netgame::scene_network_state::FindUniqueDynamicSceneObjectIndex(HashId("estatico"), objects, index),
			"lookup sigue rechazando objetos estaticos");
	}

	// ---------------- offline: nada se crea ----------------
	{
		int llamadas = 0;
		NetworkGame red([&llamadas](const NetworkConfig&) -> std::unique_ptr<ITransport> { ++llamadas; return nullptr; });
		auto pu = NuevasPuertas();
		ok(red.Iniciar(NetworkConfig{}, "escena", pu.get(), glm::vec3(0)), "offline: Iniciar devuelve true");
		PoseEntry p = PoseEn(glm::vec3(0, 0, 1.7f));
		for (int i = 0; i < 10; ++i) red.Actualizar(i * 0.016, &p);
		ok(llamadas == 0, "offline: la fabrica de transporte (ENet) NO se invoca");
		ok(!red.EnRed() && !red.Activa() && !red.EsHost() && !red.EsCliente(), "offline: sin fase de red");
		ok(red.RemotosParaDibujar(1.0).empty(), "offline: sin remotos");
		ok(!red.InterceptarPuerta(0), "offline: la puerta la alterna el juego");
		ok(red.ObtenerDiagnostico().modo == "Offline", "offline: diagnostico");
	}

	// ---------------- sin transporte (ENet no incluido) ----------------
	{
		NetworkGame red([](const NetworkConfig&) -> std::unique_ptr<ITransport> { return nullptr; });
		auto pu = NuevasPuertas(); NetworkConfig c; c.rol = Role::Host;
		ok(red.Iniciar(c, "escena", pu.get(), glm::vec3(0)), "sin transporte: arranca asincrono");
		PoseEntry p = PoseEn(glm::vec3(0));
		bool error = false; const double fin = Reloj() + 3;
		while (Reloj() < fin && !error) { red.Actualizar(Reloj(), &p); error = red.ObtenerDiagnostico().fase == NetworkPhase::Error; std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
		ok(error && !red.EnRed(), "sin transporte: queda en Error y offline");
		ok(!red.ObtenerDiagnostico().mensaje.empty(), "sin transporte: mensaje de diagnostico");
		ok(!red.InterceptarPuerta(0), "tras el error el juego vuelve a controlar las puertas");
	}

	// ---------------- poses PC / OpenXR (Z arriba) ----------------
	{
		PoseEntry p;
		ok(PoseDesdeEscritorio(glm::vec3(1, 2, 1.7f), glm::vec3(0, 1, 0), p), "pose PC");
		ok(cerca(PosDe(p.head), glm::vec3(1, 2, 1.7f)), "pose PC: cabeza");
		ok(cerca(RotDe(p.head) * glm::vec3(0, 1, 0), glm::vec3(0, 1, 0)), "pose PC: mirando +Y = identidad");
		const glm::vec3 der = RotDe(p.head) * glm::vec3(1, 0, 0);
		ok(cerca(PosDe(p.rightHand) - PosDe(p.head), der * 0.22f + glm::vec3(0, 1, 0) * 0.35f + glm::vec3(0, 0, -0.30f)), "pose PC: mano derecha a la derecha y delante");
		PoseDesdeEscritorio(glm::vec3(0), glm::vec3(1, 0, 0), p);
		ok(cerca(RotDe(p.head) * glm::vec3(0, 1, 0), glm::vec3(1, 0, 0)), "pose PC: mirando +X");
		ok(cerca((RotDe(p.head) * glm::vec3(0, 0, 1)).z, 1.0f), "pose PC: Z sigue siendo arriba");
		PoseDesdeEscritorio(glm::vec3(0), glm::vec3(0, 0, 1), p);   // mirando recto hacia arriba: no degenera
		ok(TransformValido(p.head) && PoseValida(p), "pose PC: mirada vertical valida");
		ok(!PoseDesdeEscritorio(glm::vec3(NAN, 0, 0), glm::vec3(0, 1, 0), p), "pose PC: NaN rechazado");
		ok(!PoseDesdeEscritorio(glm::vec3(0), glm::vec3(INFINITY, 0, 0), p), "pose PC: direccion infinita rechazada");

		// OpenXR: identidad con anclaje mirando +Y
		vr::Anchor anc; anc.origen = glm::vec3(5, 6, 0); anc.adelante = glm::vec3(0, 1, 0);
		vr::XrPose cab; cab.posicion = glm::vec3(0, 1.6f, 0);   // XR: Y arriba
		net::PoseEntry vrp;
		ok(PoseDesdeVR(cab, nullptr, nullptr, anc, vrp), "pose VR sin mandos");
		ok(cerca(PosDe(vrp.head), glm::vec3(5, 6, 1.6f)), "pose VR: Y de XR -> Z del motor");
		ok(cerca(RotDe(vrp.head) * glm::vec3(0, 1, 0), glm::vec3(0, 1, 0)), "pose VR: -Z de XR = adelante");
		vr::XrPose manoD; manoD.posicion = glm::vec3(0.3f, 1.2f, -0.4f);   // 30 cm a la derecha, 40 cm delante
		PoseDesdeVR(cab, nullptr, &manoD, anc, vrp);
		ok(cerca(PosDe(vrp.rightHand), glm::vec3(5.3f, 6.4f, 1.2f)), "pose VR: mano derecha real (no el offset de PC)");
		anc.adelante = glm::vec3(1, 0, 0);   // el anclaje gira: adelante de XR = +X del motor
		PoseDesdeVR(cab, nullptr, &manoD, anc, vrp);
		ok(cerca(RotDe(vrp.head) * glm::vec3(0, 1, 0), glm::vec3(1, 0, 0)), "pose VR: anclaje girado");
		ok(cerca(PosDe(vrp.rightHand), glm::vec3(5.4f, 5.7f, 1.2f)), "pose VR: mano rotada con el anclaje");
		// la conversion no modifica sus entradas (pose local intacta)
		const vr::XrPose copia = cab; PoseDesdeVR(cab, nullptr, &manoD, anc, vrp);
		ok(cab.posicion == copia.posicion && cab.orientacion == copia.orientacion, "pose VR: la pose local no se altera");
		vr::XrPose rara; rara.posicion = glm::vec3(NAN, 0, 0);
		ok(!PoseDesdeVR(rara, nullptr, nullptr, anc, vrp), "pose VR: NaN rechazado");
	}

	// ---------------- interpolacion ----------------
	{
		RemoteInterpolator ip;
		std::map<PeerId, PoseEntry> s;
		s[0] = PoseEn(glm::vec3(0, 0, 1)); s[0].peer = 0;     // el local (2) no debe aparecer; el peer 0 es otro jugador
		s[2] = PoseEn(glm::vec3(9, 9, 9)); s[2].peer = 2;
		ip.Actualizar(s, 2, 10.0);
		ok(ip.Cantidad() == 1 && ip.Muestrear(0, 10.0, s[0]), "el jugador local nunca se interpola");
		PoseEntry out;
		ok(!ip.Muestrear(2, 10.0, out), "el local no se muestrea");
		s.erase(2);
		s[0] = PoseEn(glm::vec3(2, 0, 1)); s[0].peer = 0;
		ip.Actualizar(s, 2, 10.05);   // llega 50 ms despues
		ok(ip.Muestrear(0, 10.0 + kRetardoInterpolacion + 0.025, out) && cerca(PosDe(out.head).x, 1.0f, 0.02f), "interpolacion lineal a mitad de camino");
		ok(ip.Muestrear(0, 100.0, out) && cerca(PosDe(out.head).x, 2.0f), "sin extrapolar: se mantiene la ultima");
		ok(ip.Muestrear(0, 0.0, out) && cerca(PosDe(out.head).x, 0.0f), "antes de la primera: primera muestra");
		// rotacion: slerp
		std::map<PeerId, PoseEntry> r; r[3] = PoseEn(glm::vec3(0), glm::vec3(0, 1, 0)); r[3].peer = 3;
		RemoteInterpolator ir; ir.Actualizar(r, 1, 0.0);
		r[3] = PoseEn(glm::vec3(0), glm::vec3(-1, 0, 0)); r[3].peer = 3;   // gira 90 grados
		ir.Actualizar(r, 1, 0.05);
		ir.Muestrear(3, kRetardoInterpolacion + 0.025, out);
		const glm::vec3 f = RotDe(out.head) * glm::vec3(0, 1, 0);
		ok(cerca(glm::length(f), 1.0f) && cerca(glm::degrees(std::acos(glm::dot(f, glm::vec3(0, 1, 0)))), 45.0f, 1.0f), "slerp: 45 grados a mitad de camino");
		std::map<PeerId, PoseEntry> mala; mala[4] = PoseEn(glm::vec3(0)); mala[4].head.pos[0] = NAN;
		RemoteInterpolator im; im.Actualizar(mala, 1, 0.0);
		ok(im.Cantidad() == 0, "snapshot con NaN descartado");
		im.Actualizar({}, 1, 1.0);
		s.clear(); ip.Actualizar(s, 2, 11.0);
		ok(ip.Cantidad() == 0, "peers ausentes del snapshot se descartan");
	}

	// ---------------- puertas: autoridad del host ----------------
	{
		auto pu = NuevasPuertas(); DoorSync sp; std::string err;
		ok(sp.Reconstruir(*pu, err) && sp.Cantidad() == 2, "tabla de puertas por id de escena");
		const uint32_t idE = HashId("puerta_entrada"), idL = HashId("puerta_lab");
		const glm::vec3 cerca1(0, 0, 1.7f), lejos(100, 100, 1.7f);
		ok(sp.HostSolicitud(*pu, idE, &cerca1) == RequestResult::Aplicada && pu->v[0].abierta, "host abre la puerta pedida");
		ok(sp.HostSolicitud(*pu, idE, &lejos) == RequestResult::FueraDeAlcance && pu->v[0].abierta, "fuera de alcance: rechazada");
		ok(sp.HostSolicitud(*pu, idE, nullptr) == RequestResult::SinPose, "sin pose: rechazada");
		ok(sp.HostSolicitud(*pu, 0xDEADBEEF, &cerca1) == RequestResult::IdDesconocido, "id desconocido rechazado");
		ok(sp.HostSolicitud(*pu, idL, &cerca1) == RequestResult::FueraDeAlcance && !pu->v[1].abierta, "otra puerta lejana: rechazada");
		std::vector<OutputEvent> ev; sp.HostDetectarCambios(*pu, ev);
		ok(ev.size() == 1 && ev[0].tipo == kEvPuertaEstado && ev[0].objetivo == idE && ev[0].payload == std::vector<uint8_t>{ 1 }, "cambio difundido una vez");
		ev.clear(); sp.HostDetectarCambios(*pu, ev);
		ok(ev.empty(), "sin cambios no hay eventos");
		pu->v[1].abierta = true; sp.HostDetectarCambios(*pu, ev);   // la abre el propio host (E o panel de depuracion)
		ok(ev.size() == 1 && ev[0].objetivo == idL, "cambio local del host tambien se difunde");

		auto cl = NuevasPuertas(); DoorSync sc; sc.Reconstruir(*cl, err);
		ok(sc.AplicarEstado(*cl, idE, { 1 }) && cl->v[0].abierta, "cliente aplica el estado del host");
		ok(sc.AplicarEstado(*cl, idE, { 1 }) && cl->v[0].abierta, "idempotente");
		ok(!sc.AplicarEstado(*cl, idE, { 2 }) && !sc.AplicarEstado(*cl, idE, {}) && !sc.AplicarEstado(*cl, 1234, { 1 }), "estados invalidos ignorados");
		std::vector<StateEntry> rec; sp.EstadoRecuperacion(*pu, rec);
		auto tarde = NuevasPuertas(); DoorSync st; st.Reconstruir(*tarde, err); st.AplicarRecuperacion(*tarde, rec);
		ok(tarde->v[0].abierta && tarde->v[1].abierta, "recuperacion para quien llega tarde");

		PuertasFalsas dup; dup.v.push_back({ "a", false, {} }); dup.v.push_back({ "a", false, {} }); dup.v.push_back({ "b", false, {} });
		DoorSync sd;
		ok(!sd.Reconstruir(dup, err) && sd.Cantidad() == 1 && !err.empty(), "ids duplicados: excluidos de la red con aviso");
	}

	// ---------------- propiedad temporal ----------------
	{
		OwnershipArbiter a; a.Definir(kObjetoPrueba, glm::vec3(0, 2, 1)); a.TomarCambios();
		const glm::vec3 cerca1(0, 0, 1.7f), lejos(50, 50, 1.7f);
		ok(a.Solicitar(kObjetoPrueba, 1, &lejos, 0) == RequestResult::FueraDeAlcance && a.Buscar(kObjetoPrueba)->dueno == kSinDueno, "fuera de alcance no concede");
		ok(a.Solicitar(kObjetoPrueba, 1, nullptr, 0) == RequestResult::SinPose, "sin pose no concede");
		ok(a.Solicitar(kObjetoPrueba, 1, &cerca1, 0) == RequestResult::Aplicada && a.Buscar(kObjetoPrueba)->dueno == 1, "concede al primero");
		ok(a.Solicitar(kObjetoPrueba, 2, &cerca1, 1) == RequestResult::Ocupado, "segundo solicitante: ocupado");
		ok(a.Mover(kObjetoPrueba, 2, glm::vec3(0, 1, 1), &cerca1, 1) == RequestResult::NoEsDueno, "solo el dueno mueve");
		ok(a.Mover(kObjetoPrueba, 1, glm::vec3(0, 1.5f, 1.5f), &cerca1, 1) == RequestResult::Aplicada && cerca(a.Buscar(kObjetoPrueba)->pos, glm::vec3(0, 1.5f, 1.5f)), "dueno mueve");
		ok(a.Mover(kObjetoPrueba, 1, glm::vec3(NAN, 0, 0), &cerca1, 1) == RequestResult::Invalida, "posicion NaN rechazada");
		ok(a.Mover(kObjetoPrueba, 1, glm::vec3(0, 40, 1), &cerca1, 1) == RequestResult::FueraDeAlcance, "teletransporte rechazado");
		ok(a.Liberar(kObjetoPrueba, 2) == RequestResult::NoEsDueno, "otro no puede soltar");
		a.TomarCambios();
		a.Tick(5.0);
		ok(a.Buscar(kObjetoPrueba)->dueno == 1, "la concesion sigue viva a mitad de plazo");
		a.Tick(1.0 + OwnershipArbiter::kConcesion + 0.1);
		ok(a.Buscar(kObjetoPrueba)->dueno == kSinDueno && a.TomarCambios().size() == 1, "la concesion caduca y se difunde");
		a.Solicitar(kObjetoPrueba, 3, &cerca1, 20.0); a.TomarCambios();
		a.PeerDesconectado(3);
		ok(a.Buscar(kObjetoPrueba)->dueno == kSinDueno && a.TomarCambios().size() == 1, "desconexion libera la propiedad");
		ok(a.Solicitar(kObjetoPrueba, 4, &cerca1, 30.0) == RequestResult::Aplicada, "queda disponible para otro");
		a.Liberar(kObjetoPrueba, 4);
		ok(a.Buscar(kObjetoPrueba)->dueno == kSinDueno, "soltar libera");

		SharedObjectView v; std::vector<StateEntry> rec; a.Solicitar(kObjetoPrueba, 2, &cerca1, 40.0); a.EstadoRecuperacion(rec);
		ok(v.AplicarRecuperacion(rec) && v.conocido && v.dueno == 2 && cerca(v.pos, a.Buscar(kObjetoPrueba)->pos, 0.002f), "recuperacion del objeto (dueno y posicion)");
		ok(!v.AplicarRecuperacion({}), "recuperacion incompleta ignorada");
		SharedObjectView w;
		ok(w.AplicarEstado(CodificarEstadoObjeto(1, glm::vec3(1, 2, 3))) && w.dueno == 1 && cerca(w.pos, glm::vec3(1, 2, 3)), "estado del objeto ida y vuelta");
		ok(!w.AplicarEstado({ 1, 2, 3 }), "payload corto rechazado");
		auto mal = CodificarEstadoObjeto(1, glm::vec3(1, 2, 3)); mal[2] = mal[3] = mal[4] = 0xFF; mal[5] = 0x7F;   // NaN/inf
		SharedObjectView x; ok(!x.AplicarEstado(mal) && !x.conocido, "payload con NaN rechazado");
		ok(RayoTocaEsfera(glm::vec3(0), glm::vec3(0, 1, 0), glm::vec3(0, 2, 0.2f), 0.35f, 3.0f) && !RayoTocaEsfera(glm::vec3(0), glm::vec3(0, 1, 0), glm::vec3(0, 2, 1.0f), 0.35f, 3.0f), "rayo contra el objeto");
	}

	// ---------------- integracion: host + 2 clientes (transporte en memoria) ----------------
	{
		auto hub = std::make_shared<Hub>();
		int fabricas = 0;
		auto host = Crear(hub, Role::Host, "aula", glm::vec3(0, 0, 1.7f), &fabricas);
		SceneObjectState caja;
		caja.id = HashId("caja_sobre_mesa"); caja.posicion = glm::vec3(2, 1, 0.5f);
		caja.colisionPresente = true; caja.tipoColision = 2; caja.masa = 2.0f;
		ok(host->red->FijarEstadoEscena({ caja }, false), "host registra el estado inicial de la escena");
		auto c1 = Crear(hub, Role::Client, "aula", glm::vec3(0, -0.5f, 1.7f), &fabricas);
		std::vector<Nodo*> hostYC1 = { host.get(), c1.get() };
		ok(Esperar(hostYC1, [&] { return host->red->Activa() && c1->red->Activa() && host->red->ObtenerDiagnostico().remotos == 1; }), "host y primer cliente conectan");
		std::vector<SceneObjectState> estadosEscena;
		const bool inicialRecibido = Esperar(hostYC1, [&] {
			c1->red->TomarEstadosEscena(estadosEscena);
			return std::any_of(estadosEscena.begin(), estadosEscena.end(), [&](const auto& e) { return e.id == caja.id && cerca(e.posicion, caja.posicion); });
		});
		ok(inicialRecibido, "cliente recibe el estado inicial del objeto por ID");
		caja.posicion = glm::vec3(3, 2, 1); caja.tipoColision = 1; caja.masa = 2.0f;
		ok(host->red->FijarEstadoEscena({ caja }, true), "host publica un cambio discreto de objeto");
		const bool cambioRecibido = Esperar(hostYC1, [&] {
			c1->red->TomarEstadosEscena(estadosEscena);
			return std::any_of(estadosEscena.begin(), estadosEscena.end(), [&](const auto& e) {
				return e.id == caja.id && cerca(e.posicion, caja.posicion) && e.tipoColision == 1;
			});
		});
		ok(cambioRecibido,
			"el cliente recibe transformacion y cambio dinamico a estatico");
		net::DynamicStateEntry enVuelo; enVuelo.objectId = HashId("caja_en_vuelo"); enVuelo.tick = 10;
		ok(PuedeCargarLanzamiento(false, net::Role::Client, enVuelo.objectId) &&
			!PuedeCargarLanzamiento(false, net::Role::Client, 0) &&
			PuedeCargarLanzamiento(true, net::Role::Offline, 0),
			"lanzamiento cliente usa propiedad confirmada y offline conserva el agarre local");
		ok(EstadoFisicoMasNuevo(12, true, 10) && !EstadoFisicoMasNuevo(9, true, 10) &&
			EstadoFisicoMasNuevo(0, true, 0xFFFFFFFFu),
			"estado fiable no revierte snapshot nuevo y tick admite wraparound");
		ok(PuedeAdquirirDinamico(kSinDueno, 1, 0) && !PuedeAdquirirDinamico(1, 2, 0) &&
			!PuedeAdquirirDinamico(kSinDueno, 1, 501),
			"solo un solicitante adquiere el objeto y cada jugador mantiene un agarre activo");
		ok(std::fabs(TiempoSnapshotSiguiente(4.0, 1000, 1040) - 4.04) < 1e-9 &&
			std::fabs(TiempoSnapshotSiguiente(4.0, 0xFFFFFFF0u, 0x18u) - 4.04) < 1e-9,
			"buffer fisico reconstruye intervalos de host y wraparound sin usar jitter de llegada");
		enVuelo.transform.pos[0] = 5.0f; enVuelo.flags = 0; enVuelo.owner = kSinDueno;
		ok(host->red->PublicarEstadoFisicoFinal(enVuelo), "host publica estado final fiable del objeto");
		std::vector<net::DynamicStateEntry> finales;
		const bool finalRecibido = Esperar(hostYC1, [&] {
			c1->red->TomarEstadosFisicosFiables(finales);
			return !finales.empty();
		});
		ok(finalRecibido && finales.size() == 1 && finales[0].tick == 10 && finales[0].transform.pos[0] == 5.0f,
			"evento fiable conserva tick y pose final");
		const auto peerC1 = c1->red->PeerLocal();
		ok(c1->red->SolicitarAgarreDinamico(enVuelo.objectId, { 0, 0, 1 }, { 0, 1, 0 }),
			"cliente envia solicitud fiable de agarre con su rayo");
		std::vector<DynamicObjectRequest> solicitudes;
		const bool agarreRecibido = Esperar(hostYC1, [&] {
			host->red->TomarSolicitudesFisicas(solicitudes);
			return !solicitudes.empty();
		});
		ok(agarreRecibido && solicitudes.size() == 1 && solicitudes[0].id == enVuelo.objectId && solicitudes[0].peer == peerC1 && solicitudes[0].agarrar,
			"host recibe la solicitud con peer e ID asignados por el protocolo");
		host->red->FijarPropietarioDinamico(enVuelo.objectId, peerC1);
		ok(Esperar(hostYC1, [&] { return c1->red->PropietarioDinamico(enVuelo.objectId) == peerC1; }),
			"el host confirma la propiedad al cliente");
		ok(c1->red->SolicitarSoltarDinamico(enVuelo.objectId, { 0, 0, 5 }), "cliente envia velocidad de lanzamiento");
		solicitudes.clear();
		const bool lanzamientoRecibido = Esperar(hostYC1, [&] {
			host->red->TomarSolicitudesFisicas(solicitudes);
			return !solicitudes.empty();
		});
		ok(lanzamientoRecibido && solicitudes.size() == 1 && !solicitudes[0].agarrar && solicitudes[0].peer == peerC1 && cerca(solicitudes[0].velocidad.z, 5.0f),
			"el host recibe solicitud de lanzamiento del propietario actual");
		ok(c1->red->SolicitarSoltarDinamico(enVuelo.objectId,
			{ kRapidezMaximaLanzamiento + 5e-5f, 0.0f, 0.0f }),
			"el transporte acepta el exceso float32 acotado para que decida el host");
		solicitudes.clear();
		const bool lanzamientoRedondeadoRecibido = Esperar(hostYC1, [&] {
			host->red->TomarSolicitudesFisicas(solicitudes);
			return !solicitudes.empty();
		});
		ok(lanzamientoRedondeadoRecibido && solicitudes.size() == 1 &&
			cerca(glm::length(solicitudes[0].velocidad), kRapidezMaximaLanzamiento, 1e-6f),
			"el host entrega una velocidad ajustada exactamente al límite de 15 m/s");
		const size_t ignoradosAntes = host->red->ObtenerDiagnostico().eventosIgnorados;
		ok(c1->red->SolicitarSoltarDinamico(enVuelo.objectId,
			{ kRapidezMaximaLanzamiento + 0.01f, 0.0f, 0.0f }),
			"el transporte permite enviar una solicitud excesiva para validación autoritativa");
		const bool excesoRechazado = Esperar(hostYC1, [&] {
			return host->red->ObtenerDiagnostico().eventosIgnorados > ignoradosAntes;
		});
		solicitudes.clear();
		host->red->TomarSolicitudesFisicas(solicitudes);
		ok(excesoRechazado && solicitudes.empty(),
			"la ruta real de recepción del host rechaza el exceso fuera del margen y no crea una solicitud de lanzamiento");
		auto c2 = Crear(hub, Role::Client, "aula", glm::vec3(1, -0.5f, 1.7f), &fabricas);
		std::vector<Nodo*> todos = { host.get(), c1.get(), c2.get() };
		ok(Esperar(todos, [&] { return host->red->Activa() && c1->red->Activa() && c2->red->Activa() && host->red->ObtenerDiagnostico().remotos == 2 && c1->red->ObtenerDiagnostico().remotos >= 1; }), "los tres se conectan y se ven");
		ok(fabricas == 3, "una fabrica de transporte por participante, solo al iniciar");
		const bool tardioEscena = Esperar(todos, [&] {
			c2->red->TomarEstadosEscena(estadosEscena);
			return std::any_of(estadosEscena.begin(), estadosEscena.end(), [&](const auto& e) {
				return e.id == caja.id && cerca(e.posicion, caja.posicion) && e.tipoColision == 1;
			});
		});
		ok(tardioEscena,
			"late join recibe el ultimo estado autoritativo");
		// Reutiliza 'finales' del estado anterior: drenar no debe reencolar su contenido antiguo.
		const bool tardioFisica = Esperar(todos, [&] {
			c2->red->TomarEstadosFisicosFiables(finales);
			return std::any_of(finales.begin(), finales.end(), [&](const auto& estado) {
				return estado.objectId == enVuelo.objectId && estado.owner == peerC1;
			});
		});
		if (!tardioFisica)
		{
			const auto diag = c2->red->ObtenerDiagnostico();
			std::cerr << "late-join timeout: fase=" << static_cast<int>(diag.fase) << " peer=" << c2->red->PeerLocal()
				<< " expected-owner=" << peerC1 << " dropped=" << diag.eventosDescartados << " ignorados=" << diag.eventosIgnorados << '\n';
			for (const auto& estado : finales)
				std::cerr << "late-join state: id=" << estado.objectId << " owner=" << estado.owner << " flags=" << int(estado.flags) << '\n';
		}
		ok(tardioFisica && finales.size() == 1 && finales[0].objectId == enVuelo.objectId && finales[0].owner == peerC1,
			"late join recibe el estado y propietario físico vigente");
		ok(Esperar(todos, [&] { return c1->red->Objeto().conocido && c2->red->Objeto().conocido; }), "recuperacion: los clientes conocen el objeto compartido");

		// jugadores remotos: nunca el propio
		ok(Esperar(todos, [&] { return host->red->RemotosParaDibujar(Reloj()).size() == 2 && c1->red->RemotosParaDibujar(Reloj()).size() == 2; }), "cada uno ve a los otros dos (host y el otro cliente)");
		bool propio = false;
		for (auto* n : todos) for (const auto& r : n->red->RemotosParaDibujar(Reloj())) if (r.peer == n->red->PeerLocal()) propio = true;
		ok(!propio, "nadie dibuja su propio avatar");
		// el movimiento de c2 llega interpolado a c1
		c2->pose = PoseEn(glm::vec3(1.0f, -0.5f, 1.7f));
		for (int i = 0; i < 10; ++i) { c2->pose = PoseEn(glm::vec3(1.0f + 0.1f * i, -0.5f, 1.7f)); for (auto* n : todos) n->Paso(); std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
		bool visto = false; PeerId idC2 = c2->red->PeerLocal();
		for (const auto& r : c1->red->RemotosParaDibujar(Reloj())) if (r.peer == idC2 && PosDe(r.head).x > 1.05f) visto = true;
		ok(visto, "c1 ve moverse a c2");

		// puertas: el cliente solo pide; el host decide y todos convergen
		ok(c1->red->InterceptarPuerta(0), "cliente: la puerta no se alterna localmente");
		ok(!c1->puertas->v[0].abierta, "cliente: la puerta sigue cerrada hasta que decide el host");
		ok(Esperar(todos, [&] { return host->puertas->v[0].abierta && c1->puertas->v[0].abierta && c2->puertas->v[0].abierta; }), "host abre y difunde: los tres abiertos");
		c2->red->InterceptarPuerta(1);   // puerta_lab esta a 30 m: el host la rechaza
		Esperar(todos, [&] { return false; }, 0.3);
		ok(!host->puertas->v[1].abierta && !c2->puertas->v[1].abierta, "peticion fuera de alcance rechazada por el host");
		ok(host->red->ObtenerDiagnostico().eventosIgnorados >= 1, "el host cuenta las peticiones rechazadas");
		ok(!host->red->InterceptarPuerta(0), "host: alterna localmente");
		host->puertas->v[0].abierta = false;   // E del host
		ok(Esperar(todos, [&] { return !c1->puertas->v[0].abierta && !c2->puertas->v[0].abierta; }), "puerta del host se replica a los clientes");

		// objeto compartido: c1 lo coge; c2 no puede; al desconectarse c1 se libera
		const glm::vec3 posObj = host->red->Objeto().pos;
		const glm::vec3 desde = posObj - glm::vec3(0, 1.0f, 0.2f);   // mirando al objeto desde 1 m
		const glm::vec3 dir = glm::normalize(posObj - desde);
		c1->pose = PoseEn(desde, dir); c2->pose = PoseEn(desde + glm::vec3(0.3f, 0, 0), dir);
		ok(!c1->red->AlternarObjeto(desde, glm::vec3(0, 0, 1), Reloj()), "no apuntando al objeto no se pide");
		ok(c1->red->AlternarObjeto(desde, dir, Reloj()), "c1 pide el objeto");
		const PeerId id1 = c1->red->PeerLocal();
		ok(Esperar(todos, [&] { return host->red->Objeto().dueno == id1 && c2->red->Objeto().dueno == id1 && c1->red->SoyDuenoDelObjeto(); }), "c1 es dueno y todos lo saben");
		c2->red->AlternarObjeto(desde, dir, Reloj());
		Esperar(todos, [&] { return false; }, 0.3);
		ok(host->red->Objeto().dueno == id1, "c2 no puede quitarselo a c1");
		const glm::vec3 destino = desde + dir * 1.5f;
		c1->red->ActualizarAcarreo(desde, dir, Reloj());
		ok(Esperar(todos, [&] { return cerca(host->red->Objeto().pos, destino, 0.01f); }), "el dueno mueve el objeto y el host lo acepta");
		ok(Esperar(todos, [&] { return cerca(c2->red->Objeto().pos, destino, 0.01f); }), "los demas ven el movimiento");
		c1->red->Detener();   // c1 se desconecta con el objeto en la mano
		std::vector<Nodo*> resto = { host.get(), c2.get() };
		ok(Esperar(resto, [&] { return host->red->Objeto().dueno == kSinDueno && c2->red->Objeto().dueno == kSinDueno; }), "al desconectar el dueno se libera la propiedad");
		ok(Esperar(resto, [&] { return host->red->ObtenerDiagnostico().remotos == 1; }), "el host deja de interpolar al desconectado");
		c2->pose = PoseEn(desde, dir);
		ok(c2->red->AlternarObjeto(desde, dir, Reloj()) && Esperar(resto, [&] { return c2->red->SoyDuenoDelObjeto(); }), "otro jugador puede cogerlo despues");
		c2->red->AlternarObjeto(desde, dir, Reloj());   // E de nuevo: soltar
		ok(Esperar(resto, [&] { return host->red->Objeto().dueno == kSinDueno; }), "soltar libera");

		// llegada tardia: recibe el estado actual
		host->puertas->v[1].abierta = true; host->puertas->v[1].punto = glm::vec3(0, 2, 1);
		ok(Esperar(resto, [&] { return c2->puertas->v[1].abierta; }), "puerta abierta replicada");
		auto c3 = Crear(hub, Role::Client, "aula", glm::vec3(0, -1, 1.7f), &fabricas);
		resto.push_back(c3.get());
		ok(Esperar(resto, [&] { return c3->puertas->v[1].abierta && !c3->puertas->v[0].abierta && c3->red->Objeto().conocido; }), "quien llega tarde recibe el estado de puertas y objeto");

		// el host cae: los clientes pasan a Error y vuelven a controlar sus puertas
		host->red->Detener();
		std::vector<Nodo*> clientes = { c2.get(), c3.get() };
		ok(Esperar(clientes, [&] { return c2->red->ObtenerDiagnostico().fase == NetworkPhase::Error && c3->red->ObtenerDiagnostico().fase == NetworkPhase::Error; }), "si cae el host el cliente lo detecta");
		ok(!c2->red->InterceptarPuerta(0) && c2->red->RemotosParaDibujar(Reloj()).empty(), "tras caer el host: control local y sin fantasmas");
		for (auto* n : { c2.get(), c3.get() }) n->red->Detener();
	}

	// ---------------- escena distinta: el host rechaza ----------------
	{
		auto hub = std::make_shared<Hub>();
		auto host = Crear(hub, Role::Host, "aula", glm::vec3(0, 0, 1.7f));
		auto c = Crear(hub, Role::Client, "otra_escena", glm::vec3(0, 0, 1.7f));
		std::vector<Nodo*> v = { host.get(), c.get() };
		ok(Esperar(v, [&] { return c->red->ObtenerDiagnostico().fase == NetworkPhase::Error; }), "escena distinta: el cliente queda en error");
		ok(c->red->ObtenerDiagnostico().mensaje.find("escena") != std::string::npos, "el mensaje explica el rechazo");
		host->red->Detener();
	}

	// ---------------- scene network fingerprint (ET2) ----------------
	{
		const auto stateWithIds = [](std::initializer_list<uint32_t> ids) {
			std::vector<SceneObjectState> state;
			for (uint32_t id : ids) { SceneObjectState object; object.id = id; state.push_back(object); }
			return state;
		};
		const auto state = stateWithIds({ 0x9ABCDEF0u, 0x12345678u, 0xCAFEBABEu, 0x01020304u });
		const std::string fingerprint = SceneNetworkFingerprint("aula", state);

		// Golden values produced by the pre-extraction function body (main.cpp at 9e13716).
		ok(fingerprint == "ET2-10187028", "fingerprint: golden value for unsorted multi-byte IDs");
		ok(SceneNetworkFingerprint("", {}) == "ET2-811C9DC5", "fingerprint: empty name and no IDs is the FNV-1a seed");

		ok(SceneNetworkFingerprint("aula", stateWithIds({ 0x01020304u, 0x12345678u, 0x9ABCDEF0u, 0xCAFEBABEu })) == fingerprint,
			"fingerprint: object order does not matter");
		ok(SceneNetworkFingerprint("aulb", state) != fingerprint, "fingerprint: scene name matters");
		ok(SceneNetworkFingerprint("aula", stateWithIds({ 0x9ABCDEF0u, 0x12345678u, 0xCAFEBABEu, 0x01020304u, 0x0BADF00Du })) != fingerprint,
			"fingerprint: adding an ID changes it");
		ok(SceneNetworkFingerprint("aula", stateWithIds({ 0x9ABCDEF0u, 0x12345678u, 0xCAFEBABEu })) != fingerprint,
			"fingerprint: removing an ID changes it");

		const std::string unnamed = SceneNetworkFingerprint("", state);
		ok(unnamed != fingerprint, "fingerprint: empty name differs from a named scene");
		for (const std::string& value : { fingerprint, unnamed })
			ok(value.size() == 12 && value.size() <= net::kMaxSceneIdLength && value.rfind("ET2-", 0) == 0 &&
				value.find_first_not_of("0123456789ABCDEF", 4) == std::string::npos,
				"fingerprint: ET2- prefix, 8 uppercase hex digits, fits the handshake");

		// Same inputs the game uses: file stem as name, captured architecture and objects (no animated).
		const std::filesystem::path scenePath =
			std::filesystem::path(ENTORNVGI_SOURCE_ASSETS_DIR) / "scenes" / "aula_informatica_calibrada.json";
		SceneData sceneData;
		std::string error;
		std::vector<SceneObjectState> sceneState;
		const bool captured = CargarDatosEscenaJSON(scenePath, sceneData, error) &&
			scene_network_state::CaptureSceneNetworkStates(sceneData.objetos, sceneState);
		ok(captured, "fingerprint: calibrated scene loads and captures");
		ok(captured && SceneNetworkFingerprint(scenePath.stem().string(), sceneState) == "ET2-876E0A88",
			"fingerprint: golden value for the calibrated scene");
	}

	// ---------------- throw charge arithmetic ----------------
	{
		using namespace game::interaction;
		ok(ThrowChargeFraction(0.0f) == 0.0f && cerca(ThrowChargeFraction(kMaxThrowChargeSeconds * 0.5f), 0.5f) &&
			ThrowChargeFraction(10.0f) == 1.0f && ThrowChargeFraction(-1.0f) == 0.0f,
			"throw charge: fraction is linear and clamped");
		ok(ThrowSpeed(0.0f) == 3.0f && ThrowSpeed(1.0f) == kRapidezMaximaLanzamiento,
			"throw charge: speed goes from 3 m/s to the host limit");
		ok(cerca(AdvanceThrowCharge(0.5f, 0.25f), 0.75f) && AdvanceThrowCharge(1.2f, 1.0f) == kMaxThrowChargeSeconds &&
			AdvanceThrowCharge(0.5f, -1.0f) == 0.5f, "throw charge: advance saturates and ignores negative delta");
		glm::vec3 velocity = glm::normalize(glm::vec3(0.3f, -0.7f, 0.2f)) * ThrowSpeed(1.0f);
		ok(LimitarRapidezLanzamientoHost(velocity), "throw charge: a full-charge throw is accepted by the host");
	}

	// ---------------- network session pure rules ----------------
	{
		using namespace netgame::session;
		net::PoseEntry pose;
		pose.head = ATransform(glm::vec3(0.0f, 0.0f, 1.7f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
		pose.rightHand = ATransform(glm::vec3(0.4f, 0.0f, 1.2f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
		const glm::vec3 ahead(0.0f, 1.0f, 0.0f);

		ok(RemoteGrabRayMatchesPose(glm::vec3(0.0f, 0.0f, 1.7f), ahead, pose), "session: ray from the peer head is accepted");
		ok(RemoteGrabRayMatchesPose(glm::vec3(0.4f, -0.84f, 1.2f), ahead, pose) &&
			!RemoteGrabRayMatchesPose(glm::vec3(0.4f, -0.86f, 1.2f), ahead, pose), "session: origin tolerance is 0.85 m");
		ok(RemoteGrabRayMatchesPose(glm::vec3(0.0f, 0.0f, 1.7f), glm::vec3(0.79f, 0.61f, 0.0f), pose) &&
			!RemoteGrabRayMatchesPose(glm::vec3(0.0f, 0.0f, 1.7f), glm::vec3(0.81f, 0.59f, 0.0f), pose),
			"session: alignment threshold is 0.60");
		ok(!RemoteGrabRayMatchesPose(glm::vec3(0.0f, 0.0f, 1.7f), glm::vec3(std::nanf("")), pose),
			"session: non-finite direction is rejected");

		ok(GrabAnchorsToHead(glm::vec3(0.0f, 0.0f, 1.7f), pose) && !GrabAnchorsToHead(glm::vec3(0.4f, 0.0f, 1.2f), pose),
			"session: grab anchors to the closest of head and hand");

		ok(ThrowChargeFromReleaseVelocity(glm::vec3(0.0f)) == 0.0f &&
			cerca(ThrowChargeFromReleaseVelocity(glm::vec3(0.0f, 9.0f, 0.0f)), 0.5f) &&
			ThrowChargeFromReleaseVelocity(glm::vec3(40.0f, 0.0f, 0.0f)) == 1.0f,
			"session: release velocity maps to a clamped throw charge");

		const glm::vec3 eye(1.0f, 2.0f, 1.7f);
		ok(cerca(SharedObjectSpawnPosition(eye, glm::vec3(3.0f, 0.0f, 5.0f)), glm::vec3(3.0f, 2.0f, 1.2f)),
			"session: shared object spawns 2 m ahead ignoring pitch");
		ok(cerca(SharedObjectSpawnPosition(eye, glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec3(1.0f, 4.0f, 1.2f)),
			"session: looking straight up falls back to +Y");

		ok(PhysicsSnapshotTick(1.5) == 1500u && PhysicsSnapshotTick(4294967.5) == 204u,
			"session: snapshot tick is in milliseconds and wraps");
		ok(PhysicsStateFlags(false, kSinDueno) == 0 && PhysicsStateFlags(true, kSinDueno) == 1 &&
			PhysicsStateFlags(false, net::kHostPeerId) == 2 && PhysicsStateFlags(true, net::kHostPeerId) == 3,
			"session: flags encode activity and ownership");
	}

	if (fallos == 0) std::cout << "NetJuegoTests OK\n";
	return fallos == 0 ? 0 : 1;
}
