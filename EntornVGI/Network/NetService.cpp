#include "NetService.h"

namespace net
{
	NetService::NetService(const SessionConfig& cfg, TransportFactory f) : cfg_(cfg), factory_(std::move(f)) {}
	NetService::~NetService() { Stop(); }

	bool NetService::Start()
	{
		if (cfg_.role == Role::Offline) return true;     // sin sockets, sin hilo
		if (running_.load()) return true;
		if (!factory_) return false;
		transport_ = factory_();
		if (!transport_ || !transport_->Start()) { transport_.reset(); return false; }
		stop_ = false; running_ = true;
		state_ = cfg_.role == Role::Host ? ConnectionState::Connected : ConnectionState::Connecting;
		thread_ = std::thread([this] { Run(); });
		return true;
	}

	void NetService::Stop()
	{
		if (!running_.load() && !thread_.joinable()) return;
		stop_ = true;
		if (thread_.joinable()) thread_.join();
		if (transport_) { transport_->Stop(); transport_.reset(); }
		running_ = false;
		state_ = ConnectionState::Idle;
	}

	void NetService::Run()
	{
		NetSession session(cfg_, *transport_);
		std::vector<TransportEvent> tev;
		while (!stop_.load())
		{
			// 1) Ordenes del juego.
			std::deque<std::function<void(NetSession&)>> cmds; bool hasPose; PoseEntry pose;
			{
				std::lock_guard<std::mutex> lk(mutex_);
				cmds.swap(commands_); hasPose = hasPose_; pose = pendingPose_; hasPose_ = false;
			}
			for (auto& c : cmds) c(session);
			if (hasPose) session.SetLocalPose(pose);

			// 2) Red.
			tev.clear();
			transport_->Poll(tev, 5);
			for (const auto& e : tev) session.OnTransportEvent(e);
			session.Tick(NetSession::Clock::now());

			// 3) Publicar resultados.
			state_ = session.State(); localPeer_ = session.LocalPeer();
			auto evs = session.TakeEvents();
			{
				std::lock_guard<std::mutex> lk(mutex_);
				sharedPoses_ = session.RemotePoses();
			}
			if (!evs.empty()) PushEvents(std::move(evs));
		}
	}

	void NetService::PushEvents(std::vector<NetEvent>&& evs)
	{
		std::lock_guard<std::mutex> lk(mutex_);
		for (auto& e : evs)
		{
			if (outEvents_.size() >= kMaxQueuedEvents) { outEvents_.pop_front(); ++droppedEvents_; }
			outEvents_.push_back(std::move(e));
		}
	}

	void NetService::SubmitLocalPose(const PoseEntry& pose)
	{
		if (!running_.load()) return;
		std::lock_guard<std::mutex> lk(mutex_);
		pendingPose_ = pose; hasPose_ = true;
	}

	bool NetService::QueueEvent(uint16_t type, uint32_t target, const std::vector<uint8_t>& payload, PeerId onlyTo)
	{
		if (!running_.load() || payload.size() > kMaxEventPayload) return false;
		std::lock_guard<std::mutex> lk(mutex_);
		if (commands_.size() >= kMaxQueuedCommands) return false;
		commands_.push_back([=](NetSession& s) { s.SendEvent(type, target, payload, onlyTo); });
		return true;
	}

	void NetService::SetRecoveryState(const std::vector<StateEntry>& state)
	{
		if (!running_.load()) return;
		std::lock_guard<std::mutex> lk(mutex_);
		commands_.push_back([state](NetSession& s) { s.SetRecoveryState(state); });
	}

	void NetService::RequestRecovery()
	{
		if (!running_.load()) return;
		std::lock_guard<std::mutex> lk(mutex_);
		commands_.push_back([](NetSession& s) { s.RequestRecovery(); });
	}

	size_t NetService::PollEvents(std::vector<NetEvent>& out)
	{
		std::lock_guard<std::mutex> lk(mutex_);
		const size_t n = outEvents_.size();
		for (auto& e : outEvents_) out.push_back(std::move(e));
		outEvents_.clear();
		return n;
	}

	std::map<PeerId, PoseEntry> NetService::RemotePoses() const
	{
		std::lock_guard<std::mutex> lk(mutex_);
		return sharedPoses_;
	}
}
