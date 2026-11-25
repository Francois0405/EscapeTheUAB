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
			state.posicion = object.posicion; state.rotacion = object.rotacion; state.escala = object.escala;
			state.colisionPresente = object.fisica.presente;
			state.tipoColision = TipoColisionEscena(object.fisica.presente,
				fisica::EsCuerpoDinamico(object.fisica.tipo));
			state.mitadTamano = object.fisica.mitadTamano;
			state.desplazamiento = object.fisica.desplazamiento;
			state.masa = object.fisica.masa;
			if (!EstadoObjetoEscenaValido(state)) return false;
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
			fisica::EsCuerpoDinamico(objects[index].fisica.tipo);
	}

	inline bool FindUniqueDynamicSceneObjectIndex(uint32_t id, const std::vector<SceneObject>& objects,
		std::size_t& index)
	{
		return FindUniqueDynamicSceneObjectIndex(id, objects,
			[](const std::string& sceneId) { return HashId(sceneId); }, index);
	}
}
