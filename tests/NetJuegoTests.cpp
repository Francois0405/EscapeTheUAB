// NetJuegoTests.cpp
// Adaptador de red del juego (EntornVGI/NetJuego*.h): configuracion, poses PC/OpenXR (Z arriba), interpolacion,
// puertas con el host como autoridad, propiedad temporal del objeto compartido, offline sin ENet ni hilos.
// Usa un transporte en memoria: sin sockets, sin ENet, sin OpenGL, sin Bullet, sin visor.
#include "NetJuego.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

using namespace netjuego;

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
	class PuertasFalsas : public IPuertasEscena
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
		std::unique_ptr<RedJuego> red; std::unique_ptr<PuertasFalsas> puertas; PoseEntry pose;
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
		n->red = std::make_unique<RedJuego>([hub, llamadasFabrica](const ConfigRed& c) -> std::unique_ptr<ITransport>
		{
			if (llamadasFabrica) ++*llamadasFabrica;
			if (c.rol == Role::Host) return std::make_unique<HostEndRealista>(hub);
			return std::make_unique<ClientEnd>(hub);
		});
		ConfigRed cfg; cfg.rol = rol; cfg.nombre = rol == Role::Host ? "host" : "cliente";
		ok(n->red->Iniciar(cfg, escena, n->puertas.get(), glm::vec3(0, 1, 1)), "Iniciar acepta la configuracion");
		return n;
	}
}

int main()
{
	// ---------------- configuracion ----------------
	{
		ConfigRed c; std::string err;
		char a0[] = "x";
		char* argv0[] = { a0 };
		ok(ParsearArgumentos(1, argv0, c, err) && c.rol == Role::Offline, "sin argumentos = offline por defecto");
		char h[] = "--host", p[] = "--port", pv[] = "9000";
		char* argvH[] = { a0, h, p, pv };
		ok(ParsearArgumentos(4, argvH, c, err) && c.rol == Role::Host && c.puerto == 9000, "--host --port");
		char cn[] = "--connect", ep[] = "192.168.1.20:7001";
		char* argvC[] = { a0, cn, ep };
		ok(ParsearArgumentos(3, argvC, c, err) && c.rol == Role::Client && c.host == "192.168.1.20" && c.puerto == 7001, "--connect ip:puerto");
		char mal[] = "--connect", ep2[] = "host malo:99999";
		char* argvM[] = { a0, mal, ep2 };
		ok(!ParsearArgumentos(3, argvM, c, err), "endpoint invalido rechazado");
		char pm[] = "70000"; char* argvP[] = { a0, h, p, pm };
		ok(!ParsearArgumentos(4, argvP, c, err), "puerto fuera de rango rechazado");
		std::string host; uint16_t puerto = 1;
		ok(ParsearEndpoint("10.0.0.5", host, puerto, err) && puerto == 1 && host == "10.0.0.5", "endpoint sin puerto conserva el actual");
		ok(!ParsearEndpoint("", host, puerto, err) && !ParsearEndpoint("a:0", host, puerto, err) && !ParsearEndpoint("a:x", host, puerto, err), "endpoint vacio/puerto 0/no numerico");
	}

	// ---------------- offline: nada se crea ----------------
	{
		int llamadas = 0;
		RedJuego red([&llamadas](const ConfigRed&) -> std::unique_ptr<ITransport> { ++llamadas; return nullptr; });
		auto pu = NuevasPuertas();
		ok(red.Iniciar(ConfigRed{}, "escena", pu.get(), glm::vec3(0)), "offline: Iniciar devuelve true");
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
		RedJuego red([](const ConfigRed&) -> std::unique_ptr<ITransport> { return nullptr; });
		auto pu = NuevasPuertas(); ConfigRed c; c.rol = Role::Host;
		ok(red.Iniciar(c, "escena", pu.get(), glm::vec3(0)), "sin transporte: arranca asincrono");
		PoseEntry p = PoseEn(glm::vec3(0));
		bool error = false; const double fin = Reloj() + 3;
		while (Reloj() < fin && !error) { red.Actualizar(Reloj(), &p); error = red.ObtenerDiagnostico().fase == FaseRed::Error; std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
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
		vr::Anclaje anc; anc.origen = glm::vec3(5, 6, 0); anc.adelante = glm::vec3(0, 1, 0);
		vr::PoseXR cab; cab.posicion = glm::vec3(0, 1.6f, 0);   // XR: Y arriba
		net::PoseEntry vrp;
		ok(PoseDesdeVR(cab, nullptr, nullptr, anc, vrp), "pose VR sin mandos");
		ok(cerca(PosDe(vrp.head), glm::vec3(5, 6, 1.6f)), "pose VR: Y de XR -> Z del motor");
		ok(cerca(RotDe(vrp.head) * glm::vec3(0, 1, 0), glm::vec3(0, 1, 0)), "pose VR: -Z de XR = adelante");
		vr::PoseXR manoD; manoD.posicion = glm::vec3(0.3f, 1.2f, -0.4f);   // 30 cm a la derecha, 40 cm delante
		PoseDesdeVR(cab, nullptr, &manoD, anc, vrp);
		ok(cerca(PosDe(vrp.rightHand), glm::vec3(5.3f, 6.4f, 1.2f)), "pose VR: mano derecha real (no el offset de PC)");
		anc.adelante = glm::vec3(1, 0, 0);   // el anclaje gira: adelante de XR = +X del motor
		PoseDesdeVR(cab, nullptr, &manoD, anc, vrp);
		ok(cerca(RotDe(vrp.head) * glm::vec3(0, 1, 0), glm::vec3(1, 0, 0)), "pose VR: anclaje girado");
		ok(cerca(PosDe(vrp.rightHand), glm::vec3(5.4f, 5.7f, 1.2f)), "pose VR: mano rotada con el anclaje");
		// la conversion no modifica sus entradas (pose local intacta)
		const vr::PoseXR copia = cab; PoseDesdeVR(cab, nullptr, &manoD, anc, vrp);
		ok(cab.posicion == copia.posicion && cab.orientacion == copia.orientacion, "pose VR: la pose local no se altera");
		vr::PoseXR rara; rara.posicion = glm::vec3(NAN, 0, 0);
		ok(!PoseDesdeVR(rara, nullptr, nullptr, anc, vrp), "pose VR: NaN rechazado");
	}

	// ---------------- interpolacion ----------------
	{
		InterpoladorRemotos ip;
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
		InterpoladorRemotos ir; ir.Actualizar(r, 1, 0.0);
		r[3] = PoseEn(glm::vec3(0), glm::vec3(-1, 0, 0)); r[3].peer = 3;   // gira 90 grados
		ir.Actualizar(r, 1, 0.05);
		ir.Muestrear(3, kRetardoInterpolacion + 0.025, out);
		const glm::vec3 f = RotDe(out.head) * glm::vec3(0, 1, 0);
		ok(cerca(glm::length(f), 1.0f) && cerca(glm::degrees(std::acos(glm::dot(f, glm::vec3(0, 1, 0)))), 45.0f, 1.0f), "slerp: 45 grados a mitad de camino");
		std::map<PeerId, PoseEntry> mala; mala[4] = PoseEn(glm::vec3(0)); mala[4].head.pos[0] = NAN;
		InterpoladorRemotos im; im.Actualizar(mala, 1, 0.0);
		ok(im.Cantidad() == 0, "snapshot con NaN descartado");
		im.Actualizar({}, 1, 1.0);
		s.clear(); ip.Actualizar(s, 2, 11.0);
		ok(ip.Cantidad() == 0, "peers ausentes del snapshot se descartan");
	}

	// ---------------- puertas: autoridad del host ----------------
	{
		auto pu = NuevasPuertas(); SincroPuertas sp; std::string err;
		ok(sp.Reconstruir(*pu, err) && sp.Cantidad() == 2, "tabla de puertas por id de escena");
		const uint32_t idE = HashId("puerta_entrada"), idL = HashId("puerta_lab");
		const glm::vec3 cerca1(0, 0, 1.7f), lejos(100, 100, 1.7f);
		ok(sp.HostSolicitud(*pu, idE, &cerca1) == ResultadoSolicitud::Aplicada && pu->v[0].abierta, "host abre la puerta pedida");
		ok(sp.HostSolicitud(*pu, idE, &lejos) == ResultadoSolicitud::FueraDeAlcance && pu->v[0].abierta, "fuera de alcance: rechazada");
		ok(sp.HostSolicitud(*pu, idE, nullptr) == ResultadoSolicitud::SinPose, "sin pose: rechazada");
		ok(sp.HostSolicitud(*pu, 0xDEADBEEF, &cerca1) == ResultadoSolicitud::IdDesconocido, "id desconocido rechazado");
		ok(sp.HostSolicitud(*pu, idL, &cerca1) == ResultadoSolicitud::FueraDeAlcance && !pu->v[1].abierta, "otra puerta lejana: rechazada");
		std::vector<EventoSalida> ev; sp.HostDetectarCambios(*pu, ev);
		ok(ev.size() == 1 && ev[0].tipo == kEvPuertaEstado && ev[0].objetivo == idE && ev[0].payload == std::vector<uint8_t>{ 1 }, "cambio difundido una vez");
		ev.clear(); sp.HostDetectarCambios(*pu, ev);
		ok(ev.empty(), "sin cambios no hay eventos");
		pu->v[1].abierta = true; sp.HostDetectarCambios(*pu, ev);   // la abre el propio host (E o panel de depuracion)
		ok(ev.size() == 1 && ev[0].objetivo == idL, "cambio local del host tambien se difunde");

		auto cl = NuevasPuertas(); SincroPuertas sc; sc.Reconstruir(*cl, err);
		ok(sc.AplicarEstado(*cl, idE, { 1 }) && cl->v[0].abierta, "cliente aplica el estado del host");
		ok(sc.AplicarEstado(*cl, idE, { 1 }) && cl->v[0].abierta, "idempotente");
		ok(!sc.AplicarEstado(*cl, idE, { 2 }) && !sc.AplicarEstado(*cl, idE, {}) && !sc.AplicarEstado(*cl, 1234, { 1 }), "estados invalidos ignorados");
		std::vector<StateEntry> rec; sp.EstadoRecuperacion(*pu, rec);
		auto tarde = NuevasPuertas(); SincroPuertas st; st.Reconstruir(*tarde, err); st.AplicarRecuperacion(*tarde, rec);
		ok(tarde->v[0].abierta && tarde->v[1].abierta, "recuperacion para quien llega tarde");

		PuertasFalsas dup; dup.v.push_back({ "a", false, {} }); dup.v.push_back({ "a", false, {} }); dup.v.push_back({ "b", false, {} });
		SincroPuertas sd;
		ok(!sd.Reconstruir(dup, err) && sd.Cantidad() == 1 && !err.empty(), "ids duplicados: excluidos de la red con aviso");
	}

	// ---------------- propiedad temporal ----------------
	{
		ArbitroPropiedad a; a.Definir(kObjetoPrueba, glm::vec3(0, 2, 1)); a.TomarCambios();
		const glm::vec3 cerca1(0, 0, 1.7f), lejos(50, 50, 1.7f);
		ok(a.Solicitar(kObjetoPrueba, 1, &lejos, 0) == ResultadoSolicitud::FueraDeAlcance && a.Buscar(kObjetoPrueba)->dueno == kSinDueno, "fuera de alcance no concede");
		ok(a.Solicitar(kObjetoPrueba, 1, nullptr, 0) == ResultadoSolicitud::SinPose, "sin pose no concede");
		ok(a.Solicitar(kObjetoPrueba, 1, &cerca1, 0) == ResultadoSolicitud::Aplicada && a.Buscar(kObjetoPrueba)->dueno == 1, "concede al primero");
		ok(a.Solicitar(kObjetoPrueba, 2, &cerca1, 1) == ResultadoSolicitud::Ocupado, "segundo solicitante: ocupado");
		ok(a.Mover(kObjetoPrueba, 2, glm::vec3(0, 1, 1), &cerca1, 1) == ResultadoSolicitud::NoEsDueno, "solo el dueno mueve");
		ok(a.Mover(kObjetoPrueba, 1, glm::vec3(0, 1.5f, 1.5f), &cerca1, 1) == ResultadoSolicitud::Aplicada && cerca(a.Buscar(kObjetoPrueba)->pos, glm::vec3(0, 1.5f, 1.5f)), "dueno mueve");
		ok(a.Mover(kObjetoPrueba, 1, glm::vec3(NAN, 0, 0), &cerca1, 1) == ResultadoSolicitud::Invalida, "posicion NaN rechazada");
		ok(a.Mover(kObjetoPrueba, 1, glm::vec3(0, 40, 1), &cerca1, 1) == ResultadoSolicitud::FueraDeAlcance, "teletransporte rechazado");
		ok(a.Liberar(kObjetoPrueba, 2) == ResultadoSolicitud::NoEsDueno, "otro no puede soltar");
		a.TomarCambios();
		a.Tick(5.0);
		ok(a.Buscar(kObjetoPrueba)->dueno == 1, "la concesion sigue viva a mitad de plazo");
		a.Tick(1.0 + ArbitroPropiedad::kConcesion + 0.1);
		ok(a.Buscar(kObjetoPrueba)->dueno == kSinDueno && a.TomarCambios().size() == 1, "la concesion caduca y se difunde");
		a.Solicitar(kObjetoPrueba, 3, &cerca1, 20.0); a.TomarCambios();
		a.PeerDesconectado(3);
		ok(a.Buscar(kObjetoPrueba)->dueno == kSinDueno && a.TomarCambios().size() == 1, "desconexion libera la propiedad");
		ok(a.Solicitar(kObjetoPrueba, 4, &cerca1, 30.0) == ResultadoSolicitud::Aplicada, "queda disponible para otro");
		a.Liberar(kObjetoPrueba, 4);
		ok(a.Buscar(kObjetoPrueba)->dueno == kSinDueno, "soltar libera");

		VistaObjetoCompartido v; std::vector<StateEntry> rec; a.Solicitar(kObjetoPrueba, 2, &cerca1, 40.0); a.EstadoRecuperacion(rec);
		ok(v.AplicarRecuperacion(rec) && v.conocido && v.dueno == 2 && cerca(v.pos, a.Buscar(kObjetoPrueba)->pos, 0.002f), "recuperacion del objeto (dueno y posicion)");
		ok(!v.AplicarRecuperacion({}), "recuperacion incompleta ignorada");
		VistaObjetoCompartido w;
		ok(w.AplicarEstado(CodificarEstadoObjeto(1, glm::vec3(1, 2, 3))) && w.dueno == 1 && cerca(w.pos, glm::vec3(1, 2, 3)), "estado del objeto ida y vuelta");
		ok(!w.AplicarEstado({ 1, 2, 3 }), "payload corto rechazado");
		auto mal = CodificarEstadoObjeto(1, glm::vec3(1, 2, 3)); mal[2] = mal[3] = mal[4] = 0xFF; mal[5] = 0x7F;   // NaN/inf
		VistaObjetoCompartido x; ok(!x.AplicarEstado(mal) && !x.conocido, "payload con NaN rechazado");
		ok(RayoTocaEsfera(glm::vec3(0), glm::vec3(0, 1, 0), glm::vec3(0, 2, 0.2f), 0.35f, 3.0f) && !RayoTocaEsfera(glm::vec3(0), glm::vec3(0, 1, 0), glm::vec3(0, 2, 1.0f), 0.35f, 3.0f), "rayo contra el objeto");
	}

	// ---------------- integracion: host + 2 clientes (transporte en memoria) ----------------
	{
		auto hub = std::make_shared<Hub>();
		int fabricas = 0;
		auto host = Crear(hub, Role::Host, "aula", glm::vec3(0, 0, 1.7f), &fabricas);
		auto c1 = Crear(hub, Role::Client, "aula", glm::vec3(0, -0.5f, 1.7f), &fabricas);
		auto c2 = Crear(hub, Role::Client, "aula", glm::vec3(1, -0.5f, 1.7f), &fabricas);
		std::vector<Nodo*> todos = { host.get(), c1.get(), c2.get() };
		ok(Esperar(todos, [&] { return host->red->Activa() && c1->red->Activa() && c2->red->Activa() && host->red->ObtenerDiagnostico().remotos == 2 && c1->red->ObtenerDiagnostico().remotos >= 1; }), "los tres se conectan y se ven");
		ok(fabricas == 3, "una fabrica de transporte por participante, solo al iniciar");
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
		ok(Esperar(clientes, [&] { return c2->red->ObtenerDiagnostico().fase == FaseRed::Error && c3->red->ObtenerDiagnostico().fase == FaseRed::Error; }), "si cae el host el cliente lo detecta");
		ok(!c2->red->InterceptarPuerta(0) && c2->red->RemotosParaDibujar(Reloj()).empty(), "tras caer el host: control local y sin fantasmas");
		for (auto* n : { c2.get(), c3.get() }) n->red->Detener();
	}

	// ---------------- escena distinta: el host rechaza ----------------
	{
		auto hub = std::make_shared<Hub>();
		auto host = Crear(hub, Role::Host, "aula", glm::vec3(0, 0, 1.7f));
		auto c = Crear(hub, Role::Client, "otra_escena", glm::vec3(0, 0, 1.7f));
		std::vector<Nodo*> v = { host.get(), c.get() };
		ok(Esperar(v, [&] { return c->red->ObtenerDiagnostico().fase == FaseRed::Error; }), "escena distinta: el cliente queda en error");
		ok(c->red->ObtenerDiagnostico().mensaje.find("escena") != std::string::npos, "el mensaje explica el rechazo");
		host->red->Detener();
	}

	if (fallos == 0) std::cout << "NetJuegoTests OK\n";
	return fallos == 0 ? 0 : 1;
}
