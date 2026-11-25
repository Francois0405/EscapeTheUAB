#pragma once

#include "src/app/AppIncludes.h"

const scene::runtime::SceneRuntimeContext& sceneBindings();
void releaseSceneAnimations();
bool loadAndApplyScene(const std::filesystem::path& ruta);
bool rebuildPhysicsAfterEditing(std::string& error, bool physicsAuthority = true);
bool loadEditorModel(const std::filesystem::path& ruta, std::string& modeloCargado, std::string& error);
