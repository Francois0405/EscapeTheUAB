#include "src/app/AppState.h"
#include "src/app/adapters/SceneAdapters.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/app/adapters/SessionAdapters.h"

// Contexto prestado a src/scene/SceneRuntime: el estado sigue siendo de main.cpp.
const scene::runtime::SceneRuntimeContext& sceneBindings()
{
	static const scene::runtime::SceneRuntimeContext contexto{
		sceneObjects, roomModels, sceneMaterialTextures, sceneAnimatedData, animatedObjects,
		animatedObjectsReady, sceneSerializer, physicsWorld, physicsScene,
		activeSceneName, activeSceneSpawn, sceneIndirectIntensity, sceneBaseAmbientIntensity,
		sceneDirectLightScale, sceneGroundBounceColor, sceneSkyBounceColor,
		sceneHasValidGeometry, phongOccludersDirty, sceneEditor, hiritiAnimator,
		objecte, textura, tFlag_invert_Y, shader_programID,
		[] { return feetPositionFromCamera(); },
		[] { return glm::vec3(static_cast<float>(n[0] - opvN.x), static_cast<float>(n[1] - opvN.y), static_cast<float>(n[2] - opvN.z)); },
		[](const SceneSpawn& spawn) { placeCameraAtSpawn(spawn); },
		[] { return spawnPhysicsPlayer(); },
		[] { return networkGame.EnRed(); },
		[] { stopNetwork(); }
	};
	return contexto;
}

void releaseSceneAnimations() { scene::runtime::ReleaseAnimatedObjects(sceneBindings()); }

bool loadAndApplyScene(const std::filesystem::path& ruta)
{
	return scene::runtime::LoadAndApplyScene(sceneBindings(), ruta);
}

bool rebuildPhysicsAfterEditing(std::string& error, bool autoridadFisica)
{
	return scene::runtime::RebuildPhysicsAfterEdit(sceneBindings(), error, autoridadFisica);
}

bool loadEditorModel(const std::filesystem::path& ruta, std::string& modeloCargado, std::string& error)
{
	return scene::runtime::LoadModelForEditor(sceneBindings(), ruta, modeloCargado, error);
}
