#include "src/ui/RuntimeUi.h"

#include <GL/glew.h>
#include "imgui.h"

#include <algorithm>
#include <string>
#include <vector>

#include "src/animation/DoorAnimation.h"
#include "src/animation/GlbAnimatedObjects.h"
#include "src/gameplay/player/FpsMovement.h"
#include "src/networking/replication/Game/SceneNetworkState.h"
#include "src/networking/replication/NetGame.h"
#include "src/physics/PhysicsScene.h"
#include "src/physics/PhysicsWorld.h"
#include "src/rendering/RenderMetrics.h"
#include "src/scene/SceneLoader.h"
#include "src/vr/OpenXRManager.h"

namespace runtimeui
{
void DrawDebugPanels(const DebugPanelsContext& context)
{
	auto& showPhone = context.showPhone;
	auto& phonePosition = context.phonePosition;
	auto& phoneRotation = context.phoneRotation;
	auto& phoneScale = context.phoneScale;
	auto& idleSwayIntensity = context.idleIntensity;
	auto& walkSwayIntensity = context.walkingIntensity;
	auto& mouseSwayIntensity = context.mouseIntensity;
	auto& deltaFPS = context.frameDeltaSeconds;
	auto& playerMovement = context.movement;
	auto& cameraBob = context.cameraSway;
	auto& showStaminaPercent = context.showStaminaPercent;
	auto& showFpsCounter = context.showFpsCounter;
	auto& optimizedRendering = context.optimizedRendering;
	auto& lastGpuMs = context.lastGpuMs;
	auto& lightingDiagnosticMode = context.lightingView;
	auto& demoDoorPlayback = context.doorDemoPlayer;
	auto& demoDoorAngle = context.doorDemoAngle;
	auto& showColliders = context.showColliders;
	auto& hiritiFollowPlayerEnabled = context.hiritiFollowEnabled;
	auto& hiritiFollowStopDistance = context.hiritiStopDistance;
	auto& hiritiFollowMaxSpeed = context.hiritiMaxSpeed;
	auto& stereoTestMode = context.stereoPreview;
	auto& openXrManager = context.openXR;
	auto& physicsWorld = context.physicsWorld;
	auto& physicsScene = context.physicsScene;
	auto& fallingCubeDemo = context.cubeDemo;
	auto& networkGame = context.session;
	auto& sceneObjects = context.sceneObjects;
	auto& animatedObjects = context.animatedObjects;

	ImGui::Begin("Movil FPS");
	ImGui::Checkbox("Mostrar movil", &showPhone);
	ImGui::InputFloat3("Posicion", &phonePosition.x, "%.3f");
	ImGui::InputFloat3("Rotacion (grados)", &phoneRotation.x, "%.1f");
	ImGui::SliderFloat("Escala", &phoneScale, 0.02f, 0.40f, "%.3f");
	ImGui::SliderFloat("Intensidad idle", &idleSwayIntensity, 0.0f, 0.02f, "%.4f");
	ImGui::SliderFloat("Intensidad walking", &walkSwayIntensity, 0.0f, 0.08f, "%.3f");
	ImGui::SliderFloat("Intensidad mouse", &mouseSwayIntensity, 0.0f, 0.003f, "%.4f");
	ImGui::Text("deltaTime: %.4f s", deltaFPS);
	ImGui::End();

	ImGui::Begin("Movimiento FPS");
	{
		fps::MovementParameters& pm = playerMovement.parametros;
		fps::BobParameters& pb = cameraBob.parametros;
		ImGui::Text("Estamina: %.1f / %.1f%s", playerMovement.StaminaValue(), pm.estaminaMaxima,
			playerMovement.IsExhausted() ? "  (AGOTADA)" : (playerMovement.IsSprinting() ? "  (corriendo)" : ""));
		ImGui::Text("Velocidad: %.2f m/s | agachado: %.0f %% | en suelo: %s", playerMovement.VelocityValue(),
			physicsWorld.CrouchFraction() * 100.0f, physicsWorld.IsPlayerGrounded() ? "si" : "no");
		ImGui::Checkbox("Porcentaje junto a la barra", &showStaminaPercent);
		if (ImGui::Button("Rellenar estamina")) playerMovement.ResetState();
		ImGui::Checkbox("Mostrar FPS en esquina superior derecha", &showFpsCounter);
		ImGui::Checkbox("Renderizado optimizado (frustum culling)", &optimizedRendering);
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
			ImGui::Text("GPU escena (consulta retardada): %.3f ms", lastGpuMs);
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
		ImGui::Text("fase: %.2f rad | peso: %.2f", cameraBob.CurrentPhase(), cameraBob.WeightValue());
	}
	ImGui::End();

	ImGui::Begin("Diagnostico de iluminacion");
	const char* modosIluminacion[] = { "Final", "Ambiental", "Directa", "Indirecta", "Normales", "Albedo" };
	ImGui::Combo("Vista", &lightingDiagnosticMode, modosIluminacion, IM_ARRAYSIZE(modosIluminacion));
	ImGui::TextUnformatted("Solo afecta al shader Phong; vuelve a Final para la escena normal.");
	ImGui::End();

	ImGui::Begin("Demo puerta");
	if (ImGui::Button("Abrir puerta")) context.startDoorDemo();
	ImGui::Text("tiempo: %.2f / %.2f s", demoDoorPlayback.tiempo, demoDoorPlayback.duracion);
	ImGui::Text("angulo: %.1f grados", demoDoorAngle);
	ImGui::End();

	ImGui::Begin("Fisica (Bullet)");
	ImGui::Checkbox("Mostrar colisionadores", &showColliders);
	ImGui::Text("Hiriti sigue al jugador: %s", hiritiFollowPlayerEnabled ? "BUSCANDO" : "inactivo");
	ImGui::SliderFloat("Distancia de parada Hiriti (m)", &hiritiFollowStopDistance, 1.0f, 3.0f);
	ImGui::SliderFloat("Velocidad maxima Hiriti (m/s)", &hiritiFollowMaxSpeed, 0.5f, 4.0f);
	ImGui::TextUnformatted("azul: estatico  verde: dinamico  naranja: cinematico");
	ImGui::Text("cuerpos en el mundo: %d (habitacion: %d, dinamicos: %d)", physicsWorld.BodyCount(),
		physicsScene->BodyCount(), physicsScene->DynamicCount());
	ImGui::Text("pasos fisicos: %lld", physicsWorld.PasosTotales());
	ImGui::Checkbox("Vista estereoscopica de prueba (F2) - NO es VR real", &stereoTestMode);
	ImGui::Separator();
	ImGui::Text("OpenXR (VR real, F3): %s", vr::NombreDelEstado(openXrManager.CurrentState()));
	if (!vr::OpenXrManager::IsCompiledIn())
		ImGui::TextUnformatted("No incluido en esta compilacion (macOS u OpenXR no instalado).");
	else if (ImGui::Button(openXrManager.HasResources() ? "Desactivar OpenXR" : "Activar OpenXR"))
		context.toggleOpenXR();
	ImGui::TextWrapped("%s", openXrManager.Mensaje().c_str());
	if (openXrManager.HasResources())
	{
		ImGui::Text("Sesion: %s%s | espacio: %s | ojos: %d", openXrManager.NombreEstadoSesion(),
			openXrManager.IsSessionRunning() ? " (en marcha)" : "",
			openXrManager.ReferenceSpaceHandle() == vr::ReferenceSpaceKind::Suelo ? "STAGE" : (openXrManager.ReferenceSpaceHandle() == vr::ReferenceSpaceKind::Local ? "LOCAL" : "-"),
			openXrManager.EyeCount());
	}
	if (!openXrManager.Diagnostico().empty() && ImGui::TreeNode("Diagnostico OpenXR"))
	{
		ImGui::TextWrapped("%s", openXrManager.Diagnostico().c_str());
		ImGui::TreePop();
	}
	ImGui::Separator();
	bool estadoAgarreMostrado = false;
	const int agarradoLocal = physicsScene->GrabbedObject();
	if (agarradoLocal >= 0 && agarradoLocal < static_cast<int>(sceneObjects.size()))
	{
		ImGui::Text("en tu mano: %s", sceneObjects[agarradoLocal].id.c_str());
		estadoAgarreMostrado = true;
	}
	else if (networkGame.IsClient())
	{
		const uint32_t idPropio = networkGame.DynamicObjectOfPeer(networkGame.LocalPeerId());
		const auto propio = std::find_if(sceneObjects.begin(), sceneObjects.end(), [idPropio](const SceneObject& objeto)
			{ return idPropio != 0 && netgame::HashId(objeto.id) == idPropio; });
		if (propio != sceneObjects.end())
		{
			ImGui::Text("en tu mano (host autoritativo): %s", propio->id.c_str());
			estadoAgarreMostrado = true;
		}
	}
	if (!estadoAgarreMostrado && networkGame.IsHosting())
	{
		for (const auto& par : networkGame.PropietariosDinamicos())
			if (par.second != netgame::kSinDueno && par.second != net::kHostPeerId)
			{
				std::size_t indice = 0;
				if (netgame::scene_network_state::FindUniqueDynamicSceneObjectIndex(par.first, sceneObjects, indice))
				{
					ImGui::Text("sujeto por peer %u: %s", static_cast<unsigned>(par.second), sceneObjects[indice].id.c_str());
					estadoAgarreMostrado = true;
					break;
				}
			}
	}
	if (!estadoAgarreMostrado)
		ImGui::TextUnformatted("mano vacia (E sobre un objeto dinamico para agarrarlo)");
	if (physicsWorld.HasPlayerBody())
	{
		const glm::vec3 pies = physicsWorld.PlayerFeetPosition();
		ImGui::Text("jugador (pies): (%.2f, %.2f, %.2f) m  %s", pies.x, pies.y, pies.z,
			physicsWorld.IsPlayerGrounded() ? "en el suelo" : "en el aire");
	}
	ImGui::Separator();
	ImGui::TextUnformatted("Demo: cubo que cae (independiente de la habitacion)");
	if (!fallingCubeDemo.IsRunningNow())
	{
		if (ImGui::Button("Activar demo cubo")) fallingCubeDemo.BeginPlayback();
	}
	else
	{
		if (ImGui::Button("Reiniciar demo")) fallingCubeDemo.BeginPlayback();
		ImGui::SameLine();
		if (ImGui::Button("Quitar demo")) fallingCubeDemo.StopSessionGame();
		const fisica::BodyPose pose = fallingCubeDemo.CubePose();
		const glm::vec3 velocidad = fallingCubeDemo.CubeVelocity();
		const glm::vec3 gravedad = physicsWorld.GravityValue();
		ImGui::Text("gravedad: (%.2f, %.2f, %.2f) m/s2", gravedad.x, gravedad.y, gravedad.z);
		ImGui::Text("cubo pos: (%.3f, %.3f, %.3f) m", pose.posicion.x, pose.posicion.y, pose.posicion.z);
		ImGui::Text("cubo vel: (%.3f, %.3f, %.3f) m/s", velocidad.x, velocidad.y, velocidad.z);
		ImGui::Text("estado: %s", fallingCubeDemo.CubeAtRest() ? "en reposo (dormido)" : "en movimiento");
	}
	ImGui::End();

	ImGui::Begin("Objetos animados GLB");
	ImGui::Text("Objetos animados GLB (Blender)");
	if (animatedObjects.objetos.empty())
		ImGui::TextUnformatted("Ninguno cargado (ver consola)");
	for (glb::AnimatedObject& objeto : animatedObjects.objetos)
	{
		ImGui::PushID(objeto.id.c_str());   // cada fila necesita un ID distinto para sus botones
		if (ImGui::Button("Abrir")) glb::BeginAnimation(objeto);     // depuración: reinicia la apertura
		ImGui::SameLine();
		if (ImGui::Button("Alternar") && !networkGame.IsClient()) glb::ToggleDoor(objeto);   // igual que pulsar E junto a la puerta
		ImGui::SameLine();
		ImGui::Text("%s  %s  %.2f / %.2f s", objeto.id.c_str(), glb::NombreDelEstado(objeto.estado),
			objeto.reproductor.tiempo, objeto.reproductor.duracion);
		ImGui::PopID();
	}
	ImGui::End();
}
}
