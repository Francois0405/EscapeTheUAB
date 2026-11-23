// NetSession.h
// Logica de protocolo (handshake, peer IDs, secuencias, desconexion, recuperacion).
// NO es thread-safe y no crea hilos: la ejecuta el hilo de red (ver NetService.h) o los tests.
#pragma once
#include "NetProtocol.h"
#include "NetTransport.h"

#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace net
{
	enum class Role { Offline, Host, Client };
	enum class ConnectionState { Idle, Connecting, Connected, Disconnected, Rejected };

	struct SessionConfig
	{
		Role role = Role::Offline;
		std::string playerName = "player";
		std::string sceneId;
		uint16_t maxPeers = 4;            // clientes simultaneos (sin contar al host)
		double poseRateHz = 20.0;         // frecuencia del snapshot de pose del host
	};

	// Evento hacia la aplicacion (cola de salida del hilo de red).
	struct NetEvent
	{
		enum Kind { PeerJoined, PeerLeft, GameEvent, RecoveryReceived, StateChanged, Rejected } kind = GameEvent;
		PeerId peer = 0;
		DisconnectReason leftReason = DisconnectReason::Left;
		RejectReason rejectReason = RejectReason::BadRequest;
		ConnectionState state = ConnectionState::Idle;
		EventMsg event;                   // GameEvent
		std::vector<StateEntry> recovery; // RecoveryReceived
	};

	struct SessionStats { uint64_t dropped = 0, staleDropped = 0, accepted = 0; };

	class NetSession
	{
	public:
		using Clock = std::chrono::steady_clock;
		NetSession(const SessionConfig& cfg, ITransport& transport);

		void OnTransportEvent(const TransportEvent& e);
		// Emite el snapshot de pose del host a su cadencia. Llamar con frecuencia.
		void Tick(Clock::time_point now);

		// Entradas de la aplicacion (ya trasladadas al hilo de red):
		void SetLocalPose(const PoseEntry& pose);                 // cliente: se envia; host: peer 0
		void SendEvent(uint16_t type, uint32_t target, const std::vector<uint8_t>& payload, PeerId onlyTo = kUnassignedPeerId);
		void SetRecoveryState(const std::vector<StateEntry>& s);  // host: base de la recuperacion
		void RequestRecovery();                                   // cliente

		ConnectionState State() const { return state_; }
		PeerId LocalPeer() const { return localPeer_; }
		const std::map<PeerId, PoseEntry>& RemotePoses() const { return poses_; }
		std::vector<NetEvent> TakeEvents() { std::vector<NetEvent> v; v.swap(events_); return v; }
		const SessionStats& Stats() const { return stats_; }

	private:
		struct Link { PeerId peer = kUnassignedPeerId; bool joined = false; };
		struct SeqTrack { bool seen[kChannelCount] = {false, false}; uint32_t last[kChannelCount] = {0, 0}; };

		void Handle(LinkId link, uint8_t channel, const Packet& p);
		void HandleHost(LinkId link, const Packet& p);
		void HandleClient(const Packet& p);
		bool AcceptSeq(PeerId sender, uint8_t channel, uint32_t seq);
		void SendTo(LinkId link, MessageType t, const Message& m);
		void BroadcastMsg(MessageType t, const Message& m);
		uint32_t NextSeq(uint8_t channel) { return sendSeq_[channel]++; }
		PeerId AllocPeer();
		void SetState(ConnectionState s);
		void DropLink(LinkId link, DisconnectReason why, bool notifyTransport);

		SessionConfig cfg_;
		ITransport& tr_;
		ConnectionState state_ = ConnectionState::Idle;
		PeerId localPeer_ = kUnassignedPeerId;
		uint32_t sendSeq_[kChannelCount] = {0, 0};
		std::map<LinkId, Link> links_;           // host
		LinkId serverLink_ = 0;                  // cliente
		std::map<PeerId, SeqTrack> seq_;
		std::map<PeerId, PoseEntry> poses_;
		std::vector<StateEntry> recoveryState_;
		std::vector<NetEvent> events_;
		SessionStats stats_;
		Clock::time_point lastPose_{};
		bool poseTimerInit_ = false;
	};
}
