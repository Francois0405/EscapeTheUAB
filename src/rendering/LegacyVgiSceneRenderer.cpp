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
	const bool renderizarMovil = ctx.fpsModeActive() && !ctx.sceneEditor.Activo()
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
	glUniform1f(glcache::UniformLocation(ctx.shader_programID, "uDirtStrength"), 0.0f);
	glUniform1f(glcache::UniformLocation(ctx.shader_programID, "uTextureEmission"), 0.0f);
	glUniform1i(glcache::UniformLocation(ctx.shader_programID, "uMobileSurface"), GL_FALSE);

	// Dibuix SkyBox Cúbic.
	// El Skybox se muestra en PLAY, pero no en EDIT.
	if (ctx.SkyBoxCube && !ctx.sceneEditor.Activo())
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

	// Dibuja cada objeto de la habitacion con su transformacion.
	if (!ctx.sceneObjects.empty())
	{
		for (std::size_t i = 0; i < ctx.sceneObjects.size(); ++i)
		{
			const SceneObject& obj = ctx.sceneObjects[i];
			if (!ctx.sceneEditor.ObjetoVisible(i)) continue;   // visibilidad TEMPORAL de la vista de edicion
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
				if (GenerarSeccionesPared(glm::abs(obj.escala), obj.pared.aperturas, secciones, errorPared))
				{
					const glm::mat4 giro = glm::rotate(glm::mat4(1.0f), glm::radians(obj.rotacion.z), glm::vec3(0, 0, 1));
					for (const auto& seccion : secciones)
					{
						glm::mat4 m = glm::translate(glm::mat4(1.0f), obj.posicion + glm::vec3(giro * glm::vec4(seccion.centroLocal, 0.0f)));
						m = glm::rotate(m, glm::radians(obj.rotacion.z), glm::vec3(0, 0, 1));
						matrices.push_back(glm::scale(m, seccion.mitad));
					}
				}
				else std::cerr << "Pared paramétrica '" << obj.id << "' inválida: " << errorPared << '\n';
			}
			if (matrices.empty()) matrices.push_back(ctx.sceneEditor.Activo()
				? fisica::MatrizDeObjeto(obj) : ctx.physicsScene->MatrizVisualDe(static_cast<int>(i), obj));
			if (ctx.optimizedRendering)
			{
				Vector3D minimo{}, maximo{};
				if (it->second->GetLocalBounds(minimo, maximo))
				{
					const glm::vec3 lo(minimo.fX, minimo.fY, minimo.fZ), hi(maximo.fX, maximo.fY, maximo.fZ);
					const glm::mat4 vp = ctx.ProjectionMatrix * ctx.ViewMatrix;
					matrices.erase(std::remove_if(matrices.begin(), matrices.end(), [&](const glm::mat4& m)
					{
						const bool fuera = CajaFueraDeFrustum(vp, m, lo, hi);
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

			for (const glm::mat4& matriz : matrices)
				dibuixa_EscenaGL(
					ctx.shader_programID, ctx.eixos, ctx.eixos_Id, ctx.grid, ctx.hgrid,
					OBJOBJ, ctx.col_obj, ctx.sw_material,
					true, ctx.texturesID, ctx.textura_map, ctx.tFlag_invert_Y,
					ctx.npts_T, ctx.PC_t, ctx.pas_CS, ctx.sw_Punts_Control, ctx.dibuixa_TriedreFrenet,
					it->second.get(), matriz
				);
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
			animation::MatrizPuertaConBisagra(ctx.demoDoorHalfSize, ctx.demoDoorHinge, ctx.demoDoorAngle)
		);
		break;
	}

	// DEMO FÍSICA: dibuja el cubo con la pose que calcula Bullet (reutiliza test.obj, de ±1 m).
	if (ctx.fallingCubeDemo.EstaActiva())
	{
		for (const auto& modelo : ctx.roomModels)
		{
			if (!modelo.first.ends_with("test.obj")) continue;
			const fisica::BodyPose pose = ctx.fallingCubeDemo.PoseCubo();
			// Matriz = traslación * rotación * escala (la escala es la mitad del tamaño, porque el modelo mide ±1).
			const glm::mat4 matrizCubo = glm::translate(glm::mat4(1.0f), pose.posicion)
				* glm::mat4_cast(pose.rotacion)
				* glm::scale(glm::mat4(1.0f), ctx.fallingCubeDemo.MitadTamanoCubo());
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
	if (ctx.animatedObjectsReady && ctx.sceneEditor.MostrarAnimados())
	{
		glUseProgram(ctx.shader_programID);
		glb::DibujarObjetosAnimados(ctx.animatedObjects, ctx.shader_programID,
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
	if (ctx.sceneEditor.Activo() && ctx.sceneEditor.MostrarColisionadores())
	{
		std::vector<fisica::CollisionBox> cajasEditor;
		for (std::size_t indiceObjeto = 0; indiceObjeto < ctx.sceneObjects.size(); ++indiceObjeto)
		{
			const SceneObject& objeto = ctx.sceneObjects[indiceObjeto];
			if (!objeto.fisica.presente || !ctx.sceneEditor.ObjetoVisible(indiceObjeto)) continue;
			glm::mat4 giro(1.0f);
			giro = glm::rotate(giro, glm::radians(objeto.rotacion.z), glm::vec3(0, 0, 1));
			giro = glm::rotate(giro, glm::radians(objeto.rotacion.y), glm::vec3(0, 1, 0));
			giro = glm::rotate(giro, glm::radians(objeto.rotacion.x), glm::vec3(1, 0, 0));
			fisica::CollisionBox caja;
			caja.tipo = objeto.fisica.tipo;
			caja.pose.posicion = objeto.posicion + glm::vec3(giro * glm::vec4(objeto.escala * objeto.fisica.desplazamiento, 0.0f));
			caja.pose.rotacion = glm::quat_cast(glm::mat3(giro));
			caja.mitadTamano = glm::abs(objeto.escala) * objeto.fisica.mitadTamano;
			cajasEditor.push_back(caja);
		}
		fisica::DibujarColisionadores(ctx.eixos_programID, ctx.ProjectionMatrix, ctx.ViewMatrix, cajasEditor);
	}
	else if (ctx.showColliders && !ctx.sceneEditor.Activo())
		fisica::DibujarColisionadores(ctx.eixos_programID, ctx.ProjectionMatrix, ctx.ViewMatrix, ctx.physicsWorld.CajasDeColision());

	// Multijugador: jugadores remotos (el local NUNCA se dibuja) y objeto compartido de prueba.
	if (ctx.networkGame.Activa() && !ctx.sceneEditor.Activo())
	{
		netgame::DibujarAvataresRemotos(ctx.eixos_programID, ctx.ProjectionMatrix, ctx.ViewMatrix,
			ctx.networkGame.RemotosParaDibujar(glfwGetTime()),
			ctx.networkGame.Objeto().conocido, ctx.networkGame.PosicionVisualObjeto(), ctx.networkGame.SoyDuenoDelObjeto(),
			ctx.networkGame.Objeto().dueno != netgame::kSinDueno);
	}

	// Dibujar el rayo láser del mando derecho en VR.
	ctx.drawVrControllerRay();
	ctx.endGpuQuery();
	if (renderMetrics::enabled)
		renderMetrics::escenaMs += std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - inicioEscena).count();
}
