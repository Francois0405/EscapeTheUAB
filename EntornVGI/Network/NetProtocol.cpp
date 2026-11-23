#include "NetProtocol.h"

#include <cmath>
#include <cstring>

namespace net
{
	namespace
	{
		class Writer
		{
		public:
			explicit Writer(std::vector<uint8_t>& o) : out(o) {}
			void U8(uint8_t v) { out.push_back(v); }
			void U16(uint16_t v) { U8(v & 0xFF); U8(v >> 8); }
			void U32(uint32_t v) { for (int i = 0; i < 4; ++i) U8(static_cast<uint8_t>(v >> (8 * i))); }
			void F32(float f) { uint32_t b; std::memcpy(&b, &f, 4); U32(b); }
			void Bytes(const uint8_t* p, size_t n) { out.insert(out.end(), p, p + n); }
			void Str(const std::string& s) { U8(static_cast<uint8_t>(s.size())); Bytes(reinterpret_cast<const uint8_t*>(s.data()), s.size()); }
		private:
			std::vector<uint8_t>& out;
		};

		class Reader
		{
		public:
			Reader(const uint8_t* d, size_t n) : p(d), left(n) {}
			bool U8(uint8_t& v) { if (left < 1) return false; v = *p++; --left; return true; }
			bool U16(uint16_t& v) { if (left < 2) return false; v = static_cast<uint16_t>(p[0] | (p[1] << 8)); p += 2; left -= 2; return true; }
			bool U32(uint32_t& v) { if (left < 4) return false; v = 0; for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(p[i]) << (8 * i); p += 4; left -= 4; return true; }
			bool F32(float& f) { uint32_t b; if (!U32(b)) return false; std::memcpy(&f, &b, 4); return true; }
			bool Bytes(std::vector<uint8_t>& v, size_t n) { if (left < n) return false; v.assign(p, p + n); p += n; left -= n; return true; }
			size_t Left() const { return left; }
		private:
			const uint8_t* p; size_t left;
		};

		bool ValidName(const std::string& s, size_t maxLen, bool allowEmpty)
		{
			if (s.size() > maxLen || (!allowEmpty && s.empty())) return false;
			for (unsigned char c : s) if (c < 0x20 || c == 0x7F) return false; // sin caracteres de control
			return true;
		}

		bool ValidTransform(const Transform7& t)
		{
			for (float v : t.pos) if (!std::isfinite(v) || std::fabs(v) > kMaxCoordinate) return false;
			float n2 = 0;
			for (float v : t.rot) { if (!std::isfinite(v)) return false; n2 += v * v; }
			const float n = std::sqrt(n2);
			return std::fabs(n - 1.0f) <= kQuatNormTolerance;
		}
		bool ValidPose(const PoseEntry& e)
		{
			return e.peer <= kMaxPeerId && ValidTransform(e.head) && ValidTransform(e.leftHand) && ValidTransform(e.rightHand);
		}

		void WriteTransform(Writer& w, const Transform7& t) { for (float v : t.pos) w.F32(v); for (float v : t.rot) w.F32(v); }
		bool ReadTransform(Reader& r, Transform7& t)
		{
			for (float& v : t.pos) if (!r.F32(v)) return false;
			for (float& v : t.rot) if (!r.F32(v)) return false;
			return true;
		}
		void WritePose(Writer& w, const PoseEntry& e) { w.U16(e.peer); WriteTransform(w, e.head); WriteTransform(w, e.leftHand); WriteTransform(w, e.rightHand); }
		bool ReadPose(Reader& r, PoseEntry& e)
		{
			return r.U16(e.peer) && ReadTransform(r, e.head) && ReadTransform(r, e.leftHand) && ReadTransform(r, e.rightHand);
		}

		bool ValidMessage(const Message& m)
		{
			if (auto* v = std::get_if<HelloMsg>(&m)) return ValidName(v->name, kMaxNameLength, false) && ValidName(v->sceneId, kMaxSceneIdLength, false);
			if (auto* v = std::get_if<WelcomeMsg>(&m)) return v->assignedPeer >= 1 && v->assignedPeer <= kMaxPeerId;
			if (auto* v = std::get_if<RejectMsg>(&m)) return static_cast<uint8_t>(v->reason) >= 1 && static_cast<uint8_t>(v->reason) <= 4;
			if (auto* v = std::get_if<PoseUpdateMsg>(&m)) return ValidPose(v->pose);
			if (auto* v = std::get_if<PoseSnapshotMsg>(&m))
			{
				if (v->poses.size() > kMaxPoseEntries) return false;
				for (const auto& e : v->poses) if (!ValidPose(e)) return false;
				return true;
			}
			if (auto* v = std::get_if<EventMsg>(&m)) return v->payload.size() <= kMaxEventPayload;
			if (auto* v = std::get_if<PeerDisconnectedMsg>(&m)) return v->peer >= 1 && v->peer <= kMaxPeerId && static_cast<uint8_t>(v->reason) <= 3;
			if (std::holds_alternative<RecoveryRequestMsg>(m)) return true;
			if (auto* v = std::get_if<RecoverySnapshotMsg>(&m)) return v->entries.size() <= kMaxRecoveryEntries;
			return false;
		}
	}

	const char* ToString(DecodeStatus s)
	{
		switch (s)
		{
		case DecodeStatus::Ok: return "ok";
		case DecodeStatus::TooShort: return "too short";
		case DecodeStatus::TooLarge: return "too large";
		case DecodeStatus::BadMagic: return "bad magic";
		case DecodeStatus::BadVersion: return "bad version";
		case DecodeStatus::UnknownType: return "unknown type";
		case DecodeStatus::Truncated: return "truncated";
		case DecodeStatus::TrailingBytes: return "trailing bytes";
		case DecodeStatus::FieldOutOfRange: return "field out of range";
		case DecodeStatus::InvalidNumber: return "invalid number";
		case DecodeStatus::BadSender: return "bad sender";
		}
		return "?";
	}

	uint8_t ChannelFor(MessageType t)
	{
		switch (t)
		{
		case MessageType::PoseUpdate: case MessageType::PoseSnapshot: return kChannelPose;
		case MessageType::Hello: case MessageType::Welcome: case MessageType::Reject: case MessageType::Event:
		case MessageType::PeerDisconnected: case MessageType::RecoveryRequest: case MessageType::RecoverySnapshot:
			return kChannelEvents;
		}
		return 0xFF;
	}
	bool IsReliable(MessageType t) { return ChannelFor(t) == kChannelEvents; }

	MessageType TypeOf(const Message& m)
	{
		static const MessageType map[] = { MessageType::Hello, MessageType::Welcome, MessageType::Reject, MessageType::PoseUpdate,
			MessageType::PoseSnapshot, MessageType::Event, MessageType::PeerDisconnected, MessageType::RecoveryRequest, MessageType::RecoverySnapshot };
		return map[m.index()];
	}

	bool Encode(const Header& h, const Message& m, std::vector<uint8_t>& out)
	{
		out.clear();
		if (h.type != TypeOf(m) || !ValidMessage(m)) return false;
		Writer w(out);
		w.U16(kMagic); w.U8(h.version); w.U8(static_cast<uint8_t>(h.type)); w.U16(h.sender); w.U32(h.sequence);

		if (auto* v = std::get_if<HelloMsg>(&m)) { w.Str(v->name); w.Str(v->sceneId); }
		else if (auto* v = std::get_if<WelcomeMsg>(&m)) { w.U16(v->assignedPeer); }
		else if (auto* v = std::get_if<RejectMsg>(&m)) { w.U8(static_cast<uint8_t>(v->reason)); }
		else if (auto* v = std::get_if<PoseUpdateMsg>(&m)) { WritePose(w, v->pose); }
		else if (auto* v = std::get_if<PoseSnapshotMsg>(&m)) { w.U8(static_cast<uint8_t>(v->poses.size())); for (const auto& e : v->poses) WritePose(w, e); }
		else if (auto* v = std::get_if<EventMsg>(&m)) { w.U16(v->eventType); w.U32(v->targetId); w.U16(static_cast<uint16_t>(v->payload.size())); w.Bytes(v->payload.data(), v->payload.size()); }
		else if (auto* v = std::get_if<PeerDisconnectedMsg>(&m)) { w.U16(v->peer); w.U8(static_cast<uint8_t>(v->reason)); }
		else if (auto* v = std::get_if<RecoverySnapshotMsg>(&m)) { w.U8(static_cast<uint8_t>(v->entries.size())); for (const auto& e : v->entries) { w.U32(e.objectId); w.U32(static_cast<uint32_t>(e.value)); } }

		if (out.size() > kMaxPacketSize) { out.clear(); return false; }
		return true;
	}

	DecodeStatus Decode(const uint8_t* data, size_t size, Packet& out)
	{
		if (!data || size < kHeaderSize) return DecodeStatus::TooShort;
		if (size > kMaxPacketSize) return DecodeStatus::TooLarge;
		Reader r(data, size);
		uint16_t magic; uint8_t ver, type; uint16_t sender; uint32_t seq;
		r.U16(magic); r.U8(ver); r.U8(type); r.U16(sender); r.U32(seq);
		if (magic != kMagic) return DecodeStatus::BadMagic;
		if (ver != kProtocolVersion) return DecodeStatus::BadVersion;

		Header h; h.version = ver; h.sender = sender; h.sequence = seq;
		Message body;
		bool ok = true;
		switch (type)
		{
		case 1: { HelloMsg m; uint8_t n1, n2; std::vector<uint8_t> b;
			ok = r.U8(n1) && r.Bytes(b, n1); if (ok) { m.name.assign(b.begin(), b.end());
			ok = r.U8(n2) && r.Bytes(b, n2); if (ok) m.sceneId.assign(b.begin(), b.end()); }
			h.type = MessageType::Hello; body = std::move(m); break; }
		case 2: { WelcomeMsg m; ok = r.U16(m.assignedPeer); h.type = MessageType::Welcome; body = m; break; }
		case 3: { RejectMsg m; uint8_t b = 0; ok = r.U8(b); m.reason = static_cast<RejectReason>(b); h.type = MessageType::Reject; body = m; break; }
		case 10: { PoseUpdateMsg m; ok = ReadPose(r, m.pose); h.type = MessageType::PoseUpdate; body = m; break; }
		case 11: { PoseSnapshotMsg m; uint8_t n; ok = r.U8(n);
			if (ok && n > kMaxPoseEntries) return DecodeStatus::FieldOutOfRange;
			for (uint8_t i = 0; ok && i < n; ++i) { PoseEntry e; ok = ReadPose(r, e); if (ok) m.poses.push_back(e); }
			h.type = MessageType::PoseSnapshot; body = std::move(m); break; }
		case 20: { EventMsg m; uint16_t len; ok = r.U16(m.eventType) && r.U32(m.targetId) && r.U16(len);
			if (ok && len > kMaxEventPayload) return DecodeStatus::FieldOutOfRange;
			ok = ok && r.Bytes(m.payload, len); h.type = MessageType::Event; body = std::move(m); break; }
		case 30: { PeerDisconnectedMsg m; uint8_t b = 0; ok = r.U16(m.peer) && r.U8(b); m.reason = static_cast<DisconnectReason>(b);
			h.type = MessageType::PeerDisconnected; body = m; break; }
		case 31: { h.type = MessageType::RecoveryRequest; body = RecoveryRequestMsg{}; break; }
		case 32: { RecoverySnapshotMsg m; uint8_t n; ok = r.U8(n);
			if (ok && n > kMaxRecoveryEntries) return DecodeStatus::FieldOutOfRange;
			for (uint8_t i = 0; ok && i < n; ++i) { StateEntry e; uint32_t v = 0; ok = r.U32(e.objectId) && r.U32(v); e.value = static_cast<int32_t>(v); if (ok) m.entries.push_back(e); }
			h.type = MessageType::RecoverySnapshot; body = std::move(m); break; }
		default: return DecodeStatus::UnknownType;
		}
		if (!ok) return DecodeStatus::Truncated;
		if (r.Left() != 0) return DecodeStatus::TrailingBytes;

		// Validacion semantica (rangos, finitud, normas de cuaternion, texto).
		if (!ValidMessage(body)) return DecodeStatus::FieldOutOfRange;
		// El unico mensaje que puede venir sin ID asignado es Hello; el resto no.
		if ((h.type == MessageType::Hello) != (h.sender == kUnassignedPeerId)) return DecodeStatus::BadSender;
		if (h.sender != kUnassignedPeerId && h.sender > kMaxPeerId) return DecodeStatus::BadSender;
		out.header = h; out.body = std::move(body);
		return DecodeStatus::Ok;
	}
}
