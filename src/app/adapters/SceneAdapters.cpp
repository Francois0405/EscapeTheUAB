#include "src/app/AppState.h"
#include "src/app/adapters/SceneAdapters.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/app/adapters/SessionAdapters.h"

// Contexto prestado a src/scene/SceneRuntime: el estado sigue siendo de main.cpp.
const scene::runtime::SceneRuntimeContext& contextoEscena()
{
	static const scene::runtime::SceneRuntimeContext contexto{
		objetosEscena, modelosHabitacion, texturasMaterialEscena, datosAnimadosEscena, objetosAnimados,
		objetosAnimadosListos, serializadorEscena, mundoFisico, escenaFisica,
		nombreEscenaActiva, spawnEscenaActiva, intensidadIndirectaEscena, intensidadAmbientalBaseEscena,
		escalaLuzDirectaEscena, colorReboteSueloEscena, colorReboteCieloEscena,
		escenaTieneGeometriaValida, oclusoresPhongInvalidos, editorEscenas, animacionHiriti,
		objecte, textura, tFlag_invert_Y, shader_programID,
		[] { return posicionPiesDesdeCamara(); },
		[] { return glm::vec3(static_cast<float>(n[0] - opvN.x), static_cast<float>(n[1] - opvN.y), static_cast<float>(n[2] - opvN.z)); },
		[](const SceneSpawn& spawn) { fijarCamaraAlSpawn(spawn); },
		[] { return crearJugadorFisico(); },
		[] { return redJuego.EnRed(); },
		[] { detenerRed(); }
	};
	return contexto;
}

void liberarObjetosAnimados() { scene::runtime::ReleaseAnimatedObjects(contextoEscena()); }

bool cargarYAplicarEscena(const std::filesystem::path& ruta)
{
	return scene::runtime::LoadAndApplyScene(contextoEscena(), ruta);
}

bool reconstruirFisicaTrasEdicion(std::string& error, bool autoridadFisica)
{
	return scene::runtime::RebuildPhysicsAfterEdit(contextoEscena(), error, autoridadFisica);
}

bool cargarModeloParaEditor(const std::filesystem::path& ruta, std::string& modeloCargado, std::string& error)
{
	return scene::runtime::LoadModelForEditor(contextoEscena(), ruta, modeloCargado, error);
}
