#include "src/rendering/LegacyVgiViewControls.h"

#include "file_dialog_compat.h"
#include "framework.h"
#include "src/rendering/shader.h"
#include "src/rendering/visualitzacio.h"
#include "src/scene/SceneLoader.h"

/* --------------------------------------------------------------------------------- */
/*`t`t`t`t4. CÀMERA: Esfèrica (Mobil, Zoom, ZoomO, Satelit), Navega, Geode */
/* --------------------------------------------------------------------------------- */
// CÀMERA: Mode Esfèrica (Càmera esfèrica en polars-opció booleana)
void LegacyVgiViewControls::OnCameraEsferica()
{
// TODO: Agregue aquí su código de controlador de comandos
	if (projeccio != ORTO || projeccio != CAP)
	{	camera = CAM_ESFERICA;

// Inicialitzar paràmetres Càmera Esfèrica
		OPV.R = 15.0;		OPV.alfa = 0.0;		OPV.beta = 0.0;				// Origen PV en esfèriques
		mobil = true;		zzoom = true;		satelit = false;
		Vis_Polar = POLARZ;
	}
}

// Tornar a lloc d'origen
void LegacyVgiViewControls::OnCameraOrigenEsferica()
{
// TODO: Agregue aquí su código de controlador de comandos
	if (camera == CAM_ESFERICA) {
		// Inicialitzar paràmetres Càmera Esfèrica
		OPV.R = 15.0;		OPV.alfa = 0.0;		OPV.beta = 0.0;				// Origen PV en esfèriques
		mobil = true;		zzoom = true;		satelit = false;
		Vis_Polar = POLARZ;
	}
}


// CÀMERA--> ESFERICA: Mobil. Punt de Vista Interactiu (opció booleana)
void LegacyVgiViewControls::OnVistaMobil()
{
// TODO: Agregue aquí su código de controlador de comandos
	if ((projeccio != ORTO) || (projeccio != CAP) && (camera == CAM_ESFERICA || camera == CAM_GEODE))  mobil = !mobil;
// Desactivació de Transformacions Geomètriques via mouse
//		si Visualització Interactiva activada.
	if (mobil) {
		transX = false;	transY = false; transZ = false;
	}
}


// CÀMERA--> ESFERICA: Zoom. Zoom Interactiu (opció booleana)
void LegacyVgiViewControls::OnVistaZoom()
{
	// TODO: Agregue aquí su código de controlador de comandos
	if ((projeccio == PERSPECT) && (camera == CAM_ESFERICA || camera == CAM_GEODE)) zzoom = !zzoom;
	// Desactivació de Transformacions Geomètriques via mouse
	//		si Zoom activat.
	if (zzoom) {	transX = false;	transY = false;	transZ = false;
					zzoomO = false;
				}
}


// CÀMERA--> ESFERICA: Zoom Orto. Zoom Interactiu en Ortogràfica (opció booleana)
void LegacyVgiViewControls::OnVistaZoomOrto()
{
// TODO: Agregue aquí su código de controlador de comandos
	if ((projeccio == ORTO) || (projeccio == AXONOM) && (camera == CAM_ESFERICA || camera == CAM_GEODE)) zzoomO = !zzoomO;
// Desactivació de Transformacions Geomètriques via mouse
//	si Zoom activat
	if (zzoomO) {	zzoom = false;
					transX = false;	transY = false;	transZ = false;
				}
}


// CÀMERA--> ESFERICA: Satèlit. Vista interactiva i animada en que increment de movimen és activat per mouse (opció booleana)
void LegacyVgiViewControls::OnVistaSatelit()
{
// TODO: Agregue aquí su código de controlador de comandos
	if ((projeccio != CAP && projeccio != ORTO) && (camera == CAM_ESFERICA || camera == CAM_GEODE)) satelit = !satelit;
	if (satelit) {	mobil = true;
					m_EsfeIncEAvall.alfa = 0.0;
					m_EsfeIncEAvall.beta = 0.0;
					}
	bool testA = anima;									// Testejar si hi ha alguna animació activa apart de Satèlit.
	//if ((!satelit) && (!testA)) KillTimer(WM_TIMER);	// Si es desactiva Satèlit i no hi ha cap animació activa es desactiva el Timer.
}


// CÀMERA--> ESFERICA: Polars Eix X cap amunt per a Visualització Interactiva
void LegacyVgiViewControls::OnVistaPolarsX()
{
// TODO: Agregue aquí su código de controlador de comandos
	if (projeccio != CAP) Vis_Polar = POLARX;

// EntornVGI: Inicialitzar la càmera en l'opció NAVEGA (posició i orientació eixos)
	if (camera == CAM_NAVEGA)
		{	opvN.x = 0.0;	opvN.y = 10.0;	opvN.z = 0.0;	 // opvN = (0,10,0)
			n[0] = 0.0;		n[1] = 0.0;		n[2] = 0.0;
			angleZ = 0.0;
		}
}


// CÀMERA--> ESFERICA: Polars Eix Y cap amunt per a Visualització Interactiva
void LegacyVgiViewControls::OnVistaPolarsY()
{
// TODO: Agregue aquí su código de controlador de comandos
	if (projeccio != CAP) Vis_Polar = POLARY;

// EntornVGI: Inicialitzar la càmera en l'opció NAVEGA (posició i orientació eixos)
	if (camera == CAM_NAVEGA)
		{	opvN.x = 0.0;	opvN.y = 0.0;	opvN.z = 10.0; // opvN = (0,0,10)
			n[0] = 0.0;		n[1] = 0.0;		n[2] = 0.0;
			angleZ = 0.0;
		}
}


// CÀMERA--> ESFERICA: Polars Eix Z cap amunt per a Visualització Interactiva
void LegacyVgiViewControls::OnVistaPolarsZ()
{
// TODO: Agregue aquí su código de controlador de comandos
	if (projeccio != CAP) Vis_Polar = POLARZ;

// EntornVGI: Inicialitzar la càmera en l'opció NAVEGA (posició i orientació eixos)
	if (camera == CAM_NAVEGA)
		{	opvN.x = 10.0;	opvN.y = 0.0;	opvN.z = 0.0; // opvN = (10,0,0)
			n[0] = 0.0;		n[1] = 0.0;		n[2] = 0.0;
			angleZ = 0.0;
		}
}

// CÀMERA--> NAVEGA:  Mode de navegació sobre un pla amb botons de teclat o de mouse (nav) (opció booleana)
void LegacyVgiViewControls::OnCameraNavega()
{
// Seleccionar el modo Navega conserva la pose actual; no es un teletransporte.
	if (projeccio != ORTO || projeccio != CAP)
	{
		camera = CAM_NAVEGA;
		mobil = false; zzoom = false; satelit = false;
		transX = false; transY = false; transZ = false;
		tr_cpv.x = 0.0; tr_cpv.y = 0.0; tr_cpv.z = 0.0;
		tr_cpvF.x = 0.0; tr_cpvF.y = 0.0; tr_cpvF.z = 0.0;
	}
}
// Tornar a lloc d'origen
void LegacyVgiViewControls::OnCameraOrigenNavega()
{
	if (camera == CAM_NAVEGA && activeSceneSpawn.presente) spawnPhysicsPlayer();
}

// CÀMERA--> GEODE:  Mode de navegació centrat a l'origent mirant un punt en coord. esfèriques (R,alfa,beta) (opció booleana)
void LegacyVgiViewControls::OnCameraGeode()
{
// TODO: Agregue aquí su código de controlador de comandos
	if (projeccio != ORTO || projeccio != CAP)
		{	camera = CAM_GEODE;
			// Inicialitzar paràmetres Càmera Geode
			OPV_G.R = 0.0;		OPV_G.alfa = 0.0;	OPV_G.beta = 0.0;				// Origen PV en esfèriques
			mobil = true;		zzoom = true;		satelit = false;	pan = false;
			Vis_Polar = POLARZ;
			llumGL[5].encesa = true;

			glFrontFace(GL_CW);
		}
}


void LegacyVgiViewControls::OnCameraOrigenGeode()
{
// TODO: Agregue aquí su código de controlador de comandos
// Inicialitzar paràmetres Càmera Geode
	OPV_G.R = 0.0;	OPV_G.alfa = 0.0;	OPV_G.beta = 0.0;				// Origen PV en esfèriques
	mobil = true;	zzoom = true;		zzoomO = false;		 satelit = false;	pan = false;
	Vis_Polar = POLARZ;
}

/* -------------------------------------------------------------------------------- */
/*					5. VISTA: Pantalla Completa, Pan i Eixos	                    */
/* -------------------------------------------------------------------------------- */
// VISTA: FullScreen (Pantalla Completa-opció booleana)
void LegacyVgiViewControls::OnVistaFullscreen()
{
// TODO: Agregue aquí su código de controlador de comandos

	if (!fullscreen)
		{	/*
			// I note that I go to full-screen mode
			fullscreen = true;
			// Remembers the address of the window in which the view was placed (probably a frame)
			saveParent = this->GetParent();
			// Assigns a view to a new parent - desktop
			this->SetParent(GetDesktopWindow());
			CRect rect; // it's about the dimensions of the desktop-desktop
			GetDesktopWindow()->GetWindowRect(&rect);
			// I set the window on the desktop
			MoveWindow(rect);
			*/
		}
	else {	// Switching off the full-screen mode
		/*
		fullscreen = false;
		// Assigns an old parent view
		this->SetParent(saveParent);
		CRect rect; // It's about the dimensions of the desktop-desktop
		// Get client screen dimensions
		saveParent->GetClientRect(&rect);
		// Changes the position and dimensions of the specified window.
		MoveWindow(rect, FALSE);
		*/
		}

}

// VISTA: Mode de Desplaçament horitzontal i vertical per pantalla del Punt de Vista (pan) (opció booleana)
void LegacyVgiViewControls::OnVistaPan()
{
// TODO: Agregue aquí su código de controlador de comandos
	if ((projeccio == ORTO) || (projeccio == CAP)) pan = false;
	// Desactivació de Transformacions Geomètriques via mouse i navega si pan activat
	if (pan) {	mobil = true;		zzoom = true;
				transX = false;		transY = false;		transZ = false;
				//navega = false;
			}
}

// Tornar a lloc d'origen
void LegacyVgiViewControls::OnVistaOrigenPan()
{
// TODO: Agregue aquí su código de controlador de comandos
	if (pan) {	fact_pan = 1;
				tr_cpv.x = 0;	tr_cpv.y = 0;	tr_cpv.z = 0;
			}
}

// VISTA: Visualitzar eixos coordenades món (opció booleana)
void LegacyVgiViewControls::OnVistaEixos()
{
// TODO: Agregue aquí su código de controlador de comandos
	eixos = !eixos;

}


// SKYBOX: Visualitzar Skybox en l'escena (opció booleana)
void LegacyVgiViewControls::OnVistaSkyBox()
{
// TODO: Agregue aquí su código de controlador de comandos
	//SkyBoxCube = !SkyBoxCube;

// Càrrega Shader Skybox
	if (!skC_programID) skC_programID = shader_SkyBoxC.loadFileShaders("./assets/shaders/skybox.VERT", "./assets/shaders/skybox.FRAG");

// Càrrega VAO Skybox Cube
	if (skC_VAOID.vaoId == 0) skC_VAOID = loadCubeSkybox_VAO();
	Set_VAOList(CUBE_SKYBOX, skC_VAOID);

	if (!cubemapTexture)
		{	// load Skybox textures
			// -------------
			std::vector<std::string> faces =
			{ "./assets/textures/skybox/right.jpg",
				"./assets/textures/skybox/left.jpg",
				"./assets/textures/skybox/top.jpg",
				"./assets/textures/skybox/bottom.jpg",
				"./assets/textures/skybox/front.jpg",
				"./assets/textures/skybox/back.jpg"
			};
		cubemapTexture = loadCubemap(faces);
	}
}

/* ------------------------------------------------------------------------- */
/*					6. PROJECCIÓ                                             */
/* ------------------------------------------------------------------------- */

// PROJECCIÓ: Perspectiva
void LegacyVgiViewControls::OnProjeccioPerspectiva()
{
// TODO: Agregue aquí su código de controlador de comandos
	projeccio = PERSPECT;
	mobil = true;			zzoom = true;		zzoomO = false;
}

// PROJECCIÓ: Perspectiva
void LegacyVgiViewControls::OnProjeccioOrtografica()
{
// TODO: Agregue aquí su código de controlador de comandos
	projeccio = ORTO;
	mobil = false;			zzoom = false;		zzoomO = true;
}

// PROJECCIÓ: Perspectiva
void LegacyVgiViewControls::OnProjeccioAxonometrica()
{
// TODO: Agregue aquí su código de controlador de comandos
	projeccio = AXONOM;
	mobil = true;			zzoom = true;		zzoomO = false;
}
