#include "src/app/AppState.h"
#include "src/legacy_vgi/VgiMenuCallbacks.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/legacy_vgi/VgiFileActions.h"
#include "src/legacy_vgi/VgiMaterial.h"

LegacyVgiViewControls& vgiViewBindings()
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
		.activeSceneSpawn = activeSceneSpawn,
		.spawnPhysicsPlayer = spawnPhysicsPlayer,
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

void OnCameraEsferica() { vgiViewBindings().OnCameraEsferica(); }

void OnCameraOrigenEsferica() { vgiViewBindings().OnCameraOrigenEsferica(); }

void OnVistaMobil() { vgiViewBindings().OnVistaMobil(); }

void OnVistaZoom() { vgiViewBindings().OnVistaZoom(); }

void OnVistaZoomOrto() { vgiViewBindings().OnVistaZoomOrto(); }

void OnVistaSatelit() { vgiViewBindings().OnVistaSatelit(); }

void OnVistaPolarsX() { vgiViewBindings().OnVistaPolarsX(); }

void OnVistaPolarsY() { vgiViewBindings().OnVistaPolarsY(); }

void OnVistaPolarsZ() { vgiViewBindings().OnVistaPolarsZ(); }

void OnCameraNavega() { vgiViewBindings().OnCameraNavega(); }

void OnCameraOrigenNavega() { vgiViewBindings().OnCameraOrigenNavega(); }

void OnCameraGeode() { vgiViewBindings().OnCameraGeode(); }

void OnCameraOrigenGeode() { vgiViewBindings().OnCameraOrigenGeode(); }

void OnVistaFullscreen() { vgiViewBindings().OnVistaFullscreen(); }

void OnVistaPan() { vgiViewBindings().OnVistaPan(); }

void OnVistaOrigenPan() { vgiViewBindings().OnVistaOrigenPan(); }

void OnVistaEixos() { vgiViewBindings().OnVistaEixos(); }

void OnVistaSkyBox() { vgiViewBindings().OnVistaSkyBox(); }

void OnProjeccioPerspectiva() { vgiViewBindings().OnProjeccioPerspectiva(); }

void OnProjeccioOrtografica() { vgiViewBindings().OnProjeccioOrtografica(); }

void OnProjeccioAxonometrica() { vgiViewBindings().OnProjeccioAxonometrica(); }

LegacyVgiGeometryControls& vgiGeometryBindings()
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

void OnObjecteCap() { vgiGeometryBindings().OnObjecteCap(); }

void OnObjecteCub() { vgiGeometryBindings().OnObjecteCub(); }

void OnObjecteCubRGB() { vgiGeometryBindings().OnObjecteCubRGB(); }

void OnObjecteEsfera() { vgiGeometryBindings().OnObjecteEsfera(); }

void OnObjecteTetera() { vgiGeometryBindings().OnObjecteTetera(); }

void OnObjecteArc() { vgiGeometryBindings().OnObjecteArc(); }

void OnObjecteTie() { vgiGeometryBindings().OnObjecteTie(); }

void OnObjecteCorbaBezier() { vgiGeometryBindings().OnObjecteCorbaBezier(); }

void OnObjecteCorbaLemniscata() { vgiGeometryBindings().OnObjecteCorbaLemniscata(); }

void OnObjecteCorbaHermitte() { vgiGeometryBindings().OnObjecteCorbaHermitte(); }

void OnObjecteCorbaCatmullRom() { vgiGeometryBindings().OnObjecteCorbaCatmullRom(); }

void OnObjecteCorbaBSpline() { vgiGeometryBindings().OnObjecteCorbaBSpline(); }

void OnObjectePuntsControl() { vgiGeometryBindings().OnObjectePuntsControl(); }

void OnCorbesTriedreFrenet() { vgiGeometryBindings().OnCorbesTriedreFrenet(); }

void OnCorbesTriedreDarboux() { vgiGeometryBindings().OnCorbesTriedreDarboux(); }

void OnObjecteMatriuPrimitives() { vgiGeometryBindings().OnObjecteMatriuPrimitives(); }

void OnObjecteMatriuPrimitivesVAO() { vgiGeometryBindings().OnObjecteMatriuPrimitivesVAO(); }

void OnTransformaTraslacio() { vgiGeometryBindings().OnTransformaTraslacio(); }

void OnTransformaOrigenTraslacio() { vgiGeometryBindings().OnTransformaOrigenTraslacio(); }

void OnTransformaRotacio() { vgiGeometryBindings().OnTransformaRotacio(); }

void OnTransformaOrigenRotacio() { vgiGeometryBindings().OnTransformaOrigenRotacio(); }

void OnTransformaEscalat() { vgiGeometryBindings().OnTransformaEscalat(); }

void OnTransformaOrigenEscalat() { vgiGeometryBindings().OnTransformaOrigenEscalat(); }

void OnTransformaMobilX() { vgiGeometryBindings().OnTransformaMobilX(); }

void OnTransformaMobilY() { vgiGeometryBindings().OnTransformaMobilY(); }

void OnTransformaMobilZ() { vgiGeometryBindings().OnTransformaMobilZ(); }

void OnOcultacionsFrontFaces() { vgiGeometryBindings().OnOcultacionsFrontFaces(); }

void OnOcultacionsTestvis() { vgiGeometryBindings().OnOcultacionsTestvis(); }

void OnOcultacionsZBuffer() { vgiGeometryBindings().OnOcultacionsZBuffer(); }

/* ------------------------------------------------------------------------- */
/*		10-12. IL.LUMINACIÓ, LLUMS, SHADERS								 */
/* ------------------------------------------------------------------------- */

// Implementacions a src/rendering/RuntimeRenderControls.cpp.
RuntimeRenderControls& renderControlBindings()
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
		.phongOccludersDirty = phongOccludersDirty,
		.applyMaterialIntensity = [](GLuint programa, bool ambienteActivo)
			{ applyMaterialIntensity(programa, ambienteActivo); }
	};
	return controles;
}

void OnIluminacioLlumfixe() { renderControlBindings().OnIluminacioLlumfixe(); }

void OnIluminacio2Sides() { renderControlBindings().OnIluminacio2Sides(); }

void OnIluminacioPunts() { renderControlBindings().OnIluminacioPunts(); }

void OnIluminacioFilferros() { renderControlBindings().OnIluminacioFilferros(); }

void OnIluminacioPlana() { renderControlBindings().OnIluminacioPlana(); }

void OnIluminacioSuau() { renderControlBindings().OnIluminacioSuau(); }

void OnMaterialReflmaterial() { renderControlBindings().OnMaterialReflmaterial(); }

void OnMaterialEmissio() { renderControlBindings().OnMaterialEmissio(); }

void OnMaterialAmbient() { renderControlBindings().OnMaterialAmbient(); }

void OnMaterialDifusa() { renderControlBindings().OnMaterialDifusa(); }

void OnMaterialEspecular() { renderControlBindings().OnMaterialEspecular(); }

void OnIluminacioTextures() { renderControlBindings().OnIluminacioTextures(); }

void OnIluminacioTexturaFitxerimatge() { renderControlBindings().OnIluminacioTexturaFitxerimatge(); }

void OnIluminacioTexturaFlagInvertY() { renderControlBindings().OnIluminacioTexturaFlagInvertY(); }

void OnLlumsLlumAmbient() { renderControlBindings().OnLlumsLlumAmbient(); }

void OnLlumsLlum0() { renderControlBindings().OnLlumsLlum0(); }

void OnLlumsLlum1() { renderControlBindings().OnLlumsLlum1(); }

void OnLlumsLlum2() { renderControlBindings().OnLlumsLlum2(); }

void OnLlumsLlum3() { renderControlBindings().OnLlumsLlum3(); }

void OnLlumsLlum4() { renderControlBindings().OnLlumsLlum4(); }

void OnLlumsLlum5() { renderControlBindings().OnLlumsLlum5(); }

void OnLlumsLlum6() { renderControlBindings().OnLlumsLlum6(); }

void OnLlumsLlum7() { renderControlBindings().OnLlumsLlum7(); }

void OnShaderFlat() { renderControlBindings().OnShaderFlat(); }

void OnShaderGouraud() { renderControlBindings().OnShaderGouraud(); }

void OnShaderPhong() { renderControlBindings().OnShaderPhong(); }

void OnShaderLoadFiles() { renderControlBindings().OnShaderLoadFiles(); }

void OnShaderPBinaryWrite() { renderControlBindings().OnShaderPBinaryWrite(); }

void OnShaderPBinaryRead() { renderControlBindings().OnShaderPBinaryRead(); }
