#include "src/gameplay/player/PlayerRuntime.h"

#include "src/legacy_vgi/framework.h"
#include "src/legacy_vgi/constants.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace gameplay::player
{
	bool isFpsMode(const PlayerContext& context)
	{
		return context.cameraMode == context.fpsCameraMode && context.projectionMode == context.perspectiveProjectionMode;
	}

	void applyGamePreset(PlayerContext& context)
	{
		context.showApplicationWindow = true;
		context.projectionMenu = 3;
		context.cameraMenu = 1;
		context.objectMenu = 1;
		context.lightingMenu = 3;

		context.setPerspective();
		context.setNavigationCamera();
		context.setCubeObject();
		context.setPhongShader();
		context.shaderMenu = 2;
		context.lightingMenu = 3;
		context.worldSpaceLighting = true;
		context.skyboxEnabled = true;
		context.setSkybox();
		context.setPresetLight();
	}

	void look(PlayerContext& context, double deltaY)
	{
		const double dx = context.lookAt[0] - context.cameraOrigin.x;
		const double dy = context.lookAt[1] - context.cameraOrigin.y;
		const double horizontal = sqrt(dx * dx + dy * dy);
		if (horizontal == 0.0) return;

		double inclination = atan2(context.lookAt[2] - context.cameraOrigin.z, horizontal) - deltaY * 0.12 * PI / 180.0;
		const double limit = 85.0 * PI / 180.0;
		inclination = std::clamp(inclination, -limit, limit);
		const double length = sqrt(dx * dx + dy * dy + (context.lookAt[2] - context.cameraOrigin.z) * (context.lookAt[2] - context.cameraOrigin.z));
		const double factor = length * cos(inclination) / horizontal;
		context.lookAt[0] = context.cameraOrigin.x + dx * factor;
		context.lookAt[1] = context.cameraOrigin.y + dy * factor;
		context.lookAt[2] = context.cameraOrigin.z + length * sin(inclination);
	}

	void fixCameraAtSpawn(PlayerContext& context, const SceneSpawn& spawn)
	{
		const float eyeHeight = context.physics.HayJugador() ? context.physics.AlturaOjosJugador() : fisica::PlayerDimensions::AlturaOjos;
		const glm::vec3 eyes = spawn.pies + glm::vec3(0.0f, 0.0f, eyeHeight);
		const glm::vec3 direction = glm::normalize(spawn.direccion);
		context.cameraOrigin.x = eyes.x; context.cameraOrigin.y = eyes.y; context.cameraOrigin.z = eyes.z;
		context.lookAt[0] = eyes.x + direction.x;
		context.lookAt[1] = eyes.y + direction.y;
		context.lookAt[2] = eyes.z + direction.z;
		context.cameraAngleZ = 0.0;
	}

	bool createPhysicsPlayer(PlayerContext& context)
	{
		if (!context.sceneHasValidGeometry || !context.scenePhysics || context.scenePhysics->NumeroDeCuerpos() == 0)
		{
			std::cerr << "No se puede iniciar: la escena no tiene geometria fisica valida." << std::endl;
			return false;
		}
		glm::vec3 feet, groundNormal;
		std::string error;
		if (!context.physics.ValidarSpawnJugador(context.activeSpawn.pies, feet, groundNormal, error))
		{
			std::cerr << "Spawn invalido para la escena '" << context.activeSceneName << "': " << error << std::endl;
			return false;
		}
		context.activeSpawn.pies = feet;
		if (context.physics.HayJugador())
		{
			if (!context.physics.ActualizarJugadorAgachado(false, 1.0f))
			{
				std::cerr << "No se puede reiniciar el spawn: hay un obstaculo sobre la capsula." << std::endl;
				return false;
			}
			context.physics.TeletransportarJugador(feet);
		}
		else if (!context.physics.CrearJugador(feet))
		{
			std::cerr << "No se pudo crear la capsula Bullet del jugador." << std::endl;
			return false;
		}
		fixCameraAtSpawn(context, context.activeSpawn);
		std::cout << "Spawn validado: pies (" << feet.x << ", " << feet.y << ", " << feet.z
			<< "), ojos Z=" << feet.z + context.physics.AlturaOjosJugador() << ", normal suelo ("
			<< groundNormal.x << ", " << groundNormal.y << ", " << groundNormal.z << ")." << std::endl;
		return true;
	}

	void moveFpsPlayer(PlayerContext& context, GLFWwindow* window, double speed)
	{
		const double dx = context.lookAt[0] - context.cameraOrigin.x;
		const double dy = context.lookAt[1] - context.cameraOrigin.y;
		const double length = sqrt(dx * dx + dy * dy);
		if (length == 0.0) return;

		const double forwardX = dx / length;
		const double forwardY = dy / length;
		double moveX = 0.0;
		double moveY = 0.0;

		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { moveX += forwardX; moveY += forwardY; }
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { moveX -= forwardX; moveY -= forwardY; }
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { moveX += forwardY; moveY -= forwardX; }
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { moveX -= forwardY; moveY += forwardX; }

		const double distance = sqrt(moveX * moveX + moveY * moveY);
		if (distance == 0.0) return;
		context.physics.FijarVelocidadJugador(glm::vec3(moveX / distance * speed,
			moveY / distance * speed, 0.0));
	}

	void synchronizePlayerAndCamera(PlayerContext& context)
	{
		if (!context.physics.HayJugador()) return;
		if (!isFpsMode(context))
		{
			context.physics.FijarVelocidadJugador(glm::vec3(0.0f));
			return;
		}
		const glm::vec3 feet = context.physics.PosicionPiesJugador();
		const double eyeX = feet.x, eyeY = feet.y, eyeZ = feet.z + context.physics.AlturaOjosJugador();
		const double moveX = eyeX - context.cameraOrigin.x, moveY = eyeY - context.cameraOrigin.y, moveZ = eyeZ - context.cameraOrigin.z;
		context.cameraOrigin.x += moveX; context.cameraOrigin.y += moveY; context.cameraOrigin.z += moveZ;
		context.lookAt[0] += moveX; context.lookAt[1] += moveY; context.lookAt[2] += moveZ;
	}

	void updateMobile(PlayerContext& context, float deltaTime)
	{
		deltaTime = std::clamp(deltaTime, 0.0f, 0.05f);
		context.fpsElapsed += deltaTime;
		context.fpsDelta = deltaTime;

		const float mouseLimit = 0.12f;
		context.mouseSwayX = std::clamp(context.mouseSwayX + static_cast<float>(context.mouseDeltaX) * context.mouseSwayIntensity,
			-mouseLimit, mouseLimit);
		context.mouseSwayY = std::clamp(context.mouseSwayY + static_cast<float>(context.mouseDeltaY) * context.mouseSwayIntensity,
			-mouseLimit, mouseLimit);
		context.mouseDeltaX = context.mouseDeltaY = 0.0;
		const float mouseReturn = 1.0f - std::exp(-5.0f * deltaTime);
		context.mouseSwayX += (0.0f - context.mouseSwayX) * mouseReturn;
		context.mouseSwayY += (0.0f - context.mouseSwayY) * mouseReturn;
	}

	void updateVisualBob(PlayerContext& context, float deltaTime)
	{
		const glm::vec3 feet = context.physics.HayJugador() ? context.physics.PosicionPiesJugador() : glm::vec3(0.0f);
		float distance = 0.0f;
		if (context.hasPreviousFeet)
			distance = glm::length(glm::vec2(feet.x - context.previousFeet.x, feet.y - context.previousFeet.y));
		context.previousFeet = feet;
		context.hasPreviousFeet = context.physics.HayJugador();
		if (distance > 1.0f) distance = 0.0f;

		const float crouchFraction = context.physics.FraccionAgachado();
		context.cameraBob.Actualizar(deltaTime, distance, context.physics.JugadorEnSuelo(), context.fpsInputActive() && context.physics.HayJugador(),
			context.movement.Velocidad(), context.movement.parametros, crouchFraction);
		context.walkWeight = context.cameraBob.Peso() * context.cameraBob.FactorDeAmplitud();
		context.walkPhase = context.cameraBob.Fase();
	}

	glm::dvec3 visualCameraDisplacement(const PlayerContext& context)
	{
		if (!isFpsMode(context) || !context.physics.HayJugador()) return glm::dvec3(0.0);
		const fps::VisualOffset displacement = context.cameraBob.Desplazamiento();
		const double dx = context.lookAt[0] - context.cameraOrigin.x, dy = context.lookAt[1] - context.cameraOrigin.y;
		const double length = std::sqrt(dx * dx + dy * dy);
		glm::dvec3 result(0.0, 0.0, displacement.vertical);
		if (length > 1e-6) result += glm::dvec3(dy / length, -dx / length, 0.0) * static_cast<double>(displacement.lateral);
		return result;
	}
}
