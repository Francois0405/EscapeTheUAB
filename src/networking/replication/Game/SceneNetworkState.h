#pragma once

#include "src/networking/replication/NetGame.h"
#include "src/scene/SceneLoader.h"

#include <map>
#include <string>
#include <vector>

namespace netgame::scene_network_state
{
	template <typename HashIdFn>
	inline bool CaptureSceneNetworkStates(const std::vector<SceneObject>& objects,
		std::vector<SceneObjectState>& output, HashIdFn hashFunction)
	{
		std::map<uint32_t, std::string> persistentIds;
		output.clear();
		output.reserve(objects.size());
		for (const SceneObject& object : objects)
		{
			SceneObjectState state;
			state.id = hashFunction(object.id);
			if (state.id == 0 || !persistentIds.emplace(state.id, object.id).second) return false;
			state.position = object.position; state.eulerRotation = object.eulerRotation; state.escala = object.escala;
			state.hasCollision = object.fisica.presente;
			state.collisionKind = SceneCollisionType(object.fisica.presente,
				physics::IsDynamicBody(object.fisica.tipo));
			state.halfExtents = object.fisica.halfExtents;
			state.desplazamiento = object.fisica.desplazamiento;
			state.masa = object.fisica.masa;
			if (!IsValidSceneObjectState(state)) return false;
			output.push_back(state);
		}
		return true;
	}

	inline bool CaptureSceneNetworkStates(const std::vector<SceneObject>& objects,
		std::vector<SceneObjectState>& output)
	{
		return CaptureSceneNetworkStates(objects, output,
			[](const std::string& objectId) { return HashId(objectId); });
	}

	template <typename HashIdFn>
	inline bool FindUniqueDynamicSceneObjectIndex(uint32_t id, const std::vector<SceneObject>& objects,
		HashIdFn hashFunction, std::size_t& index)
	{
		std::size_t matchCount = 0;
		for (std::size_t i = 0; i < objects.size(); ++i)
			if (hashFunction(objects[i].id) == id) { index = i; ++matchCount; }
		return matchCount == 1 && index < objects.size() && objects[index].fisica.presente &&
			physics::IsDynamicBody(objects[index].fisica.tipo);
	}

	inline bool FindUniqueDynamicSceneObjectIndex(uint32_t id, const std::vector<SceneObject>& objects,
		std::size_t& index)
	{
		return FindUniqueDynamicSceneObjectIndex(id, objects,
			[](const std::string& sceneId) { return HashId(sceneId); }, index);
	}
}
