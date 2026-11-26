#include "src/app/AppIncludes.h"
#include "src/app/state/SceneState.h"
#include "src/app/state/PlayerState.h"
#include "src/app/state/VrState.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/app/input/CursorCapture.h"
#include "src/legacy_vgi/VgiMenuCallbacks.h"

// State used only by this file.
namespace
{
	glm::vec3 previousFrameFeet(0.0f);
}

gameplay::player::PlayerContext& playerBindings()
{
	static gameplay::player::PlayerContext context{
		physicsWorld, physicsScene, sceneHasValidGeometry, activeSceneSpawn, activeSceneName,
		opvN, n, angleZ, camera, projeccio, CAM_NAVEGA, PERSPECT,
		show_EntornVGI_window, oProjeccio, oCamera, oObjecte, oIlumina, oShader, ifixe, SkyBoxCube,
		[] { OnProjeccioPerspectiva(); }, [] { OnCameraNavega(); }, [] { OnObjecteCub(); },
		[] { OnShaderPhong(); }, [] { OnVistaSkyBox(); },
		[] {
			llumGL[0].encesa = true;
			llumGL[0].difusa.r = 0.12f; llumGL[0].difusa.g = 0.15f; llumGL[0].difusa.b = 0.18f;
			llumGL[0].especular.r = 0.03f; llumGL[0].especular.g = 0.03f; llumGL[0].especular.b = 0.03f;
		},
		 playerMovement, cameraBob, hasPreviousFrameFeet, previousFrameFeet,
		 walkWeight, walkPhase, mouseSwayX, mouseSwayY, mouseDeltaX, mouseDeltaY, mouseSwayIntensity,
		 fpsTime, deltaFPS, [] { return fpsInputActive(); }
	};
	return context;
}

void placeCameraAtSpawn(const SceneSpawn& spawn) { gameplay::player::fixCameraAtSpawn(playerBindings(), spawn); }

void applyStartupPreset() { gameplay::player::applyGamePreset(playerBindings()); }

void pitchFpsPlayer(double deltaY) { gameplay::player::look(playerBindings(), deltaY); }

// --- JUGADOR FÍSICO (Fase 3) ---------------------------------------------------------
// Reparto de trabajo (el mismo que se usará con VR):
//   * El teclado (o el mando, en VR) solo dice "quiero ir en esta dirección a 3 m/s".
//   * Bullet (la cápsula del jugador) decide hasta dónde se puede llegar: choca, desliza, cae.
//   * La cámara se COLOCA donde quedó la cápsula (ojos = pies + 1,7 m). La cámara nunca
//     mueve a la cápsula, salvo al salir del modo FPS (ver sincronizarJugadorYCamara).
// La orientación (ratón) NO pasa por Bullet: sigue siendo cosa de la cámara.
// (las velocidades -caminar, agachado, sprint- viven en fps::ParametrosMovimiento, editables en F1)

bool movementKeyPressed(GLFWwindow* window)
{
	return glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
		glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
}

glm::vec3 feetPositionFromCamera()
{
	return glm::vec3(opvN.x, opvN.y, opvN.z - physicsWorld.PlayerEyeHeight());
}

void gazeOriginAndDirection(glm::vec3& origen, glm::vec3& direccion)
{
	if (openXrManager.IsSessionRunning())
	{
		vr::XrPose controllerPose;
		if (openXrManager.ControllerAimPose(1, controllerPose)) // Solo mando derecho (1)
		{
			const glm::vec3 pies = physicsWorld.HasPlayerBody() ? physicsWorld.PlayerFeetPosition() : feetPositionFromCamera();
			const glm::vec3 frenteFPS(static_cast<float>(n[0] - opvN.x), static_cast<float>(n[1] - opvN.y), static_cast<float>(n[2] - opvN.z));
			const vr::Anchor anclaje = vr::PlayerAnchor(pies, frenteFPS,
				openXrManager.ReferenceSpaceHandle() == vr::ReferenceSpaceKind::Floor, physicsWorld.PlayerEyeHeight());

			const glm::mat4 vistaMando = vr::VrEyeViewMatrix(controllerPose, anclaje);
			const glm::mat4 mundoMando = glm::inverse(vistaMando);

			origen = glm::vec3(mundoMando[3]);
			direccion = glm::normalize(-glm::vec3(mundoMando[2]));
			return;
		}
	}

	// Modo escritorio (PC): cámara FPS
	origen = glm::vec3(opvN.x, opvN.y, opvN.z);
	direccion = glm::vec3(n[0] - opvN.x, n[1] - opvN.y, n[2] - opvN.z);
}

// Crea la cápsula bajo la cámara actual. Se llama tras cargar la primera habitación, para que
// el suelo ya exista y el jugador no caiga al vacío.
bool spawnPhysicsPlayer() { return gameplay::player::createPhysicsPlayer(playerBindings()); }

// Convierte las teclas WASD en una velocidad horizontal DEL MUNDO (sin tocar la posición).
void moveFirstPersonPlayer(GLFWwindow* window, double velocidadJugador) { gameplay::player::moveFpsPlayer(playerBindings(), window, velocidadJugador); }

// Después de simular: la cámara va a donde está la cápsula (conservando hacia dónde mira).
// Si NO estamos en modo FPS (cámaras de depuración), ocurre lo contrario: la cápsula sigue a la
// cámara, para que al volver al modo FPS esté donde toca y no haya un salto.
void syncPlayerWithCamera() { gameplay::player::synchronizePlayerAndCamera(playerBindings()); }

void updatePhoneMotion(GLFWwindow* window, float deltaTime) { (void)window; gameplay::player::updateMobile(playerBindings(), deltaTime); }

// Despues de simular: el balanceo visual sigue el desplazamiento REAL de la capsula (no la tecla pulsada),
// asi que la camara y el movil comparten la misma fase y se quedan quietos al chocar con una pared.
void updateCameraBobbing(float delta) { gameplay::player::updateVisualBob(playerBindings(), delta); }

// Desplazamiento SOLO VISUAL del ojo en pantalla (derecha y arriba de la camara). No modifica opvN/n, que usan la
// fisica, las puertas y el agarre. No se aplica en VR: alli manda la cabeza real.
glm::dvec3 cameraVisualOffset() { return gameplay::player::visualCameraDisplacement(playerBindings()); }
