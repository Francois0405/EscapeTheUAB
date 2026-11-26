#pragma once

// Runtime ImGui UI. src/app owns every piece of game state and lends it through these
// contexts; nothing here may include the application state headers.
// All drawing is ImGui-native and happens after the PS1 scene pass.
#include <functional>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

class AudioManager;
struct GLFWwindow;
struct SceneObject;
namespace animation { struct Playback; }
namespace physics { class FallingCubeDemo; class PhysicsScene; class PhysicsWorld; }
namespace fps { class CameraBob; class StaminaBar; class PlayerMovement; }
namespace glb { struct AnimatedScene; }
namespace game { class HiritiAnimator; }
namespace netgame { class NetworkGame; struct NetworkConfig; }
namespace retro { struct Settings; }
namespace subtitles { class Manager; }
namespace vr { class OpenXrManager; }

enum GameState {
	GAME_STATE_MAIN_MENU,
	GAME_STATE_PLAYING,
	GAME_STATE_PAUSED
};

namespace runtimeui
{
	// Predicates are callbacks because their answer can change in the middle of a frame.
	struct HudContext
	{
		std::function<bool()> isFpsMode;
		std::function<bool()> isFpsInputActive;
		std::function<bool()> isEditorActive;
		std::function<bool()> playerHoldsObject;
		std::function<bool()> isDesktopHudVisible;
		const fps::StaminaBar& staminaBar;
		const fps::PlayerMovement& movement;
		const bool& showStaminaPercent;
		const bool& throwCharging;
		const float& throwChargeSeconds;
		float maxThrowChargeSeconds;
		const std::unique_ptr<physics::PhysicsScene>& physicsScene;
		std::function<const char*()> interactionPrompt;   // nullptr when nothing is targeted
		const unsigned int& handTexture;
		const bool& showPickupNotice;
	};

	struct MenuContext
	{
		GLFWwindow* window;
		GameState& state;
		const unsigned int& backgroundTexture;
		float& masterVolumePercent;
		AudioManager& audio;
		std::function<void()> resetMouseLook;
	};

	struct SubtitleContext
	{
		subtitles::Manager& manager;
		const retro::Settings& settings;
		std::function<bool(unsigned int)> isPlaying;
		std::function<float(unsigned int)> cursorSeconds;
	};

	struct MultiplayerContext
	{
		netgame::NetworkGame& session;
		netgame::NetworkConfig& pendingConfig;
		bool& panelVisible;
		std::function<void(const netgame::NetworkConfig&)> startNetwork;
		std::function<void()> stopNetwork;
	};

	struct DynamicObjectsContext
	{
		netgame::NetworkGame& session;
		const std::unique_ptr<physics::PhysicsScene>& physicsScene;
		const std::vector<SceneObject>& sceneObjects;
		const bool& hiritiFollowEnabled;
		const game::HiritiAnimator& hiriti;
	};

	// F1 gameplay panels: phone, movement/render metrics, lighting, door demo, physics/OpenXR, GLB.
	struct DebugPanelsContext
	{
		bool& showPhone;
		glm::vec3& phonePosition;
		glm::vec3& phoneRotation;
		float& phoneScale;
		float& idleIntensity;
		float& walkingIntensity;
		float& mouseIntensity;
		const float& frameDeltaSeconds;
		fps::PlayerMovement& movement;
		fps::CameraBob& cameraSway;
		bool& showStaminaPercent;
		bool& showFpsCounter;
		bool& optimizedRendering;
		const double& lastGpuMs;
		int& lightingView;
		const animation::Playback& doorDemoPlayer;
		const float& doorDemoAngle;
		bool& showColliders;
		const bool& hiritiFollowEnabled;
		float& hiritiStopDistance;
		float& hiritiMaxSpeed;
		bool& stereoPreview;
		vr::OpenXrManager& openXR;
		physics::PhysicsWorld& physicsWorld;
		const std::unique_ptr<physics::PhysicsScene>& physicsScene;
		physics::FallingCubeDemo& cubeDemo;
		netgame::NetworkGame& session;
		const std::vector<SceneObject>& sceneObjects;
		glb::AnimatedScene& animatedObjects;
		std::function<void()> startDoorDemo;
		std::function<void()> toggleOpenXR;
		retro::Settings& retroSettings;
	};

	struct FrameContext
	{
		HudContext hud;
		MenuContext menu;
		MultiplayerContext network;
		DynamicObjectsContext dynamicObjects;
		DebugPanelsContext debug;
		SubtitleContext subtitle;
		bool useLightStyle;
		const bool& showDebugPanels;
		const bool& showFpsCounter;
		std::function<void()> drawLegacyWindows;            // legacy VGI windows stay in src/app
		std::function<void(int width, int height)> drawEditor;
	};

	// Starts the ImGui frame, builds every window and calls ImGui::Render().
	void DrawRuntimeUiFrame(const FrameContext& context);

	void DrawMultiplayerPanel(const MultiplayerContext& context, float uiScale);
	void DrawDynamicObjectsPanel(const DynamicObjectsContext& context);
	void DrawDebugPanels(const DebugPanelsContext& context);
}
