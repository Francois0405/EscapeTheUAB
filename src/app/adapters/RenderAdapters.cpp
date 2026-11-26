#include "src/app/AppIncludes.h"
#include "src/app/state/SceneState.h"
#include "src/app/state/RenderState.h"
#include "src/app/state/NetworkState.h"
#include "src/app/state/VrState.h"
#include "src/app/adapters/RenderAdapters.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/app/input/CursorCapture.h"
#include "src/legacy_vgi/VgiStatusBar.h"
#include "src/app/render/GpuFrameTimer.h"
#include "src/app/render/HandheldDevices.h"
#include "src/app/render/MaterialTextures.h"
#include "src/app/render/SceneLighting.h"

// State used only by this file.
namespace
{
	const glm::vec3 demoDoorHalfSize(0.05f, 0.5f, 1.0f);   // puerta de 1 m de ancho, 10 cm de grosor y 2 m de alto
	const glm::vec3 demoDoorHinge(6.0f, -0.5f, -1.7f);      // punto del mundo (a ras de suelo)
}

// Viewport i dibuix del fotograma: implementacions a src/rendering/RuntimeRenderer.cpp.
RuntimeRenderer& runtimeRenderer()
{
	static RuntimeRenderer renderizador{
		.w = w,
		.h = h,
		.width_old = width_old,
		.height_old = height_old,
		.projeccio = projeccio,
		.camera = camera,
		.Vis_Polar = Vis_Polar,
		.OPV = OPV,
		.OPV_G = OPV_G,
		.opvN = opvN,
		.n = n,
		.pan = pan,
		.tr_cpv = tr_cpv,
		.tr_cpvF = tr_cpvF,
		.ViewMatrix = ViewMatrix,
		.ProjectionMatrix = ProjectionMatrix,
		.c_fons = c_fons,
		.col_obj = col_obj,
		.objecte = objecte,
		.mida = mida,
		.pas = pas,
		.front_faces = front_faces,
		.oculta = oculta,
		.test_vis = test_vis,
		.back_line = back_line,
		.ilumina = ilumina,
		.llum_ambient = llum_ambient,
		.llumGL = llumGL,
		.ifixe = ifixe,
		.ilum2sides = ilum2sides,
		.eixos = eixos,
		.grid = grid,
		.hgrid = hgrid,
		.statusB = statusB,
		.stereoTestMode = stereoTestMode,
		.shader_programID = shader_programID,
		.eixos_programID = eixos_programID,
		.eixos_Id = eixos_Id,
		.shaderEixos = shaderEixos,
		.shaderLighting = shaderLighting,
		.sceneEditor = sceneEditor,
		.openXrManager = openXrManager,
		.lastVrHead = lastVrHead,
		.hasVrHead = hasVrHead,
		.configura_Escena = [] { configura_Escena(); },
		.dibuixa_Escena = [] { dibuixa_Escena(); },
		.Barra_Estat = [] { Barra_Estat(); },
		.cameraVisualOffset = [] { return cameraVisualOffset(); },
		.piesJugador = [] { return physicsWorld.HasPlayerBody() ? physicsWorld.PlayerFeetPosition() : feetPositionFromCamera(); },
		.alturaOjosJugador = [] { return physicsWorld.PlayerEyeHeight(); }
	};
	return renderizador;
}

void OnSize(GLFWwindow* window, int width, int height) { runtimeRenderer().OnSize(window, width, height); }

void OnFramebufferSize(GLFWwindow* window, int width, int height) { runtimeRenderer().OnFramebufferSize(window, width, height); }

bool drawVrScene() { return runtimeRenderer().drawVrScene(); }

void OnPaint(GLFWwindow* window) { runtimeRenderer().OnPaint(window); }

// configura_Escena: Funcio que configura els parametres de Model i dibuixa les
//                   primitives OpenGL dins classe Model
void configura_Escena() {

// Aplicar Transformacions Geometriques segons persiana Transformacio i Quaternions
	GTMatrix = instancia(transf, TG, TGF);
}

void dibuixa_Escena()
{
	RenderLegacyVgiScene({
		.shader_programID = shader_programID,
		.eixos_programID = eixos_programID,
		.eixos_Id = eixos_Id,
		.ProjectionMatrix = ProjectionMatrix,
		.ViewMatrix = ViewMatrix,
		.SkyBoxCube = SkyBoxCube,
		.skC_programID = skC_programID,
		.cubemapTexture = cubemapTexture,
		.Vis_Polar = Vis_Polar,
		.eixos = eixos,
		.grid = grid,
		.hgrid = hgrid,
		.objecte = objecte,
		.col_obj = col_obj,
		.sw_material = sw_material,
		.textura = textura,
		.texturesID = texturesID,
		.textura_map = textura_map,
		.tFlag_invert_Y = tFlag_invert_Y,
		.npts_T = npts_T,
		.PC_t = PC_t,
		.pas_CS = pas_CS,
		.sw_Punts_Control = sw_Punts_Control,
		.dibuixa_TriedreFrenet = dibuixa_TriedreFrenet,
		.ObOBJ = ObOBJ,
		.GTMatrix = GTMatrix,
		.optimizedRendering = optimizedRendering,
		.sceneIndirectIntensity = sceneIndirectIntensity,
		.sceneBaseAmbientIntensity = sceneBaseAmbientIntensity,
		.sceneDirectLightScale = sceneDirectLightScale,
		.lightingDiagnosticMode = lightingDiagnosticMode,
		.sceneGroundBounceColor = sceneGroundBounceColor,
		.sceneSkyBounceColor = sceneSkyBounceColor,
		.labLampsEnabled = labLampsEnabled,
		.llumGL = llumGL,
		.showHeldViewModel = retroSettings.heldViewModel && !stereoTestMode && !openXrManager.IsSessionRunning(),
		.showPhone = showPhone,
		.phoneOBJ = phoneOBJ,
		.phoneScale = phoneScale,
		.sceneObjects = sceneObjects,
		.roomModels = roomModels,
		.sceneEditor = sceneEditor,
		.physicsScene = physicsScene,
		.demoDoorHalfSize = demoDoorHalfSize,
		.demoDoorHinge = demoDoorHinge,
		.demoDoorAngle = demoDoorAngle,
		.fallingCubeDemo = fallingCubeDemo,
		.animatedObjectsReady = animatedObjectsReady,
		.animatedObjects = animatedObjects,
		.showColliders = showColliders,
		.physicsWorld = physicsWorld,
		.networkGame = networkGame,
		.beginGpuQuery = beginGpuQuery,
		.updatePhongOccluders = updatePhongOccluders,
		.fpsModeActive = fpsModeActive,
		.leftControllerWorldMatrix = leftControllerWorldMatrix,
		.currentPhoneLocalMatrix = currentPhoneLocalMatrix,
		.updatePhoneFlashlight = updatePhoneFlashlight,
		.uploadPhoneFlashlight = uploadPhoneFlashlight,
		.materialTexture = materialTexture,
		.drawVrControllerRay = drawVrControllerRay,
		.endGpuQuery = endGpuQuery,
	});
}
