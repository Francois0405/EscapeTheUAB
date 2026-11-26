#pragma once

// Graphics-free decisions used by the multiplayer panels.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "src/gameplay/interaction/ThrowCharge.h"
#include "src/networking/replication/NetGameLogic.h"

namespace runtimeui
{
	// Que se dibuja alrededor del centro de la pantalla en este fotograma.
	struct ReticleState
	{
		bool reticle = false;     // cuadrado central
		bool arc = false;         // anillo de carga
		float fraction = 0.0f;
		bool full = false;
	};

	inline ReticleState EvaluateReticle(bool desktopGameplay, bool imguiCapture, bool holdsObject,
		bool charging, float chargeSeconds)
	{
		ReticleState estado;
		estado.reticle = desktopGameplay && !imguiCapture;
		estado.arc = estado.reticle && holdsObject && charging;
		if (!estado.arc) return estado;
		estado.fraction = game::interaction::ThrowChargeFraction(chargeSeconds);
		estado.full = estado.fraction >= 1.0f;
		return estado;
	}

	// mode: 1 = host, 2 = client (the "Offline / Host / Cliente" radio buttons).
	inline bool BuildNetworkStartRequest(int mode, const char* name, const char* ip, int port,
		netgame::NetworkConfig& config, std::string& error)
	{
		config.rol = mode == 1 ? net::Role::Host : net::Role::Client;
		config.nombre = name[0] ? name : "jugador";
		std::string parseError;
		if (!netgame::IsValidPort(port)) error = "Puerto fuera de rango: usa un valor entre 1 y 65535.";
		else if (config.rol == net::Role::Client && !netgame::ParseEndpoint(std::string(ip) + ":" + std::to_string(port), config.host, config.puerto, parseError))
			error = parseError;
		else
		{
			config.puerto = static_cast<uint16_t>(port);
			return true;
		}
		return false;
	}

	inline const char* HiritiFollowStatus(bool isClient, bool followEnabled, bool followSuspended)
	{
		if (isClient) return " | Hiriti: seguimiento autoritativo desconocido (host)";
		if (!followEnabled) return " | Hiriti: seguimiento local inactivo";
		if (followSuspended) return " | Hiriti: seguimiento local suspendido";
		return " | Hiriti: seguimiento local elegible";
	}

	// Indicador de carga pixel art: anillo de bloques cuadrados sobre una rejilla.
	constexpr int kChargeSegments = 20;     // multiplo de 4: 25/50/75 % son exactos
	constexpr int kChargeRadiusCells = 5;
	constexpr float kChargeCellUnits = 4.0f;   // lado del bloque en unidades de UI

	struct Cell { int x; int y; };   // en celdas desde el centro; y crece hacia abajo, como ImGui

	// Segmento 0 arriba; los siguientes avanzan en sentido horario.
	inline Cell ChargeSegmentCell(int index)
	{
		// Un cuadrante del circulo pixelado; los otros tres son giros de 90 grados.
		constexpr Cell cuadrante[kChargeSegments / 4] = { { 0, -5 }, { 2, -5 }, { 3, -4 }, { 4, -3 }, { 5, -2 } };
		Cell celda = cuadrante[index % (kChargeSegments / 4)];
		for (int giro = index / (kChargeSegments / 4); giro > 0; --giro) celda = { -celda.y, celda.x };
		return celda;
	}

	// El ultimo bloque se reserva para la carga completa.
	inline int LitChargeSegments(float fraction, bool full)
	{
		if (full) return kChargeSegments;
		const int encendidos = static_cast<int>(std::floor(std::clamp(fraction, 0.0f, 1.0f) * kChargeSegments + 1.0e-3f));
		return std::min(encendidos, kChargeSegments - 1);
	}

	// Lado del bloque en pixeles fisicos: entero para que todos midan igual y sin bordes suavizados.
	inline int ChargeCellPixels(float uiScale, float framebufferScale)
	{
		return std::max(2, static_cast<int>(std::lround(kChargeCellUnits * uiScale * framebufferScale)));
	}

	// Held objects first, then active ones, then by id; the F4 table shows at most 8.
	template <typename Row>
	void OrderAndLimitDynamicRows(std::vector<Row>& rows, std::size_t limit = 8)
	{
		std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b)
		{
			if (a.sujeto != b.sujeto) return a.sujeto > b.sujeto;
			if (a.activo != b.activo) return a.activo > b.activo;
			return a.id < b.id;
		});
		if (rows.size() > limit) rows.resize(limit);
	}
}
