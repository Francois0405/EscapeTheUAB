#include "src/gameplay/player/PlayerRuntime.h"
#include "src/legacy_vgi/framework.h"
#include "src/legacy_vgi/constants.h"

#include <cmath>
#include <iostream>

namespace
{
	int failures = 0;
	void check(bool condition, const char* name)
	{
		if (!condition) { std::cerr << "FAIL: " << name << '\n'; ++failures; }
	}

	struct Fixture
	{
		physics::PhysicsWorld physics;
		std::unique_ptr<physics::PhysicsScene> scenePhysics;
		bool validGeometry = false;
		SceneSpawn spawn;
		std::string sceneName = "test";
		CPunt3D camera{};
		GLdouble lookAt[3]{ 1.0, 0.0, 0.0 };
		double angleZ = 1.0;
		char cameraMode = CAM_NAVEGA;
		char projectionMode = PERSPECT;
		bool showWindow = false;
		int projectionMenu = 0, cameraMenu = 0, objectMenu = 0, lightingMenu = 0, shaderMenu = 0;
		bool worldLighting = false, skybox = false;
		int callbacks = 0;
		fps::PlayerMovement movement;
		fps::CameraBob bob;
		bool hasPreviousFeet = false;
		glm::vec3 previousFeet{ 0.0f };
		float walkWeight = 0.0f, walkPhase = 0.0f;
		float swayX = 0.0f, swayY = 0.0f;
		double mouseX = 0.0, mouseY = 0.0;
		float mouseIntensity = 0.0008f;
		float elapsed = 0.0f, delta = 0.0f;
		gameplay::player::PlayerContext context{
			physics, scenePhysics, validGeometry, spawn, sceneName, camera, lookAt, angleZ,
			cameraMode, projectionMode, CAM_NAVEGA, PERSPECT, showWindow,
			projectionMenu, cameraMenu, objectMenu, lightingMenu, shaderMenu, worldLighting, skybox,
			[this] { ++callbacks; }, [this] { ++callbacks; }, [this] { ++callbacks; },
			[this] { ++callbacks; }, [this] { ++callbacks; }, [this] { ++callbacks; },
			movement, bob, hasPreviousFeet, previousFeet, walkWeight, walkPhase,
			swayX, swayY, mouseX, mouseY, mouseIntensity, elapsed, delta, [] { return true; }
		};
	};
}

int main()
{
	Fixture fixture;
	check(gameplay::player::isFpsMode(fixture.context), "FPS mode checks camera and projection");
	fixture.projectionMode = ORTO;
	check(!gameplay::player::isFpsMode(fixture.context), "non-perspective camera is not FPS mode");
	fixture.projectionMode = PERSPECT;

	gameplay::player::look(fixture.context, 1000.0);
	check(std::abs(fixture.lookAt[2]) > 0.99 && fixture.lookAt[2] < 0.0, "look pitch remains clamped to 85 degrees");
	check(std::abs(std::sqrt(fixture.lookAt[0] * fixture.lookAt[0] + fixture.lookAt[2] * fixture.lookAt[2]) - 1.0) < 1e-9,
		"looking preserves direction-vector length");

	fixture.mouseX = 1000.0;
	fixture.mouseY = -1000.0;
	gameplay::player::updateMobile(fixture.context, 0.5f);
	check(fixture.delta == 0.05f && std::abs(fixture.elapsed - 0.05f) < 1e-6f, "mobile delta is capped at 50 ms");
	check(fixture.mouseX == 0.0 && fixture.mouseY == 0.0, "mobile update consumes mouse deltas");
	check(std::abs(fixture.swayX) <= 0.12f && std::abs(fixture.swayY) <= 0.12f, "mouse sway stays within its legacy limit");

	gameplay::player::applyGamePreset(fixture.context);
	check(fixture.showWindow && fixture.projectionMenu == 3 && fixture.cameraMenu == 1 &&
		fixture.objectMenu == 1 && fixture.lightingMenu == 3 && fixture.shaderMenu == 2,
		"game preset sets the legacy menu state");
	check(fixture.worldLighting && fixture.skybox && fixture.callbacks == 6,
		"game preset enables world lighting and skybox and invokes each legacy action");

	if (failures != 0) std::cerr << failures << " PlayerRuntime checks failed\n";
	return failures == 0 ? 0 : 1;
}
