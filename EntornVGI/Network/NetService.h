// NetService.h
// Fachada thread-safe para el juego. Ejecuta NetSession + transporte en su propio hilo, sin tocar
// OpenGL ni Bullet: el hilo principal solo intercambia datos por colas/slots protegidos por mutex.
// Modo Offline: Start() no crea transporte, ni socket, ni hilo.
#pragma once
#include "NetSession.h"

#include <atomic>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace net
{
	class NetService
	{
	public:
		using TransportFactory = std::function<std::unique_ptr<ITransport>()>;
		static constexpr size_t kMaxQueuedEvents = 1024;   // colas acotadas: al llenarse se descartan los mas antiguos
		static constexpr size_t kMaxQueuedCommands = 256;

		NetService(const SessionConfig& cfg, TransportFactory factory);
		~NetService();
		NetService(const NetService&) = delete;
		NetService& operator=(const NetService&) = delete;

		bool Start();   // Offline: true sin hacer nada. Online: crea transporte (factory) e hilo.
		void Stop();
		bool IsNetworked() const { return cfg_.role != Role::Offline; }
		bool IsRunning() const { return running_.load(); }

		// ---- API del hilo principal (thread-safe) ----
		void SubmitLocalPose(const PoseEntry& pose);   // ultimo valor gana
		bool QueueEvent(uint16_t type, uint32_t target, const std::vector<uint8_t>& payload, PeerId onlyTo = kUnassignedPeerId);
		void SetRecoveryState(const std::vector<StateEntry>& state);
		void RequestRecovery();
		size_t PollEvents(std::vector<NetEvent>& out);  // vacia la cola de salida
		std::map<PeerId, PoseEntry> RemotePoses() const;
		ConnectionState State() const { return state_.load(); }
		PeerId LocalPeer() const { return localPeer_.load(); }
		size_t DroppedEvents() const { return droppedEvents_.load(); }

	private:
		void Run();
		void PushEvents(std::vector<NetEvent>&& evs);

		SessionConfig cfg_;
		TransportFactory factory_;
		std::unique_ptr<ITransport> transport_;
		std::thread thread_;
		std::atomic<bool> running_{false}, stop_{false};
		std::atomic<ConnectionState> state_{ConnectionState::Idle};
		std::atomic<PeerId> localPeer_{kUnassignedPeerId};
		std::atomic<size_t> droppedEvents_{0};

		mutable std::mutex mutex_;
		std::deque<std::function<void(NetSession&)>> commands_;
		bool hasPose_ = false; PoseEntry pendingPose_;
		std::deque<NetEvent> outEvents_;
		std::map<PeerId, PoseEntry> sharedPoses_;
	};
}
