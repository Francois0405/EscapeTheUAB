#include "src/ui/RuntimeUi.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include <GLFW/glfw3.h>

#include "Network/PreferenciasRed.h"
#include "src/gameplay/hirity/AnimacionHiriti.h"
#include "src/networking/replication/NetAdaptadores.h"
#include "src/networking/replication/NetJuego.h"
#include "src/physics/EscenaFisica.h"
#include "src/scene/SceneLoader.h"
#include "src/ui/RuntimeUiLogic.h"

namespace runtimeui
{
// Panel de multijugador: seleccion Offline/Host/Cliente, IP y puerto, y diagnostico. Offline es el valor por defecto.
void DrawMultiplayerPanel(const MultiplayerContext& context, float uiScale)
{
	auto& redJuego = context.session;
	auto& configRedPendiente = context.pendingConfig;
	using netjuego::FaseRed;
	const netjuego::Diagnostico d = redJuego.ObtenerDiagnostico();
	ImGui::SetNextWindowPos(ImVec2(12.0f * uiScale, 12.0f * uiScale), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(360.0f * uiScale, 0.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("Multijugador", &context.panelVisible);
	ImGui::Text("Modo: %s | Estado: %s", d.modo.c_str(), d.estado.c_str());
	if (d.fase != FaseRed::Offline)
	{
		if (d.modo == "Host") ImGui::Text("Escucha UDP: %s:%u (todas las interfaces)", d.endpoint.c_str(), static_cast<unsigned>(d.puerto));
		else ImGui::Text("Servidor: %s:%u", d.endpoint.c_str(), static_cast<unsigned>(d.puerto));
	}
	if (!d.mensaje.empty()) ImGui::TextWrapped("%s", d.mensaje.c_str());
	const bool libre = d.fase == FaseRed::Offline || d.fase == FaseRed::Error;
	static int modo = -1; static char ip[64]; static int puerto = netjuego::kPuertoPorDefecto; static char nombre[33];
	static std::string errorEntrada;
	if (modo < 0)
	{
		modo = configRedPendiente.rol == net::Role::Host ? 1 : (configRedPendiente.rol == net::Role::Client ? 2 : 0);
		netjuego::UltimoEndpoint ultimo;
		const bool hayUltimo = netjuego::CargarUltimoEndpoint(ultimo);
		const std::string& hostInicial = configRedPendiente.rol == net::Role::Client ? configRedPendiente.host : (hayUltimo ? ultimo.host : configRedPendiente.host);
		std::snprintf(ip, sizeof ip, "%s", hostInicial.c_str());
		std::snprintf(nombre, sizeof nombre, "%s", configRedPendiente.nombre.c_str());
		puerto = configRedPendiente.rol == net::Role::Client || configRedPendiente.rol == net::Role::Host
			? configRedPendiente.puerto : (hayUltimo ? ultimo.puerto : configRedPendiente.puerto);
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
			if (enet < 0) enet = netjuego::RedJuego::EnetDisponible() ? 1 : 0;   // solo al elegir red; no inicializa ENet
			if (!enet) ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "ENet no incluido en esta compilacion.");
			if (!errorEntrada.empty()) ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", errorEntrada.c_str());
			if (ImGui::Button("Iniciar"))
			{
				errorEntrada.clear();
				netjuego::ConfigRed cfg;
				if (BuildNetworkStartRequest(modo, nombre, ip, puerto, cfg, errorEntrada))
				{
					if (cfg.rol == net::Role::Client)
					{
						std::string errorGuardado;
						if (!netjuego::GuardarUltimoEndpoint({ cfg.host, cfg.puerto }, errorGuardado))
							errorEntrada = "No se guardo la ultima direccion: " + errorGuardado;
					}
					configRedPendiente = cfg; context.startNetwork(cfg);
				}
			}
		}
	}
	else if (ImGui::Button(d.fase == FaseRed::Arrancando ? "Cancelar" : "Desconectar")) context.stopNetwork();
	if (d.fase == FaseRed::Activa)
	{
		ImGui::Separator();
		ImGui::Text("Escena: %s | peer local: %u", d.escena.c_str(), static_cast<unsigned>(d.peerLocal));
		ImGui::Text("Remotos: %zu | puertas en red: %zu", d.remotos, d.puertasEnRed);
		ImGui::Text("Eventos ignorados: %zu | descartados: %zu", d.eventosIgnorados, d.eventosDescartados);
		const auto& obj = redJuego.Objeto();
		if (obj.conocido)
			ImGui::Text("Objeto compartido: %s", obj.dueno == netjuego::kSinDueno ? "libre"
				: (redJuego.SoyDuenoDelObjeto() ? "lo tengo yo" : "en manos de otro jugador"));
		ImGui::TextUnformatted("E / gatillo apuntando al cubo amarillo: coger o soltar.");
	}
	ImGui::End();
}

// Snapshot solo de lectura para el panel F4. Bullet se consulta en una captura
// agrupada y acotada; la tabla no vuelve a consultar cuerpos durante el dibujo.
void DrawDynamicObjectsPanel(const DynamicObjectsContext& context)
{
	auto& redJuego = context.session;
	const auto& escenaFisica = context.physicsScene;
	const auto& objetosEscena = context.sceneObjects;
	struct Fila
	{
		std::string id;
		uint32_t idRed = 0;
		net::PeerId propietario = netjuego::kSinDueno;
		fisica::PoseCuerpo aplicado;
		fisica::PoseCuerpo autoritativo;
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
		if (escenaFisica)
		{
			std::vector<fisica::EscenaFisica::EstadoDinamico> cuerpos;
			escenaFisica->CapturarEstadosDinamicos(cuerpos);
			const auto remotos = redJuego.EsCliente()
				? redJuego.EstadosFisicosRemotos() : std::map<uint32_t, net::DynamicStateEntry>{};
			for (const auto& cuerpo : cuerpos)
			{
				if (cuerpo.indiceObjeto < 0 || cuerpo.indiceObjeto >= static_cast<int>(objetosEscena.size())) continue;
				const SceneObject& objeto = objetosEscena[static_cast<std::size_t>(cuerpo.indiceObjeto)];
				Fila fila;
				fila.id = objeto.id;
				fila.idRed = netjuego::HashId(objeto.id);
				fila.propietario = redJuego.Activa()
					? redJuego.PropietarioDinamico(fila.idRed) : netjuego::kSinDueno;
				fila.aplicado = cuerpo.pose;
				fila.activo = cuerpo.activo;
				fila.sujeto = cuerpo.agarrado || fila.propietario != netjuego::kSinDueno;
				fila.hiriti = objeto.id == "hiriti";
				if (fila.hiriti)
					fila.estadoSeguimiento = HiritiFollowStatus(redJuego.EsCliente(), context.hiritiFollowEnabled,
						context.hiriti.SeguimientoSuspendido());
				if (redJuego.EsCliente())
				{
					const auto remoto = remotos.find(fila.idRed);
					if (remoto != remotos.end() && netjuego::EstadoFisicoValido(remoto->second))
					{
						fila.autoritativo.posicion = netjuego::PosDe(remoto->second.transform);
						fila.autoritativo.rotacion = netjuego::RotDe(remoto->second.transform);
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
		redJuego.EsHost() ? "host" : redJuego.EsCliente() ? "client" : "offline");
	if (filas.empty()) ImGui::TextUnformatted("Sin objetos dinamicos en la escena.");
	for (const Fila& fila : filas)
	{
		const char* rol = redJuego.EsHost() ? "host" : redJuego.EsCliente() ? "client" : "offline";
		const char* cuerpo = redJuego.EsCliente() ? "replica cinematica" : "body dinamico";
		if (fila.propietario == netjuego::kSinDueno)
			ImGui::Text("%s  id=%08x  owner=libre  %s/%s  %s%s", fila.id.c_str(), fila.idRed,
				rol, cuerpo, fila.sujeto ? "SUJETO" : (fila.activo ? "activo" : "reposo"),
				fila.estadoSeguimiento);
		else
			ImGui::Text("%s  id=%08x  owner=%u  %s/%s  %s%s", fila.id.c_str(), fila.idRed,
				static_cast<unsigned>(fila.propietario), rol, cuerpo,
				fila.sujeto ? "SUJETO" : (fila.activo ? "activo" : "reposo"),
				fila.estadoSeguimiento);
		const auto dibujarPose = [](const char* nombre, const fisica::PoseCuerpo& pose)
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
