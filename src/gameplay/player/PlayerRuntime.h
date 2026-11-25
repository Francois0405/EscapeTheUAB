#pragma once

#include "src/gameplay/player/FpsMovement.h"
#include "src/physics/PhysicsScene.h"
#include "src/physics/PhysicsWorld.h"
#include "src/scene/SceneLoader.h"

#include <functional>
#include <memory>
#include <string>

struct CPunt3D;
struct GLFWwindow;

namespace gameplay::player
{
	// Borrowed application state required by the extracted desktop player runtime.
	// Ownership stays in src/app; callbacks bridge the legacy camera preset API.
	struct PlayerContext
	{
		fisica::PhysicsWorld& physics;
		std::unique_ptr<fisica::PhysicsScene>& scenePhysics;
		bool& sceneHasValidGeometry;
		SceneSpawn& activeSpawn;
		const std::string& activeSceneName;
		CPunt3D& cameraOrigin;
		double* lookAt;
		double& cameraAngleZ;
		char& cameraMode;
		char& projectionMode;
		char fpsCameraMode;
		char perspectiveProjectionMode;
		bool& showApplicationWindow;
		int& projectionMenu;
		int& cameraMenu;
		int& objectMenu;
		int& lightingMenu;
		int& shaderMenu;
		bool& worldSpaceLighting;
		bool& skyboxEnabled;
		std::function<void()> setPerspective;
		std::function<void()> setNavigationCamera;
		std::function<void()> setCubeObject;
		std::function<void()> setPhongShader;
		std::function<void()> setSkybox;
		std::function<void()> setPresetLight;

		fps::PlayerMovement& movement;
		fps::CameraBob& cameraBob;
		bool& hasPreviousFeet;
		glm::vec3& previousFeet;
		float& walkWeight;
		float& walkPhase;
		float& mouseSwayX;
		float& mouseSwayY;
		double& mouseDeltaX;
		double& mouseDeltaY;
		float& mouseSwayIntensity;
		float& fpsElapsed;
		float& fpsDelta;
		std::function<bool()> fpsInputActive;
	};

	bool isFpsMode(const PlayerContext& context);
	void fixCameraAtSpawn(PlayerContext& context, const SceneSpawn& spawn);
	void applyGamePreset(PlayerContext& context);
	void look(PlayerContext& context, double deltaY);
	bool createPhysicsPlayer(PlayerContext& context);
	void moveFpsPlayer(PlayerContext& context, GLFWwindow* window, double speed);
	void synchronizePlayerAndCamera(PlayerContext& context);
	void updateMobile(PlayerContext& context, float deltaTime);
	void updateVisualBob(PlayerContext& context, float deltaTime);
	glm::dvec3 visualCameraDisplacement(const PlayerContext& context);
}
