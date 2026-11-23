// ENetTransport.h
// Unica unidad que incluye <enet/enet.h>. Solo se compila si ENTORNVGI_HAS_ENET esta definido
// (CMake: ENTORNVGI_ENABLE_NETWORK=ON y ENet encontrado).
#pragma once
#include "NetTransport.h"

#include <memory>
#include <string>

namespace net
{
	struct ENetOptions
	{
		bool isHost = false;
		std::string address = "127.0.0.1";  // cliente: host remoto
		uint16_t port = 7777;
		size_t maxClients = 4;
		uint32_t connectTimeoutMs = 5000;
	};
	std::unique_ptr<ITransport> CreateENetTransport(const ENetOptions& opt); // nullptr si no hay ENet
}
