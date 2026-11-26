#pragma once

// Gameplay state shared by the application glue. Defined in src/app/AppState.cpp.
#include "src/app/AppIncludes.h" // types: each slice compiles on its own
#include "src/audio/AudioManager.h"
#include "src/gameplay/hirity/HiritiAnimation.h"
#include "src/gameplay/interaction/InteractionRuntime.h"
#include "src/legacy_vgi/constants.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using game::interaction::InteractionAction;
using game::interaction::InteractionTarget;

inline constexpr float kMaxThrowChargeDuration = game::interaction::kMaxThrowChargeSeconds;

extern AudioManager audioManager;
extern game::HiritiAnimator hiritiAnimator;
extern bool hiritiFollowPlayerEnabled;
extern float hiritiFollowStopDistance;
extern float hiritiFollowMaxSpeed;
extern bool throwChargeActive;
extern float throwChargeTime;
extern InteractionTarget currentInteractionTarget;
