#pragma once

// Carga y aplicacion de escenas en ejecucion: carga transaccional desde JSON, reconstruccion de la
// fisica tras editar y carga de modelos OBJ para el editor. No define el formato JSON ni la fisica.
// main.cpp es el dueno de todo el estado y lo presta mediante este contexto; aqui no se puede
// incluir main.h (define globales).
// La API publica va en ingles; main.cpp enlaza cada miembro en contextoEscena().
#include "src/animation/ObjetosAnimadosGLB.h"
#include "src/editor/SerializadorEscenas.h"
#include "src/gameplay/hirity/AnimacionHiriti.h"
#include "src/physics/EscenaFisica.h"
#include "src/physics/MundoFisico.h"
#include "src/scene/SceneLoader.h"

#include <glm/glm.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Solo referencias: sus cabeceras arrastran OpenGL/GLFW.
class COBJModel;
namespace editor { class EditorEscenas; }

namespace scene::runtime
{
	struct SceneRuntimeContext
	{
		// Contenido de la habitacion activa
		std::vector<SceneObject>& sceneObjects;
		std::unordered_map<std::string, std::unique_ptr<COBJModel>>& objModels;	// compartidos entre instancias
		std::unordered_map<std::string, unsigned int>& materialTextures;			// nombres de textura OpenGL
		std::vector<SceneObjetoAnimado>& animatedData;
		glb::EscenaAnimada& animatedObjects;
		bool& animatedObjectsOnGpu;
		editor::SerializadorEscenas& serializer;

		// Fisica: referencia al unique_ptr porque cargar o reconstruir sustituye la escena fisica.
		fisica::MundoFisico& physicsWorld;
		std::unique_ptr<fisica::EscenaFisica>& physicsScene;

		// Datos de la escena activa
		std::string& activeSceneName;
		SceneSpawn& activeSpawn;
		float& indirectIntensity;
		float& indirectAmbientBase;
		float& directSceneLightScale;
		glm::vec3& indirectFloorColor;
		glm::vec3& indirectSkyColor;
		bool& hasValidGeometry;
		bool& phongOccludersDirty;

		editor::EditorEscenas& sceneEditor;
		juego::AnimacionHiriti& hiritiAnimation;

		// Estado de render heredado (main.h); el programa puede cambiar entre cargas.
		char& objectType;
		bool& textureEnabled;
		bool& textureInvertY;
		const unsigned int& shaderProgram;

		// Servicios que siguen en main.cpp (camara, jugador y red)
		std::function<glm::vec3()> cameraFeetPosition;	// pies deducidos de la camara
		std::function<glm::vec3()> viewForward;			// punto mirado menos ojos, sin normalizar
		std::function<void(const SceneSpawn& spawn)> fixCameraAtSpawn;
		std::function<bool()> createPhysicsPlayer;
		std::function<bool()> networkActive;
		std::function<void()> stopNetwork;
	};

	// Carga transaccional: si falla la lectura, los modelos o la fisica, la escena anterior no se toca.
	// Tambien devuelve false si, ya aplicada la escena nueva, no se puede recrear el jugador.
	// Necesita un contexto OpenGL activo.
	bool LoadAndApplyScene(const SceneRuntimeContext& context, const std::filesystem::path& path);
	bool RebuildPhysicsAfterEdit(const SceneRuntimeContext& context, std::string& error, bool physicsAuthority);
	bool LoadModelForEditor(const SceneRuntimeContext& context, const std::filesystem::path& path,
		std::string& loadedModel, std::string& error);
	void ReleaseAnimatedObjects(const SceneRuntimeContext& context);
}
