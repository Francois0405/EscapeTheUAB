// framework.h primero: GLEW debe incluirse antes que GLFW, y objLoader.h necesita CVAO.
#include "src/legacy_vgi/framework.h"
#include "src/scene/objLoader.h"
#include "src/scene/SceneRuntime.h"

#include "src/editor/SceneEditor.h"
#include "src/rendering/GlbDrawing.h"
#include "src/rendering/UniformCache.h"

#include <iostream>
#include <system_error>
#include <utility>

namespace scene::runtime
{
	namespace
	{
		void ReleaseMaterialTextures(const SceneRuntimeContext& context)
		{
			for (const auto& textura : context.materialTextures)
				if (textura.second != 0) glDeleteTextures(1, &textura.second);
			context.materialTextures.clear();
		}
	}

	void ReleaseAnimatedObjects(const SceneRuntimeContext& context)
	{
		if (context.animatedObjectsOnGpu) glb::ReleaseAnimatedSceneFromGpu(context.animatedObjects);
		glb::ClearAnimatedScene(context.animatedObjects);
		context.animatedObjectsOnGpu = false;
	}

	bool LoadAndApplyScene(const SceneRuntimeContext& context, const std::filesystem::path& ruta)
	{
		// 1) Leer y validar el JSON (si falla, no se ha tocado nada).
		SceneData datos;
		std::string error;
		if (!LoadSceneDataJson(ruta, datos, error))
		{
			std::cerr << "No se cargo la escena: " << error << '\n';
			return false;
		}
		editor::SceneSerializer nuevoSerializador;
		if (!nuevoSerializador.LoadScene(ruta, error))
		{
			std::cerr << "No se pudo preparar la persistencia de la escena: " << error << '\n';
			return false;
		}

		// 2) Cargar los modelos OBJ en estructuras TEMPORALES.
		std::unordered_map<std::string, std::unique_ptr<COBJModel>> nuevosModelos;
		std::vector<SceneObject> objetosValidos;
		auto liberarModelosTemporales = [&nuevosModelos]()
		{
			for (auto& entrada : nuevosModelos) entrada.second->netejaTextures_OBJ();
		};
		for (auto& obj : datos.objetos)
		{
			const auto rutaObj = std::filesystem::absolute(ruta.parent_path() / obj.modelo).lexically_normal();
			if (!std::filesystem::exists(rutaObj) || !std::filesystem::is_regular_file(rutaObj))
			{
				std::cerr << "No existe el OBJ: " << rutaObj << '\n';
				liberarModelosTemporales();
				return false;
			}
			const std::string clave = rutaObj.string();
			if (nuevosModelos.find(clave) == nuevosModelos.end())
			{
				auto modelo = std::make_unique<COBJModel>();
				std::string rutaEditable = clave;
				const int resultado = modelo->LoadModel(rutaEditable.data());
				if (resultado != 0)
				{
					std::cerr << "Error cargando OBJ " << clave << " (codigo " << resultado << ")\n";
					modelo->netejaTextures_OBJ();
					liberarModelosTemporales();
					return false;
				}
				// Todas las instancias de un mismo OBJ comparten el modelo ya cargado.
				nuevosModelos.emplace(clave, std::move(modelo));
			}
			obj.modelo = clave;
			objetosValidos.push_back(std::move(obj));
		}

		// 3) Objetos animados GLB de la lista "animated", también temporales.
		glb::AnimatedScene nuevosAnimados;
		if (!physics::LoadSceneAnimatedObjects(datos.animados, std::filesystem::absolute(ruta.parent_path()),
			nuevosAnimados, error))
		{
			std::cerr << "No se cargo la escena: " << error << '\n';
			liberarModelosTemporales();
			return false;
		}
		for (const auto& modelo : nuevosAnimados.modelos)
			for (const std::string& aviso : modelo.second.avisos)
				std::cerr << modelo.first << ": " << aviso << '\n';

		// 4) Cuerpos físicos NUEVOS (los de la habitación anterior siguen en el mundo hasta el final).
		auto nuevaFisica = std::make_unique<physics::PhysicsScene>(context.physicsWorld);
		if (!nuevaFisica->Create(objetosValidos, nuevosAnimados, datos.animados, error))
		{
			std::cerr << "No se cargo la escena (fisicas): " << error << '\n';
			liberarModelosTemporales();
			return false;
		}
		if ((objetosValidos.empty() && datos.animados.empty()) || nuevaFisica->BodyCount() == 0)
		{
			std::cerr << "La escena no contiene geometria/colliders fisicos cargables; se cancela el arranque." << std::endl;
			liberarModelosTemporales();
			return false;
		}

		// 5) Todo ha ido bien: AHORA se sustituye la habitación anterior.
		context.physicsScene->ClearBodies();                // retira sus colisiones del mundo
		context.physicsScene = std::move(nuevaFisica);
		ReleaseMaterialTextures(context);
		for (auto& entrada : context.objModels) entrada.second->netejaTextures_OBJ();
		context.objModels = std::move(nuevosModelos);
		context.sceneObjects = std::move(objetosValidos);
		context.hiritiAnimation.Reset();
		context.phongOccludersDirty = true;
		ReleaseAnimatedObjects(context);               // libera de la GPU los GLB anteriores
		context.animatedObjects = std::move(nuevosAnimados);
		glb::UploadAnimatedSceneToGpu(context.animatedObjects);  // el contexto OpenGL ya existe en este punto
		context.animatedObjectsOnGpu = true;
		context.activeSceneName = std::move(datos.nombre);
		context.animatedData = std::move(datos.animados);
		context.serializer = std::move(nuevoSerializador);
		context.activeSpawn = datos.spawn;
		context.indirectIntensity = datos.indirectIntensity;
		context.indirectAmbientBase = datos.indirectAmbientBase;
		context.directSceneLightScale = datos.directSceneLightScale;
		context.indirectFloorColor = datos.indirectFloorColor;
		context.indirectSkyColor = datos.indirectSkyColor;
		if (!context.activeSpawn.presente)
		{
			context.activeSpawn.presente = true; // escenas antiguas conservan la vista existente como fallback
			context.activeSpawn.pies = context.physicsWorld.HasPlayerBody()
				? context.physicsWorld.PlayerFeetPosition() : context.cameraFeetPosition();
			const glm::vec3 mirada = context.viewForward();
			context.activeSpawn.direccion = glm::length(mirada) > 1e-4f ? glm::normalize(mirada) : glm::vec3(1, 0, 0);
		}
		context.hasValidGeometry = context.physicsScene->BodyCount() > 0;
		context.fixCameraAtSpawn(context.activeSpawn);
		if (context.physicsWorld.HasPlayerBody() && !context.createPhysicsPlayer())
		{
			context.physicsWorld.RemovePlayerBody();
			context.hasValidGeometry = false;
			return false;
		}
		context.sceneEditor.NewScene();
		{
			std::error_code rutaModelos;
			const auto raiz = std::filesystem::absolute(ruta.parent_path() / ".." / "models", rutaModelos).lexically_normal();
			if (!rutaModelos && std::filesystem::is_directory(raiz)) context.sceneEditor.SetModelsRoot(raiz);
			context.sceneEditor.SetSceneRoot(std::filesystem::absolute(ruta.parent_path(), rutaModelos).lexically_normal());
			context.sceneEditor.SetAnimationsRoot(std::filesystem::absolute(ruta.parent_path() / ".." / "animations", rutaModelos).lexically_normal());
		}
		context.objectType = OBJOBJ;
		context.textureEnabled = true;
		context.textureInvertY = false;
		glUseProgram(context.shaderProgram);
		glUniform1i(glcache::UniformLocation(context.shaderProgram, "textur"), context.textureEnabled);
		glUniform1i(glcache::UniformLocation(context.shaderProgram, "flag_invert_y"), context.textureInvertY);
		std::error_code rutaEscenaError;
		const auto rutaEscenaAbsoluta = std::filesystem::absolute(ruta, rutaEscenaError).lexically_normal();
		std::cout << "Escena cargada desde " << (rutaEscenaError ? ruta.string() : rutaEscenaAbsoluta.string()) << ": "
			<< context.activeSceneName << " (" << context.sceneObjects.size() << " instancias, " << context.animatedObjects.objetos.size()
			<< " animadas, " << context.physicsScene->BodyCount() << " cuerpos fisicos)." << std::endl;
		if (context.networkActive())
		{
			std::cerr << "Red: la escena ha cambiado; se detiene la sesion (host y clientes deben usar la misma escena)." << std::endl;
			context.stopNetwork();
		}
		return true;
	}

	bool RebuildPhysicsAfterEdit(const SceneRuntimeContext& context, std::string& error, bool physicsAuthority)
	{
		auto nuevaFisica = std::make_unique<physics::PhysicsScene>(context.physicsWorld);
		if (!nuevaFisica->Create(context.sceneObjects, context.animatedObjects, context.animatedData, error, physicsAuthority)) return false;
		context.physicsScene->ClearBodies();
		context.physicsScene = std::move(nuevaFisica);
		return true;
	}

	bool LoadModelForEditor(const SceneRuntimeContext& context, const std::filesystem::path& ruta,
		std::string& modeloCargado, std::string& error)
	{
		std::error_code ec;
		const auto absoluta = std::filesystem::absolute(ruta, ec).lexically_normal();
		if (ec || !std::filesystem::is_regular_file(absoluta, ec))
		{
			error = "El archivo OBJ seleccionado no existe.";
			return false;
		}
		modeloCargado = absoluta.string();
		if (context.objModels.contains(modeloCargado)) return true;
		auto modelo = std::make_unique<COBJModel>();
		std::string rutaMutable = modeloCargado;
		if (modelo->LoadModel(rutaMutable.data()) != 0)
		{
			modelo->netejaTextures_OBJ();
			error = "El cargador OBJ rechazó el modelo seleccionado.";
			return false;
		}
		context.objModels.emplace(modeloCargado, std::move(modelo));
		return true;
	}
}
