#pragma once

// Player state shared by the application glue. Defined in src/app/AppState.cpp.
#include "src/app/AppIncludes.h" // types: each slice compiles on its own
#include "src/gameplay/player/FpsMovement.h"
#include "src/gameplay/sanity/Sanity.h"
#include "src/gameplay/sanity/SanityEffects.h"
#include "src/legacy_vgi/framework.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

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
extern fps::PlayerMovement playerMovement;
extern fps::CameraBob cameraBob;
extern fps::StaminaBar staminaBar;
extern sanity::SanitySystem sanitySystem;
extern sanity::SanityEffects sanityEffects;
extern bool showStaminaPercent;
extern bool hasPreviousFrameFeet;
extern bool fpsPaused;
extern bool fpsCaptureRequested;
extern bool fpsCursorCaptured;
extern bool fpsHasPreviousPosition;
extern bool windowFocused;
extern bool showDebugPanels;
extern bool showFpsCounter;
extern GameState currentGameState;
