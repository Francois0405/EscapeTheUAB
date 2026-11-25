#pragma once

// Game state shared by the application modules. Definitions live in AppState.cpp.
#include "src/app/AppIncludes.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using game::interaction::InteractionAction;
using game::interaction::InteractionTarget;

extern std::vector<SceneObject> sceneObjects;
extern std::unordered_map<std::string, std::unique_ptr<COBJModel>> roomModels;
extern editor::SceneEditor sceneEditor;
extern editor::SceneSerializer sceneSerializer;
extern std::vector<SceneAnimatedObject> sceneAnimatedData;
extern COBJModel* phoneOBJ;
extern std::unordered_map<std::string, GLuint> sceneMaterialTextures;
extern bool stereoTestMode;
extern vr::OpenXrManager openXrManager;
extern GLFWwindow* mainWindow;
extern int glMajorVersion, glMinorVersion;
extern bool fpsPaused;
extern bool fpsCaptureRequested;
extern bool fpsCursorCaptured;
extern bool fpsHasPreviousPosition;
extern bool windowFocused;
extern bool showDebugPanels;
extern bool showFpsCounter;
extern bool optimizedRendering;
extern bool phongOccludersDirty;
extern GLuint gpuQueries[3];
extern bool gpuQueryPending[3];
extern int currentGpuQuery;
extern double lastGpuMs;
extern bool gpuQueryActive;
extern int lightingDiagnosticMode;
extern double fpsLastX;
extern double fpsLastY;
extern bool showPhone;
extern glm::vec3 phonePosition;
extern glm::vec3 phoneRotation;
extern float phoneScale;
extern float idleSwayIntensity;
extern float walkSwayIntensity;
extern float mouseSwayIntensity;
extern float walkWeight;
extern float walkPhase;
extern float mouseSwayX;
extern float mouseSwayY;
extern double mouseDeltaX;
extern double mouseDeltaY;
extern float fpsTime;
extern float deltaFPS;
extern AudioManager audioManager;
extern float masterVolumePercent;
extern game::HiritiAnimator hiritiAnimator;
extern game::HiritiAudioTimer hiritiAudioTimer;
extern fps::PlayerMovement playerMovement;
extern fps::CameraBob cameraBob;
extern fps::StaminaBar staminaBar;
extern bool showStaminaPercent;
extern glm::vec3 previousFrameFeet;
extern bool hasPreviousFrameFeet;
extern std::vector<animation::AngleKeyframe> demoDoorKeyframes;
extern animation::Reproductor demoDoorPlayback;
extern float demoDoorAngle;
extern const glm::vec3 demoDoorHalfSize;
extern const glm::vec3 demoDoorHinge;
extern glb::AnimatedScene animatedObjects;
extern fisica::PhysicsWorld physicsWorld;
extern std::unique_ptr<fisica::PhysicsScene> physicsScene;
extern fisica::FallingCubeDemo fallingCubeDemo;
extern bool showColliders;
extern bool hiritiFollowPlayerEnabled;
extern float hiritiFollowStopDistance;
extern float hiritiFollowMaxSpeed;
extern bool animatedObjectsReady;
extern GLuint interactionHandTexture;
extern bool throwChargeActive;
extern float throwChargeTime;
extern bool phoneFlashlightEnabled;
extern bool labLampsEnabled;
extern netgame::NetworkGame networkGame;
extern bool showNetworkPanel;
extern std::unique_ptr<netgame::GlbDoors> networkDoors;
extern netgame::NetworkConfig pendingNetworkConfig;
extern netgame::session::SessionRuntimeState networkSessionState;
extern vr::XrPose lastVrHead;
extern bool hasVrHead;
extern InteractionTarget currentInteractionTarget;
extern std::string activeSceneName;
extern SceneSpawn activeSceneSpawn;
extern float sceneIndirectIntensity;
extern float sceneBaseAmbientIntensity;
extern float sceneDirectLightScale;
extern glm::vec3 sceneGroundBounceColor;
extern glm::vec3 sceneSkyBounceColor;
extern bool sceneHasValidGeometry;
extern GLuint rayVao, rayVbo;
extern GLuint menuBackgroundTexture;
extern EstadoJuego currentGameState;

inline constexpr float kMaxThrowChargeDuration = game::interaction::kMaxThrowChargeSeconds;
