#include "src/ui/RuntimeUi.h"

#include <GL/glew.h>
#include "imgui.h"

#include <algorithm>
#include <string>
#include <vector>

#include "src/animation/AnimacionPuerta.h"
#include "src/animation/ObjetosAnimadosGLB.h"
#include "src/gameplay/player/MovimientoFPS.h"
#include "src/networking/replication/Game/SceneNetworkState.h"
#include "src/networking/replication/NetJuego.h"
#include "src/physics/EscenaFisica.h"
#include "src/physics/MundoFisico.h"
#include "src/rendering/RenderMetrics.h"
#include "src/scene/SceneLoader.h"
#include "src/vr/OpenXRManager.h"

namespace runtimeui
{
void DrawDebugPanels(const DebugPanelsContext& context)
{
	auto& mostrarMovil = context.showPhone;
	auto& movilPosicion = context.phonePosition;
	auto& movilRotacion = context.phoneRotation;
	auto& movilEscala = context.phoneScale;
	auto& intensidadIdle = context.idleIntensity;
	auto& intensidadWalking = context.walkingIntensity;
	auto& intensidadMouse = context.mouseIntensity;
	auto& deltaFPS = context.frameDeltaSeconds;
	auto& movimientoJugador = context.movement;
	auto& balanceoCamara = context.cameraSway;
	auto& mostrarPorcentajeEstamina = context.showStaminaPercent;
	auto& mostrarContadorFPS = context.showFpsCounter;
	auto& renderizadoOptimizado = context.optimizedRendering;
	auto& ultimoGpuMs = context.lastGpuMs;
	auto& modoDiagnosticoIluminacion = context.lightingView;
	auto& reproductorPuertaDemo = context.doorDemoPlayer;
	auto& anguloPuertaDemo = context.doorDemoAngle;
	auto& mostrarColisionadores = context.showColliders;
	auto& hiritiFollowPlayerEnabled = context.hiritiFollowEnabled;
	auto& hiritiFollowStopDistance = context.hiritiStopDistance;
	auto& hiritiFollowMaxSpeed = context.hiritiMaxSpeed;
	auto& modoEstereoPrueba = context.stereoPreview;
	auto& gestorOpenXR = context.openXR;
	auto& mundoFisico = context.physicsWorld;
	auto& escenaFisica = context.physicsScene;
	auto& demoFisica = context.cubeDemo;
	auto& redJuego = context.session;
	auto& objetosEscena = context.sceneObjects;
	auto& objetosAnimados = context.animatedObjects;

	ImGui::Begin("Movil FPS");
	ImGui::Checkbox("Mostrar movil", &mostrarMovil);
	ImGui::InputFloat3("Posicion", &movilPosicion.x, "%.3f");
	ImGui::InputFloat3("Rotacion (grados)", &movilRotacion.x, "%.1f");
	ImGui::SliderFloat("Escala", &movilEscala, 0.02f, 0.40f, "%.3f");
	ImGui::SliderFloat("Intensidad idle", &intensidadIdle, 0.0f, 0.02f, "%.4f");
	ImGui::SliderFloat("Intensidad walking", &intensidadWalking, 0.0f, 0.08f, "%.3f");
	ImGui::SliderFloat("Intensidad mouse", &intensidadMouse, 0.0f, 0.003f, "%.4f");
	ImGui::Text("deltaTime: %.4f s", deltaFPS);
	ImGui::End();

	ImGui::Begin("Movimiento FPS");
	{
		fps::ParametrosMovimiento& pm = movimientoJugador.parametros;
		fps::ParametrosBalanceo& pb = balanceoCamara.parametros;
		ImGui::Text("Estamina: %.1f / %.1f%s", movimientoJugador.Estamina(), pm.estaminaMaxima,
			movimientoJugador.Agotada() ? "  (AGOTADA)" : (movimientoJugador.Sprintando() ? "  (corriendo)" : ""));
		ImGui::Text("Velocidad: %.2f m/s | agachado: %.0f %% | en suelo: %s", movimientoJugador.Velocidad(),
			mundoFisico.FraccionAgachado() * 100.0f, mundoFisico.JugadorEnSuelo() ? "si" : "no");
		ImGui::Checkbox("Porcentaje junto a la barra", &mostrarPorcentajeEstamina);
		if (ImGui::Button("Rellenar estamina")) movimientoJugador.Reiniciar();
		ImGui::Checkbox("Mostrar FPS en esquina superior derecha", &mostrarContadorFPS);
		ImGui::Checkbox("Renderizado optimizado (frustum culling)", &renderizadoOptimizado);
		ImGui::Checkbox("Instrumentacion de render", &renderMetrics::enabled);
		if (renderMetrics::enabled)
		{
			ImGui::Text("Frame actual %.2f ms (%.1f FPS), muestras: %zu", renderMetrics::lastFrameMs,
				renderMetrics::lastFrameMs > 0.0 ? 1000.0 / renderMetrics::lastFrameMs : 0.0, renderMetrics::frameMs.size());
			ImGui::Text("Frame P50/P95/P99: %.2f / %.2f / %.2f ms", renderMetrics::Percentile(0.50), renderMetrics::Percentile(0.95), renderMetrics::Percentile(0.99));
			ImGui::Text("Draw calls OBJ/GLB: %llu | objetos visibles/descartados: %llu / %llu",
				renderMetrics::drawCalls, renderMetrics::visibles, renderMetrics::descartados);
			ImGui::Text("CPU escena %.3f ms | oclusores %.3f ms", renderMetrics::escenaMs, renderMetrics::oclusoresMs);
			ImGui::Text("CPU fisica %.3f ms | animacion %.3f ms | update luces %.3f ms", renderMetrics::fisicaMs, renderMetrics::animacionMs, renderMetrics::lucesMs);
			ImGui::Text("GPU escena (consulta retardada): %.3f ms", ultimoGpuMs);
		}
		ImGui::SeparatorText("Velocidades (m/s)");
		ImGui::SliderFloat("Caminar", &pm.velocidadCaminar, 0.5f, 6.0f, "%.2f");
		ImGui::SliderFloat("Agachado", &pm.velocidadAgachado, 0.3f, 4.0f, "%.2f");
		ImGui::SliderFloat("Sprint", &pm.velocidadSprint, 1.0f, 9.0f, "%.2f");
		ImGui::SliderFloat("Suavizado de velocidad", &pm.suavizadoVelocidad, 1.0f, 30.0f, "%.1f");
		ImGui::SeparatorText("Estamina");
		ImGui::SliderFloat("Maxima", &pm.estaminaMaxima, 10.0f, 300.0f, "%.0f");
		ImGui::SliderFloat("Consumo / s", &pm.consumoPorSegundo, 1.0f, 100.0f, "%.1f");
		ImGui::SliderFloat("Recuperacion / s", &pm.recuperacionPorSegundo, 1.0f, 100.0f, "%.1f");
		ImGui::SliderFloat("Retardo de recuperacion (s)", &pm.retardoRecuperacion, 0.0f, 5.0f, "%.2f");
		ImGui::SliderFloat("Minimo tras agotarse", &pm.minimoTrasAgotarse, 0.0f, 100.0f, "%.0f");
		ImGui::SeparatorText("Balanceo de camara y movil");
		ImGui::SliderFloat("Intensidad global", &pb.multiplicador, 0.0f, 3.0f, "%.2f");
		ImGui::SliderFloat("Amplitud caminar (m)", &pb.amplitudCaminar, 0.0f, 0.05f, "%.4f");
		ImGui::SliderFloat("Amplitud correr (m)", &pb.amplitudCorrer, 0.0f, 0.08f, "%.4f");
		ImGui::SliderFloat("Amplitud agachado (m)", &pb.amplitudAgachado, 0.0f, 0.03f, "%.4f");
		ImGui::SliderFloat("Lateral relativo", &pb.lateralRelativo, 0.0f, 1.5f, "%.2f");
		ImGui::SliderFloat("Zancada caminar (m)", &pb.zancadaCaminar, 0.5f, 3.0f, "%.2f");
		ImGui::SliderFloat("Zancada correr (m)", &pb.zancadaCorrer, 0.5f, 4.0f, "%.2f");
		ImGui::SliderFloat("Amortiguacion al parar", &pb.salida, 1.0f, 20.0f, "%.1f");
		ImGui::Text("fase: %.2f rad | peso: %.2f", balanceoCamara.Fase(), balanceoCamara.Peso());
	}
	ImGui::End();

	ImGui::Begin("Diagnostico de iluminacion");
	const char* modosIluminacion[] = { "Final", "Ambiental", "Directa", "Indirecta", "Normales", "Albedo" };
	ImGui::Combo("Vista", &modoDiagnosticoIluminacion, modosIluminacion, IM_ARRAYSIZE(modosIluminacion));
	ImGui::TextUnformatted("Solo afecta al shader Phong; vuelve a Final para la escena normal.");
	ImGui::End();

	ImGui::Begin("Demo puerta");
	if (ImGui::Button("Abrir puerta")) context.startDoorDemo();
	ImGui::Text("tiempo: %.2f / %.2f s", reproductorPuertaDemo.tiempo, reproductorPuertaDemo.duracion);
	ImGui::Text("angulo: %.1f grados", anguloPuertaDemo);
	ImGui::End();

	ImGui::Begin("Fisica (Bullet)");
	ImGui::Checkbox("Mostrar colisionadores", &mostrarColisionadores);
	ImGui::Text("Hiriti sigue al jugador: %s", hiritiFollowPlayerEnabled ? "BUSCANDO" : "inactivo");
	ImGui::SliderFloat("Distancia de parada Hiriti (m)", &hiritiFollowStopDistance, 1.0f, 3.0f);
	ImGui::SliderFloat("Velocidad maxima Hiriti (m/s)", &hiritiFollowMaxSpeed, 0.5f, 4.0f);
	ImGui::TextUnformatted("azul: estatico  verde: dinamico  naranja: cinematico");
	ImGui::Text("cuerpos en el mundo: %d (habitacion: %d, dinamicos: %d)", mundoFisico.NumeroDeCuerpos(),
		escenaFisica->NumeroDeCuerpos(), escenaFisica->NumeroDeDinamicos());
	ImGui::Text("pasos fisicos: %lld", mundoFisico.PasosTotales());
	ImGui::Checkbox("Vista estereoscopica de prueba (F2) - NO es VR real", &modoEstereoPrueba);
	ImGui::Separator();
	ImGui::Text("OpenXR (VR real, F3): %s", vr::NombreDelEstado(gestorOpenXR.Estado()));
	if (!vr::GestorOpenXR::EstaCompilado())
		ImGui::TextUnformatted("No incluido en esta compilacion (macOS u OpenXR no instalado).");
	else if (ImGui::Button(gestorOpenXR.TieneRecursos() ? "Desactivar OpenXR" : "Activar OpenXR"))
		context.toggleOpenXR();
	ImGui::TextWrapped("%s", gestorOpenXR.Mensaje().c_str());
	if (gestorOpenXR.TieneRecursos())
	{
		ImGui::Text("Sesion: %s%s | espacio: %s | ojos: %d", gestorOpenXR.NombreEstadoSesion(),
			gestorOpenXR.SesionEnMarcha() ? " (en marcha)" : "",
			gestorOpenXR.Espacio() == vr::EspacioReferencia::Suelo ? "STAGE" : (gestorOpenXR.Espacio() == vr::EspacioReferencia::Local ? "LOCAL" : "-"),
			gestorOpenXR.NumeroDeOjos());
	}
	if (!gestorOpenXR.Diagnostico().empty() && ImGui::TreeNode("Diagnostico OpenXR"))
	{
		ImGui::TextWrapped("%s", gestorOpenXR.Diagnostico().c_str());
		ImGui::TreePop();
	}
	ImGui::Separator();
	bool estadoAgarreMostrado = false;
	const int agarradoLocal = escenaFisica->ObjetoAgarrado();
	if (agarradoLocal >= 0 && agarradoLocal < static_cast<int>(objetosEscena.size()))
	{
		ImGui::Text("en tu mano: %s", objetosEscena[agarradoLocal].id.c_str());
		estadoAgarreMostrado = true;
	}
	else if (redJuego.EsCliente())
	{
		const uint32_t idPropio = redJuego.ObjetoDinamicoDePeer(redJuego.PeerLocal());
		const auto propio = std::find_if(objetosEscena.begin(), objetosEscena.end(), [idPropio](const SceneObject& objeto)
			{ return idPropio != 0 && netjuego::HashId(objeto.id) == idPropio; });
		if (propio != objetosEscena.end())
		{
			ImGui::Text("en tu mano (host autoritativo): %s", propio->id.c_str());
			estadoAgarreMostrado = true;
		}
	}
	if (!estadoAgarreMostrado && redJuego.EsHost())
	{
		for (const auto& par : redJuego.PropietariosDinamicos())
			if (par.second != netjuego::kSinDueno && par.second != net::kHostPeerId)
			{
				std::size_t indice = 0;
				if (netjuego::scene_network_state::FindUniqueDynamicSceneObjectIndex(par.first, objetosEscena, indice))
				{
					ImGui::Text("sujeto por peer %u: %s", static_cast<unsigned>(par.second), objetosEscena[indice].id.c_str());
					estadoAgarreMostrado = true;
					break;
				}
			}
	}
	if (!estadoAgarreMostrado)
		ImGui::TextUnformatted("mano vacia (E sobre un objeto dinamico para agarrarlo)");
	if (mundoFisico.HayJugador())
	{
		const glm::vec3 pies = mundoFisico.PosicionPiesJugador();
		ImGui::Text("jugador (pies): (%.2f, %.2f, %.2f) m  %s", pies.x, pies.y, pies.z,
			mundoFisico.JugadorEnSuelo() ? "en el suelo" : "en el aire");
	}
	ImGui::Separator();
	ImGui::TextUnformatted("Demo: cubo que cae (independiente de la habitacion)");
	if (!demoFisica.EstaActiva())
	{
		if (ImGui::Button("Activar demo cubo")) demoFisica.Iniciar();
	}
	else
	{
		if (ImGui::Button("Reiniciar demo")) demoFisica.Iniciar();
		ImGui::SameLine();
		if (ImGui::Button("Quitar demo")) demoFisica.Detener();
		const fisica::PoseCuerpo pose = demoFisica.PoseCubo();
		const glm::vec3 velocidad = demoFisica.VelocidadCubo();
		const glm::vec3 gravedad = mundoFisico.Gravedad();
		ImGui::Text("gravedad: (%.2f, %.2f, %.2f) m/s2", gravedad.x, gravedad.y, gravedad.z);
		ImGui::Text("cubo pos: (%.3f, %.3f, %.3f) m", pose.posicion.x, pose.posicion.y, pose.posicion.z);
		ImGui::Text("cubo vel: (%.3f, %.3f, %.3f) m/s", velocidad.x, velocidad.y, velocidad.z);
		ImGui::Text("estado: %s", demoFisica.CuboEnReposo() ? "en reposo (dormido)" : "en movimiento");
	}
	ImGui::End();

	ImGui::Begin("Objetos animados GLB");
	ImGui::Text("Objetos animados GLB (Blender)");
	if (objetosAnimados.objetos.empty())
		ImGui::TextUnformatted("Ninguno cargado (ver consola)");
	for (glb::ObjetoAnimado& objeto : objetosAnimados.objetos)
	{
		ImGui::PushID(objeto.id.c_str());   // cada fila necesita un ID distinto para sus botones
		if (ImGui::Button("Abrir")) glb::IniciarAnimacion(objeto);     // depuración: reinicia la apertura
		ImGui::SameLine();
		if (ImGui::Button("Alternar") && !redJuego.EsCliente()) glb::AlternarPuerta(objeto);   // igual que pulsar E junto a la puerta
		ImGui::SameLine();
		ImGui::Text("%s  %s  %.2f / %.2f s", objeto.id.c_str(), glb::NombreDelEstado(objeto.estado),
			objeto.reproductor.tiempo, objeto.reproductor.duracion);
		ImGui::PopID();
	}
	ImGui::End();
}
}
