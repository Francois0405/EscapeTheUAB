#pragma once

// Vr state shared by the application glue. Defined in src/app/AppState.cpp.
#include "src/app/AppIncludes.h" // types: each slice compiles on its own
#include "src/vr/OpenXRManager.h"
#include "src/legacy_vgi/framework.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

extern vr::OpenXrManager openXrManager;
extern GLFWwindow* mainWindow;
extern int glMajorVersion, glMinorVersion;
extern bool stereoTestMode;
extern vr::XrPose lastVrHead;
extern bool hasVrHead;
