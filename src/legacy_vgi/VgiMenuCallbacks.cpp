#include "src/app/AppState.h"
#include "src/legacy_vgi/VgiMenuCallbacks.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/legacy_vgi/VgiFileActions.h"
#include "src/legacy_vgi/VgiMaterial.h"

LegacyVgiViewControls& controlesVistaVgi()
{
	static LegacyVgiViewControls controles{
		.camera = camera,
		.OPV = OPV,
		.OPV_G = OPV_G,
		.Vis_Polar = Vis_Polar,
		.mobil = mobil,
		.zzoom = zzoom,
		.zzoomO = zzoomO,
		.satelit = satelit,
		.m_EsfeIncEAvall = m_EsfeIncEAvall,
		.anima = anima,
		.opvN = opvN,
		.n = n,
		.angleZ = angleZ,
		.spawnEscenaActiva = spawnEscenaActiva,
		.crearJugadorFisico = crearJugadorFisico,
		.fullscreen = fullscreen,
		.pan = pan,
		.fact_pan = fact_pan,
		.tr_cpv = tr_cpv,
		.tr_cpvF = tr_cpvF,
		.transX = transX,
		.transY = transY,
		.transZ = transZ,
		.eixos = eixos,
		.llumGL = llumGL,
		.shader_SkyBoxC = shader_SkyBoxC,
		.skC_programID = skC_programID,
		.skC_VAOID = skC_VAOID,
		.cubemapTexture = cubemapTexture,
		.projeccio = projeccio,
	};
	return controles;
}

void OnCameraEsferica() { controlesVistaVgi().OnCameraEsferica(); }

void OnCameraOrigenEsferica() { controlesVistaVgi().OnCameraOrigenEsferica(); }

void OnVistaMobil() { controlesVistaVgi().OnVistaMobil(); }

void OnVistaZoom() { controlesVistaVgi().OnVistaZoom(); }

void OnVistaZoomOrto() { controlesVistaVgi().OnVistaZoomOrto(); }

void OnVistaSatelit() { controlesVistaVgi().OnVistaSatelit(); }

void OnVistaPolarsX() { controlesVistaVgi().OnVistaPolarsX(); }

void OnVistaPolarsY() { controlesVistaVgi().OnVistaPolarsY(); }

void OnVistaPolarsZ() { controlesVistaVgi().OnVistaPolarsZ(); }

void OnCameraNavega() { controlesVistaVgi().OnCameraNavega(); }

void OnCameraOrigenNavega() { controlesVistaVgi().OnCameraOrigenNavega(); }

void OnCameraGeode() { controlesVistaVgi().OnCameraGeode(); }

void OnCameraOrigenGeode() { controlesVistaVgi().OnCameraOrigenGeode(); }

void OnVistaFullscreen() { controlesVistaVgi().OnVistaFullscreen(); }

void OnVistaPan() { controlesVistaVgi().OnVistaPan(); }

void OnVistaOrigenPan() { controlesVistaVgi().OnVistaOrigenPan(); }

void OnVistaEixos() { controlesVistaVgi().OnVistaEixos(); }

void OnVistaSkyBox() { controlesVistaVgi().OnVistaSkyBox(); }

void OnProjeccioPerspectiva() { controlesVistaVgi().OnProjeccioPerspectiva(); }

void OnProjeccioOrtografica() { controlesVistaVgi().OnProjeccioOrtografica(); }

void OnProjeccioAxonometrica() { controlesVistaVgi().OnProjeccioAxonometrica(); }

LegacyVgiGeometryControls& controlesGeometriaVgi()
{
	static LegacyVgiGeometryControls controles{
		.objecte = objecte,
		.col_obj = col_obj,
		.texturesID = texturesID,
		.ObOBJ = ObOBJ,
		.sw_material = sw_material,
		.npts_T = npts_T,
		.PC_t = PC_t,
		.pas_CS = pas_CS,
		.sw_Punts_Control = sw_Punts_Control,
		.dibuixa_TriedreFrenet = dibuixa_TriedreFrenet,
		.dibuixa_TriedreDarboux = dibuixa_TriedreDarboux,
		.llegir_ptsC = llegir_ptsC,
		.transf = transf,
		.trasl = trasl,
		.rota = rota,
		.escal = escal,
		.fact_Tras = fact_Tras,
		.fact_Rota = fact_Rota,
		.TG = TG,
		.transX = transX,
		.transY = transY,
		.transZ = transZ,
		.mobil = mobil,
		.zzoom = zzoom,
		.pan = pan,
		.front_faces = front_faces,
		.test_vis = test_vis,
		.oculta = oculta,
	};
	return controles;
}

void OnObjecteCap() { controlesGeometriaVgi().OnObjecteCap(); }

void OnObjecteCub() { controlesGeometriaVgi().OnObjecteCub(); }

void OnObjecteCubRGB() { controlesGeometriaVgi().OnObjecteCubRGB(); }

void OnObjecteEsfera() { controlesGeometriaVgi().OnObjecteEsfera(); }

void OnObjecteTetera() { controlesGeometriaVgi().OnObjecteTetera(); }

void OnObjecteArc() { controlesGeometriaVgi().OnObjecteArc(); }

void OnObjecteTie() { controlesGeometriaVgi().OnObjecteTie(); }

void OnObjecteCorbaBezier() { controlesGeometriaVgi().OnObjecteCorbaBezier(); }

void OnObjecteCorbaLemniscata() { controlesGeometriaVgi().OnObjecteCorbaLemniscata(); }

void OnObjecteCorbaHermitte() { controlesGeometriaVgi().OnObjecteCorbaHermitte(); }

void OnObjecteCorbaCatmullRom() { controlesGeometriaVgi().OnObjecteCorbaCatmullRom(); }

void OnObjecteCorbaBSpline() { controlesGeometriaVgi().OnObjecteCorbaBSpline(); }

void OnObjectePuntsControl() { controlesGeometriaVgi().OnObjectePuntsControl(); }

void OnCorbesTriedreFrenet() { controlesGeometriaVgi().OnCorbesTriedreFrenet(); }

void OnCorbesTriedreDarboux() { controlesGeometriaVgi().OnCorbesTriedreDarboux(); }

void OnObjecteMatriuPrimitives() { controlesGeometriaVgi().OnObjecteMatriuPrimitives(); }

void OnObjecteMatriuPrimitivesVAO() { controlesGeometriaVgi().OnObjecteMatriuPrimitivesVAO(); }

void OnTransformaTraslacio() { controlesGeometriaVgi().OnTransformaTraslacio(); }

void OnTransformaOrigenTraslacio() { controlesGeometriaVgi().OnTransformaOrigenTraslacio(); }

void OnTransformaRotacio() { controlesGeometriaVgi().OnTransformaRotacio(); }

void OnTransformaOrigenRotacio() { controlesGeometriaVgi().OnTransformaOrigenRotacio(); }

void OnTransformaEscalat() { controlesGeometriaVgi().OnTransformaEscalat(); }

void OnTransformaOrigenEscalat() { controlesGeometriaVgi().OnTransformaOrigenEscalat(); }

void OnTransformaMobilX() { controlesGeometriaVgi().OnTransformaMobilX(); }

void OnTransformaMobilY() { controlesGeometriaVgi().OnTransformaMobilY(); }

void OnTransformaMobilZ() { controlesGeometriaVgi().OnTransformaMobilZ(); }

void OnOcultacionsFrontFaces() { controlesGeometriaVgi().OnOcultacionsFrontFaces(); }

void OnOcultacionsTestvis() { controlesGeometriaVgi().OnOcultacionsTestvis(); }

void OnOcultacionsZBuffer() { controlesGeometriaVgi().OnOcultacionsZBuffer(); }

/* ------------------------------------------------------------------------- */
/*		10-12. IL.LUMINACIÓ, LLUMS, SHADERS								 */
/* ------------------------------------------------------------------------- */

// Implementacions a src/rendering/RuntimeRenderControls.cpp.
RuntimeRenderControls& controlesRender()
{
	static RuntimeRenderControls controles{
		.ifixe = ifixe,
		.ilum2sides = ilum2sides,
		.ilumina = ilumina,
		.oIlumina = oIlumina,
		.test_vis = test_vis,
		.oculta = oculta,
		.sw_material = sw_material,
		.sw_il = sw_il,
		.llum_ambient = llum_ambient,
		.llumGL = llumGL,
		.textura = textura,
		.t_textura = t_textura,
		.tFlag_invert_Y = tFlag_invert_Y,
		.texturesID = texturesID,
		.shader = shader,
		.shader_programID = shader_programID,
		.shaderLighting = shaderLighting,
		.oclusoresPhongInvalidos = oclusoresPhongInvalidos,
		.aplicarIntensidadMaterial = [](GLuint programa, bool ambienteActivo)
			{ aplicarIntensidadMaterial(programa, ambienteActivo); }
	};
	return controles;
}

void OnIluminacioLlumfixe() { controlesRender().OnIluminacioLlumfixe(); }

void OnIluminacio2Sides() { controlesRender().OnIluminacio2Sides(); }

void OnIluminacioPunts() { controlesRender().OnIluminacioPunts(); }

void OnIluminacioFilferros() { controlesRender().OnIluminacioFilferros(); }

void OnIluminacioPlana() { controlesRender().OnIluminacioPlana(); }

void OnIluminacioSuau() { controlesRender().OnIluminacioSuau(); }

void OnMaterialReflmaterial() { controlesRender().OnMaterialReflmaterial(); }

void OnMaterialEmissio() { controlesRender().OnMaterialEmissio(); }

void OnMaterialAmbient() { controlesRender().OnMaterialAmbient(); }

void OnMaterialDifusa() { controlesRender().OnMaterialDifusa(); }

void OnMaterialEspecular() { controlesRender().OnMaterialEspecular(); }

void OnIluminacioTextures() { controlesRender().OnIluminacioTextures(); }

void OnIluminacioTexturaFitxerimatge() { controlesRender().OnIluminacioTexturaFitxerimatge(); }

void OnIluminacioTexturaFlagInvertY() { controlesRender().OnIluminacioTexturaFlagInvertY(); }

void OnLlumsLlumAmbient() { controlesRender().OnLlumsLlumAmbient(); }

void OnLlumsLlum0() { controlesRender().OnLlumsLlum0(); }

void OnLlumsLlum1() { controlesRender().OnLlumsLlum1(); }

void OnLlumsLlum2() { controlesRender().OnLlumsLlum2(); }

void OnLlumsLlum3() { controlesRender().OnLlumsLlum3(); }

void OnLlumsLlum4() { controlesRender().OnLlumsLlum4(); }

void OnLlumsLlum5() { controlesRender().OnLlumsLlum5(); }

void OnLlumsLlum6() { controlesRender().OnLlumsLlum6(); }

void OnLlumsLlum7() { controlesRender().OnLlumsLlum7(); }

void OnShaderFlat() { controlesRender().OnShaderFlat(); }

void OnShaderGouraud() { controlesRender().OnShaderGouraud(); }

void OnShaderPhong() { controlesRender().OnShaderPhong(); }

void OnShaderLoadFiles() { controlesRender().OnShaderLoadFiles(); }

void OnShaderPBinaryWrite() { controlesRender().OnShaderPBinaryWrite(); }

void OnShaderPBinaryRead() { controlesRender().OnShaderPBinaryRead(); }
