#pragma once

// Graphics-free decisions used by the multiplayer panels.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "src/networking/replication/NetGameLogic.h"

namespace runtimeui
{
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
