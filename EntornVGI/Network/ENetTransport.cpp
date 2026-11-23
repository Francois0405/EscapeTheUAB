#include "ENetTransport.h"
#include "NetProtocol.h"   // kChannelCount, kMaxPacketSize

#ifdef ENTORNVGI_HAS_ENET
#include <enet/enet.h>
#include <map>

namespace net
{
	namespace
	{
		class ENetTransport final : public ITransport
		{
		public:
			explicit ENetTransport(const ENetOptions& o) : opt_(o) {}
			~ENetTransport() override { Stop(); }

			bool Start() override
			{
				if (enet_initialize() != 0) return false;
				inited_ = true;
				if (opt_.isHost)
				{
					ENetAddress a{}; a.host = ENET_HOST_ANY; a.port = opt_.port;
					host_ = enet_host_create(&a, opt_.maxClients, kChannelCount, 0, 0);
					return host_ != nullptr;
				}
				host_ = enet_host_create(nullptr, 1, kChannelCount, 0, 0);
				if (!host_) return false;
				ENetAddress a{}; a.port = opt_.port;
				if (enet_address_set_host(&a, opt_.address.c_str()) != 0) return false;
				ENetPeer* p = enet_host_connect(host_, &a, kChannelCount, 0);
				if (!p) return false;
				enet_peer_timeout(p, 0, opt_.connectTimeoutMs, opt_.connectTimeoutMs * 2);
				return true;
			}
			void Stop() override
			{
				if (host_) { for (auto& kv : peers_) enet_peer_disconnect_now(kv.second, 0); enet_host_flush(host_); enet_host_destroy(host_); host_ = nullptr; }
				peers_.clear();
				if (inited_) { enet_deinitialize(); inited_ = false; }
			}
			void Poll(std::vector<TransportEvent>& out, int timeoutMs) override
			{
				if (!host_) return;
				ENetEvent ev;
				int rc = enet_host_service(host_, &ev, static_cast<enet_uint32>(timeoutMs));
				while (rc > 0)
				{
					TransportEvent t;
					switch (ev.type)
					{
					case ENET_EVENT_TYPE_CONNECT:
						t.kind = TransportEvent::Connected; t.link = next_++; peers_[t.link] = ev.peer; ev.peer->data = reinterpret_cast<void*>(static_cast<uintptr_t>(t.link));
						out.push_back(std::move(t)); break;
					case ENET_EVENT_TYPE_DISCONNECT:   // ENet 1.3.18 notifica tambien los timeouts como DISCONNECT
						t.kind = TransportEvent::Disconnected; t.link = static_cast<LinkId>(reinterpret_cast<uintptr_t>(ev.peer->data));
						peers_.erase(t.link); out.push_back(std::move(t)); break;
					case ENET_EVENT_TYPE_RECEIVE:
						t.kind = TransportEvent::Received; t.link = static_cast<LinkId>(reinterpret_cast<uintptr_t>(ev.peer->data));
						t.channel = ev.channelID;
						if (ev.packet->dataLength <= kMaxPacketSize) t.data.assign(ev.packet->data, ev.packet->data + ev.packet->dataLength);
						enet_packet_destroy(ev.packet);
						if (!t.data.empty()) out.push_back(std::move(t));
						break;
					default: break;
					}
					rc = enet_host_check_events(host_, &ev);
				}
			}
			void Send(LinkId link, uint8_t channel, bool reliable, const std::vector<uint8_t>& d) override
			{
				auto it = peers_.find(link); if (it == peers_.end()) return;
				ENetPacket* p = enet_packet_create(d.data(), d.size(), reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
				if (p && enet_peer_send(it->second, channel, p) < 0) enet_packet_destroy(p);
			}
			void Broadcast(uint8_t channel, bool reliable, const std::vector<uint8_t>& d) override
			{ for (auto& kv : peers_) Send(kv.first, channel, reliable, d); }
			void Disconnect(LinkId link) override
			{ auto it = peers_.find(link); if (it != peers_.end()) enet_peer_disconnect(it->second, 0); }
		private:
			ENetOptions opt_; ENetHost* host_ = nullptr; bool inited_ = false;
			LinkId next_ = 1; std::map<LinkId, ENetPeer*> peers_;
		};
	}
	std::unique_ptr<ITransport> CreateENetTransport(const ENetOptions& opt) { return std::make_unique<ENetTransport>(opt); }
}
#else
namespace net { std::unique_ptr<ITransport> CreateENetTransport(const ENetOptions&) { return nullptr; } }
#endif
