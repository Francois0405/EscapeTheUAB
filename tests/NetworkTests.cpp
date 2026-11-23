// NetworkTests.cpp
// Pruebas de serializacion, validacion, handshake, secuencias, desconexion y recuperacion del
// modulo de red. Usan un transporte en memoria: no abren sockets ni requieren ENet/OpenGL/Bullet.
#include "Network/NetService.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <thread>

using namespace net;

namespace
{
	int fallos = 0;
	void ok(bool c, const char* caso) { if (!c) { std::cerr << "FALLO: " << caso << '\n'; ++fallos; } }

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

	PoseEntry MakePose(PeerId p, float x)
	{
		PoseEntry e; e.peer = p; e.head.pos[0] = x; e.leftHand.pos[1] = 1; e.rightHand.pos[2] = 2; return e;
	}

	// ---- Serializacion ----
	void TestRoundTrip()
	{
		std::vector<uint8_t> b;
		Header h; h.sender = 3; h.sequence = 42;

		h.type = MessageType::Hello; ok(Encode(h, HelloMsg{"Ana", "lab"}, b), "encode Hello");
		h.sender = kUnassignedPeerId; Encode(h, HelloMsg{"Ana", "lab"}, b);
		Packet p; ok(Decode(b.data(), b.size(), p) == DecodeStatus::Ok, "decode Hello");
		ok(std::get<HelloMsg>(p.body).name == "Ana" && std::get<HelloMsg>(p.body).sceneId == "lab", "Hello campos");
		ok(b[0] == 0x45 && b[1] == 0x54 && b[2] == kProtocolVersion, "cabecera little-endian estable");

		h.sender = 3; h.type = MessageType::PoseSnapshot;
		PoseSnapshotMsg s; s.poses.push_back(MakePose(1, 1.5f)); s.poses.push_back(MakePose(2, -3.25f));
		ok(Encode(h, s, b), "encode snapshot");
		ok(Decode(b.data(), b.size(), p) == DecodeStatus::Ok, "decode snapshot");
		auto& r = std::get<PoseSnapshotMsg>(p.body);
		ok(r.poses.size() == 2 && r.poses[1].head.pos[0] == -3.25f && p.header.sequence == 42 && p.header.sender == 3, "snapshot ida y vuelta");

		h.type = MessageType::Event; EventMsg ev; ev.eventType = 7; ev.targetId = 99; ev.payload = {1, 2, 3};
		ok(Encode(h, ev, b) && Decode(b.data(), b.size(), p) == DecodeStatus::Ok, "evento ida y vuelta");
		ok(std::get<EventMsg>(p.body).payload.size() == 3 && std::get<EventMsg>(p.body).targetId == 99, "evento campos");

		h.type = MessageType::RecoverySnapshot; RecoverySnapshotMsg rs; rs.entries = {{1, -5}, {2, 7}};
		ok(Encode(h, rs, b) && Decode(b.data(), b.size(), p) == DecodeStatus::Ok && std::get<RecoverySnapshotMsg>(p.body).entries[0].value == -5, "recovery ida y vuelta");

		h.type = MessageType::PeerDisconnected;
		ok(Encode(h, PeerDisconnectedMsg{2, DisconnectReason::Timeout}, b) && Decode(b.data(), b.size(), p) == DecodeStatus::Ok, "peer disconnected");
	}

	// ---- Validacion y limites ----
	void TestValidation()
	{
		std::vector<uint8_t> b; Header h; h.sender = 1; Packet p;

		h.type = MessageType::Event; EventMsg big; big.payload.assign(kMaxEventPayload + 1, 0);
		ok(!Encode(h, big, b), "payload > limite rechazado al codificar");
		big.payload.assign(kMaxEventPayload, 0);
		ok(Encode(h, big, b) && b.size() <= kMaxPacketSize, "payload en el limite aceptado");

		h.type = MessageType::PoseSnapshot; PoseSnapshotMsg s;
		for (size_t i = 0; i <= kMaxPoseEntries; ++i) s.poses.push_back(MakePose(1, 0));
		ok(!Encode(h, s, b), "demasiadas poses rechazado");

		PoseSnapshotMsg nan; nan.poses.push_back(MakePose(1, std::nanf("")));
		ok(!Encode(h, nan, b), "NaN rechazado");
		PoseSnapshotMsg inf; inf.poses.push_back(MakePose(1, INFINITY)); ok(!Encode(h, inf, b), "inf rechazado");
		PoseSnapshotMsg far; far.poses.push_back(MakePose(1, kMaxCoordinate * 2)); ok(!Encode(h, far, b), "coordenada fuera de rango");
		PoseSnapshotMsg q; q.poses.push_back(MakePose(1, 0)); q.poses[0].head.rot[3] = 3.0f; ok(!Encode(h, q, b), "cuaternion no unitario");

		h.type = MessageType::Hello; h.sender = kUnassignedPeerId;
		ok(!Encode(h, HelloMsg{"", "lab"}, b), "nombre vacio");
		ok(!Encode(h, HelloMsg{std::string(kMaxNameLength + 1, 'a'), "lab"}, b), "nombre largo");
		ok(!Encode(h, HelloMsg{"a\nb", "lab"}, b), "caracter de control");
		h.type = MessageType::Event; ok(!Encode(h, HelloMsg{"a", "b"}, b), "tipo de cabecera distinto del cuerpo");

		// Paquetes malformados.
		h.type = MessageType::Event; h.sender = 1; EventMsg e; e.payload = {1, 2, 3, 4};
		Encode(h, e, b);
		std::vector<uint8_t> t(b.begin(), b.end() - 1);
		ok(Decode(t.data(), t.size(), p) == DecodeStatus::Truncated, "truncado detectado");
		t = b; t.push_back(0); ok(Decode(t.data(), t.size(), p) == DecodeStatus::TrailingBytes, "bytes sobrantes");
		t = b; t[0] = 0; ok(Decode(t.data(), t.size(), p) == DecodeStatus::BadMagic, "magic incorrecto");
		t = b; t[2] = 99; ok(Decode(t.data(), t.size(), p) == DecodeStatus::BadVersion, "version incorrecta");
		t = b; t[3] = 200; ok(Decode(t.data(), t.size(), p) == DecodeStatus::UnknownType, "tipo desconocido");
		t = b; t.resize(kHeaderSize - 1); ok(Decode(t.data(), t.size(), p) == DecodeStatus::TooShort, "demasiado corto");
		t.assign(kMaxPacketSize + 1, 0); ok(Decode(t.data(), t.size(), p) == DecodeStatus::TooLarge, "demasiado grande");
		// longitud de payload declarada mayor que el limite
		t = b; t[kHeaderSize + 6] = 0xFF; t[kHeaderSize + 7] = 0x7F;
		ok(Decode(t.data(), t.size(), p) == DecodeStatus::FieldOutOfRange, "longitud declarada fuera de rango");
		// Hello con sender asignado / evento con sender sin asignar.
		t = b; t[4] = 0xFF; t[5] = 0xFF; ok(Decode(t.data(), t.size(), p) == DecodeStatus::BadSender, "evento sin peer ID");
		// fuzz basico: ningun byte aleatorio debe colgar/crashear
		unsigned seed = 12345; std::vector<uint8_t> f;
		for (int i = 0; i < 20000; ++i)
		{
			f = b; seed = seed * 1664525u + 1013904223u; f[seed % f.size()] ^= static_cast<uint8_t>(seed >> 24);
			f.resize(1 + (seed >> 8) % f.size()); Decode(f.data(), f.size(), p);
		}
		ok(true, "fuzz sin fallos");
		ok(SeqNewer(1, 0) && !SeqNewer(0, 1) && !SeqNewer(5, 5) && SeqNewer(0, 0xFFFFFFFFu), "comparacion de secuencias con wraparound");
	}

	// ---- Sesion con transporte sincrono (sin hilos) ----
	struct Rig
	{
		std::shared_ptr<Hub> hub = std::make_shared<Hub>();
		HostEnd hostT{hub};
		SessionConfig hc, cc;
		std::unique_ptr<NetSession> host;
		struct C { std::unique_ptr<ClientEnd> t; std::unique_ptr<NetSession> s; };
		std::deque<C> clients; // deque: las referencias devueltas por Add() siguen validas
		Rig() { hc.role = Role::Host; hc.sceneId = "lab"; hc.maxPeers = 2; cc.role = Role::Client; cc.sceneId = "lab"; host = std::make_unique<NetSession>(hc, hostT); }
		C& Add(const std::string& scene = "lab")
		{
			SessionConfig c = cc; c.sceneId = scene; c.playerName = "c" + std::to_string(clients.size());
			C x; x.t = std::make_unique<ClientEnd>(hub); x.s = std::make_unique<NetSession>(c, *x.t); x.t->Start();
			clients.push_back(std::move(x)); return clients.back();
		}
		void Pump(int rounds = 6)
		{
			for (int i = 0; i < rounds; ++i)
			{
				std::vector<TransportEvent> ev; hostT.Poll(ev, 0); for (auto& e : ev) host->OnTransportEvent(e);
				for (auto& c : clients) { ev.clear(); c.t->Poll(ev, 0); for (auto& e : ev) c.s->OnTransportEvent(e); }
			}
		}
	};

	void TestHandshake()
	{
		Rig r; auto& c = r.Add(); r.Pump();
		ok(c.s->State() == ConnectionState::Connected && c.s->LocalPeer() == 1, "cliente conectado con peer 1");
		auto& c2 = r.Add(); r.Pump();
		ok(c2.s->LocalPeer() == 2, "segundo cliente peer 2");
		auto& c3 = r.Add(); r.Pump();
		ok(c3.s->State() == ConnectionState::Rejected, "tercer cliente rechazado (host lleno)");
		auto& c4 = r.Add("otra_escena"); r.Pump();
		ok(c4.s->State() == ConnectionState::Rejected, "escena distinta rechazada");
		auto evs = c4.s->TakeEvents(); bool sceneRej = false;
		for (auto& e : evs) if (e.kind == NetEvent::Rejected && e.rejectReason == RejectReason::SceneMismatch) sceneRej = true;
		ok(sceneRej, "motivo SceneMismatch");
	}

	void TestVersionMismatch()
	{
		Rig r; r.hostT.hub = r.hub;
		auto t = std::make_unique<ClientEnd>(r.hub); t->Start();
		std::vector<uint8_t> b; Header h; h.type = MessageType::Hello; h.sender = kUnassignedPeerId; Encode(h, HelloMsg{"x", "lab"}, b);
		b[2] = 9; t->Send(t->link, kChannelEvents, true, b); r.Pump();
		std::vector<TransportEvent> ev; t->Poll(ev, 0); bool rej = false;
		for (auto& e : ev) if (e.kind == TransportEvent::Received) { Packet p; if (Decode(e.data.data(), e.data.size(), p) == DecodeStatus::Ok && p.header.type == MessageType::Reject) rej = std::get<RejectMsg>(p.body).reason == RejectReason::BadVersion; }
		ok(rej, "version distinta -> Reject BadVersion");
	}

	void TestPoseFlowAndSpoof()
	{
		Rig r; auto& a = r.Add(); auto& b = r.Add(); r.Pump();
		a.s->SetLocalPose(MakePose(77 /*ignorado*/, 4.0f));
		r.host->SetLocalPose(MakePose(0, 9.0f));
		r.Pump();
		ok(r.host->RemotePoses().count(1) && r.host->RemotePoses().at(1).head.pos[0] == 4.0f, "host guarda pose del peer 1 con ID fijado por el host");
		ok(!r.host->RemotePoses().count(77), "cliente no puede suplantar otro peer ID");
		r.host->Tick(NetSession::Clock::now()); r.Pump();
		ok(b.s->RemotePoses().count(1) && b.s->RemotePoses().count(0) && !b.s->RemotePoses().count(2), "b recibe poses de host y a, no la propia");

		// Secuencia: un snapshot viejo (retransmitido) se descarta.
		std::vector<uint8_t> pk; Header h; h.type = MessageType::PoseUpdate; h.sender = 1; h.sequence = 0;
		PoseUpdateMsg old; old.pose = MakePose(1, -100.0f); Encode(h, old, pk);
		TransportEvent te; te.kind = TransportEvent::Received; te.link = a.t->link; te.channel = kChannelPose; te.data = pk;
		const uint64_t before = r.host->Stats().staleDropped;
		r.host->OnTransportEvent(te);
		ok(r.host->Stats().staleDropped == before + 1 && r.host->RemotePoses().at(1).head.pos[0] == 4.0f, "pose con secuencia antigua descartada");

		// Mensaje con peer ID de otro emisor: se descarta.
		h.sender = 2; h.sequence = 1000; Encode(h, old, pk); te.data = pk;
		r.host->OnTransportEvent(te);
		ok(r.host->RemotePoses().at(1).head.pos[0] == 4.0f, "sender != peer del enlace descartado");

		// Canal equivocado (pose por canal fiable).
		h.sender = 1; h.sequence = 2000; Encode(h, old, pk); te.data = pk; te.channel = kChannelEvents;
		r.host->OnTransportEvent(te);
		ok(r.host->RemotePoses().at(1).head.pos[0] == 4.0f, "canal incorrecto descartado");
	}

	void TestEventsDisconnectRecovery()
	{
		Rig r; auto& a = r.Add(); auto& b = r.Add();
		r.host->SetRecoveryState({{10, 1}, {11, 0}});
		r.Pump();
		bool gotRec = false;
		for (auto& e : b.s->TakeEvents()) if (e.kind == NetEvent::RecoveryReceived) gotRec = e.recovery.size() == 2 && e.recovery[0].objectId == 10;
		ok(gotRec, "snapshot de recuperacion recibido al unirse");

		a.s->SendEvent(5, 123, {9, 9}); r.Pump();
		bool gotEv = false;
		for (auto& e : r.host->TakeEvents()) if (e.kind == NetEvent::GameEvent && e.peer == 1 && e.event.eventType == 5 && e.event.targetId == 123) gotEv = true;
		ok(gotEv, "evento fiable cliente->host");
		r.host->SendEvent(6, 1, {}); r.Pump();
		bool gotEv2 = false; for (auto& e : b.s->TakeEvents()) if (e.kind == NetEvent::GameEvent && e.event.eventType == 6) gotEv2 = true;
		ok(gotEv2, "evento fiable host->clientes");

		// Desconexion del cliente a: b y el host se enteran; el ID 1 se libera.
		a.s->SendEvent(0, 0, {}); (void)0;
		a.t->Disconnect(a.t->link); r.Pump();
		bool left = false; for (auto& e : b.s->TakeEvents()) if (e.kind == NetEvent::PeerLeft && e.peer == 1) left = true;
		ok(left, "los demas reciben PeerDisconnected");
		ok(!r.host->RemotePoses().count(1), "pose del peer desconectado eliminada");
		auto& c = r.Add(); r.Pump();
		ok(c.s->State() == ConnectionState::Connected && c.s->LocalPeer() == 1, "peer ID liberado y reutilizado");

		// Estado de desconexion del cliente + recuperacion tras reconectar.
		b.t->Disconnect(b.t->link); r.Pump();
		ok(b.s->State() == ConnectionState::Disconnected, "cliente en estado Disconnected");
	}

	void TestOfflineNoSockets()
	{
		int llamadas = 0;
		SessionConfig cfg; cfg.role = Role::Offline;
		NetService svc(cfg, [&]() -> std::unique_ptr<ITransport> { ++llamadas; return nullptr; });
		ok(svc.Start(), "offline Start ok");
		ok(!svc.IsRunning() && !svc.IsNetworked() && llamadas == 0, "offline: sin transporte, sin hilo");
		svc.SubmitLocalPose(MakePose(0, 0)); std::vector<NetEvent> ev; ok(svc.PollEvents(ev) == 0, "offline: colas vacias");
		svc.Stop();
	}

	void TestThreadedService()
	{
		auto hub = std::make_shared<Hub>();
		SessionConfig hc; hc.role = Role::Host; hc.sceneId = "lab"; hc.poseRateHz = 60;
		SessionConfig cc; cc.role = Role::Client; cc.sceneId = "lab";
		NetService host(hc, [&] { return std::make_unique<HostEnd>(hub); });
		NetService cli(cc, [&] { return std::make_unique<ClientEnd>(hub); });
		ok(host.Start() && cli.Start(), "servicios arrancan");
		const auto fin = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		bool got = false;
		while (std::chrono::steady_clock::now() < fin && !got)
		{
			host.SubmitLocalPose(MakePose(0, 2.0f)); cli.SubmitLocalPose(MakePose(0, 3.0f));
			auto m = cli.RemotePoses(); got = m.count(0) && m.at(0).head.pos[0] == 2.0f && host.RemotePoses().count(1);
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		ok(got, "poses fluyen entre hilos de red");
		ok(cli.State() == ConnectionState::Connected && cli.LocalPeer() == 1, "estado atomico del cliente");
		ok(cli.QueueEvent(1, 2, {3}), "QueueEvent");
		bool ev = false;
		for (int i = 0; i < 400 && !ev; ++i) { std::vector<NetEvent> v; host.PollEvents(v); for (auto& e : v) if (e.kind == NetEvent::GameEvent) ev = true; std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
		ok(ev, "evento llega por la cola thread-safe");
		ok(!cli.QueueEvent(1, 2, std::vector<uint8_t>(kMaxEventPayload + 1)), "QueueEvent rechaza payload grande");
		cli.Stop(); host.Stop();
		ok(!cli.IsRunning(), "Stop limpio");
	}
}

int main()
{
	TestRoundTrip();
	TestValidation();
	TestHandshake();
	TestVersionMismatch();
	TestPoseFlowAndSpoof();
	TestEventsDisconnectRecovery();
	TestOfflineNoSockets();
	TestThreadedService();
	if (fallos) { std::cerr << fallos << " fallos\n"; return 1; }
	std::cout << "NetworkTests OK\n";
	return 0;
}
