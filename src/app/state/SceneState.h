#pragma once

// Scene state shared by the application glue. Defined in src/app/AppState.cpp.
#include "src/scene/SceneLoader.h"
#include "src/editor/SceneEditor.h"
#include "src/editor/SceneSerializer.h"
#include "src/legacy_vgi/framework.h"
#include "objLoader.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

extern std::vector<SceneObject> sceneObjects;
extern std::unordered_map<std::string, std::unique_ptr<COBJModel>> roomModels;
extern editor::SceneEditor sceneEditor;
extern editor::SceneSerializer sceneSerializer;
extern std::vector<SceneAnimatedObject> sceneAnimatedData;
extern COBJModel* phoneOBJ;
extern std::unordered_map<std::string, GLuint> sceneMaterialTextures;
extern std::string activeSceneName;
extern SceneSpawn activeSceneSpawn;
extern bool sceneHasValidGeometry;
extern glb::AnimatedScene animatedObjects;
extern bool animatedObjectsReady;
extern fisica::PhysicsWorld physicsWorld;
extern std::unique_ptr<fisica::PhysicsScene> physicsScene;
extern fisica::FallingCubeDemo fallingCubeDemo;
extern bool showColliders;
extern animation::Reproductor demoDoorPlayback;
extern float demoDoorAngle;
