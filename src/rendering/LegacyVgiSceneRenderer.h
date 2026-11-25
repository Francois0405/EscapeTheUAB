#pragma once

#include "framework.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class COBJModel;
struct SceneMaterial;
struct SceneObject;
namespace editor { class SceneEditor; }
namespace fisica { class FallingCubeDemo; class PhysicsScene; class PhysicsWorld; }
namespace glb { struct AnimatedScene; }
namespace netgame { class NetworkGame; }

// Borrows live scene state, including the current view and projection for each eye.
struct SceneRenderContext
{
	// Programs, camera and legacy geometry
	const GLuint& shader_programID;
	const GLuint& eixos_programID;
	const GLuint& eixos_Id;
	const glm::mat4& ProjectionMatrix;
	const glm::mat4& ViewMatrix;
	const bool& SkyBoxCube;
	const GLuint& skC_programID;
	const GLuint& cubemapTexture;
	const char& Vis_Polar;
	const bool& eixos;
	const CMask3D& grid;
	const CPunt3D& hgrid;
	const char& objecte;
	const CColor& col_obj;
	bool (&sw_material)[5];
	bool& textura;
	GLuint (&texturesID)[NUM_MAX_TEXTURES];
	const bool& textura_map;
	const bool& tFlag_invert_Y;
	const int& npts_T;
	CPunt3D (&PC_t)[MAX_PATCH_CORBA];
	const GLdouble& pas_CS;
	const bool& sw_Punts_Control;
	const bool& dibuixa_TriedreFrenet;
	COBJModel* const& ObOBJ;
	const glm::mat4& GTMatrix;

	// Lighting and phone
	const bool& renderizadoOptimizado;
	const float& intensidadIndirectaEscena;
	const float& intensidadAmbientalBaseEscena;
	const float& escalaLuzDirectaEscena;
	const int& modoDiagnosticoIluminacion;
	const glm::vec3& colorReboteSueloEscena;
	const glm::vec3& colorReboteCieloEscena;
	const bool& lamparasLaboratorioActivas;
	LLUM (&llumGL)[NUM_MAX_LLUMS];
	const bool& mostrarMovil;
	COBJModel* const& phoneOBJ;
	const float& movilEscala;

	// Scene and live runtime state
	const std::vector<SceneObject>& objetosEscena;
	const std::unordered_map<std::string, std::unique_ptr<COBJModel>>& modelosHabitacion;
	const editor::SceneEditor& editorEscenas;
	const std::unique_ptr<fisica::PhysicsScene>& escenaFisica;
	const glm::vec3& mitadTamanoPuertaDemo;
	const glm::vec3& bisagraPuertaDemo;
	const float& anguloPuertaDemo;
	const fisica::FallingCubeDemo& demoFisica;
	const bool& objetosAnimadosListos;
	const glb::AnimatedScene& objetosAnimados;
	const bool& mostrarColisionadores;
	const fisica::PhysicsWorld& mundoFisico;
	const netgame::NetworkGame& redJuego;

	// Services provided by the application
	void (*iniciarConsultaGpu)();
	void (*actualizarOclusoresPhong)();
	bool (*modoJuegoFPS)();
	bool (*matrizMandoIzquierdoMundo)(glm::mat4&);
	glm::mat4 (*matrizLocalMovilActual)();
	void (*actualizarLinternaMovil)(const glm::mat4&);
	GLuint (*texturaDeMaterial)(const SceneMaterial&);
	void (*dibujarRayoMandoVR)();
	void (*finalizarConsultaGpu)();
};

void RenderLegacyVgiScene(const SceneRenderContext& ctx);
