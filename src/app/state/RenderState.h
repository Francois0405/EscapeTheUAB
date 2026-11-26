#pragma once

// Render state shared by the application glue. Defined in src/app/AppState.cpp.
#include "src/app/AppIncludes.h" // types: each slice compiles on its own
#include "src/legacy_vgi/framework.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

extern bool optimizedRendering;
extern bool phongOccludersDirty;
extern GLuint gpuQueries[3];
extern double lastGpuMs;
extern int lightingDiagnosticMode;
extern bool showPhone;
extern glm::vec3 phonePosition;
extern glm::vec3 phoneRotation;
extern float phoneScale;
extern bool phoneFlashlightEnabled;
extern bool labLampsEnabled;
extern float sceneIndirectIntensity;
extern float sceneBaseAmbientIntensity;
extern float sceneDirectLightScale;
extern glm::vec3 sceneGroundBounceColor;
extern glm::vec3 sceneSkyBounceColor;
extern GLuint rayVao, rayVbo;
extern GLuint menuBackgroundTexture;
extern GLuint interactionHandTexture;
