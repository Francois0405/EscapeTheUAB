// NetProtocol.h
// Protocolo binario versionado del multijugador. Sin dependencias de ENet, OpenGL ni Bullet:
// solo serializacion (little-endian explicita), validacion estricta de campos y limites.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace net
{
	// ---- Constantes y limites estrictos ----
	constexpr uint16_t kMagic = 0x5445;            // "ET" en little-endian: bytes 45 54
	constexpr uint8_t  kProtocolVersion = 2;
	constexpr size_t   kHeaderSize = 10;
	constexpr size_t   kMaxPacketSize = 1200;      // < MTU tipico; se rechaza todo lo mayor
	constexpr size_t   kMaxPoseEntries = 8;
	constexpr size_t   kMaxDynamicEntries = 16;
	constexpr size_t   kMaxEventPayload = 256;
	constexpr size_t   kMaxNameLength = 32;
	constexpr size_t   kMaxSceneIdLength = 64;
	constexpr size_t   kMaxRecoveryEntries = 64;
	constexpr float    kMaxCoordinate = 10000.0f;  // |x|,|y|,|z| maximo aceptado (metros)
	constexpr float    kQuatNormTolerance = 0.05f; // |q| debe estar en [1-tol, 1+tol]

	using PeerId = uint16_t;
	constexpr PeerId kHostPeerId = 0;              // el host siempre es el peer 0
	constexpr PeerId kUnassignedPeerId = 0xFFFF;   // solo valido en Hello (aun sin ID)
	constexpr PeerId kMaxPeerId = 254;

	// Streams independientes: evita que snapshots físicos desplacen poses por secuenciación ENet.
	enum Channel : uint8_t { kChannelPose = 0, kChannelEvents = 1, kChannelDynamics = 2, kChannelCount = 3 };

	enum class MessageType : uint8_t
	{
		Hello = 1,            // cliente -> host (fiable)
		Welcome = 2,          // host -> cliente (fiable)
		Reject = 3,           // host -> cliente (fiable)
		PoseUpdate = 10,      // cliente -> host (no fiable, 1 entrada)
		PoseSnapshot = 11,    // host -> clientes (no fiable)
		Event = 20,           // ambos (fiable)
		PeerDisconnected = 30,// host -> clientes (fiable)
		RecoveryRequest = 31, // cliente -> host (fiable)
		RecoverySnapshot = 32, // host -> cliente (fiable)
		DynamicSnapshot = 12   // host -> clientes (no fiable, estados físicos)
	};

	enum class RejectReason : uint8_t { BadVersion = 1, SceneMismatch = 2, ServerFull = 3, BadRequest = 4 };
	enum class DisconnectReason : uint8_t { Left = 0, Timeout = 1, Kicked = 2, ProtocolError = 3 };

	// ---- Mensajes ----
	struct Transform7 { float pos[3] = {0, 0, 0}; float rot[4] = {0, 0, 0, 1}; }; // rot = (x,y,z,w)
	struct PoseEntry { PeerId peer = 0; Transform7 head, leftHand, rightHand; };

	struct HelloMsg { std::string name; std::string sceneId; };
	struct WelcomeMsg { PeerId assignedPeer = 0; };
	struct RejectMsg { RejectReason reason = RejectReason::BadRequest; };
	struct PoseUpdateMsg { PoseEntry pose; };
	struct PoseSnapshotMsg { std::vector<PoseEntry> poses; };
	struct DynamicStateEntry
	{
		uint32_t objectId = 0;
		uint32_t tick = 0; // reloj monotónico del host en milisegundos (wrap-safe)
		Transform7 transform;
		float linearVelocity[3] = { 0, 0, 0 };
		float angularVelocity[3] = { 0, 0, 0 };
		uint8_t flags = 0; // bit 0 despierto, bit 1 agarrado; sin bit 0 = en reposo
		PeerId owner = kUnassignedPeerId;
	};
	struct DynamicSnapshotMsg { std::vector<DynamicStateEntry> objects; };
	struct EventMsg { uint16_t eventType = 0; uint32_t targetId = 0; std::vector<uint8_t> payload; };
	struct PeerDisconnectedMsg { PeerId peer = 0; DisconnectReason reason = DisconnectReason::Left; };
	struct RecoveryRequestMsg {};
	struct StateEntry { uint32_t objectId = 0; int32_t value = 0; };
	struct RecoverySnapshotMsg { std::vector<StateEntry> entries; };

	using Message = std::variant<HelloMsg, WelcomeMsg, RejectMsg, PoseUpdateMsg, PoseSnapshotMsg,
		EventMsg, PeerDisconnectedMsg, RecoveryRequestMsg, RecoverySnapshotMsg, DynamicSnapshotMsg>;

	struct Header
	{
		uint8_t  version = kProtocolVersion;
		MessageType type = MessageType::Hello;
		PeerId   sender = kUnassignedPeerId;
		uint32_t sequence = 0;   // por emisor y por canal
	};

	struct Packet { Header header; Message body; };

	enum class DecodeStatus
	{
		Ok, TooShort, TooLarge, BadMagic, BadVersion, UnknownType, Truncated, TrailingBytes,
		FieldOutOfRange, InvalidNumber, BadSender
	};
	const char* ToString(DecodeStatus s);

	// Canal por el que debe viajar cada tipo; (uint8_t)-1 si el tipo no es valido.
	uint8_t ChannelFor(MessageType t);
	bool IsReliable(MessageType t);
	MessageType TypeOf(const Message& m);

	// Serializa (validando antes). false si el mensaje incumple los limites o supera kMaxPacketSize.
	bool Encode(const Header& h, const Message& m, std::vector<uint8_t>& out);
	// Parsea y valida un paquete recibido. Nunca lee fuera de [data, data+size).
	DecodeStatus Decode(const uint8_t* data, size_t size, Packet& out);

	// Comparacion de secuencias con wraparound: true si a es estrictamente mas nuevo que b.
	inline bool SeqNewer(uint32_t a, uint32_t b) { return a != b && static_cast<uint32_t>(a - b) < 0x80000000u; }
}
