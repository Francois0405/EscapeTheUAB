#include "LegacyVgiSceneRenderer.h"

#include "GlbDrawing.h"
#include "FrustumCulling.h"
#include "RenderMetrics.h"
#include "UniformCache.h"
#include "src/animation/DoorAnimation.h"
#include "src/editor/SceneEditor.h"
#include "src/networking/replication/NetAvatar.h"
#include "src/networking/replication/NetGame.h"
#include "src/physics/ColliderDrawing.h"
#include "src/physics/PhysicsScene.h"
#include "src/physics/PhysicsWorld.h"
#include "src/scene/SceneLoader.h"
#include "src/scene/escena.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>

namespace
{
	// Indice del objeto que sostiene ESTE jugador (host/offline por Bullet, cliente por el host).
	int LocalHeldObject(const SceneRenderContext& ctx)
	{
		const int local = ctx.physicsScene->GrabbedObject();
		if (local >= 0 || !ctx.networkGame.IsClient()) return local;
		const uint32_t id = ctx.networkGame.DynamicObjectOfPeer(ctx.networkGame.LocalPeerId());
		for (std::size_t i = 0; id != 0 && i < ctx.sceneObjects.size(); ++i)
			if (netgame::HashId(ctx.sceneObjects[i].id) == id) return static_cast<int>(i);
		return -1;
	}

	// Presentacion en primera persona: abajo a la derecha, anclada a la camara y reducida si es grande.
	// Solo cambia el dibujo local; la pose fisica, la red y el lanzamiento no se tocan.
	glm::mat4 HeldViewModelMatrix(const SceneObject& obj, const Vector3D& minimo, const Vector3D& maximo,
		const glm::mat4& view, const glm::mat4& projection)
	{
		// El juego usa Z arriba; en coordenadas de vista arriba es Y. Se conserva el giro de autor.
		glm::mat4 giro = glm::rotate(glm::mat4(1.0f), glm::radians(-25.0f), glm::vec3(0, 1, 0));
		giro = glm::rotate(giro, glm::radians(-90.0f), glm::vec3(1, 0, 0));
		giro = glm::rotate(giro, glm::radians(obj.eulerRotation.z), glm::vec3(0, 0, 1));
		giro = glm::rotate(giro, glm::radians(obj.eulerRotation.y), glm::vec3(0, 1, 0));
		giro = glm::rotate(giro, glm::radians(obj.eulerRotation.x), glm::vec3(1, 0, 0));
		const glm::mat4 orientado = glm::scale(giro, obj.escala);

		glm::vec3 lo(std::numeric_limits<float>::max()), hi(-std::numeric_limits<float>::max());
		for (int i = 0; i < 8; ++i)
		{
			const glm::vec3 esquina((i & 1) ? maximo.fX : minimo.fX, (i & 2) ? maximo.fY : minimo.fY,
				(i & 4) ? maximo.fZ : minimo.fZ);
			const glm::vec3 p = glm::vec3(orientado * glm::vec4(esquina, 1.0f));
			lo = glm::min(lo, p);
			hi = glm::max(hi, p);
		}
		const glm::vec3 tamano = hi - lo;
		const float mayor = std::max({ tamano.x, tamano.y, tamano.z, 1.0e-4f });

		constexpr float profundidad = 0.7f;
		const float medioAncho = profundidad / projection[0][0];
		const float medioAlto = profundidad / projection[1][1];
		const float escala = std::min(1.0f, 0.45f * medioAlto / mayor);   // como mucho ~22 % del alto
		const glm::vec3 destino(0.55f * medioAncho, -0.55f * medioAlto, -profundidad);

		return glm::inverse(view) * glm::translate(glm::mat4(1.0f), destino) *
			glm::scale(glm::mat4(1.0f), glm::vec3(escala)) *
			glm::translate(glm::mat4(1.0f), -0.5f * (lo + hi)) * orientado;
	}
}

void RenderLegacyVgiScene(const SceneRenderContext& ctx)
{
	const auto inicioEscena = std::chrono::steady_clock::now();
	glcache::enabled = ctx.optimizedRendering;
	ctx.beginGpuQuery();
	ctx.updatePhongOccluders();
	glUseProgram(ctx.shader_programID);
	glUniform1f(glcache::UniformLocation(ctx.shader_programID, "uIndirectIntensity"), ctx.sceneIndirectIntensity);
	glUniform1f(glcache::UniformLocation(ctx.shader_programID, "uIndirectAmbientBase"), ctx.sceneBaseAmbientIntensity);
	glUniform1f(glcache::UniformLocation(ctx.shader_programID, "uSceneDirectScale"), ctx.sceneDirectLightScale);
	glUniform1i(glcache::UniformLocation(ctx.shader_programID, "uLightingDebugMode"), ctx.lightingDiagnosticMode);
	glUniform3f(glcache::UniformLocation(ctx.shader_programID, "uIndirectFloorColor"),
		ctx.sceneGroundBounceColor.r, ctx.sceneGroundBounceColor.g, ctx.sceneGroundBounceColor.b);
	glUniform3f(glcache::UniformLocation(ctx.shader_programID, "uIndirectSkyColor"),
		ctx.sceneSkyBounceColor.r, ctx.sceneSkyBounceColor.g, ctx.sceneSkyBounceColor.b);
	glUniform1f(glcache::UniformLocation(ctx.shader_programID, "uLampEmissionScale"), 1.0f);
	const bool renderizarMovil = ctx.fpsModeActive() && !ctx.sceneEditor.IsEnabled()
		&& ctx.showPhone && ctx.phoneOBJ != nullptr;
	glm::mat4 matrizMovilMundo(1.0f);
	if (renderizarMovil)
	{
		glm::mat4 mundoMandoIzq(1.0f);
		if (ctx.leftControllerWorldMatrix(mundoMandoIzq))
		{
			// En VR: anclado al mando izquierdo
			glm::mat4 ajusteLocal(1.0f);
			ajusteLocal = glm::rotate(ajusteLocal, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			ajusteLocal = glm::scale(ajusteLocal, glm::vec3(ctx.phoneScale));
			matrizMovilMundo = mundoMandoIzq * ajusteLocal;
		}
		else
		{
			// En PC: anclado a la cámara
			matrizMovilMundo = glm::inverse(ctx.ViewMatrix) * ctx.currentPhoneLocalMatrix();
		}

		ctx.updatePhoneFlashlight(matrizMovilMundo);
	}
	else ctx.llumGL[3].encesa = false;
	ctx.uploadPhoneFlashlight();
	glUniform1f(glcache::UniformLocation(ctx.shader_programID, "uDirtStrength"), 0.0f);
	glUniform1f(glcache::UniformLocation(ctx.shader_programID, "uTextureEmission"), 0.0f);
	glUniform1i(glcache::UniformLocation(ctx.shader_programID, "uMobileSurface"), GL_FALSE);

	// Dibuix SkyBox Cúbic.
	// El Skybox se muestra en PLAY, pero no en EDIT.
	if (ctx.SkyBoxCube && !ctx.sceneEditor.IsEnabled())
	{
		dibuixa_Skybox(
			ctx.skC_programID,
			ctx.cubemapTexture,
			ctx.Vis_Polar,
			ctx.ProjectionMatrix,
			ctx.ViewMatrix
		);
	}
	// Dibuix Coordenades Món i Reixes.
	dibuixa_Eixos(ctx.eixos_programID, ctx.eixos, ctx.eixos_Id, ctx.grid, ctx.hgrid, ctx.ProjectionMatrix, ctx.ViewMatrix);

	const int sostenido = ctx.showHeldViewModel && ctx.fpsModeActive() && !ctx.sceneEditor.IsEnabled()
		? LocalHeldObject(ctx) : -1;

	// Dibuja cada objeto de la habitacion con su transformacion.
	if (!ctx.sceneObjects.empty())
	{
		for (std::size_t i = 0; i < ctx.sceneObjects.size(); ++i)
		{
			const SceneObject& obj = ctx.sceneObjects[i];
			if (!ctx.sceneEditor.IsObjectVisible(i)) continue;   // visibilidad TEMPORAL de la vista de edicion
			auto it = ctx.roomModels.find(obj.modelo);

			if (it == ctx.roomModels.end())
				continue;

			// Un muro paramétrico se dibuja como paños generados al vuelo. El OBJ compartido
			// sigue intacto y el material/UV de mundo se comparte entre todas las secciones.
			std::vector<glm::mat4> matrices;
			if (obj.pared.presente && !obj.pared.aperturas.empty())
			{
				std::vector<SceneWallSection> secciones;
				std::string errorPared;
				if (GenerateWallSections(glm::abs(obj.escala), obj.pared.aperturas, secciones, errorPared))
				{
					const glm::mat4 giro = glm::rotate(glm::mat4(1.0f), glm::radians(obj.eulerRotation.z), glm::vec3(0, 0, 1));
					for (const auto& seccion : secciones)
					{
						glm::mat4 m = glm::translate(glm::mat4(1.0f), obj.position + glm::vec3(giro * glm::vec4(seccion.centroLocal, 0.0f)));
						m = glm::rotate(m, glm::radians(obj.eulerRotation.z), glm::vec3(0, 0, 1));
						matrices.push_back(glm::scale(m, seccion.mitad));
					}
				}
				else std::cerr << "Pared paramétrica '" << obj.id << "' inválida: " << errorPared << '\n';
			}
			if (matrices.empty()) matrices.push_back(ctx.sceneEditor.IsEnabled()
				? physics::ObjectMatrix(obj) : ctx.physicsScene->VisualMatrixOf(static_cast<int>(i), obj));
			bool enMano = false;
			if (static_cast<int>(i) == sostenido)
			{
				Vector3D minimo{}, maximo{};
				if (it->second->GetLocalBounds(minimo, maximo))
				{
					matrices.assign(1, HeldViewModelMatrix(obj, minimo, maximo, ctx.ViewMatrix, ctx.ProjectionMatrix));
					enMano = true;
				}
			}
			if (ctx.optimizedRendering && !enMano)
			{
				Vector3D minimo{}, maximo{};
				if (it->second->GetLocalBounds(minimo, maximo))
				{
					const glm::vec3 lo(minimo.fX, minimo.fY, minimo.fZ), hi(maximo.fX, maximo.fY, maximo.fZ);
					const glm::mat4 vp = ctx.ProjectionMatrix * ctx.ViewMatrix;
					matrices.erase(std::remove_if(matrices.begin(), matrices.end(), [&](const glm::mat4& m)
					{
						const bool fuera = BoxOutsideFrustum(vp, m, lo, hi);
						if (renderMetrics::enabled) { if (fuera) ++renderMetrics::descartados; else ++renderMetrics::visibles; }
						return fuera;
					}), matrices.end());
				}
				else if (renderMetrics::enabled) ++renderMetrics::visibles;
			}
			else if (renderMetrics::enabled) ++renderMetrics::visibles;
			if (matrices.empty()) continue;
			const GLuint texturaOverride = ctx.materialTexture(obj.material);
			const float rugosidad = std::clamp(obj.material.roughness, 0.0f, 1.0f);
			const float brilloPhong = 4.0f + std::pow(1.0f - rugosidad, 4.0f) * 124.0f;
			it->second->SetInstanceMaterialOverride(texturaOverride, obj.material.presente ? brilloPhong : -1.0f);
			glUseProgram(ctx.shader_programID);
			glUniform1i(glcache::UniformLocation(ctx.shader_programID, "uWorldTiling"),
				obj.material.presente ? GL_TRUE : GL_FALSE);
			glUniform2f(glcache::UniformLocation(ctx.shader_programID, "uWorldTileSize"),
				obj.material.tileSize.x, obj.material.tileSize.y);
			glUniform1f(glcache::UniformLocation(ctx.shader_programID, "uDirtStrength"), obj.material.dirtStrength);
			glUniform1f(glcache::UniformLocation(ctx.shader_programID, "uDirtScale"), obj.material.dirtScale);
			glUniform1f(glcache::UniformLocation(ctx.shader_programID, "uDirtSeed"), obj.material.dirtSeed);
			const float emisionLuminaria = obj.luz.presente
				? (obj.luz.encendida && ctx.labLampsEnabled ? 1.0f : 0.0f) : 1.0f;
			glUniform1f(glcache::UniformLocation(ctx.shader_programID, "uLampEmissionScale"), emisionLuminaria);

			if (enMano) glDepthRange(0.0, 0.05);   // delante de las paredes cercanas, sin perder auto-oclusion
			for (const glm::mat4& matriz : matrices)
				dibuixa_EscenaGL(
					ctx.shader_programID, ctx.eixos, ctx.eixos_Id, ctx.grid, ctx.hgrid,
					OBJOBJ, ctx.col_obj, ctx.sw_material,
					true, ctx.texturesID, ctx.textura_map, ctx.tFlag_invert_Y,
					ctx.npts_T, ctx.PC_t, ctx.pas_CS, ctx.sw_Punts_Control, ctx.dibuixa_TriedreFrenet,
					it->second.get(), matriz
				);
			if (enMano) glDepthRange(0.0, 1.0);
			it->second->SetInstanceMaterialOverride(0, -1.0f);
			glUniform1f(glcache::UniformLocation(ctx.shader_programID, "uLampEmissionScale"), 1.0f);
			glUniform1i(glcache::UniformLocation(ctx.shader_programID, "uWorldTiling"), GL_FALSE);
			glUniform1f(glcache::UniformLocation(ctx.shader_programID, "uDirtStrength"), 0.0f);
		}
	}
	else
	{
		// Dibujar el objeto seleccionado cuando no hay una habitación cargada.
		dibuixa_EscenaGL(ctx.shader_programID, ctx.eixos, ctx.eixos_Id, ctx.grid, ctx.hgrid,
			ctx.objecte, ctx.col_obj, ctx.sw_material,
			ctx.textura, ctx.texturesID, ctx.textura_map, ctx.tFlag_invert_Y,
			ctx.npts_T, ctx.PC_t, ctx.pas_CS, ctx.sw_Punts_Control, ctx.dibuixa_TriedreFrenet,
			ctx.ObOBJ, ctx.GTMatrix);
	}

	// DEMO FASE 1: la puerta usa el cubo test.obj ya cargado por la escena.
	for (const auto& modelo : ctx.roomModels)
	{
		if (!modelo.first.ends_with("test.obj")) continue;
		dibuixa_EscenaGL(
			ctx.shader_programID, ctx.eixos, ctx.eixos_Id, ctx.grid, ctx.hgrid,
			OBJOBJ, ctx.col_obj, ctx.sw_material,
			false, ctx.texturesID, ctx.textura_map, ctx.tFlag_invert_Y,
			ctx.npts_T, ctx.PC_t, ctx.pas_CS, ctx.sw_Punts_Control, ctx.dibuixa_TriedreFrenet,
			modelo.second.get(),
			animation::HingedDoorMatrix(ctx.demoDoorHalfSize, ctx.demoDoorHinge, ctx.demoDoorAngle)
		);
		break;
	}

	// DEMO FÍSICA: dibuja el cubo con la pose que calcula Bullet (reutiliza test.obj, de ±1 m).
	if (ctx.fallingCubeDemo.IsRunningNow())
	{
		for (const auto& modelo : ctx.roomModels)
		{
			if (!modelo.first.ends_with("test.obj")) continue;
			const physics::BodyPose pose = ctx.fallingCubeDemo.CubePose();
			// Matriz = traslación * rotación * escala (la escala es la mitad del tamaño, porque el modelo mide ±1).
			const glm::mat4 matrizCubo = glm::translate(glm::mat4(1.0f), pose.position)
				* glm::mat4_cast(pose.eulerRotation)
				* glm::scale(glm::mat4(1.0f), ctx.fallingCubeDemo.CubeHalfSize());
			dibuixa_EscenaGL(
				ctx.shader_programID, ctx.eixos, ctx.eixos_Id, ctx.grid, ctx.hgrid,
				OBJOBJ, ctx.col_obj, ctx.sw_material,
				false, ctx.texturesID, ctx.textura_map, ctx.tFlag_invert_Y,
				ctx.npts_T, ctx.PC_t, ctx.pas_CS, ctx.sw_Punts_Control, ctx.dibuixa_TriedreFrenet,
				modelo.second.get(), matrizCubo
			);
			break;
		}
	}

	// Objetos animados GLB (puertas...): mismo shader y cámara que el resto de la escena.
	if (ctx.animatedObjectsReady && ctx.sceneEditor.ShowAnimated())
	{
		glUseProgram(ctx.shader_programID);
		glb::DrawAnimatedObjects(ctx.animatedObjects, ctx.shader_programID,
			ctx.ProjectionMatrix * ctx.ViewMatrix, ctx.optimizedRendering);
	}

	if (renderizarMovil)
	{
		const bool texturaAnterior = ctx.textura;
		ctx.textura = true;
		glUniform1i(glcache::UniformLocation(ctx.shader_programID, "uMobileSurface"), GL_TRUE);
		glUniform1f(glcache::UniformLocation(ctx.shader_programID, "uTextureEmission"), 0.045f);
		dibuixa_EscenaGL(ctx.shader_programID, ctx.eixos, ctx.eixos_Id, ctx.grid, ctx.hgrid,
			OBJOBJ, ctx.col_obj, ctx.sw_material,
			ctx.textura, ctx.texturesID, ctx.textura_map, ctx.tFlag_invert_Y,
			ctx.npts_T, ctx.PC_t, ctx.pas_CS, ctx.sw_Punts_Control, ctx.dibuixa_TriedreFrenet,
			ctx.phoneOBJ, matrizMovilMundo);
		glUniform1f(glcache::UniformLocation(ctx.shader_programID, "uTextureEmission"), 0.0f);
		glUniform1i(glcache::UniformLocation(ctx.shader_programID, "uMobileSurface"), GL_FALSE);
		ctx.textura = texturaAnterior;
	}

	// En EDIT se visualizan las cajas derivadas de la transformación actual; en PLAY las de Bullet.
	if (ctx.sceneEditor.IsEnabled() && ctx.sceneEditor.ShowCollidersEnabled())
	{
		std::vector<physics::CollisionBox> cajasEditor;
		for (std::size_t objectIndex = 0; objectIndex < ctx.sceneObjects.size(); ++objectIndex)
		{
			const SceneObject& objeto = ctx.sceneObjects[objectIndex];
			if (!objeto.fisica.presente || !ctx.sceneEditor.IsObjectVisible(objectIndex)) continue;
			glm::mat4 giro(1.0f);
			giro = glm::rotate(giro, glm::radians(objeto.eulerRotation.z), glm::vec3(0, 0, 1));
			giro = glm::rotate(giro, glm::radians(objeto.eulerRotation.y), glm::vec3(0, 1, 0));
			giro = glm::rotate(giro, glm::radians(objeto.eulerRotation.x), glm::vec3(1, 0, 0));
			physics::CollisionBox caja;
			caja.tipo = objeto.fisica.tipo;
			caja.pose.position = objeto.position + glm::vec3(giro * glm::vec4(objeto.escala * objeto.fisica.desplazamiento, 0.0f));
			caja.pose.eulerRotation = glm::quat_cast(glm::mat3(giro));
			caja.halfExtents = glm::abs(objeto.escala) * objeto.fisica.halfExtents;
			cajasEditor.push_back(caja);
		}
		physics::DrawColliders(ctx.eixos_programID, ctx.ProjectionMatrix, ctx.ViewMatrix, cajasEditor);
	}
	else if (ctx.showColliders && !ctx.sceneEditor.IsEnabled())
		physics::DrawColliders(ctx.eixos_programID, ctx.ProjectionMatrix, ctx.ViewMatrix, ctx.physicsWorld.CollisionBoxes());

	// Multijugador: jugadores remotos (el local NUNCA se dibuja) y objeto compartido de prueba.
	if (ctx.networkGame.IsActive() && !ctx.sceneEditor.IsEnabled())
	{
		netgame::DrawRemoteAvatars(ctx.eixos_programID, ctx.ProjectionMatrix, ctx.ViewMatrix,
			ctx.networkGame.RemotePlayersToDraw(glfwGetTime()),
			ctx.networkGame.Objeto().conocido, ctx.networkGame.ObjectVisualPosition(), ctx.networkGame.OwnsObject(),
			ctx.networkGame.Objeto().dueno != netgame::kSinDueno);
	}

	// Dibujar el rayo láser del mando derecho en VR.
	ctx.drawVrControllerRay();
	ctx.endGpuQuery();
	if (renderMetrics::enabled)
		renderMetrics::escenaMs += std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - inicioEscena).count();
}
