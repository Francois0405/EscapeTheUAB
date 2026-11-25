#include "NetSession.h"

#include <algorithm>

namespace net
{
	NetSession::NetSession(const SessionConfig& cfg, ITransport& transport) : cfg_(cfg), tr_(transport)
	{
		// Un host nunca admite mas clientes de los que caben en un snapshot (host + clientes <= kMaxPoseEntries).
		cfg_.maxPeers = std::min<uint16_t>(cfg_.maxPeers, static_cast<uint16_t>(kMaxPoseEntries - 1));
		if (cfg_.poseRateHz < 1.0) cfg_.poseRateHz = 1.0;
		if (cfg_.poseRateHz > 60.0) cfg_.poseRateHz = 60.0;
		if (cfg_.role == Role::Host) { localPeer_ = kHostPeerId; state_ = ConnectionState::Connected; }
		else if (cfg_.role == Role::Client) state_ = ConnectionState::Connecting;
	}

	void NetSession::SetState(ConnectionState s)
	{
		if (s == state_) return;
		state_ = s;
		NetEvent e; e.kind = NetEvent::StateChanged; e.state = s; events_.push_back(e);
	}

	PeerId NetSession::AllocPeer()
	{
		for (PeerId id = 1; id <= cfg_.maxPeers; ++id)
		{
			bool used = false;
			for (const auto& kv : links_) if (kv.second.joined && kv.second.peer == id) { used = true; break; }
			if (!used) return id;
		}
		return kUnassignedPeerId;
	}

	bool NetSession::AcceptSeq(PeerId sender, uint8_t channel, uint32_t seq)
	{
		SeqTrack& t = seq_[sender];
		if (t.seen[channel] && !SeqNewer(seq, t.last[channel])) { ++stats_.staleDropped; return false; }
		t.seen[channel] = true; t.last[channel] = seq;
		return true;
	}

	void NetSession::SendTo(LinkId link, MessageType t, const Message& m)
	{
		Header h; h.type = t; h.sender = localPeer_; h.sequence = NextSeq(ChannelFor(t));
		std::vector<uint8_t> buf;
		if (!Encode(h, m, buf)) { ++stats_.dropped; return; }
		tr_.Send(link, ChannelFor(t), IsReliable(t), buf);
	}

	void NetSession::BroadcastMsg(MessageType t, const Message& m)
	{
		Header h; h.type = t; h.sender = localPeer_; h.sequence = NextSeq(ChannelFor(t));
		std::vector<uint8_t> buf;
		if (!Encode(h, m, buf)) { ++stats_.dropped; return; }
		for (const auto& kv : links_)
			if (kv.second.joined) tr_.Send(kv.first, ChannelFor(t), IsReliable(t), buf);
	}

	void NetSession::DropLink(LinkId link, DisconnectReason why, bool notifyTransport)
	{
		auto it = links_.find(link);
		if (it == links_.end()) return;
		const Link l = it->second;
		links_.erase(it);
		if (notifyTransport) tr_.Disconnect(link);
		if (!l.joined) return;
		poses_.erase(l.peer); seq_.erase(l.peer);
		PeerDisconnectedMsg m; m.peer = l.peer; m.reason = why;
		BroadcastMsg(MessageType::PeerDisconnected, m);
		NetEvent e; e.kind = NetEvent::PeerLeft; e.peer = l.peer; e.leftReason = why; events_.push_back(e);
	}

	void NetSession::OnTransportEvent(const TransportEvent& ev)
	{
		if (cfg_.role == Role::Offline) return;
		switch (ev.kind)
		{
		case TransportEvent::Connected:
			if (cfg_.role == Role::Host) { links_[ev.link] = Link{}; }
			else
			{
				serverLink_ = ev.link; seq_.clear(); poses_.clear();
				for (auto& seq : sendSeq_) seq = 0;
				HelloMsg h; h.name = cfg_.playerName; h.sceneId = cfg_.sceneId;
				SendTo(ev.link, MessageType::Hello, h);
			}
			break;
		case TransportEvent::Disconnected:
			if (cfg_.role == Role::Host) DropLink(ev.link, DisconnectReason::Timeout, false);
			else if (state_ != ConnectionState::Rejected)
			{
				poses_.clear(); localPeer_ = kUnassignedPeerId;
				SetState(ConnectionState::Disconnected);
			}
			break;
		case TransportEvent::Received:
		{
			Packet p;
			const DecodeStatus st = Decode(ev.data.data(), ev.data.size(), p);
			if (st != DecodeStatus::Ok)
			{
				++stats_.dropped;
				if (cfg_.role == Role::Host)
				{
					auto it = links_.find(ev.link);
					if (it != links_.end() && !it->second.joined)
					{
						if (st == DecodeStatus::BadVersion) { SendTo(ev.link, MessageType::Reject, RejectMsg{RejectReason::BadVersion}); }
						DropLink(ev.link, DisconnectReason::ProtocolError, true);
					}
				}
				return;
			}
			if (ev.channel != ChannelFor(p.header.type)) { ++stats_.dropped; return; }
			Handle(ev.link, ev.channel, p);
			break;
		}
		}
	}

	void NetSession::Handle(LinkId link, uint8_t channel, const Packet& p)
	{
		if (cfg_.role == Role::Host)
		{
			auto it = links_.find(link);
			if (it == links_.end()) { ++stats_.dropped; return; }
			if (!it->second.joined)
			{
				if (p.header.type != MessageType::Hello) { DropLink(link, DisconnectReason::ProtocolError, true); ++stats_.dropped; return; }
				HandleHost(link, p);
				return;
			}
			if (p.header.sender != it->second.peer || !AcceptSeq(it->second.peer, channel, p.header.sequence))
			{
				if (p.header.sender != it->second.peer) ++stats_.dropped;
				return;
			}
			HandleHost(link, p);
		}
		else
		{
			// Antes del Welcome solo se admiten Welcome/Reject; despues, solo mensajes del host (peer 0).
			if (p.header.sender != kHostPeerId) { ++stats_.dropped; return; }
			if (state_ == ConnectionState::Connecting && p.header.type != MessageType::Welcome && p.header.type != MessageType::Reject)
			{ ++stats_.dropped; return; }
			if (!AcceptSeq(kHostPeerId, channel, p.header.sequence)) return;
			HandleClient(p);
		}
	}

	void NetSession::HandleHost(LinkId link, const Packet& p)
	{
		Link& l = links_[link];
		switch (p.header.type)
		{
		case MessageType::Hello:
		{
			const auto& h = std::get<HelloMsg>(p.body);
			if (h.sceneId != cfg_.sceneId) { SendTo(link, MessageType::Reject, RejectMsg{RejectReason::SceneMismatch}); DropLink(link, DisconnectReason::Kicked, true); return; }
			const PeerId id = AllocPeer();
			if (id == kUnassignedPeerId) { SendTo(link, MessageType::Reject, RejectMsg{RejectReason::ServerFull}); DropLink(link, DisconnectReason::Kicked, true); return; }
			l.peer = id; l.joined = true; seq_[id] = SeqTrack{};
			WelcomeMsg w; w.assignedPeer = id;
			SendTo(link, MessageType::Welcome, w);
			NetEvent e; e.kind = NetEvent::PeerJoined; e.peer = id; events_.push_back(e);
			break;
		}
		case MessageType::PoseUpdate:
		{
			PoseEntry e = std::get<PoseUpdateMsg>(p.body).pose;
			e.peer = l.peer; // el ID lo fija el host: un cliente no puede suplantar a otro
			poses_[l.peer] = e; ++stats_.accepted;
			break;
		}
		case MessageType::Event:
		{
			NetEvent e; e.kind = NetEvent::GameEvent; e.peer = l.peer; e.event = std::get<EventMsg>(p.body);
			events_.push_back(std::move(e)); ++stats_.accepted;
			break;
		}
		case MessageType::RecoveryRequest:
		{
			RecoverySnapshotMsg m; m.entries = recoveryState_;
			if (m.entries.size() > kMaxRecoveryEntries) m.entries.resize(kMaxRecoveryEntries);
			SendTo(link, MessageType::RecoverySnapshot, m);
			break;
		}
		default: ++stats_.dropped; break; // un cliente no puede enviar Welcome, Snapshot, etc.
		}
	}

	void NetSession::HandleClient(const Packet& p)
	{
		switch (p.header.type)
		{
		case MessageType::Welcome:
			if (state_ != ConnectionState::Connecting) { ++stats_.dropped; return; }
			localPeer_ = std::get<WelcomeMsg>(p.body).assignedPeer;
			SetState(ConnectionState::Connected);
			RequestRecovery(); // al unirse o reconectar siempre se pide el estado actual
			break;
		case MessageType::Reject:
		{
			NetEvent e; e.kind = NetEvent::Rejected; e.rejectReason = std::get<RejectMsg>(p.body).reason; events_.push_back(e);
			SetState(ConnectionState::Rejected);
			tr_.Disconnect(serverLink_);
			break;
		}
		case MessageType::PoseSnapshot:
		{
			std::map<PeerId, PoseEntry> fresh;
			for (const auto& e : std::get<PoseSnapshotMsg>(p.body).poses) if (e.peer != localPeer_) fresh[e.peer] = e;
			poses_.swap(fresh); ++stats_.accepted;
			break;
		}
		case MessageType::DynamicSnapshot:
		{
			for (const auto& state : std::get<DynamicSnapshotMsg>(p.body).objects)
			{
				auto it = dynamicTicks_.find(state.objectId);
				if (it == dynamicTicks_.end() || SeqNewer(state.tick, it->second))
				{
					dynamicTicks_[state.objectId] = state.tick;
					dynamicStates_[state.objectId] = state;
				}
			}
			++stats_.accepted;
			break;
		}
		case MessageType::Event:
		{
			NetEvent e; e.kind = NetEvent::GameEvent; e.peer = kHostPeerId; e.event = std::get<EventMsg>(p.body);
			events_.push_back(std::move(e)); ++stats_.accepted;
			break;
		}
		case MessageType::PeerDisconnected:
		{
			const auto& m = std::get<PeerDisconnectedMsg>(p.body);
			poses_.erase(m.peer);
			NetEvent e; e.kind = NetEvent::PeerLeft; e.peer = m.peer; e.leftReason = m.reason; events_.push_back(e);
			break;
		}
		case MessageType::RecoverySnapshot:
		{
			NetEvent e; e.kind = NetEvent::RecoveryReceived; e.recovery = std::get<RecoverySnapshotMsg>(p.body).entries;
			events_.push_back(std::move(e));
			break;
		}
		default: ++stats_.dropped; break;
		}
	}

	void NetSession::Tick(Clock::time_point now)
	{
		if (cfg_.role != Role::Host) return;
		const auto period = std::chrono::duration<double>(1.0 / cfg_.poseRateHz);
		if (poseTimerInit_ && now - lastPose_ < period) return;
		poseTimerInit_ = true; lastPose_ = now;
		bool any = false;
		for (const auto& kv : links_) if (kv.second.joined) { any = true; break; }
		if (!any) return;
		PoseSnapshotMsg m;
		for (const auto& kv : poses_) { if (m.poses.size() >= kMaxPoseEntries) break; m.poses.push_back(kv.second); }
		BroadcastMsg(MessageType::PoseSnapshot, m);
		if (!localDynamics_.empty())
		{
			DynamicSnapshotMsg dynamics;
			const size_t count = std::min(kMaxDynamicEntries, localDynamics_.size());
			for (size_t i = 0; i < count; ++i) dynamics.objects.push_back(localDynamics_[(dynamicCursor_ + i) % localDynamics_.size()]);
			dynamicCursor_ = (dynamicCursor_ + count) % localDynamics_.size();
			BroadcastMsg(MessageType::DynamicSnapshot, dynamics);
		}
	}

	void NetSession::SetLocalPose(const PoseEntry& pose)
	{
		if (cfg_.role == Role::Host) { PoseEntry e = pose; e.peer = kHostPeerId; poses_[kHostPeerId] = e; }
		else if (cfg_.role == Role::Client && state_ == ConnectionState::Connected)
		{
			PoseUpdateMsg m; m.pose = pose; m.pose.peer = localPeer_;
			SendTo(serverLink_, MessageType::PoseUpdate, m);
		}
	}

	void NetSession::SetLocalDynamics(const std::vector<DynamicStateEntry>& state)
	{
		if (cfg_.role != Role::Host) return;
		localDynamics_ = state;
		if (localDynamics_.size() > 4096) localDynamics_.resize(4096);
		if (!localDynamics_.empty()) dynamicCursor_ %= localDynamics_.size(); else dynamicCursor_ = 0;
		for (const auto& entry : localDynamics_) dynamicStates_[entry.objectId] = entry;
	}

	void NetSession::SendEvent(uint16_t type, uint32_t target, const std::vector<uint8_t>& payload, PeerId onlyTo)
	{
		if (state_ != ConnectionState::Connected) return;
		EventMsg m; m.eventType = type; m.targetId = target; m.payload = payload;
		if (cfg_.role == Role::Client) { SendTo(serverLink_, MessageType::Event, m); return; }
		if (cfg_.role != Role::Host) return;
		if (onlyTo == kUnassignedPeerId) { BroadcastMsg(MessageType::Event, m); return; }
		for (const auto& kv : links_) if (kv.second.joined && kv.second.peer == onlyTo) SendTo(kv.first, MessageType::Event, m);
	}

	void NetSession::SetRecoveryState(const std::vector<StateEntry>& s) { recoveryState_ = s; }

	void NetSession::RequestRecovery()
	{
		if (cfg_.role == Role::Client && state_ == ConnectionState::Connected)
			SendTo(serverLink_, MessageType::RecoveryRequest, RecoveryRequestMsg{});
	}
}
