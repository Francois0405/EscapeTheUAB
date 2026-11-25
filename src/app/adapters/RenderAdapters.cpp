#include "src/app/AppState.h"
#include "src/app/adapters/RenderAdapters.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/app/input/CursorCapture.h"
#include "src/legacy_vgi/VgiStatusBar.h"
#include "src/rendering/GpuFrameTimer.h"
#include "src/rendering/HandheldDevices.h"
#include "src/rendering/MaterialTextures.h"
#include "src/rendering/SceneLighting.h"

// Viewport i dibuix del fotograma: implementacions a src/rendering/RuntimeRenderer.cpp.
RuntimeRenderer& renderizadorRuntime()
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
		.modoEstereoPrueba = modoEstereoPrueba,
		.shader_programID = shader_programID,
		.eixos_programID = eixos_programID,
		.eixos_Id = eixos_Id,
		.shaderEixos = shaderEixos,
		.shaderLighting = shaderLighting,
		.editorEscenas = editorEscenas,
		.gestorOpenXR = gestorOpenXR,
		.ultimaCabezaVR = ultimaCabezaVR,
		.hayCabezaVR = hayCabezaVR,
		.configura_Escena = [] { configura_Escena(); },
		.dibuixa_Escena = [] { dibuixa_Escena(); },
		.Barra_Estat = [] { Barra_Estat(); },
		.desplazamientoVisualDeCamara = [] { return desplazamientoVisualDeCamara(); },
		.piesJugador = [] { return mundoFisico.HayJugador() ? mundoFisico.PosicionPiesJugador() : posicionPiesDesdeCamara(); },
		.alturaOjosJugador = [] { return mundoFisico.AlturaOjosJugador(); }
	};
	return renderizador;
}

void OnSize(GLFWwindow* window, int width, int height) { renderizadorRuntime().OnSize(window, width, height); }

void OnFramebufferSize(GLFWwindow* window, int width, int height) { renderizadorRuntime().OnFramebufferSize(window, width, height); }

bool dibujarEscenaVR() { return renderizadorRuntime().dibujarEscenaVR(); }

void OnPaint(GLFWwindow* window) { renderizadorRuntime().OnPaint(window); }

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
		.renderizadoOptimizado = renderizadoOptimizado,
		.intensidadIndirectaEscena = intensidadIndirectaEscena,
		.intensidadAmbientalBaseEscena = intensidadAmbientalBaseEscena,
		.escalaLuzDirectaEscena = escalaLuzDirectaEscena,
		.modoDiagnosticoIluminacion = modoDiagnosticoIluminacion,
		.colorReboteSueloEscena = colorReboteSueloEscena,
		.colorReboteCieloEscena = colorReboteCieloEscena,
		.lamparasLaboratorioActivas = lamparasLaboratorioActivas,
		.llumGL = llumGL,
		.mostrarMovil = mostrarMovil,
		.phoneOBJ = phoneOBJ,
		.movilEscala = movilEscala,
		.objetosEscena = objetosEscena,
		.modelosHabitacion = modelosHabitacion,
		.editorEscenas = editorEscenas,
		.escenaFisica = escenaFisica,
		.mitadTamanoPuertaDemo = mitadTamanoPuertaDemo,
		.bisagraPuertaDemo = bisagraPuertaDemo,
		.anguloPuertaDemo = anguloPuertaDemo,
		.demoFisica = demoFisica,
		.objetosAnimadosListos = objetosAnimadosListos,
		.objetosAnimados = objetosAnimados,
		.mostrarColisionadores = mostrarColisionadores,
		.mundoFisico = mundoFisico,
		.redJuego = redJuego,
		.iniciarConsultaGpu = iniciarConsultaGpu,
		.actualizarOclusoresPhong = actualizarOclusoresPhong,
		.modoJuegoFPS = modoJuegoFPS,
		.matrizMandoIzquierdoMundo = matrizMandoIzquierdoMundo,
		.matrizLocalMovilActual = matrizLocalMovilActual,
		.actualizarLinternaMovil = actualizarLinternaMovil,
		.texturaDeMaterial = texturaDeMaterial,
		.dibujarRayoMandoVR = dibujarRayoMandoVR,
		.finalizarConsultaGpu = finalizarConsultaGpu,
	});
}
