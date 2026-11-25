// NetJuegoLogica.h
// -----------------------------------------------------------------------------
// Logica PURA del multijugador del juego (sin OpenGL, sin Bullet, sin OpenXR, sin hilos): se prueba en
// tests/NetJuegoTests.cpp. Es el adaptador entre la capa de red estable (Network/) y el motor:
//   * configuracion offline/host/cliente, IP y puerto (linea de comandos y menu);
//   * conversion de poses PC y OpenXR -> protocolo (Z arriba, metros) SIN tocar la pose local;
//   * interpolacion de jugadores remotos a partir de snapshots a 20 Hz;
//   * puertas con IDs de escena y el host como autoridad;
//   * propiedad temporal de un objeto compartido de prueba (alcance, concesion, liberacion al desconectar).
// Convenio de ejes (igual que el motor): X derecha, Y adelante, Z ARRIBA. La rotacion de red es la del
// triedro (X=derecha, Y=adelante, Z=arriba): la identidad mira hacia +Y.
// Nada de esto se usa en modo Offline.
// -----------------------------------------------------------------------------
#pragma once

#include "Network/NetProtocol.h"
#include "Network/NetSession.h"
#include "VistaVR.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace netjuego
{
	// ------------------------------------------------------------------ configuracion
	constexpr uint16_t kPuertoPorDefecto = 7777;
	constexpr double kHzSubidaPose = 30.0;     // el cliente no envia mas de esto, aunque el juego vaya a 144 fps
	constexpr double kRetardoInterpolacion = 0.10;   // s: dos snapshots a 20 Hz

	struct ConfigRed
	{
		net::Role rol = net::Role::Offline;   // OFFLINE por defecto: ni hilo, ni sockets, ni ENet
		std::string host = "127.0.0.1";       // solo cliente
		uint16_t puerto = kPuertoPorDefecto;
		std::string nombre = "jugador";
	};

	inline bool PuertoValido(long p) { return p >= 1 && p <= 65535; }

	inline bool IPv4LiteralValida(const std::string& host)
	{
		if (host.find_first_not_of("0123456789.") != std::string::npos || host.find('.') == std::string::npos) return true; // nombre DNS
		size_t inicio = 0;
		for (int octeto = 0; octeto < 4; ++octeto)
		{
			const size_t fin = host.find('.', inicio);
			const size_t final = octeto == 3 ? host.size() : fin;
			if (final == std::string::npos || final == inicio || final - inicio > 3 || (octeto == 3 && fin != std::string::npos)) return false;
			unsigned valor = 0;
			for (size_t i = inicio; i < final; ++i) valor = valor * 10 + static_cast<unsigned>(host[i] - '0');
			if (valor > 255) return false;
			inicio = final + 1;
		}
		return true;
	}

	// "host" o "host:puerto" (IPv4 o nombre DNS). No acepta espacios ni caracteres raros.
	inline bool ParsearEndpoint(const std::string& texto, std::string& host, uint16_t& puerto, std::string& error)
	{
		std::string h = texto; long p = puerto;
		const size_t dos = texto.rfind(':');
		if (dos != std::string::npos)
		{
			h = texto.substr(0, dos);
			const std::string ps = texto.substr(dos + 1);
			if (ps.empty() || ps.size() > 5 || !std::all_of(ps.begin(), ps.end(), [](char c) { return c >= '0' && c <= '9'; }))
			{ error = "Puerto no valido"; return false; }
			p = std::stol(ps);
		}
		if (h.empty() || h.size() > 253) { error = "IP o nombre de host vacio o demasiado largo"; return false; }
		if (!IPv4LiteralValida(h)) { error = "La direccion IPv4 no es valida"; return false; }
		for (char c : h)
		{
			const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '-' || c == '_';
			if (!ok) { error = "El host contiene caracteres no validos"; return false; }
		}
		if (!PuertoValido(p)) { error = "Puerto fuera de rango (1-65535)"; return false; }
		host = h; puerto = static_cast<uint16_t>(p);
		return true;
	}

	// --host [--port N] | --connect HOST[:PORT] [--name NOMBRE]. Sin argumentos de red: Offline.
	inline bool ParsearArgumentos(int argc, char** argv, ConfigRed& cfg, std::string& error)
	{
		ConfigRed r;
		long puertoCli = -1; std::string endpoint;
		for (int i = 1; i < argc; ++i)
		{
			const std::string a = argv[i] ? argv[i] : "";
			auto valor = [&](std::string& out) { if (i + 1 >= argc) return false; out = argv[++i]; return true; };
			if (a == "--host") r.rol = net::Role::Host;
			else if (a == "--connect") { std::string v; if (!valor(v)) { error = "--connect necesita HOST[:PUERTO]"; return false; } r.rol = net::Role::Client; endpoint = v; }
			else if (a == "--port")
			{
				std::string v; if (!valor(v) || v.empty() || v.size() > 5 || !std::all_of(v.begin(), v.end(), [](char c) { return c >= '0' && c <= '9'; }))
				{ error = "--port necesita un numero"; return false; }
				puertoCli = std::stol(v);
				if (!PuertoValido(puertoCli)) { error = "Puerto fuera de rango (1-65535)"; return false; }
			}
			else if (a == "--name") { std::string v; if (!valor(v) || v.empty()) { error = "--name necesita un nombre"; return false; } r.nombre = v.substr(0, net::kMaxNameLength); }
		}
		if (puertoCli > 0) r.puerto = static_cast<uint16_t>(puertoCli);
		if (r.rol == net::Role::Client && !ParsearEndpoint(endpoint, r.host, r.puerto, error)) return false;
		cfg = r;
		return true;
	}

	// ------------------------------------------------------------------ ids y payloads
	// FNV-1a de 32 bits: el ID de red de una puerta es el hash estable de su id de escena ("puerta_entrada").
	inline uint32_t HashId(const std::string& s)
	{
		uint32_t h = 2166136261u;
		for (unsigned char c : s) { h ^= c; h *= 16777619u; }
		return h;
	}
	constexpr uint32_t kObjetoPrueba = 0x53480001u;      // objeto compartido de prueba (usa +0..+3 en recuperacion)
	constexpr uint32_t kRangoReservadoIni = 0x53480000u, kRangoReservadoFin = 0x5348000Fu;
	inline bool IdReservado(uint32_t id) { return id >= kRangoReservadoIni && id <= kRangoReservadoFin; }

	enum EventoJuego : uint16_t
	{
		kEvPuertaSolicitud = 0x0100,   // cliente -> host: "alterna esta puerta" (target = ID de red de la puerta)
		kEvPuertaEstado    = 0x0101,   // host -> todos: payload u8 (1 = abierta)
		kEvPropSolicitar   = 0x0110,   // cliente -> host
		kEvPropLiberar     = 0x0111,   // cliente -> host
		kEvPropMover       = 0x0112,   // dueno -> host: 3 x f32
		kEvPropEstado      = 0x0113,   // host -> todos: u16 dueno (0xFFFF = nadie) + 3 x f32
		kEvEstadoObjetoEscena = 0x0120, // host -> cliente(s): estado fiable de un objeto JSON existente
		kEvSolicitarAgarreDinamico = 0x0130, // cliente -> host: solicita agarrar targetId
		kEvSolicitarSoltarDinamico = 0x0131, // cliente -> host: velocidad lineal (3 x f32)
		kEvPropiedadDinamica = 0x0132, // host -> todos: u16 propietario, 0xFFFF = libre
		kEvEstadoFisicoFinal = 0x0133 // host -> todos/cliente tardío: estado fiable y vigente
	};

	struct SolicitudObjetoDinamico
	{
		uint32_t id = 0;
		net::PeerId peer = net::kUnassignedPeerId;
		bool agarrar = false;
		glm::vec3 velocidad{ 0.0f }, origen{ 0.0f }, direccion{ 0.0f };
	};

	inline bool PuedeCargarLanzamiento(bool agarreFisicoLocal, net::Role rol, uint32_t objetoPropio)
	{
		return agarreFisicoLocal || (rol == net::Role::Client && objetoPropio != 0);
	}

	inline bool EstadoFisicoMasNuevo(uint32_t tickCandidato, bool hayEstado, uint32_t tickActual)
	{
		return !hayEstado || net::SeqNewer(tickCandidato, tickActual);
	}

	inline bool PuedeAdquirirDinamico(net::PeerId propietarioActual, net::PeerId solicitante, uint32_t objetoYaSujeto)
	{
		return propietarioActual == net::kUnassignedPeerId && solicitante <= net::kMaxPeerId && objetoYaSujeto == 0;
	}

	inline double TiempoSnapshotSiguiente(double tiempoAnterior, uint32_t tickAnterior, uint32_t tickNuevo)
	{
		return tiempoAnterior + static_cast<double>(static_cast<uint32_t>(tickNuevo - tickAnterior)) / 1000.0;
	}

	inline bool TransformValido(const net::Transform7& t);
	inline bool CoordenadaValida(const glm::vec3& v);
	namespace detalle { inline void PonF32(std::vector<uint8_t>& b, float f); inline float LeeF32(const uint8_t* p); }

	inline bool EstadoFisicoValido(const net::DynamicStateEntry& e)
	{
		if (e.objectId == 0 || !TransformValido(e.transform) || (e.flags & ~uint8_t(3)) != 0 ||
			(e.owner != net::kUnassignedPeerId && e.owner > net::kMaxPeerId) ||
			((e.flags & 2) != 0) != (e.owner != net::kUnassignedPeerId)) return false;
		for (float v : e.linearVelocity) if (!std::isfinite(v) || std::fabs(v) > 1000.0f) return false;
		for (float v : e.angularVelocity) if (!std::isfinite(v) || std::fabs(v) > 1000.0f) return false;
		return true;
	}

	inline std::vector<uint8_t> CodificarEstadoFisico(const net::DynamicStateEntry& e)
	{
		std::vector<uint8_t> b;
		if (!EstadoFisicoValido(e)) return b;
		b.reserve(59);
		for (float f : e.transform.pos) detalle::PonF32(b, f);
		for (float f : e.transform.rot) detalle::PonF32(b, f);
		for (float f : e.linearVelocity) detalle::PonF32(b, f);
		for (float f : e.angularVelocity) detalle::PonF32(b, f);
		for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(e.tick >> (8 * i)));
		b.push_back(e.flags);
		b.push_back(static_cast<uint8_t>(e.owner & 0xFF)); b.push_back(static_cast<uint8_t>(e.owner >> 8));
		return b;
	}

	inline bool DecodificarEstadoFisico(uint32_t id, const std::vector<uint8_t>& b, net::DynamicStateEntry& out)
	{
		if (b.size() != 59) return false;
		net::DynamicStateEntry e; e.objectId = id;
		std::size_t at = 0;
		for (float& f : e.transform.pos) { f = detalle::LeeF32(&b[at]); at += 4; }
		for (float& f : e.transform.rot) { f = detalle::LeeF32(&b[at]); at += 4; }
		for (float& f : e.linearVelocity) { f = detalle::LeeF32(&b[at]); at += 4; }
		for (float& f : e.angularVelocity) { f = detalle::LeeF32(&b[at]); at += 4; }
		for (int i = 0; i < 4; ++i) e.tick |= uint32_t(b[at++]) << (8 * i);
		e.flags = b[at++]; e.owner = static_cast<net::PeerId>(b[at] | (uint16_t(b[at + 1]) << 8));
		if (!EstadoFisicoValido(e)) return false;
		out = e; return true;
	}

	inline std::vector<uint8_t> CodificarRayoInteraccion(const glm::vec3& origen, const glm::vec3& direccion)
	{
		if (!CoordenadaValida(origen) || !CoordenadaValida(direccion) || glm::length(direccion) < 0.5f || glm::length(direccion) > 1.5f) return {};
		std::vector<uint8_t> b; b.reserve(24);
		for (float f : { origen.x, origen.y, origen.z, direccion.x, direccion.y, direccion.z }) detalle::PonF32(b, f);
		return b;
	}

	inline bool DecodificarRayoInteraccion(const std::vector<uint8_t>& b, glm::vec3& origen, glm::vec3& direccion)
	{
		if (b.size() != 24) return false;
		origen = glm::vec3(detalle::LeeF32(&b[0]), detalle::LeeF32(&b[4]), detalle::LeeF32(&b[8]));
		direccion = glm::vec3(detalle::LeeF32(&b[12]), detalle::LeeF32(&b[16]), detalle::LeeF32(&b[20]));
		return CoordenadaValida(origen) && CoordenadaValida(direccion) && glm::length(direccion) >= 0.5f && glm::length(direccion) <= 1.5f;
	}

	struct EventoSalida { uint16_t tipo = 0; uint32_t objetivo = 0; std::vector<uint8_t> payload; };

	// El contrato de escena distingue solo cuerpos estáticos de cuerpos replicables.
	// La subclase física (dynamic/bouncy) sigue siendo responsabilidad de la autoridad.
	inline uint8_t TipoColisionEscena(bool presente, bool cuerpoDinamico)
	{
		return !presente ? 0 : (cuerpoDinamico ? 2 : 1);
	}

	template <typename T, typename IdFn, typename ReplicableFn>
	inline bool IndiceUnicoObjetoRed(uint32_t id, const std::vector<T>& objetos, IdFn hashId,
		ReplicableFn esReplicable, std::size_t& indice)
	{
		std::size_t encontrados = 0;
		for (std::size_t i = 0; i < objetos.size(); ++i)
			if (hashId(objetos[i]) == id) { indice = i; ++encontrados; }
		return encontrados == 1 && indice < objetos.size() && esReplicable(objetos[indice]);
	}

	// Estado replicable de un objeto OBJ existente. El identificador se deriva del id
	// persistente del JSON; no depende del orden de las listas ni de punteros de Bullet.
	struct EstadoObjetoEscena
	{
		uint32_t id = 0;
		glm::vec3 posicion{ 0.0f }, rotacion{ 0.0f }, escala{ 1.0f };
		bool colisionPresente = false;
		uint8_t tipoColision = 0; // 0 = none, 1 = static, 2 = dynamic
		glm::vec3 mitadTamano{ 0.5f }, desplazamiento{ 0.0f };
		float masa = 0.0f;
	};

	inline bool EstadoObjetoEscenaValido(const EstadoObjetoEscena& e)
	{
		if (e.id == 0 || IdReservado(e.id)) return false;
		auto vectorValido = [](const glm::vec3& v, float maximo)
		{
			return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) &&
				std::fabs(v.x) <= maximo && std::fabs(v.y) <= maximo && std::fabs(v.z) <= maximo;
		};
		if (!vectorValido(e.posicion, net::kMaxCoordinate) || !vectorValido(e.rotacion, 360000.0f) ||
			!vectorValido(e.escala, 10000.0f) || e.escala.x == 0.0f || e.escala.y == 0.0f || e.escala.z == 0.0f ||
			!std::isfinite(e.masa) || e.masa < 0.0f || e.masa > 1000000.0f) return false;
		if (!e.colisionPresente) return e.tipoColision == 0;
		return (e.tipoColision == 1 || e.tipoColision == 2) &&
			vectorValido(e.mitadTamano, 10000.0f) && e.mitadTamano.x > 0.0f && e.mitadTamano.y > 0.0f && e.mitadTamano.z > 0.0f &&
			vectorValido(e.desplazamiento, net::kMaxCoordinate) &&
			(e.tipoColision != 2 || e.masa > 0.0f);
	}

	inline bool IgualEstadoObjetoEscena(const EstadoObjetoEscena& a, const EstadoObjetoEscena& b)
	{
		return a.id == b.id && a.posicion == b.posicion && a.rotacion == b.rotacion && a.escala == b.escala &&
			a.colisionPresente == b.colisionPresente && a.tipoColision == b.tipoColision &&
			a.mitadTamano == b.mitadTamano && a.desplazamiento == b.desplazamiento && a.masa == b.masa;
	}

	inline std::vector<uint8_t> CodificarEstadoObjetoEscena(const EstadoObjetoEscena& e)
	{
		std::vector<uint8_t> b;
		if (!EstadoObjetoEscenaValido(e)) return b;
		b.reserve(66);
		for (float f : { e.posicion.x, e.posicion.y, e.posicion.z, e.rotacion.x, e.rotacion.y, e.rotacion.z,
			e.escala.x, e.escala.y, e.escala.z, e.mitadTamano.x, e.mitadTamano.y, e.mitadTamano.z,
			e.desplazamiento.x, e.desplazamiento.y, e.desplazamiento.z, e.masa }) detalle::PonF32(b, f);
		b.push_back(e.colisionPresente ? 1 : 0);
		b.push_back(e.tipoColision);
		return b;
	}

	inline bool DecodificarEstadoObjetoEscena(uint32_t id, const std::vector<uint8_t>& b, EstadoObjetoEscena& out)
	{
		if (b.size() != 66) return false;
		EstadoObjetoEscena e; e.id = id;
		float* valores[] = { &e.posicion.x, &e.posicion.y, &e.posicion.z, &e.rotacion.x, &e.rotacion.y, &e.rotacion.z,
			&e.escala.x, &e.escala.y, &e.escala.z, &e.mitadTamano.x, &e.mitadTamano.y, &e.mitadTamano.z,
			&e.desplazamiento.x, &e.desplazamiento.y, &e.desplazamiento.z, &e.masa };
		for (std::size_t i = 0; i < 16; ++i) *valores[i] = detalle::LeeF32(&b[i * 4]);
		if (b[64] > 1) return false;
		e.colisionPresente = b[64] != 0; e.tipoColision = b[65];
		if (!EstadoObjetoEscenaValido(e)) return false;
		out = e;
		return true;
	}

	inline bool CoordenadaValida(const glm::vec3& v)
	{
		return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) &&
			std::fabs(v.x) <= net::kMaxCoordinate && std::fabs(v.y) <= net::kMaxCoordinate && std::fabs(v.z) <= net::kMaxCoordinate;
	}

	namespace detalle
	{
		inline void PonU16(std::vector<uint8_t>& b, uint16_t v) { b.push_back(uint8_t(v)); b.push_back(uint8_t(v >> 8)); }
		inline void PonF32(std::vector<uint8_t>& b, float f) { uint32_t u; std::memcpy(&u, &f, 4); for (int i = 0; i < 4; ++i) b.push_back(uint8_t(u >> (8 * i))); }
		inline uint16_t LeeU16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
		inline float LeeF32(const uint8_t* p) { uint32_t u = uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); float f; std::memcpy(&f, &u, 4); return f; }
	}

	inline std::vector<uint8_t> CodificarVec3(const glm::vec3& v)
	{
		std::vector<uint8_t> b; detalle::PonF32(b, v.x); detalle::PonF32(b, v.y); detalle::PonF32(b, v.z); return b;
	}
	inline bool DecodificarVec3(const std::vector<uint8_t>& b, glm::vec3& v)
	{
		if (b.size() != 12) return false;
		glm::vec3 t(detalle::LeeF32(&b[0]), detalle::LeeF32(&b[4]), detalle::LeeF32(&b[8]));
		if (!CoordenadaValida(t)) return false;
		v = t; return true;
	}
	constexpr net::PeerId kSinDueno = net::kUnassignedPeerId;
	inline std::vector<uint8_t> CodificarEstadoObjeto(net::PeerId dueno, const glm::vec3& p)
	{
		std::vector<uint8_t> b; detalle::PonU16(b, dueno);
		const auto v = CodificarVec3(p); b.insert(b.end(), v.begin(), v.end()); return b;
	}
	inline bool DecodificarEstadoObjeto(const std::vector<uint8_t>& b, net::PeerId& dueno, glm::vec3& p)
	{
		if (b.size() != 14) return false;
		const net::PeerId d = detalle::LeeU16(&b[0]);
		if (d != kSinDueno && d > net::kMaxPeerId) return false;
		glm::vec3 t; if (!DecodificarVec3(std::vector<uint8_t>(b.begin() + 2, b.end()), t)) return false;
		dueno = d; p = t; return true;
	}

	// ------------------------------------------------------------------ poses (PC y OpenXR -> red)
	inline net::Transform7 ATransform(const glm::vec3& p, const glm::quat& q)
	{
		net::Transform7 t; t.pos[0] = p.x; t.pos[1] = p.y; t.pos[2] = p.z;
		const glm::quat n = glm::normalize(q);
		t.rot[0] = n.x; t.rot[1] = n.y; t.rot[2] = n.z; t.rot[3] = n.w;
		return t;
	}
	inline glm::vec3 PosDe(const net::Transform7& t) { return glm::vec3(t.pos[0], t.pos[1], t.pos[2]); }
	inline glm::quat RotDe(const net::Transform7& t)
	{
		glm::quat q(t.rot[3], t.rot[0], t.rot[1], t.rot[2]);   // glm: w,x,y,z
		const float l = glm::length(q);
		return l > 1e-6f ? q / l : glm::quat(1, 0, 0, 0);
	}
	inline bool TransformValido(const net::Transform7& t)
	{
		for (float f : t.pos) if (!std::isfinite(f) || std::fabs(f) > net::kMaxCoordinate) return false;
		float n2 = 0; for (float f : t.rot) { if (!std::isfinite(f)) return false; n2 += f * f; }
		return n2 > 1e-6f;
	}
	inline bool PoseValida(const net::PoseEntry& p) { return TransformValido(p.head) && TransformValido(p.leftHand) && TransformValido(p.rightHand); }

	// Rotacion del triedro (derecha, adelante, arriba) con Z como vertical. Robusto si 'adelante' es casi vertical.
	inline glm::quat RotacionDesdeAdelante(const glm::vec3& adelante)
	{
		glm::vec3 f = adelante;
		if (!(glm::length(f) > 1e-6f)) f = glm::vec3(0, 1, 0);
		f = glm::normalize(f);
		glm::vec3 d = glm::cross(f, glm::vec3(0, 0, 1));
		d = glm::length(d) > 1e-4f ? glm::normalize(d) : glm::vec3(1, 0, 0);
		const glm::vec3 a = glm::cross(d, f);
		return glm::normalize(glm::quat_cast(glm::mat3(d, f, a)));
	}

	// ADAPTADOR PC: ojos y direccion de mirada de la camara FPS. Las manos son un offset fijo respecto a la cabeza.
	// Solo LEE la camara: no modifica nada del jugador local.
	inline bool PoseDesdeEscritorio(const glm::vec3& ojos, const glm::vec3& adelante, net::PoseEntry& out)
	{
		if (!CoordenadaValida(ojos) || !std::isfinite(adelante.x + adelante.y + adelante.z)) return false;
		const glm::quat q = RotacionDesdeAdelante(adelante);
		const glm::vec3 der = q * glm::vec3(1, 0, 0), fwd = q * glm::vec3(0, 1, 0), arr = q * glm::vec3(0, 0, 1);
		out = net::PoseEntry{};
		out.head = ATransform(ojos, q);
		out.rightHand = ATransform(ojos + der * 0.22f + fwd * 0.35f - arr * 0.30f, q);
		out.leftHand = ATransform(ojos - der * 0.22f + fwd * 0.35f - arr * 0.30f, q);
		return true;
	}

	// ADAPTADOR OpenXR: poses del espacio de OpenXR + el mismo Anclaje que usa el render -> mundo del motor.
	// Rotacion de red = rotacion de mundo del dispositivo * (cambio de ejes OpenXR local -> triedro de red).
	inline net::Transform7 TransformDesdeXR(const vr::PoseXR& pose, const vr::Anclaje& anclaje)
	{
		const glm::mat4 m = vr::MundoDeOjo(pose, anclaje);
		const glm::mat3 xrAEjesRed(glm::vec3(1, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));   // X->X, Y(red)->-Z(xr), Z(red)->Y(xr)
		return ATransform(glm::vec3(m[3]), glm::quat_cast(glm::mat3(m) * xrAEjesRed));
	}
	// Si un mando no esta disponible se usa el offset de PC respecto a la cabeza.
	inline bool PoseDesdeVR(const vr::PoseXR& cabeza, const vr::PoseXR* izq, const vr::PoseXR* der,
		const vr::Anclaje& anclaje, net::PoseEntry& out)
	{
		net::PoseEntry base;
		base.head = TransformDesdeXR(cabeza, anclaje);
		if (!TransformValido(base.head)) return false;
		const glm::vec3 fwd = RotDe(base.head) * glm::vec3(0, 1, 0);
		net::PoseEntry fallback;
		if (!PoseDesdeEscritorio(PosDe(base.head), fwd, fallback)) return false;
		base.leftHand = izq ? TransformDesdeXR(*izq, anclaje) : fallback.leftHand;
		base.rightHand = der ? TransformDesdeXR(*der, anclaje) : fallback.rightHand;
		if (!TransformValido(base.leftHand)) base.leftHand = fallback.leftHand;
		if (!TransformValido(base.rightHand)) base.rightHand = fallback.rightHand;
		out = base;
		return true;
	}

	// ------------------------------------------------------------------ interpolacion de remotos
	inline bool PoseIgual(const net::PoseEntry& a, const net::PoseEntry& b)
	{
		return std::memcmp(&a.head, &b.head, sizeof(net::Transform7)) == 0 &&
			std::memcmp(&a.leftHand, &b.leftHand, sizeof(net::Transform7)) == 0 &&
			std::memcmp(&a.rightHand, &b.rightHand, sizeof(net::Transform7)) == 0;
	}

	inline net::Transform7 MezclaTransform(const net::Transform7& a, const net::Transform7& b, float t)
	{
		const glm::vec3 p = glm::mix(PosDe(a), PosDe(b), t);
		return ATransform(p, glm::slerp(RotDe(a), RotDe(b), t));
	}
	inline net::PoseEntry MezclaPose(const net::PoseEntry& a, const net::PoseEntry& b, float t)
	{
		net::PoseEntry r; r.peer = b.peer;
		r.head = MezclaTransform(a.head, b.head, t);
		r.leftHand = MezclaTransform(a.leftHand, b.leftHand, t);
		r.rightHand = MezclaTransform(a.rightHand, b.rightHand, t);
		return r;
	}

	// Guarda las ultimas muestras (con su hora de llegada) de cada jugador remoto y las reproduce con un
	// retraso fijo: lerp de posicion + slerp de rotacion. Sin extrapolar: tras la ultima muestra se mantiene.
	class InterpoladorRemotos
	{
	public:
		static constexpr size_t kMaxMuestras = 8;

		// 'snapshot' = RemotePoses() del servicio. El jugador local se excluye SIEMPRE (nunca se dibuja ni se
		// interpola el propio). Los peers que ya no estan en el snapshot se descartan.
		void Actualizar(const std::map<net::PeerId, net::PoseEntry>& snapshot, net::PeerId local, double ahora)
		{
			for (auto it = datos_.begin(); it != datos_.end();)
				if (snapshot.find(it->first) == snapshot.end()) it = datos_.erase(it); else ++it;
			for (const auto& kv : snapshot)
			{
				if (kv.first == local || kv.first == net::kUnassignedPeerId || !PoseValida(kv.second)) continue;
				auto& cola = datos_[kv.first];
				if (!cola.empty() && PoseIgual(cola.back().pose, kv.second)) continue;
				cola.push_back({ ahora, kv.second });
				while (cola.size() > kMaxMuestras) cola.pop_front();
			}
		}
		void Quitar(net::PeerId p) { datos_.erase(p); }
		void Limpiar() { datos_.clear(); }
		size_t Cantidad() const { return datos_.size(); }
		std::vector<net::PeerId> Peers() const { std::vector<net::PeerId> v; for (const auto& kv : datos_) v.push_back(kv.first); return v; }

		bool Muestrear(net::PeerId peer, double ahora, net::PoseEntry& out) const
		{
			const auto it = datos_.find(peer);
			if (it == datos_.end() || it->second.empty()) return false;
			const auto& c = it->second;
			const double t = ahora - kRetardoInterpolacion;
			if (t <= c.front().t) { out = c.front().pose; out.peer = peer; return true; }
			if (t >= c.back().t) { out = c.back().pose; out.peer = peer; return true; }
			for (size_t i = 1; i < c.size(); ++i)
				if (t <= c[i].t)
				{
					const double span = c[i].t - c[i - 1].t;
					const float f = span > 1e-9 ? static_cast<float>((t - c[i - 1].t) / span) : 1.0f;
					out = MezclaPose(c[i - 1].pose, c[i].pose, std::clamp(f, 0.0f, 1.0f)); out.peer = peer; return true;
				}
			out = c.back().pose; out.peer = peer; return true;
		}
	private:
		struct Muestra { double t; net::PoseEntry pose; };
		std::map<net::PeerId, std::deque<Muestra>> datos_;
	};

	// ------------------------------------------------------------------ puertas (host = autoridad)
	class IPuertasEscena
	{
	public:
		virtual ~IPuertasEscena() = default;
		virtual size_t Cantidad() const = 0;
		virtual std::string Id(size_t i) const = 0;              // id estable de escena
		virtual bool Abierta(size_t i) const = 0;                // abierta o abriendose
		virtual void FijarAbierta(size_t i, bool abrir) = 0;     // idempotente
		virtual glm::vec3 Punto(size_t i) const = 0;             // punto de interaccion (mundo, Z arriba)
	};

	enum class ResultadoSolicitud { Aplicada, IdDesconocido, FueraDeAlcance, SinPose, NoEsDueno, Ocupado, Invalida };

	class SincroPuertas
	{
	public:
		static constexpr float kAlcanceHost = 3.0f;   // RADIO_INTERACCION (2 m) + margen de latencia

		// Construye la tabla ID de red -> puerta. Devuelve false (y 'error') si hay IDs repetidos, colisiones de
		// hash o IDs reservados; las puertas conflictivas quedan EXCLUIDAS de la red (siguen funcionando en local).
		bool Reconstruir(const IPuertasEscena& p, std::string& error)
		{
			porId_.clear(); idDe_.assign(p.Cantidad(), 0); ultimo_.assign(p.Cantidad(), -1);
			std::map<uint32_t, int> cuenta; error.clear();
			std::map<std::string, int> nombres;
			for (size_t i = 0; i < p.Cantidad(); ++i)
			{
				const std::string n = p.Id(i);
				const uint32_t h = HashId(n);
				++cuenta[h]; ++nombres[n];
				idDe_[i] = h;
			}
			bool todoBien = true;
			for (size_t i = 0; i < p.Cantidad(); ++i)
			{
				const std::string n = p.Id(i);
				if (n.empty() || nombres[n] > 1 || cuenta[idDe_[i]] > 1 || IdReservado(idDe_[i]))
				{ error += (error.empty() ? "" : "; ") + std::string("puerta '") + n + "' excluida de la red (id vacio/repetido/colision)"; todoBien = false; continue; }
				porId_[idDe_[i]] = i; ultimo_[i] = p.Abierta(i) ? 1 : 0;
			}
			return todoBien;
		}
		size_t Cantidad() const { return porId_.size(); }
		bool IndiceDe(uint32_t id, size_t& idx) const { auto it = porId_.find(id); if (it == porId_.end()) return false; idx = it->second; return true; }
		uint32_t IdRed(size_t idx) const { return idx < idDe_.size() ? idDe_[idx] : 0; }
		bool EnRed(size_t idx) const { return idx < idDe_.size() && porId_.count(idDe_[idx]) > 0; }

		static EventoSalida Solicitud(uint32_t id) { EventoSalida e; e.tipo = kEvPuertaSolicitud; e.objetivo = id; return e; }

		// HOST: valida y aplica. 'cabeza' = ultima pose conocida del solicitante (nullptr si no hay).
		ResultadoSolicitud HostSolicitud(IPuertasEscena& p, uint32_t id, const glm::vec3* cabeza)
		{
			size_t i; if (!IndiceDe(id, i) || i >= p.Cantidad()) return ResultadoSolicitud::IdDesconocido;
			if (!cabeza) return ResultadoSolicitud::SinPose;
			if (glm::distance(*cabeza, p.Punto(i)) > kAlcanceHost) return ResultadoSolicitud::FueraDeAlcance;
			p.FijarAbierta(i, !p.Abierta(i));
			return ResultadoSolicitud::Aplicada;
		}
		// HOST: emite el estado de las puertas que cambiaron (por E del host, por clientes o por depuracion).
		void HostDetectarCambios(const IPuertasEscena& p, std::vector<EventoSalida>& out)
		{
			for (const auto& kv : porId_)
			{
				const size_t i = kv.second; if (i >= p.Cantidad()) continue;
				const int abierta = p.Abierta(i) ? 1 : 0;
				if (abierta == ultimo_[i]) continue;
				ultimo_[i] = abierta;
				EventoSalida e; e.tipo = kEvPuertaEstado; e.objetivo = kv.first; e.payload = { uint8_t(abierta) };
				out.push_back(std::move(e));
			}
		}
		// CLIENTE: aplica el estado que dicta el host (idempotente).
		bool AplicarEstado(IPuertasEscena& p, uint32_t id, const std::vector<uint8_t>& payload)
		{
			size_t i; if (payload.size() != 1 || payload[0] > 1 || !IndiceDe(id, i) || i >= p.Cantidad()) return false;
			p.FijarAbierta(i, payload[0] == 1);
			return true;
		}
		void EstadoRecuperacion(const IPuertasEscena& p, std::vector<net::StateEntry>& out) const
		{
			for (const auto& kv : porId_) if (kv.second < p.Cantidad()) out.push_back({ kv.first, p.Abierta(kv.second) ? 1 : 0 });
		}
		void AplicarRecuperacion(IPuertasEscena& p, const std::vector<net::StateEntry>& v)
		{
			for (const auto& e : v) { size_t i; if (IndiceDe(e.objectId, i) && i < p.Cantidad() && (e.value == 0 || e.value == 1)) p.FijarAbierta(i, e.value == 1); }
		}
	private:
		std::map<uint32_t, size_t> porId_;
		std::vector<uint32_t> idDe_;
		std::vector<int> ultimo_;
	};

	// ------------------------------------------------------------------ propiedad temporal de objetos
	struct ObjetoCompartido
	{
		uint32_t id = 0; glm::vec3 pos{ 0 };
		net::PeerId dueno = kSinDueno; double expira = 0.0;
	};

	// Solo la usa el HOST. Un objeto tiene como mucho un dueno; la propiedad es una CONCESION que caduca si
	// el dueno deja de moverlo, y se libera al soltar, al caducar o al desconectarse.
	class ArbitroPropiedad
	{
	public:
		static constexpr float kAlcance = 3.0f;          // cabeza <-> objeto para pedirlo
		static constexpr float kAlcanceAcarreo = 4.0f;   // cabeza <-> objeto mientras se mueve
		static constexpr float kSaltoMaximo = 5.0f;      // m entre dos movimientos consecutivos
		static constexpr double kConcesion = 10.0;       // s sin moverlo = se libera

		void Definir(uint32_t id, const glm::vec3& pos) { ObjetoCompartido o; o.id = id; o.pos = pos; obj_[id] = o; cambios_.push_back(id); }
		const ObjetoCompartido* Buscar(uint32_t id) const { auto it = obj_.find(id); return it == obj_.end() ? nullptr : &it->second; }

		ResultadoSolicitud Solicitar(uint32_t id, net::PeerId quien, const glm::vec3* cabeza, double ahora)
		{
			auto it = obj_.find(id); if (it == obj_.end()) return ResultadoSolicitud::IdDesconocido;
			if (!cabeza || !CoordenadaValida(*cabeza)) return ResultadoSolicitud::SinPose;
			ObjetoCompartido& o = it->second;
			if (o.dueno != kSinDueno && o.dueno != quien) return ResultadoSolicitud::Ocupado;
			if (glm::distance(*cabeza, o.pos) > kAlcance) return ResultadoSolicitud::FueraDeAlcance;
			if (o.dueno != quien) { o.dueno = quien; cambios_.push_back(id); }
			o.expira = ahora + kConcesion;
			return ResultadoSolicitud::Aplicada;
		}
		ResultadoSolicitud Liberar(uint32_t id, net::PeerId quien)
		{
			auto it = obj_.find(id); if (it == obj_.end()) return ResultadoSolicitud::IdDesconocido;
			if (it->second.dueno != quien) return ResultadoSolicitud::NoEsDueno;
			it->second.dueno = kSinDueno; cambios_.push_back(id);
			return ResultadoSolicitud::Aplicada;
		}
		ResultadoSolicitud Mover(uint32_t id, net::PeerId quien, const glm::vec3& nueva, const glm::vec3* cabeza, double ahora)
		{
			auto it = obj_.find(id); if (it == obj_.end()) return ResultadoSolicitud::IdDesconocido;
			ObjetoCompartido& o = it->second;
			if (o.dueno != quien) return ResultadoSolicitud::NoEsDueno;
			if (!CoordenadaValida(nueva)) return ResultadoSolicitud::Invalida;
			if (!cabeza || !CoordenadaValida(*cabeza)) return ResultadoSolicitud::SinPose;
			if (glm::distance(*cabeza, nueva) > kAlcanceAcarreo || glm::distance(o.pos, nueva) > kSaltoMaximo) return ResultadoSolicitud::FueraDeAlcance;
			o.pos = nueva; o.expira = ahora + kConcesion; cambios_.push_back(id);
			return ResultadoSolicitud::Aplicada;
		}
		// Desconexion o expulsion: libera todo lo que tuviera ese jugador.
		void PeerDesconectado(net::PeerId p)
		{
			for (auto& kv : obj_) if (kv.second.dueno == p) { kv.second.dueno = kSinDueno; cambios_.push_back(kv.first); }
		}
		void Tick(double ahora)
		{
			for (auto& kv : obj_) if (kv.second.dueno != kSinDueno && ahora > kv.second.expira) { kv.second.dueno = kSinDueno; cambios_.push_back(kv.first); }
		}
		// Ids cambiados desde la ultima llamada (sin repetidos) para difundirlos.
		std::vector<uint32_t> TomarCambios()
		{
			std::vector<uint32_t> r; for (uint32_t id : cambios_) if (std::find(r.begin(), r.end(), id) == r.end()) r.push_back(id);
			cambios_.clear(); return r;
		}
		void EstadoRecuperacion(std::vector<net::StateEntry>& out) const
		{
			for (const auto& kv : obj_)
			{
				const ObjetoCompartido& o = kv.second;
				out.push_back({ o.id, o.dueno == kSinDueno ? 0 : int32_t(o.dueno) + 1 });
				out.push_back({ o.id + 1, int32_t(std::lround(o.pos.x * 1000.0f)) });
				out.push_back({ o.id + 2, int32_t(std::lround(o.pos.y * 1000.0f)) });
				out.push_back({ o.id + 3, int32_t(std::lround(o.pos.z * 1000.0f)) });
			}
		}
	private:
		std::map<uint32_t, ObjetoCompartido> obj_;
		std::vector<uint32_t> cambios_;
	};

	inline EventoSalida EventoEstadoObjeto(const ObjetoCompartido& o)
	{
		EventoSalida e; e.tipo = kEvPropEstado; e.objetivo = o.id; e.payload = CodificarEstadoObjeto(o.dueno, o.pos); return e;
	}

	// Vista del objeto en el CLIENTE (y en el host para dibujar): refleja lo que dicta el host.
	struct VistaObjetoCompartido
	{
		bool conocido = false; net::PeerId dueno = kSinDueno; glm::vec3 pos{ 0 };

		bool AplicarEstado(const std::vector<uint8_t>& payload)
		{
			net::PeerId d; glm::vec3 p; if (!DecodificarEstadoObjeto(payload, d, p)) return false;
			conocido = true; dueno = d; pos = p; return true;
		}
		bool AplicarRecuperacion(const std::vector<net::StateEntry>& v)
		{
			int hay = 0; int32_t d = 0, x = 0, y = 0, z = 0;
			for (const auto& e : v)
			{
				if (e.objectId == kObjetoPrueba) { d = e.value; ++hay; }
				else if (e.objectId == kObjetoPrueba + 1) { x = e.value; ++hay; }
				else if (e.objectId == kObjetoPrueba + 2) { y = e.value; ++hay; }
				else if (e.objectId == kObjetoPrueba + 3) { z = e.value; ++hay; }
			}
			if (hay != 4 || d < 0 || d > int32_t(net::kMaxPeerId) + 1) return false;
			const glm::vec3 p(x / 1000.0f, y / 1000.0f, z / 1000.0f);
			if (!CoordenadaValida(p)) return false;
			conocido = true; dueno = d == 0 ? kSinDueno : net::PeerId(d - 1); pos = p; return true;
		}
		void Reiniciar() { *this = VistaObjetoCompartido{}; }
	};

	// Rayo (origen, direccion) contra la esfera del objeto: para elegir el objeto compartido con ratón/mando.
	inline bool RayoTocaEsfera(const glm::vec3& o, const glm::vec3& dir, const glm::vec3& c, float radio, float maxDist, float* tOut = nullptr)
	{
		if (!(glm::length(dir) > 1e-6f)) return false;
		const glm::vec3 d = glm::normalize(dir), oc = c - o;
		const float t = glm::dot(oc, d);
		if (t < 0.0f || t > maxDist + radio) return false;
		const float dist2 = glm::dot(oc, oc) - t * t;
		if (dist2 > radio * radio) return false;
		if (tOut) *tOut = t;
		return true;
	}

	// Lineas (x,y,z,r,g,b,a) de una caja orientada: la usa el avatar y el objeto compartido (shader de ejes).
	inline void AnadirCajaLineas(std::vector<float>& v, const glm::vec3& centro, const glm::quat& rot, const glm::vec3& mitad, const glm::vec4& c)
	{
		glm::vec3 esq[8];
		for (int i = 0; i < 8; ++i)
			esq[i] = centro + rot * glm::vec3((i & 1 ? 1 : -1) * mitad.x, (i & 2 ? 1 : -1) * mitad.y, (i & 4 ? 1 : -1) * mitad.z);
		static const int aristas[12][2] = { {0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7} };
		for (const auto& a : aristas)
			for (int k = 0; k < 2; ++k) { const glm::vec3& p = esq[a[k]]; v.insert(v.end(), { p.x, p.y, p.z, c.r, c.g, c.b, c.a }); }
	}
	inline void AnadirLinea(std::vector<float>& v, const glm::vec3& a, const glm::vec3& b, const glm::vec4& c)
	{
		v.insert(v.end(), { a.x, a.y, a.z, c.r, c.g, c.b, c.a, b.x, b.y, b.z, c.r, c.g, c.b, c.a });
	}
	// Avatar sencillo: cabeza (con linea de mirada), torso que cuelga de la cabeza (solo guinada) y dos manos.
	inline void ConstruirAvatar(std::vector<float>& v, const net::PoseEntry& p, const glm::vec4& color)
	{
		const glm::vec3 cab = PosDe(p.head); const glm::quat rc = RotDe(p.head);
		AnadirCajaLineas(v, cab, rc, glm::vec3(0.11f, 0.11f, 0.12f), color);
		AnadirLinea(v, cab, cab + rc * glm::vec3(0, 0.5f, 0), glm::vec4(1, 1, 1, 1));
		const glm::vec3 fwd = rc * glm::vec3(0, 1, 0);
		const glm::quat guinada = RotacionDesdeAdelante(glm::vec3(fwd.x, fwd.y, 0.0f));
		AnadirCajaLineas(v, cab - glm::vec3(0, 0, 0.45f), guinada, glm::vec3(0.18f, 0.09f, 0.28f), color);
		AnadirCajaLineas(v, PosDe(p.leftHand), RotDe(p.leftHand), glm::vec3(0.04f), color);
		AnadirCajaLineas(v, PosDe(p.rightHand), RotDe(p.rightHand), glm::vec3(0.04f), color);
	}
}
