// NetTransport.h
// Interfaz de transporte. Solo la usa el hilo de red; ENet queda encapsulado en ENetTransport.
#pragma once
#include <cstdint>
#include <vector>

namespace net
{
	using LinkId = uint32_t; // identificador local de conexion (no es el PeerId del protocolo)

	struct TransportEvent
	{
		enum Kind { Connected, Disconnected, Received } kind = Received;
		LinkId link = 0;
		uint8_t channel = 0;
		std::vector<uint8_t> data;
	};

	class ITransport
	{
	public:
		virtual ~ITransport() = default;
		virtual bool Start() = 0;     // host: escucha; cliente: inicia conexion
		virtual void Stop() = 0;
		// Espera hasta timeoutMs y vuelca los eventos pendientes.
		virtual void Poll(std::vector<TransportEvent>& out, int timeoutMs) = 0;
		virtual void Send(LinkId link, uint8_t channel, bool reliable, const std::vector<uint8_t>& data) = 0;
		virtual void Broadcast(uint8_t channel, bool reliable, const std::vector<uint8_t>& data) = 0;
		virtual void Disconnect(LinkId link) = 0;
	};
}
