#include "src/ui/RuntimeUi.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include <GLFW/glfw3.h>

#include "src/networking/preferences/NetworkPreferences.h"
#include "src/gameplay/hirity/HiritiAnimation.h"
#include "src/networking/replication/NetAdapters.h"
#include "src/networking/replication/NetGame.h"
#include "src/physics/PhysicsScene.h"
#include "src/scene/SceneLoader.h"
#include "src/ui/RuntimeUiLogic.h"

namespace runtimeui
{
// Panel de multijugador: seleccion Offline/Host/Cliente, IP y puerto, y diagnostico. Offline es el valor por defecto.
void DrawMultiplayerPanel(const MultiplayerContext& context, float uiScale)
{
	auto& networkGame = context.session;
	auto& pendingNetworkConfig = context.pendingConfig;
	using netgame::NetworkPhase;
	const netgame::Diagnostico d = networkGame.ObtenerDiagnostico();
	ImGui::SetNextWindowPos(ImVec2(12.0f * uiScale, 12.0f * uiScale), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(360.0f * uiScale, 0.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("Multijugador", &context.panelVisible);
	ImGui::Text("Modo: %s | Estado: %s", d.modo.c_str(), d.estado.c_str());
	if (d.fase != NetworkPhase::Offline)
	{
		if (d.modo == "Host") ImGui::Text("Escucha UDP: %s:%u (todas las interfaces)", d.endpoint.c_str(), static_cast<unsigned>(d.puerto));
		else ImGui::Text("Servidor: %s:%u", d.endpoint.c_str(), static_cast<unsigned>(d.puerto));
	}
	if (!d.mensaje.empty()) ImGui::TextWrapped("%s", d.mensaje.c_str());
	const bool libre = d.fase == NetworkPhase::Offline || d.fase == NetworkPhase::Error;
	static int modo = -1; static char ip[64]; static int puerto = netgame::kPuertoPorDefecto; static char nombre[33];
	static std::string errorEntrada;
	if (modo < 0)
	{
		modo = pendingNetworkConfig.rol == net::Role::Host ? 1 : (pendingNetworkConfig.rol == net::Role::Client ? 2 : 0);
		netgame::LastEndpoint ultimo;
		const bool hayUltimo = netgame::CargarUltimoEndpoint(ultimo);
		const std::string& hostInicial = pendingNetworkConfig.rol == net::Role::Client ? pendingNetworkConfig.host : (hayUltimo ? ultimo.host : pendingNetworkConfig.host);
		std::snprintf(ip, sizeof ip, "%s", hostInicial.c_str());
		std::snprintf(nombre, sizeof nombre, "%s", pendingNetworkConfig.nombre.c_str());
		puerto = pendingNetworkConfig.rol == net::Role::Client || pendingNetworkConfig.rol == net::Role::Host
			? pendingNetworkConfig.puerto : (hayUltimo ? ultimo.puerto : pendingNetworkConfig.puerto);
	}
	if (libre)
	{
		ImGui::RadioButton("Offline", &modo, 0); ImGui::SameLine();
		ImGui::RadioButton("Host", &modo, 1); ImGui::SameLine();
		ImGui::RadioButton("Cliente", &modo, 2);
		if (modo == 2) ImGui::InputText("IP del host", ip, sizeof ip);
		if (modo != 0)
		{
			ImGui::InputInt("Puerto", &puerto);
			ImGui::InputText("Nombre", nombre, sizeof nombre);
			static int enet = -1;
			if (enet < 0) enet = netgame::NetworkGame::EnetDisponible() ? 1 : 0;   // solo al elegir red; no inicializa ENet
			if (!enet) ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "ENet no incluido en esta compilacion.");
			if (!errorEntrada.empty()) ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", errorEntrada.c_str());
			if (ImGui::Button("Iniciar"))
			{
				errorEntrada.clear();
				netgame::NetworkConfig cfg;
				if (BuildNetworkStartRequest(modo, nombre, ip, puerto, cfg, errorEntrada))
				{
					if (cfg.rol == net::Role::Client)
					{
						std::string errorGuardado;
						if (!netgame::GuardarUltimoEndpoint({ cfg.host, cfg.puerto }, errorGuardado))
							errorEntrada = "No se guardo la ultima direccion: " + errorGuardado;
					}
					pendingNetworkConfig = cfg; context.startNetwork(cfg);
				}
			}
		}
	}
	else if (ImGui::Button(d.fase == NetworkPhase::Arrancando ? "Cancelar" : "Desconectar")) context.stopNetwork();
	if (d.fase == NetworkPhase::Activa)
	{
		ImGui::Separator();
		ImGui::Text("Escena: %s | peer local: %u", d.escena.c_str(), static_cast<unsigned>(d.peerLocal));
		ImGui::Text("Remotos: %zu | puertas en red: %zu", d.remotos, d.puertasEnRed);
		ImGui::Text("Eventos ignorados: %zu | descartados: %zu", d.eventosIgnorados, d.eventosDescartados);
		const auto& obj = networkGame.Objeto();
		if (obj.conocido)
			ImGui::Text("Objeto compartido: %s", obj.dueno == netgame::kSinDueno ? "libre"
				: (networkGame.SoyDuenoDelObjeto() ? "lo tengo yo" : "en manos de otro jugador"));
		ImGui::TextUnformatted("E / gatillo apuntando al cubo amarillo: coger o soltar.");
	}
	ImGui::End();
}

// Snapshot solo de lectura para el panel F4. Bullet se consulta en una captura
// agrupada y acotada; la tabla no vuelve a consultar cuerpos durante el dibujo.
void DrawDynamicObjectsPanel(const DynamicObjectsContext& context)
{
	auto& networkGame = context.session;
	const auto& physicsScene = context.physicsScene;
	const auto& sceneObjects = context.sceneObjects;
	struct Fila
	{
		std::string id;
		uint32_t idRed = 0;
		net::PeerId propietario = netgame::kSinDueno;
		fisica::BodyPose aplicado;
		fisica::BodyPose autoritativo;
		bool tieneAutoritativo = false;
		bool activo = false;
		bool sujeto = false;
		bool hiriti = false;
		const char* estadoSeguimiento = "";
	};
	static std::vector<Fila> filas;
	static double siguienteActualizacion = 0.0;
	const double ahora = glfwGetTime();
	if (ahora >= siguienteActualizacion)
	{
		filas.clear();
		siguienteActualizacion = ahora + 0.5;
		if (physicsScene)
		{
			std::vector<fisica::PhysicsScene::DynamicState> cuerpos;
			physicsScene->CapturarEstadosDinamicos(cuerpos);
			const auto remotos = networkGame.EsCliente()
				? networkGame.EstadosFisicosRemotos() : std::map<uint32_t, net::DynamicStateEntry>{};
			for (const auto& cuerpo : cuerpos)
			{
				if (cuerpo.indiceObjeto < 0 || cuerpo.indiceObjeto >= static_cast<int>(sceneObjects.size())) continue;
				const SceneObject& objeto = sceneObjects[static_cast<std::size_t>(cuerpo.indiceObjeto)];
				Fila fila;
				fila.id = objeto.id;
				fila.idRed = netgame::HashId(objeto.id);
				fila.propietario = networkGame.Activa()
					? networkGame.PropietarioDinamico(fila.idRed) : netgame::kSinDueno;
				fila.aplicado = cuerpo.pose;
				fila.activo = cuerpo.activo;
				fila.sujeto = cuerpo.agarrado || fila.propietario != netgame::kSinDueno;
				fila.hiriti = objeto.id == "hiriti";
				if (fila.hiriti)
					fila.estadoSeguimiento = HiritiFollowStatus(networkGame.EsCliente(), context.hiritiFollowEnabled,
						context.hiriti.SeguimientoSuspendido());
				if (networkGame.EsCliente())
				{
					const auto remoto = remotos.find(fila.idRed);
					if (remoto != remotos.end() && netgame::EstadoFisicoValido(remoto->second))
					{
						fila.autoritativo.posicion = netgame::PosDe(remoto->second.transform);
						fila.autoritativo.rotacion = netgame::RotDe(remoto->second.transform);
						fila.tieneAutoritativo = true;
					}
				}
				else
				{
					fila.autoritativo = cuerpo.pose;
					fila.tieneAutoritativo = true;
				}
				filas.push_back(std::move(fila));
			}
			OrderAndLimitDynamicRows(filas);
		}
	}

	ImGui::Begin("Objetos dinamicos | F4");
	ImGui::Text("Modo: %s | maximo 8 | refresco 500 ms",
		networkGame.EsHost() ? "host" : networkGame.EsCliente() ? "client" : "offline");
	if (filas.empty()) ImGui::TextUnformatted("Sin objetos dinamicos en la escena.");
	for (const Fila& fila : filas)
	{
		const char* rol = networkGame.EsHost() ? "host" : networkGame.EsCliente() ? "client" : "offline";
		const char* cuerpo = networkGame.EsCliente() ? "replica cinematica" : "body dinamico";
		if (fila.propietario == netgame::kSinDueno)
			ImGui::Text("%s  id=%08x  owner=libre  %s/%s  %s%s", fila.id.c_str(), fila.idRed,
				rol, cuerpo, fila.sujeto ? "SUJETO" : (fila.activo ? "activo" : "reposo"),
				fila.estadoSeguimiento);
		else
			ImGui::Text("%s  id=%08x  owner=%u  %s/%s  %s%s", fila.id.c_str(), fila.idRed,
				static_cast<unsigned>(fila.propietario), rol, cuerpo,
				fila.sujeto ? "SUJETO" : (fila.activo ? "activo" : "reposo"),
				fila.estadoSeguimiento);
		const auto dibujarPose = [](const char* nombre, const fisica::BodyPose& pose)
		{
			ImGui::Text("%s p(%.2f,%.2f,%.2f) q(%.2f,%.2f,%.2f,%.2f)", nombre,
				pose.posicion.x, pose.posicion.y, pose.posicion.z,
				pose.rotacion.x, pose.rotacion.y, pose.rotacion.z, pose.rotacion.w);
		};
		if (fila.tieneAutoritativo) dibujarPose("  auth", fila.autoritativo);
		else ImGui::TextUnformatted("  auth: esperando snapshot del host");
		dibujarPose("  aplicado", fila.aplicado);
	}
	ImGui::End();
}
}
