#pragma once

// Runtime ImGui UI. main.cpp owns every piece of game state and lends it through these
// contexts; nothing here may include main.h (it defines globals).
// All drawing is ImGui-native and happens after the PS1 scene pass.
#include <functional>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

class AudioManager;
struct GLFWwindow;
struct SceneObject;
namespace animacion { struct Reproductor; }
namespace fisica { class DemoCuboCaida; class EscenaFisica; class MundoFisico; }
namespace fps { class BalanceoCamara; class BarraEstamina; class MovimientoJugador; }
namespace glb { struct EscenaAnimada; }
namespace juego { class AnimacionHiriti; }
namespace netjuego { class RedJuego; struct ConfigRed; }
namespace vr { class GestorOpenXR; }

enum EstadoJuego {
	ESTADO_MENU_PRINCIPAL,
	ESTADO_JUGANDO,
	ESTADO_PAUSA
};

namespace runtimeui
{
	// Predicates are callbacks because their answer can change in the middle of a frame.
	struct HudContext
	{
		std::function<bool()> isFpsMode;
		std::function<bool()> isFpsInputActive;
		std::function<bool()> isEditorActive;
		const fps::BarraEstamina& staminaBar;
		const fps::MovimientoJugador& movement;
		const bool& showStaminaPercent;
		const bool& throwCharging;
		const float& throwChargeSeconds;
		float maxThrowChargeSeconds;
		const std::unique_ptr<fisica::EscenaFisica>& physicsScene;
		std::function<const char*()> interactionPrompt;   // nullptr when nothing is targeted
		const unsigned int& handTexture;
	};

	struct MenuContext
	{
		GLFWwindow* window;
		EstadoJuego& state;
		const unsigned int& backgroundTexture;
		float& masterVolumePercent;
		AudioManager& audio;
		std::function<void()> resetMouseLook;
	};

	struct MultiplayerContext
	{
		netjuego::RedJuego& session;
		netjuego::ConfigRed& pendingConfig;
		bool& panelVisible;
		std::function<void(const netjuego::ConfigRed&)> startNetwork;
		std::function<void()> stopNetwork;
	};

	struct DynamicObjectsContext
	{
		netjuego::RedJuego& session;
		const std::unique_ptr<fisica::EscenaFisica>& physicsScene;
		const std::vector<SceneObject>& sceneObjects;
		const bool& hiritiFollowEnabled;
		const juego::AnimacionHiriti& hiriti;
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
		fps::MovimientoJugador& movement;
		fps::BalanceoCamara& cameraSway;
		bool& showStaminaPercent;
		bool& showFpsCounter;
		bool& optimizedRendering;
		const double& lastGpuMs;
		int& lightingView;
		const animacion::Reproductor& doorDemoPlayer;
		const float& doorDemoAngle;
		bool& showColliders;
		const bool& hiritiFollowEnabled;
		float& hiritiStopDistance;
		float& hiritiMaxSpeed;
		bool& stereoPreview;
		vr::GestorOpenXR& openXR;
		fisica::MundoFisico& physicsWorld;
		const std::unique_ptr<fisica::EscenaFisica>& physicsScene;
		fisica::DemoCuboCaida& cubeDemo;
		netjuego::RedJuego& session;
		const std::vector<SceneObject>& sceneObjects;
		glb::EscenaAnimada& animatedObjects;
		std::function<void()> startDoorDemo;
		std::function<void()> toggleOpenXR;
	};

	struct FrameContext
	{
		HudContext hud;
		MenuContext menu;
		MultiplayerContext network;
		DynamicObjectsContext dynamicObjects;
		DebugPanelsContext debug;
		bool useLightStyle;
		const bool& showDebugPanels;
		const bool& showFpsCounter;
		std::function<void()> drawLegacyWindows;            // legacy VGI windows stay in main.cpp
		std::function<void(int width, int height)> drawEditor;
	};

	// Starts the ImGui frame, builds every window and calls ImGui::Render().
	void DrawRuntimeUiFrame(const FrameContext& context);

	void DrawMultiplayerPanel(const MultiplayerContext& context, float uiScale);
	void DrawDynamicObjectsPanel(const DynamicObjectsContext& context);
	void DrawDebugPanels(const DebugPanelsContext& context);
}
