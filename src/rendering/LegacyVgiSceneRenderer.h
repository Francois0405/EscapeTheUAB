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
namespace physics { class FallingCubeDemo; class PhysicsScene; class PhysicsWorld; }
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
	const bool& optimizedRendering;
	const float& sceneIndirectIntensity;
	const float& sceneBaseAmbientIntensity;
	const float& sceneDirectLightScale;
	const int& lightingDiagnosticMode;
	const glm::vec3& sceneGroundBounceColor;
	const glm::vec3& sceneSkyBounceColor;
	const bool& labLampsEnabled;
	LLUM (&llumGL)[NUM_MAX_LLUMS];
	bool showHeldViewModel;
	const bool& showPhone;
	COBJModel* const& phoneOBJ;
	const float& phoneScale;

	// Scene and live runtime state
	const std::vector<SceneObject>& sceneObjects;
	const std::unordered_map<std::string, std::unique_ptr<COBJModel>>& roomModels;
	const editor::SceneEditor& sceneEditor;
	const std::unique_ptr<physics::PhysicsScene>& physicsScene;
	const glm::vec3& demoDoorHalfSize;
	const glm::vec3& demoDoorHinge;
	const float& demoDoorAngle;
	const physics::FallingCubeDemo& fallingCubeDemo;
	const bool& animatedObjectsReady;
	const glb::AnimatedScene& animatedObjects;
	const bool& showColliders;
	const physics::PhysicsWorld& physicsWorld;
	const netgame::NetworkGame& networkGame;

	// Services provided by the application
	void (*beginGpuQuery)();
	void (*updatePhongOccluders)();
	bool (*fpsModeActive)();
	bool (*leftControllerWorldMatrix)(glm::mat4&);
	glm::mat4 (*currentPhoneLocalMatrix)();
	void (*updatePhoneFlashlight)(const glm::mat4&);
	void (*uploadPhoneFlashlight)();
	GLuint (*materialTexture)(const SceneMaterial&);
	void (*drawVrControllerRay)();
	void (*endGpuQuery)();
};

void RenderLegacyVgiScene(const SceneRenderContext& ctx);
