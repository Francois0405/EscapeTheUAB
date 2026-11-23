// ENetLoopbackTests.cpp
// Prueba con ENet REAL por 127.0.0.1: host + cliente, handshake, pose y evento fiable, y un
// cliente apuntando a un puerto sin host (debe acabar en Disconnected, sin colgarse).
// Solo se compila si CMake encontro ENet (ver tests/CMakeLists.txt).
#include "Network/ENetTransport.h"
#include "Network/NetService.h"

#include <chrono>
#include <iostream>
#include <thread>

using namespace net;
using namespace std::chrono;

namespace
{
	int fallos = 0;
	void ok(bool c, const char* caso) { if (!c) { std::cerr << "FALLO: " << caso << '\n'; ++fallos; } }

	template <class F> bool Esperar(F f, int ms)
	{
		const auto fin = steady_clock::now() + milliseconds(ms);
		while (steady_clock::now() < fin) { if (f()) return true; std::this_thread::sleep_for(milliseconds(10)); }
		return f();
	}

	NetService::TransportFactory Fabrica(bool host, uint16_t puerto)
	{
		return [=]() { ENetOptions o; o.isHost = host; o.port = puerto; o.address = "127.0.0.1"; o.connectTimeoutMs = 1500; return CreateENetTransport(o); };
	}
}

int main()
{
	if (!CreateENetTransport(ENetOptions{})) { std::cerr << "ENet no incluido: prueba omitida.\n"; return 0; }
	const uint16_t puerto = 47891;

	SessionConfig ch; ch.role = Role::Host; ch.playerName = "host"; ch.sceneId = "demo";
	SessionConfig cc; cc.role = Role::Client; cc.playerName = "cli"; cc.sceneId = "demo";
	{
		NetService host(ch, Fabrica(true, puerto));
		ok(host.Start(), "host arranca");
		NetService cli(cc, Fabrica(false, puerto));
		ok(cli.Start(), "cliente arranca");
		ok(Esperar([&] { return cli.State() == ConnectionState::Connected; }, 5000), "cliente conecta por ENet");
		ok(cli.LocalPeer() != kUnassignedPeerId && cli.LocalPeer() != kHostPeerId, "cliente recibe peer ID");

		PoseEntry p; p.peer = cli.LocalPeer(); p.head.pos[0] = 1.5f;
		ok(Esperar([&] { cli.SubmitLocalPose(p); return host.RemotePoses().count(cli.LocalPeer()) != 0; }, 3000), "el host recibe la pose del cliente");

		cli.QueueEvent(0x0100, 42, {1});
		std::vector<NetEvent> evs; bool visto = false;
		Esperar([&] { host.PollEvents(evs); for (auto& e : evs) if (e.kind == NetEvent::GameEvent && e.event.targetId == 42) visto = true; return visto; }, 3000);
		ok(visto, "el host recibe el evento fiable del cliente");
		cli.Stop(); host.Stop();
	}
	{	// Sin host: no debe colgarse; termina Disconnected (y Stop() vuelve rapido).
		NetService cli(cc, Fabrica(false, puerto + 1));
		cli.Start();
		ok(Esperar([&] { return cli.State() == ConnectionState::Disconnected; }, 8000), "sin host: Disconnected en vez de colgarse");
		const auto t0 = steady_clock::now(); cli.Stop();
		ok(steady_clock::now() - t0 < seconds(3), "Stop() no se bloquea");
	}
	if (fallos == 0) std::cout << "ENetLoopbackTests OK\n";
	return fallos == 0 ? 1 - 1 : 1;
}
