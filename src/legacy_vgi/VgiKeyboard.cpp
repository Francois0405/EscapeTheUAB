#include "src/app/AppState.h"
#include "src/legacy_vgi/VgiKeyboard.h"
#include "src/app/WindowCallbacks.h"
#include "src/legacy_vgi/VgiFileActions.h"
#include "src/legacy_vgi/VgiMenuCallbacks.h"
#include "src/legacy_vgi/VgiMenus.h"

// Teclat_Shift: Shortcuts per Pop Ups Fitxer, Finestra, Vista, Projecció i Objecte
void Teclat_Shift(int key, GLFWwindow* window)
{
	//const char* nomfitxer;
	bool testA = false;
	int error = 0;

	//	nfdchar_t* nomFitxer = NULL;
	//	nfdresult_t result; // = NFD_OpenDialog(NULL, NULL, &nomFitxer);

	CColor color_Mar = { 0.0,0.0,0.0,1.0 };

	switch (key)
	{
	// ----------- POP UP Fitxers
	// Tecla Obrir Fractal
	case GLFW_KEY_F1:
		OnArxiuObrirFractal();
/*
		nomFitxer = NULL;
		// Entorn VGI: Obrir diàleg de lectura de fitxer (fitxers (*.MNT)
		result = NFD_OpenDialog(NULL, NULL, &nomFitxer);

		if (result == NFD_OKAY) {
			puts("Fitxer Fractal Success!");
			puts(nomFitxer);

			objecte = O_FRACTAL;
			// Entorn VGI: TO DO -> Llegir fitxer fractal (nomFitxer) i inicialitzar alçades

			free(nomFitxer);
			}
*/
		break;

	// Tecla Obrir Fitxer OBJ
	case GLFW_KEY_F2:
		OnArxiuObrirFitxerObj();
/*
		// Entorn VGI: Obrir diàleg de lectura de fitxer (fitxers (*.OBJ)
		nomFitxer = NULL;
		result = NFD_OpenDialog(NULL, NULL, &nomFitxer);

		if (result == NFD_OKAY) {
			puts("Success!");
			puts(nomFitxer);

			objecte = OBJOBJ;	textura = true;		tFlag_invert_Y = false;
			//char* nomfitx = nomFitxer;
			if (ObOBJ == NULL) ObOBJ = ::new COBJModel;
			else { // Si instància ja s'ha utilitzat en un objecte OBJ
				ObOBJ->netejaVAOList_OBJ();		// Netejar VAO, EBO i VBO
				ObOBJ->netejaTextures_OBJ();	// Netejar buffers de textures
				}

			//int error = ObOBJ->LoadModel(nomfitx);			// Carregar objecte OBJ amb textura com a varis VAO's
			int error = ObOBJ->LoadModel(nomFitxer);			// Carregar objecte OBJ amb textura com a varis VAO's

			//	Pas de paràmetres textura al shader
			if (shader_programID) glUniform1i(glcache::UniformLocation(shader_programID, "textur"), textura);
			if (shader_programID) glUniform1i(glcache::UniformLocation(shader_programID, "flag_invert_y"), tFlag_invert_Y);
			free(nomFitxer);
			}
	*/
		break;

	// ----------- POP UP Finestra
	// Tecla Obrir nova finestra
	case GLFW_KEY_W:

		break;

// ----------- POP UP Camera

	// 		// Tecla Camera Esferica
	case GLFW_KEY_L:
		OnCameraEsferica();
		oCamera = shortCut_Camera();	// Actualitzar opció de menú CAMERA
		/*
		if ((projeccio != ORTO) && (projeccio != CAP)) camera = CAM_ESFERICA;
		// Activació de zoom, mobil
		mobil = true;	zzoom = true;
		*/
		break;

	// Tecla Mobil?
	case GLFW_KEY_M:
		OnVistaMobil();
		/*
		if ((projeccio != ORTO) && (projeccio != CAP)) mobil = !mobil;
		// Desactivació de Transformacions Geomètriques via mouse
		//		si Visualització Interactiva activada.
		if (mobil) {	transX = false;	transY = false; transZ = false;
					}
		*/
		break;

	// Tecla Zoom
	case GLFW_KEY_Q:
		OnVistaZoom();
		/*if ((projeccio != ORTO) && (projeccio != CAP)) zzoom = !zzoom;
		// Desactivació de Transformacions Geomètriques via mouse
		//		si Zoom activat.
		if (zzoom) {
			zzoomO = false;  transX = false;	transY = false;	transZ = false;
			}
		*/
		break;

	// Tecla Zoom Orto
	case GLFW_KEY_F3:
		OnVistaZoomOrto();
		/*
		if (projeccio == ORTO || projeccio==AXONOM) zzoomO = !zzoomO;
		// Desactivació de Transformacions Geomètriques via mouse
		//		si Zoom Orto activat.
		if (zzoomO) {
			zzoom = false;  transX = false;		transY = false;		transZ = false;
			}
		*/
		break;

	// Tecla Satèl.lit?
	case GLFW_KEY_S:
		OnVistaSatelit();
/*
		if ((projeccio != CAP && projeccio != ORTO)) satelit = !satelit;
		if (satelit) mobil = true;
		testA = anima;				// Testejar si hi ha alguna animació activa apart de Satèlit.
*/
		break;

	// Tecla Camera Navega
	case GLFW_KEY_N:
		OnCameraNavega();
		oCamera = shortCut_Camera();	// Actualitzar opció de menú CAMERA
/*
		if ((projeccio != ORTO) && (projeccio != CAP)) camera = CAM_NAVEGA;
		// Desactivació de zoom, mobil, Transformacions Geomètriques via mouse i pan
		//		si navega activat
		if (camera == CAM_NAVEGA)
		{	mobil = false;	zzoom = false;
			transX = false;	transY = false;	transZ = false;
			pan = false;
			tr_cpv.x = 0.0;		tr_cpv.y = 0.0;		tr_cpv.z = 0.0;	// Inicialitzar a 0 desplaçament de pantalla
			tr_cpvF.x = 0.0;	tr_cpvF.y = 0.0;	tr_cpvF.x = 0.0; // Inicialitzar a 0 desplaçament de pantalla
		}
		else {	mobil = true;
				zzoom = true;
			}
*/
		break;

	// Tecla Camera Geode
	case GLFW_KEY_J:
		OnCameraGeode();
		oCamera = shortCut_Camera();	// Actualitzar opció de menú CAMERA
/*
		if ((projeccio != ORTO) && (projeccio != CAP)) camera = CAM_GEODE;
		// Desactivació de zoom, mobil, Transformacions Geomètriques via mouse i pan
		//		si navega activat
		if (camera == CAM_GEODE)
		{
			OPV_G.R = 0.0;		OPV_G.alfa = 0.0;		OPV_G.beta = 0.0;
			OPV.R = 0.0;		OPV.alfa = 0.0;			OPV.beta = 0.0;				// Origen PV en esfèriques
			mobil = true;		zzoom = true;	zzoomO = false;	 satelit = false;	pan = false;
			Vis_Polar = POLARZ;	llumGL[5].encesa = true;
			glFrontFace(GL_CW);
		}
*/
		break;

		// Tecla Origen (per a Pan i Navega)
	case GLFW_KEY_HOME:
		if (pan) OnVistaOrigenPan();
/*
		{	fact_pan = 1;
					tr_cpv.x = 0;	tr_cpv.y = 0;	tr_cpv.z = 0;
				}
*/
		if (camera == CAM_NAVEGA) OnCameraOrigenNavega();
/*
			{	n[0] = 0.0;		n[1] = 0.0;		n[2] = 0.0;
				opvN.x = 10.0;	opvN.y = 0.0;		opvN.z = 0.0;
				angleZ = 0.0;
			}
*/
		else if (camera == CAM_NAVEGA) OnCameraOrigenGeode();

		break;

	// Tecla Polars Eix X?
	case GLFW_KEY_X:
		OnVistaPolarsX();
		//if ((projeccio != CAP) && (camera != CAM_NAVEGA)) Vis_Polar = POLARX;
		break;

	// Tecla Polars Eix Y?
	case GLFW_KEY_Y:
		OnVistaPolarsY();
		//if ((projeccio != CAP) && (camera != CAM_NAVEGA)) Vis_Polar = POLARY;
		break;

	// Tecla Polars Eix Z?
	case GLFW_KEY_Z:
		OnVistaPolarsZ();
		//if ((projeccio != CAP) && (camera != CAM_NAVEGA)) Vis_Polar = POLARZ;
		break;

// ----------- POP UP Vista
	// 
				// Tecla Full Screen?
	case GLFW_KEY_F:
		OnFull_Screen(primary, window);
		break;

		// Tecla Eixos?
	case GLFW_KEY_F4:
		OnVistaEixos();
		//eixos = !eixos;
		break;

	// Tecla Skybox Cube
	case GLFW_KEY_K:
		SkyBoxCube = !SkyBoxCube;
		if (SkyBoxCube) OnVistaSkyBox();
/*
		{	Vis_Polar = POLARY;
			// Càrrega VAO Skybox Cube
			if (skC_VAOID.vaoId == 0) skC_VAOID = loadCubeSkybox_VAO();

			if (!cubemapTexture)
				{	// load Skybox textures
					// -------------
					std::vector<std::string> faces =
						{	"./assets/textures/skybox/right.jpg",
							"./assets/textures/skybox/right.jpg",
							"./assets/textures/skybox/left.jpg",
							"./assets/textures/skybox/top.jpg",
							"./assets/textures/skybox/bottom.jpg",
							"./assets/textures/skybox/front.jpg",
							"./assets/textures/skybox/back.jpg"
						};
					cubemapTexture = loadCubemap(faces);
				}
		}
*/
		break;

	// Tecla Pan?
	case GLFW_KEY_G:
		OnVistaPan();
/*
		if ((projeccio != ORTO) && (projeccio != CAP)) pan = !pan;
		// Desactivació de Transformacions Geomètriques via mouse i navega si pan activat
		if (pan) {	mobil = true;		zzoom = true;
					transX = false;		transY = false;	transZ = false;
				}
*/
		break;

// ----------- POP UP Projecció
	// Tecla Axonomètrica
	case GLFW_KEY_A:
		OnProjeccioAxonometrica();
		oProjeccio = shortCut_Projeccio();	// Actualitzar opció de menú PROJECCIO
/*
		if (projeccio != AXONOM) {
			projeccio = AXONOM;
			mobil = true;			zzoom = true;
		}
*/
		break;

	// Tecla Ortogràfica
	case GLFW_KEY_O:
		OnProjeccioOrtografica();
		oProjeccio = shortCut_Projeccio();	// Actualitzar opció de menú PROJECCIO
/*
		if (projeccio != ORTO) {
			projeccio = ORTO;
			mobil = false;			zzoom = false;
			}
*/
		break;

	// Tecla Perspectiva
	case GLFW_KEY_P:
		OnProjeccioPerspectiva();
		oProjeccio = shortCut_Projeccio();	// Actualitzar opció de menú PROJECCIO
/*
		if (projeccio != PERSPECT) {
			projeccio = PERSPECT;
			mobil = true;			zzoom = true;
			}
*/
		break;

// ----------- POP UP Objecte
	// Tecla CAP
	case GLFW_KEY_B:
		if (objecte != CAP) OnObjecteCap();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		if (objecte != CAP) {
			objecte = CAP;
			netejaVAOList();							// Neteja Llista VAO.
			}
*/
		break;

	// Tecla Cub
	case GLFW_KEY_C:
		if (objecte != CUB)	OnObjecteCub();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		objecte = CUB;
		//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

		//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)
		netejaVAOList();											// Neteja Llista VAO.

		// Posar color objecte (col_obj) al vector de colors del VAO.
		SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);
		Set_VAOList(GLUT_CUBE, loadglutSolidCube_EBO(1.0));		// Genera EBO de cub mida 1 i el guarda a la posició GLUT_CUBE.
*/
		break;

	// Tecla Cub RGB
	case GLFW_KEY_D:
		if (objecte != CUB_RGB)	OnObjecteCubRGB();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		objecte = CUB_RGB;
		//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

		//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)
		netejaVAOList();						// Neteja Llista VAO.

		Set_VAOList(GLUT_CUBE_RGB, loadglutSolidCubeRGB_EBO(1.0));	// Genera EBO de cub mida 1 i el guarda a la posició GLUT_CUBE_RGB.
*/
		break;

	// Tecla Esfera
	case GLFW_KEY_E:
		if (objecte != ESFERA)	OnObjecteEsfera();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		objecte = ESFERA;
		//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

		//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)
		netejaVAOList();						// Neteja Llista VAO.

		// Posar color objecte (col_obj) al vector de colors del VAO.
		SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

		Set_VAOList(GLU_SPHERE, loadgluSphere_EBO(1.0, 30, 30));
*/
		break;

	// Tecla Tetera
	case GLFW_KEY_T:
		if (objecte != TETERA)	OnObjecteTetera();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		objecte = TETERA;
		//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

		//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)
		netejaVAOList();						// Neteja Llista VAO.

		// Posar color objecte (col_obj) al vector de colors del VAO.
		SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

		//if (Get_VAOId(GLUT_TEAPOT) != 0) deleteVAOList(GLUT_TEAPOT);
		Set_VAOList(GLUT_TEAPOT, loadglutSolidTeapot_VAO()); //Genera VAO tetera mida 1 i el guarda a la posició GLUT_TEAPOT.
*/
		break;

	// Tecla Arc
	case GLFW_KEY_R:
		if (objecte != ARC)	OnObjecteArc();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		objecte = ARC;
		//  Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)
		//	Canviar l'escala per a centrar la vista (Ortogràfica)

		color_Mar.r = 0.5;	color_Mar.g = 0.4; color_Mar.b = 0.9; color_Mar.a = 1.0;
		//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

		//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)

		// Càrrega dels VAO's per a construir objecte ARC
		netejaVAOList();						// Neteja Llista VAO.

		// Posar color objecte (col_obj) al vector de colors del VAO.
		SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

		//if (Get_VAOId(GLUT_CUBE) != 0) deleteVAOList(GLUT_CUBE);
		Set_VAOList(GLUT_CUBE, loadglutSolidCube_EBO(1.0));		// Càrrega Cub de costat 1 com a EBO a la posició GLUT_CUBE.

		//if (Get_VAOId(GLU_SPHERE) != 0) deleteVAOList(GLU_SPHERE);
		Set_VAOList(GLU_SPHERE, loadgluSphere_EBO(0.5, 20, 20));	// Càrrega Esfera a la posició GLU_SPHERE.

		//if (Get_VAOId(GLUT_TEAPOT) != 0) deleteVAOList(GLUT_TEAPOT);
		Set_VAOList(GLUT_TEAPOT, loadglutSolidTeapot_VAO());		// Carrega Tetera a la posició GLUT_TEAPOT.

		//if (Get_VAOId(MAR_FRACTAL_VAO) != 0) deleteVAOList(MAR_FRACTAL_VAO);
		Set_VAOList(MAR_FRACTAL_VAO, loadSea_VAO(color_Mar));		// Carrega Mar a la posició MAR_FRACTAL_VAO.
*/
		break;

	// Tecla Tie (Star Wars)
	case GLFW_KEY_I:
		if (objecte != TIE)	OnObjecteTie();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		objecte = TIE;		textura = true;
			//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

			//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)

		// Càrrega dels VAO's per a construir objecte TIE
		netejaVAOList();						// Neteja Llista VAO.

		// Posar color objecte (col_obj) al vector de colors del VAO.
		SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

		//if (Get_VAOId(GLU_CYLINDER) != 0) deleteVAOList(GLU_CYLINDER);
		Set_VAOList(GLUT_CYLINDER, loadgluCylinder_EBO(5.0f, 5.0f, 0.5f, 6, 1));// Càrrega cilindre com a VAO.

		//if (Get_VAOId(GLU_DISK) != 0)deleteVAOList(GLU_DISK);
		Set_VAOList(GLU_DISK, loadgluDisk_EBO(0.0f, 5.0f, 6, 1));	// Càrrega disc com a VAO

		//if (Get_VAOId(GLU_SPHERE) != 0)deleteVAOList(GLU_SPHERE);
		Set_VAOList(GLU_SPHERE, loadgluSphere_EBO(10.0f, 80, 80));	// Càrrega disc com a VAO

		//if (Get_VAOId(GLUT_USER1) != 0)deleteVAOList(GLUT_USER1);
		Set_VAOList(GLUT_USER1, loadgluCylinder_EBO(5.0f, 5.0f, 2.0f, 6, 1)); // Càrrega cilindre com a VAO

		//if (Get_VAOId(GLUT_CUBE) != 0)deleteVAOList(GLUT_CUBE);
		Set_VAOList(GLUT_CUBE, loadglutSolidCube_EBO(1.0));			// Càrrega cub com a EBO

		//if (Get_VAOId(GLUT_TORUS) != 0)deleteVAOList(GLUT_TORUS);
		Set_VAOList(GLUT_TORUS, loadglutSolidTorus_EBO(1.0, 5.0, 20, 20));

		//if (Get_VAOId(GLUT_USER2) != 0)deleteVAOList(GLUT_USER2);
		Set_VAOList(GLUT_USER2, loadgluCylinder_EBO(1.0f, 0.5f, 5.0f, 60, 1)); // Càrrega cilindre com a VAO

		//if (Get_VAOId(GLUT_USER3) != 0)deleteVAOList(GLUT_USER3);
		Set_VAOList(GLUT_USER3, loadgluCylinder_EBO(0.35f, 0.35f, 5.0f, 80, 1)); // Càrrega cilindre com a VAO

		//if (Get_VAOId(GLUT_USER4) != 0)deleteVAOList(GLUT_USER4);
		Set_VAOList(GLUT_USER4, loadgluCylinder_EBO(4.0f, 2.0f, 10.25f, 40, 1)); // Càrrega cilindre com a VAO

		//if (Get_VAOId(GLUT_USER5) != 0) deleteVAOList(GLUT_USER5);
		Set_VAOList(GLUT_USER5, loadgluCylinder_EBO(1.5f, 4.5f, 2.0f, 8, 1)); // Càrrega cilindre com a VAO

		//if (Get_VAOId(GLUT_USER6) != 0) deleteVAOList(GLUT_USER6);
		Set_VAOList(GLUT_USER6, loadgluDisk_EBO(0.0f, 1.5f, 8, 1)); // Càrrega disk com a VAO
*/
		break;

	// Tecla Corbes Bezier
	case GLFW_KEY_F5:
		if (objecte != C_BEZIER)	OnObjecteCorbaBezier();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		nomFitxer = NULL;
		// Entorn VGI: Obrir diàleg de lectura de fitxer (fitxers (*.MNT)
		result = NFD_OpenDialog(NULL, NULL, &nomFitxer);

		if (result == NFD_OKAY) {
			puts("Bezier File Success!");
			puts(nomFitxer);

			nom = "";			objecte = C_BEZIER;
			npts_T = llegir_ptsC(nomFitxer);
			free(nomFitxer);

			// Càrrega dels VAO's per a construir la corba Bezier
			netejaVAOList();						// Neteja Llista VAO.

			// Posar color objecte (col_obj) al vector de colors del VAO.
			SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

			// Definir Esfera EBO per a indicar punts de control de la corba
			Set_VAOList(GLU_SPHERE, loadgluSphere_EBO(5.0, 20, 20));	// Genera esfera i la guarda a la posició GLUT_CUBE.

			// Definir Corba Bezier com a VAO
				//Set_VAOList(CRV_BEZIER, load_Bezier_Curve_VAO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
			Set_VAOList(CRV_BEZIER, load_Bezier_Curve_EBO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
			}
*/
		break;

	// Tecla Corbes B-Spline
	case GLFW_KEY_F6:
		if (objecte != C_BSPLINE)	OnObjecteCorbaBSpline();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		nomFitxer = NULL;
		// Entorn VGI: Obrir diàleg de lectura de fitxer (fitxers (*.MNT)
		result = NFD_OpenDialog(NULL, NULL, &nomFitxer);

		if (result == NFD_OKAY) {
			puts("B-Spline File Success!");
			puts(nomFitxer);

			nom = "";			objecte = C_BSPLINE;
			npts_T = llegir_ptsC(nomFitxer);
			free(nomFitxer);

			// Càrrega dels VAO's per a construir la corba BSpline
			netejaVAOList();						// Neteja Llista VAO.

			// Posar color objecte (col_obj) al vector de colors del VAO.
			SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

			// Definir Esfera EBO per a indicar punts de control de la corba
			Set_VAOList(GLU_SPHERE, loadgluSphere_EBO(5.0, 20, 20));	// Guarda (vaoId, vboId, nVertexs) a la posició GLUT_CUBE.

			// Definr Corba BSpline com a VAO
				//Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
			Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
			}
*/
		break;

	// Tecla Corbes Lemniscata
	case GLFW_KEY_F7:
		if (objecte != C_LEMNISCATA)	OnObjecteCorbaLemniscata();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		if (objecte != C_LEMNISCATA) {
			objecte = C_LEMNISCATA;
			// Càrrega dels VAO's per a construir la corba Bezier
			netejaVAOList();						// Neteja Llista VAO.

			// Posar color objecte (col_obj) al vector de colors del VAO.
			SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

			// Definr Corba Lemniscata 3D com a VAO
				//Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_VAO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.
			Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_EBO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.
			}
*/
		break;

	// Tecla Corbes Hermitte
	case GLFW_KEY_F8:
		if (objecte != C_HERMITTE)	OnObjecteCorbaHermitte();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
		break;

	// Tecla Corbes Catmull-Rom
	case GLFW_KEY_F9:
		if (objecte != C_CATMULL_ROM)	OnObjecteCorbaCatmullRom();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
		break;

	// Tecla Punts de Control?
	case GLFW_KEY_F10:
		OnObjectePuntsControl();
		break;

	// Tecla F11: alternar pantalla completa
	case GLFW_KEY_F11:
		OnFull_Screen(primary, window);
		break;

	// Tecla Triedre de Darboux?
	case GLFW_KEY_F12:
		OnCorbesTriedreDarboux();
		break;

	// Tecla Matriu Primitives
	case GLFW_KEY_H:
		if (objecte != MATRIUP)	OnObjecteMatriuPrimitives();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
		//objecte = MATRIUP;
		break;

	// Tecla Matriu Primitives VAO
	case GLFW_KEY_V:
		if (objecte != MATRIUP_VAO)	OnObjecteMatriuPrimitivesVAO();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		if (objecte != MATRIUP_VAO) {
			objecte = MATRIUP_VAO;

			// Càrrega dels VAO's per a construir objecte ARC
			netejaVAOList();						// Neteja Llista VAO.

			// Posar color objecte (col_obj) al vector de colors del VAO.
			SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

			//if (Get_VAOId(GLUT_CUBE) != 0) deleteVAOList(GLUT_CUBE);
			Set_VAOList(GLUT_CUBE, loadglutSolidCube_EBO(1.0f));

			//if (Get_VAOId(GLUT_TORUS) != 0)deleteVAOList(GLUT_TORUS);
			Set_VAOList(GLUT_TORUS, loadglutSolidTorus_EBO(2.0, 3.0, 20, 20));

			//if (Get_VAOId(GLUT_SPHERE) != 0) deleteVAOList(GLU_SPHERE);
			Set_VAOList(GLU_SPHERE, loadgluSphere_EBO(1.0, 20, 20));
		}
*/
		break;

	default:
		break;
	}
}

// Teclat_Ctrl: Shortcuts per Pop Ups Transforma, Iluminació, llums, Shaders
void Teclat_Ctrl(int key)
{
	std::string nomVert, nomFrag;	// Nom de fitxer.

	switch (key)
	{
// ----------- POP UP TRANSFORMA
	// Tecla Transforma --> Traslació?
	case GLFW_KEY_T:
		OnTransformaTraslacio();
		break;

	// Tecla Transforma --> Rotació?
	case GLFW_KEY_R:
		OnTransformaRotacio();
		break;

	// Tecla Transforma --> Escalat?
	case GLFW_KEY_S:
		OnTransformaEscalat();
		break;

	// Tecla Escape (per a Transforma --> Origen Traslació, Transforma --> Origen Rotació i Transforma --> Origen Escalat)
	case GLFW_KEY_O:
		if (trasl)
		{	OnTransformaOrigenTraslacio();
		}
		else if (rota) {	OnTransformaOrigenRotacio();
						}
		if (escal) {	OnTransformaOrigenEscalat();
					}
		break;

	// Tecla Transforma --> Mobil Eix X? (opció booleana).
	case GLFW_KEY_X:
		OnTransformaMobilX();
		break;

	// Tecla Transforma --> Mobil Eix Y? (opció booleana).
	case GLFW_KEY_Y:
		OnTransformaMobilY();
		break;

	// Tecla Transforma --> Mobil Eix Z? (opció booleana).
	case GLFW_KEY_Z:
		OnTransformaMobilZ();
		break;

		// ----------- POP UP OCULTACIONS
	// Tecla Ocultacions --> Front faces? (opció booleana).
	case GLFW_KEY_D:
		OnOcultacionsFrontFaces();
		//front_faces = !front_faces;
		break;

	// Tecla Ocultacions --> Test Visibilitat? (back face culling)
	case GLFW_KEY_C:
		OnOcultacionsTestvis();
		//test_vis = !test_vis;
		break;

	// Tecla Ocultacions --> Z-Buffer? (opció booleana).
	case GLFW_KEY_B:
		OnOcultacionsZBuffer();
		//oculta = !oculta;
		break;

		// Tecla Ocultacions --> Back-lines? (opció booleana).
		//case GLFW_KEY_B:
		//	back_line = !back_line;
		//	break;

	// ----------- POP UP ILUMINACIÓ
	// Tecla Llum Fixe? (opció booleana).
	case GLFW_KEY_F:
		OnIluminacioLlumfixe();
		//ifixe = !ifixe;
		break;

	// Tecla Iluminació --> Punts
	case GLFW_KEY_F1:
		OnIluminacioPunts();
		// Actualitza el menú Iluminació segons el canvi.
		oIlumina = shortCut_Iluminacio();
		break;

	// Tecla Iluminació --> Filferros
	case GLFW_KEY_F2:
		OnIluminacioFilferros();
		// Actualitza el menú Iluminació segons el canvi.
		oIlumina = shortCut_Iluminacio();
		break;

	// Tecla Iluminació --> Plana
	case GLFW_KEY_F3:
		OnIluminacioPlana();
		// Actualitza el menú Iluminació segons el canvi.
		oIlumina = shortCut_Iluminacio();
		break;

	// Tecla Iluminació --> Suau
	case GLFW_KEY_F4:
		OnIluminacioSuau();
		// Actualitza el menú Iluminació segons el canvi.
		oIlumina = shortCut_Iluminacio();
		break;

	// Tecla Iluminació --> Reflexió Material --> Emissió?
	case GLFW_KEY_F6:
		OnMaterialEmissio();
		break;

	// Tecla Iluminació --> Reflexió Material -> Ambient?
	case GLFW_KEY_F7:
		OnMaterialAmbient();
		break;

	// Tecla Iluminació --> Reflexió Material -> Difusa?
	case GLFW_KEY_F8:
		OnMaterialDifusa();
		break;

	// Tecla Iluminació --> Reflexió Material -> Especular?
	case GLFW_KEY_F9:
		OnMaterialEspecular();
		break;

	// Tecla Iluminació --> Reflexió Material -> Especular?
	case GLFW_KEY_F10:
		OnMaterialReflmaterial();
		break;

	// Tecla Iluminació --> Textura?.
	case GLFW_KEY_I:
		OnIluminacioTextures();
		break;

	// Tecla Iluminació --> Fitxer Textura?
	case GLFW_KEY_J:
		OnIluminacioTexturaFitxerimatge();
		break;

	case GLFW_KEY_K:
		OnIluminacioTexturaFlagInvertY();
		break;
// ----------- POP UP Llums
	// Tecla Llums --> Llum Ambient?.
	case GLFW_KEY_A:
		llum_ambient = !llum_ambient;
		OnLlumsLlumAmbient();
		break;

	// Tecla Llums --> Llum #0? (+Z)
	case GLFW_KEY_0:
		llumGL[0].encesa = !llumGL[0].encesa;
		OnLlumsLlum0();
		break;

	// Tecla Llums --> Llum #1? (+X)
	case GLFW_KEY_1:
		llumGL[1].encesa = !llumGL[1].encesa;
		OnLlumsLlum1();
		break;

	// Tecla Llums --> Llum #2? (+Y)
	case GLFW_KEY_2:
		llumGL[2].encesa = !llumGL[2].encesa;
		OnLlumsLlum2();
		break;

	// Tecla Llums --> Llum #3? (Z=Y=X)
	case GLFW_KEY_3:
		llumGL[3].encesa = !llumGL[3].encesa;
		OnLlumsLlum3();
		break;

	// Tecla Llums --> Llum #4? (-Z)
	case GLFW_KEY_4:
		llumGL[4].encesa = !llumGL[4].encesa;
		OnLlumsLlum4();
		break;

	// Tecla Llums --> Llum #5?
	case GLFW_KEY_5:
		llumGL[5].encesa = !llumGL[5].encesa;
		OnLlumsLlum5();
		break;

		// Tecla Llums --> Llum #6?
	case GLFW_KEY_6:
		llumGL[6].encesa = !llumGL[6].encesa;
		OnLlumsLlum6();
		break;

	// Tecla Llums --> Llum #7?
	case GLFW_KEY_7:
		llumGL[7].encesa = !llumGL[7].encesa;
		OnLlumsLlum7();
		break;

	// Tecla Shaders --> Flat
	case GLFW_KEY_L:
		if (shader != FLAT_SHADER) {
			OnShaderFlat();
			oShader = shortCut_Shader();
		}
		break;

	// Tecla Shaders --> Gouraud
	case GLFW_KEY_G:
		if (shader != GOURAUD_SHADER) {
			OnShaderGouraud();
			oShader = shortCut_Shader();
		}
		break;

	// Tecla Shaders --> Phong
	case GLFW_KEY_P:
		if (shader != PHONG_SHADER) {
			OnShaderPhong();
			oShader = shortCut_Shader();
		}
		break;

	// Tecla Shaders --> Fitxers Shaders
	case GLFW_KEY_V:
		OnShaderLoadFiles();
		oShader = shortCut_Shader();
		break;

	default:
		break;
	}
}

// Teclat_ColorObjecte: Teclat pels canvis de color de l'objecte per teclat.
void Teclat_ColorObjecte(int key, int action)
{
	const double incr = 0.025f;

	if (action == GLFW_PRESS)
	{	// FRACTAL: Canvi resolució del fractal pe tecles '+' i'-'
		if (objecte == O_FRACTAL)
		{	if (key == GLFW_KEY_KP_SUBTRACT) // Caràcter '-' (ASCII 109)
			{	pas = pas * 2;
				if (pas > 64) pas = 64;
				sw_il = true;
			}
			else if (key == GLFW_KEY_KP_ADD) // Caràcter '+' (ASCII 107)
			{	pas = pas / 2;
				if (pas < 1) pas = 1;
				sw_il = true;
			}
		}
		//	else 
		if (key == GLFW_KEY_DOWN) // Caràcter VK_DOWN
			{	if (fonsR) {	col_obj.r -= incr;
								if (col_obj.r < 0.0) col_obj.r = 0.0;
							}
				if (fonsG) {	col_obj.g -= incr;
								if (col_obj.g < 0.0) col_obj.g = 0.0;
							}
				if (fonsB) {	col_obj.b -= incr;
								if (col_obj.b < 0.0) col_obj.b = 0.0;
							}
			}
		else if (key == GLFW_KEY_UP)
			{	if (fonsR) {
				col_obj.r += incr;
				if (col_obj.r > 1.0) col_obj.r = 1.0;
			}
			if (fonsG) {	col_obj.g += incr;
							if (col_obj.g > 1.0) col_obj.g = 1.0;
						}
			if (fonsB) {	col_obj.b += incr;
							if (col_obj.b > 1.0) col_obj.b = 1.0;
						}
		}
		else if (key == GLFW_KEY_SPACE)
		{
			if ((fonsR) && (fonsG) && (fonsB)) {
				fonsG = false;
				fonsB = false;
			}
			else if ((fonsR) && (fonsG)) {
				fonsG = false;
				fonsB = true;
			}
			else if ((fonsR) && (fonsB)) {
				fonsR = false;
				fonsG = true;
			}
			else if ((fonsG) && (fonsB)) fonsR = true;
			else if (fonsR) {
				fonsR = false;
				fonsG = true;
			}
			else if (fonsG) {
				fonsG = false;
				fonsB = true;
			}
			else if (fonsB) {
				fonsR = true;
				fonsG = true;
				fonsB = false;
			}
		}
		else if (key == GLFW_KEY_O || key == 'o') sw_color = !sw_color;
		else if (key == GLFW_KEY_B || key == 'b') sw_color = !sw_color;
	}
}

// Teclat_ColorFons: Teclat pels canvis del color de fons.
void Teclat_ColorFons(int key, int action)
{
	const double incr = 0.025f;

	if (action == GLFW_PRESS)
	{	// FRACTAL: Canvi resolució del fractal pe tecles '+' i'-'
		if (objecte == O_FRACTAL)
			{	if (key == GLFW_KEY_KP_SUBTRACT) // Caràcter '-' - (ASCII:109)
					{	pas = pas * 2;
						if (pas > 64) pas = 64;
						sw_il = true;
					}
				else if (key == GLFW_KEY_KP_ADD) // Caràcter '+' - (ASCII:107)
					{	pas = pas / 2;
						if (pas < 1) pas = 1;
						sw_il = true;
					}
			}
		//	else 
		if (key == GLFW_KEY_DOWN) 
			{	if (fonsR) {	c_fons.r -= incr;
								if (c_fons.r < 0.0) c_fons.r = 0.0;
							}
				if (fonsG) {	c_fons.g -= incr;
								if (c_fons.g < 0.0) c_fons.g = 0.0;
							}
				if (fonsB) {	c_fons.b -= incr;
								if (c_fons.b < 0.0) c_fons.b = 0.0;
							}
		}
		else if (key == GLFW_KEY_UP) {
			if (fonsR) 
				{	c_fons.r += incr;
					if (c_fons.r > 1.0) c_fons.r = 1.0;
				}
			if (fonsG) {	c_fons.g += incr;
							if (c_fons.g > 1.0) c_fons.g = 1.0;
						}
			if (fonsB) {	c_fons.b += incr;
							if (c_fons.b > 1.0) c_fons.b = 1.0;
						}
			}
		else if (key == GLFW_KEY_SPACE) {
			if ((fonsR) && (fonsG) && (fonsB)) {
				fonsG = false;
				fonsB = false;
			}
			else if ((fonsR) && (fonsG)) {
				fonsG = false;
				fonsB = true;
			}
			else if ((fonsR) && (fonsB)) {
				fonsR = false;
				fonsG = true;
			}
			else if ((fonsG) && (fonsB)) fonsR = true;
			else if (fonsR) {
				fonsR = false;
				fonsG = true;
			}
			else if (fonsG) {
				fonsG = false;
				fonsB = true;
			}
			else if (fonsB) {
				fonsR = true;
				fonsG = true;
				fonsB = false;
			}
		}
		else if (key == GLFW_KEY_O || key == 'o') sw_color = !sw_color;
		else if (key == GLFW_KEY_B || key == 'b') sw_color = !sw_color;
	}
}

// Teclat_Navega: Teclat pels moviments de navegació.
void Teclat_Navega(int key, int action)
{
	GLdouble vdir[3] = { 0, 0, 0 };
	double modul = 0;

	// Entorn VGI: Controls de moviment de navegació
	vdir[0] = n[0] - opvN.x;
	vdir[1] = n[1] - opvN.y;
	vdir[2] = n[2] - opvN.z;
	modul = sqrt(vdir[0] * vdir[0] + vdir[1] * vdir[1] + vdir[2] * vdir[2]);
	vdir[0] = vdir[0] / modul;
	vdir[1] = vdir[1] / modul;
	vdir[2] = vdir[2] / modul;

	if (action == GLFW_PRESS)
	{
		switch (key)
		{	// Tecla cursor amunt
		case GLFW_KEY_UP:
				if (Vis_Polar == POLARZ) 
					{  // (X,Y,Z)
						opvN.x += fact_pan * vdir[0];
						opvN.y += fact_pan * vdir[1];
						n[0] += fact_pan * vdir[0];
					n[1] += fact_pan * vdir[1];
				}
				else if (Vis_Polar == POLARY) 
				{ //(X,Y,Z) --> (Z,X,Y)
					opvN.x += fact_pan * vdir[0];
					opvN.z += fact_pan * vdir[2];
					n[0] += fact_pan * vdir[0];
					n[2] += fact_pan * vdir[2];
			}	
				else if (Vis_Polar == POLARX) 
				{	//(X,Y,Z) --> (Y,Z,X)
					opvN.y += fact_pan * vdir[1];
					opvN.z += fact_pan * vdir[2];
					n[1] += fact_pan * vdir[1];
					n[2] += fact_pan * vdir[2];
				}
			break;

		// Tecla cursor avall
		case GLFW_KEY_DOWN:
			if (Vis_Polar == POLARZ) 
			{ // (X,Y,Z)
				opvN.x -= fact_pan * vdir[0];
				opvN.y -= fact_pan * vdir[1];
				n[0] -= fact_pan * vdir[0];
				n[1] -= fact_pan * vdir[1];
			}
			else if (Vis_Polar == POLARY) 
			{ //(X,Y,Z) --> (Z,X,Y)
				opvN.x -= fact_pan * vdir[0];
				opvN.z -= fact_pan * vdir[2];
				n[0] -= fact_pan * vdir[0];
				n[2] -= fact_pan * vdir[2];
			}
			else if (Vis_Polar == POLARX) 
			{ //(X,Y,Z) --> (Y,Z,X)
				opvN.y -= fact_pan * vdir[1];
				opvN.z -= fact_pan * vdir[2];
				n[1] -= fact_pan * vdir[1];
				n[2] -= fact_pan * vdir[2];
			}
			break;

		// Tecla cursor esquerra
		case GLFW_KEY_LEFT:
			angleZ += fact_pan;
			if (Vis_Polar == POLARZ) 
			{ // (X,Y,Z)
				n[0] = vdir[0]; // n[0] - opvN.x;
				n[1] = vdir[1]; // n[1] - opvN.y;
				n[0] = n[0] * cos(angleZ * PI / 180) - n[1] * sin(angleZ * PI / 180.0);
				n[1] = n[0] * sin(angleZ * PI / 180) + n[1] * cos(angleZ * PI / 180.0);
				n[0] = n[0] + opvN.x;
				n[1] = n[1] + opvN.y;
			}
			else if (Vis_Polar == POLARY) 
			{ //(X,Y,Z) --> (Z,X,Y)
				n[2] = vdir[2]; // n[2] - opvN.z;
				n[0] = vdir[0]; // n[0] - opvN.x;
				n[2] = n[2] * cos(angleZ * PI / 180.0) - n[0] * sin(angleZ * PI / 180.0);
				n[0] = n[2] * sin(angleZ * PI / 180.0) + n[0] * cos(angleZ * PI / 180.0);
				n[2] = n[2] + opvN.z;
				n[0] = n[0] + opvN.x;
			}
			else if (Vis_Polar == POLARX) 
			{ //(X,Y,Z) --> (Y,Z,X)
				n[1] = vdir[1]; // n[1] - opvN.y;
				n[2] = vdir[2]; // n[2] - opvN.z;
				n[1] = n[1] * cos(angleZ * PI / 180.0) - n[2] * sin(angleZ * PI / 180.0);
				n[2] = n[1] * sin(angleZ * PI / 180.0) + n[2] * cos(angleZ * PI / 180.0);
				n[1] = n[1] + opvN.y;
				n[2] = n[2] + opvN.z;
			}
			break;

		// Tecla cursor dret
		case GLFW_KEY_RIGHT:
			angleZ = 360 - fact_pan;
			if (Vis_Polar == POLARZ) 
			{ // (X,Y,Z)
				n[0] = vdir[0]; // n[0] - opvN.x;
				n[1] = vdir[1]; // n[1] - opvN.y;
				n[0] = n[0] * cos(angleZ * PI / 180.0) - n[1] * sin(angleZ * PI / 180.0);
				n[1] = n[0] * sin(angleZ * PI / 180.0) + n[1] * cos(angleZ * PI / 180.0);
				n[0] = n[0] + opvN.x;
				n[1] = n[1] + opvN.y;
			}
			else if (Vis_Polar == POLARY) 
			{ //(X,Y,Z) --> (Z,X,Y)
				n[2] = vdir[2]; // n[2] - opvN.z;
				n[0] = vdir[0]; // n[0] - opvN.x;
				n[2] = n[2] * cos(angleZ * PI
					/ 180.0) - n[0] * sin(angleZ * PI / 180.0);
				n[0] = n[2] * sin(angleZ * PI / 180.0) + n[0] * cos(angleZ * PI / 180.0);
				n[2] = n[2] + opvN.z;
				n[0] = n[0] + opvN.x;
			}
			else if (Vis_Polar == POLARX) 
			{ //(X,Y,Z) --> (Y,Z,X)
				n[1] = vdir[1]; // n[1] - opvN.y;
				n[2] = vdir[2]; // n[2] - opvN.z;
				n[1] = n[1] * cos(angleZ * PI / 180.0) - n[2] * sin(angleZ * PI / 180.0);
				n[2] = n[1] * sin(angleZ * PI / 180.0) + n[2] * cos(angleZ * PI / 180.0);
				n[1] = n[1] + opvN.y;
				n[2] = n[2] + opvN.z;
			}
			break;

		// Tecla Inicio ('7')
		case GLFW_KEY_KP_7: //GLFW_KEY_HOME:
			if (Vis_Polar == POLARZ) {	opvN.z += fact_pan;
										n[2] += fact_pan;
									}
			else if (Vis_Polar == POLARY) {	opvN.y += fact_pan;
											n[1] += fact_pan;
											}
			else if (Vis_Polar == POLARX) {	opvN.x += fact_pan;
											n[0] += fact_pan;
										}
			break;

		// Tecla Fin
		case GLFW_KEY_END:
			if (Vis_Polar == POLARZ) {	opvN.z -= fact_pan;
										n[2] -= fact_pan;
									}
			else if (Vis_Polar == POLARY) {	opvN.y -= fact_pan;
											n[1] -= fact_pan;
											}
			else if (Vis_Polar == POLARX) {	opvN.x -= fact_pan;
											n[0] -= fact_pan;
										}
			break;

			// Tecla PgUp ('9)
		case GLFW_KEY_KP_9: //GLFW_KEY_PAGE_UP:
			fact_pan /= 2;
			if (fact_pan < 0.125) fact_pan = 0.125;
			break;

			// Tecla PgDown ('3')
		case GLFW_KEY_KP_3: //GLFW_KEY_PAGE_DOWN:
			fact_pan *= 2;
			if (fact_pan > 2048) fact_pan = 2048;
			break;

		default:
			if (transf)
				{	if (rota) Teclat_TransRota(key, action);
						else if (trasl) Teclat_TransTraslada(key, action);
								else if (escal) Teclat_TransEscala(key, action);
				}
			/*else if (camera == CAM_NAVEGA) Teclat_Navega(key, action);
					else if (!sw_color) Teclat_ColorFons(key, action);
								else Teclat_ColorObjecte(key, action);*/
			break;
		}
	}
}

// Teclat_Pan: Teclat pels moviments de Pan.
void Teclat_Pan(int key, int action)
{
	if (action == GLFW_PRESS)
	{	switch (key)
		{
		// Tecla cursor amunt
		case GLFW_KEY_UP:
			tr_cpv.y -= fact_pan;
			if (tr_cpv.y < -100000) tr_cpv.y = 100000;
			break;

		// Tecla cursor avall
		case GLFW_KEY_DOWN:
			tr_cpv.y += fact_pan;
			if (tr_cpv.y > 100000) tr_cpv.y = 100000;
			break;

		// Tecla cursor esquerra
		case GLFW_KEY_LEFT:
			tr_cpv.x += fact_pan;
			if (tr_cpv.x > 100000.0) tr_cpv.x = 100000.0;
			break;

		// Tecla cursor dret
		case GLFW_KEY_RIGHT:
			tr_cpv.x -= fact_pan;
			if (tr_cpv.x < -100000.0) tr_cpv.x = 100000;
			break;

		// Tecla PgUp ('9')
		case GLFW_KEY_KP_9: //GLFW_KEY_PAGE_UP.0
			fact_pan /= 2.0;
			if (fact_pan < 0.125) fact_pan = 0.125;
			break;

		// Tecla PgDown ('3')
		case GLFW_KEY_KP_3: //GLFW_KEY_PAGE_DOWN:
			fact_pan *= 2.0;
			if (fact_pan > 2048.0) fact_pan = 2048.0;
			break;

		// Tecla Insert: Fixar el desplaçament de pantalla (pan)
		case GLFW_KEY_INSERT:
			// Acumular desplaçaments de pan (tr_cpv) en variables fixes (tr_cpvF).
			tr_cpvF.x += tr_cpv.x;		tr_cpv.x = 0.0;
			if (tr_cpvF.x > 100000.0)	tr_cpvF.x = 100000.0;
			tr_cpvF.y += tr_cpv.y;		tr_cpv.y = 0.0;
			if (tr_cpvF.y > 100000.0)	tr_cpvF.y = 100000.0;
			tr_cpvF.z += tr_cpv.z;		tr_cpv.z = 0.0;
			if (tr_cpvF.z > 100000.0)	tr_cpvF.z = 100000.0;
			break;

		// Tecla Delete: Inicialitzar el desplaçament de pantalla (pan)
		case GLFW_KEY_DELETE:
			// Inicialitzar els valors de pan tant de la variable tr_cpv com de la tr_cpvF.
			tr_cpv.x = 0.0;			tr_cpv.y = 0.0;			tr_cpv.z = 0.0;
			tr_cpvF.x = 0.0;		tr_cpvF.y = 0.0;		tr_cpvF.z = 0.0;
			break;

		default:
			if (transf)
				{	if (rota) Teclat_TransRota(key, action);
						else if (trasl) Teclat_TransTraslada(key, action);
								else if (escal) Teclat_TransEscala(key, action);
				}																				
			else if (camera == CAM_NAVEGA) Teclat_Navega(key, action);
			else if (!sw_color) Teclat_ColorFons(key, action);
			else Teclat_ColorObjecte(key, action);
			break;
		}
	}
}

// Teclat_TransEscala: Teclat pels canvis del valor d'escala per X,Y,Z.
void Teclat_TransEscala(int key, int action)
{
	if (action == GLFW_PRESS)
	{	switch (key)
		{
// Modificar vector d'Escalatge per teclat (actiu amb Escalat únicament)
		// Tecla '+' (augmentar tot l'escalat)
		case GLFW_KEY_KP_ADD:
			TG.VScal.x = TG.VScal.x * 2.0;
			if (TG.VScal.x > 8192.0) TG.VScal.x = 8192.0;
			TG.VScal.y = TG.VScal.y * 2.0;
			if (TG.VScal.y > 8192.0) TG.VScal.y = 8192.0;
			TG.VScal.z = TG.VScal.z * 2.0;
			if (TG.VScal.z > 8192.0) TG.VScal.z = 8192.0;
			break;

		// Tecla '-' (disminuir tot l'escalat)
		case GLFW_KEY_KP_SUBTRACT:
			TG.VScal.x = TG.VScal.x / 2.0;
			if (TG.VScal.x < 0.125) TG.VScal.x = 0.125;
			TG.VScal.y = TG.VScal.y / 2.0;
			if (TG.VScal.y < 0.125) TG.VScal.y = 0.125;
			TG.VScal.z = TG.VScal.z / 2.0;
			if (TG.VScal.z < 0.125) TG.VScal.z = 0.125;
			break;

		// Tecla cursor amunt ('8')
		case GLFW_KEY_UP:
			TG.VScal.x = TG.VScal.x * 2.0;
			if (TG.VScal.x > 8192.0) TG.VScal.x = 8192.0;
			break;

		// Tecla cursor avall ('2')
		case GLFW_KEY_DOWN:
			TG.VScal.x = TG.VScal.x / 2.0;
			if (TG.VScal.x < 0.25) TG.VScal.x = 0.25;
			break;

		// Tecla cursor esquerra ('4')
		case GLFW_KEY_LEFT:
			TG.VScal.y = TG.VScal.y / 2.0;
			if (TG.VScal.y < 0.125) TG.VScal.y = 0.125;
			break;

		// Tecla cursor dret ('6')
		case GLFW_KEY_RIGHT:
			TG.VScal.y = TG.VScal.y * 2.0;
			if (TG.VScal.y > 8192.0) TG.VScal.y = 8192.0;
			break;

		// Tecla HOME ('7')
		case GLFW_KEY_KP_7: //GLFW_KEY_HOME:
			TG.VScal.z = TG.VScal.z * 2.0;
			if (TG.VScal.z > 8192.0) TG.VScal.z = 8192.0;
			break;

		// Tecla END ('1')
		case GLFW_KEY_KP_1: //GLFW_KEY_END:
			TG.VScal.z = TG.VScal.z / 2.0;
			if (TG.VScal.z < 0.125) TG.VScal.z = 0.125;
			break;

		// Tecla INSERT
		case GLFW_KEY_INSERT:
			// Acumular transformacions Geomètriques (variable TG) i de pan en variables fixes (variable TGF)
			TGF.VScal.x *= TG.VScal.x;	TGF.VScal.y *= TG.VScal.y; TGF.VScal.z *= TG.VScal.z;
			if (TGF.VScal.x > 8192.0) TGF.VScal.x = 8192.0;		if (TG.VScal.x < 0.125) TG.VScal.x = 0.125;
			if (TGF.VScal.y > 8192.0) TGF.VScal.y = 8192.0;		if (TG.VScal.y < 0.125) TG.VScal.y = 0.125;
			if (TGF.VScal.z > 8192.0) TGF.VScal.z = 8192.0;		if (TG.VScal.z < 0.125) TG.VScal.z = 0.125;
			TG.VScal.x = 1.0;				TG.VScal.y = 1.0;		TG.VScal.z = 1.0;
			TGF.VRota.x += TG.VRota.x;		TGF.VRota.y += TG.VRota.y;	TGF.VRota.z += TG.VRota.z;
			if (TGF.VRota.x >= 360.0) TGF.VRota.x -= 360.0; 		if (TGF.VRota.x < 0) TGF.VRota.x += 360.0;
			if (TGF.VRota.y >= 360.0) TGF.VRota.y -= 360.0;		if (TGF.VRota.y < 0.0) TGF.VRota.y += 360.0;
			if (TGF.VRota.z >= 360.0) TGF.VRota.z -= 360.0;		if (TGF.VRota.z < 0.0) TGF.VRota.z += 360.0;
			TG.VRota.x = 0.0;				TG.VRota.y = 0.0;			TG.VRota.z = 0.0;
			TGF.VTras.x += TG.VTras.x;		TGF.VTras.y += TG.VTras.y;	TGF.VTras.z += TG.VTras.z;
			if (TGF.VTras.x < -100000.0) TGF.VTras.x = 100000.0;	if (TGF.VTras.x > 10000.0) TGF.VTras.x = 100000.0;
			if (TGF.VTras.y < -100000.0) TGF.VTras.y = 100000.0;	if (TGF.VTras.y > 10000.0) TGF.VTras.y = 100000.0;
			if (TGF.VTras.z < -100000.0) TGF.VTras.z = 100000.0;	if (TGF.VTras.z > 10000.0) TGF.VTras.z = 100000.0;
			TG.VTras.x = 0.0;				TG.VTras.y = 0.0;		TG.VTras.z = 0.0;
			break;

		// Tecla Delete: Esborrar les Transformacions Geomètriques Calculades
		case GLFW_KEY_DELETE:
			// Inicialitzar els valors de transformacions Geomètriques i de pan en variables fixes.
			TGF.VScal.x = 1.0;		TGF.VScal.y = 1.0;;		TGF.VScal.z = 1.0;
			TG.VScal.x = 1.0;		TG.VScal.y = 1.0;		TG.VScal.z = 1.0;
			TGF.VRota.x = 0.0;		TGF.VRota.y = 0.0;		TGF.VRota.z = 0.0;
			TG.VRota.x = 0.0;		TG.VRota.y = 0.0;		TG.VRota.z = 0.0;
			TGF.VTras.x = 0.0;		TGF.VTras.y = 0.0;		TGF.VTras.z = 0.0;
			TG.VTras.x = 0.0;		TG.VTras.y = 0.0;		TG.VTras.z = 0.0;
			break;

		default:
			if (pan) Teclat_Pan(key, action);
				else if (camera == CAM_NAVEGA) Teclat_Navega(key, action);
						else if (!sw_color) Teclat_ColorFons(key, action);
								else Teclat_ColorObjecte(key, action);
			break;
		}
	}
}

// Teclat_TransRota: Teclat pels canvis del valor del vector de l'angle de rotació per X,Y,Z.
void Teclat_TransRota(int key, int action)
{
	if (action == GLFW_PRESS)
	{	switch (key)
		{
		// Tecla cursor amunt ('8')
		case GLFW_KEY_UP:
			TG.VRota.x += fact_Rota;
			if (TG.VRota.x >= 360.0) TG.VRota.x -= 360.0;
			// Entorn VGI. QUATERNIONS: Definir nou angle rotació X com a quaternió (angleQ) i multiplicar-lo per qT --> qT=angleQ*qT. 
			if (rotaQ) {
/*							angle += nRepCnt * fact_Rota;
							while (angle >= 360.0f) angle -= 360.0f;
							EixAngleToQuat(1.0, 0.0, 0.0, angle, angleQ);
							QuatMul(angleQ, qT, qT);
							QuatToMatrix(qT, QMatrix);
*/
			}
			break;

		// Tecla cursor avall ('2')
		case GLFW_KEY_DOWN:
			TG.VRota.x -= fact_Rota;
			if (TG.VRota.x < 1.0) TG.VRota.x += 360.0;
			// Entorn VGI. QUATERNIONS: Definir nou angle rotació X com a quaternió (angleQ) i multiplicar-lo per qT --> qT=angleQ*qT.
			if (rotaQ) { 	// TO DO. PENDENT DE FER.

						}
			break;

		// Tecla cursor esquerra ('4')
		case GLFW_KEY_LEFT:
			TG.VRota.y -= fact_Rota;
			if (TG.VRota.y < 1.0) TG.VRota.y += 360.0;
			// Entorn VGI. QUATERNIONS: Definir nou angle rotació X com a quaternió (angleQ) i multiplicar-lo per qT --> qT=angleQ*qT.
			if (rotaQ) {	// TO DO. PENDENT DE FER.

						}
			break;

		// Tecla cursor dret ('6')
		case GLFW_KEY_RIGHT:
			TG.VRota.y += fact_Rota;
			if (TG.VRota.y >= 360.0) TG.VRota.y -= 360.0;
			// Entorn VGI. QUATERNIONS: Definir nou angle rotació X com a quaternió (angleQ) i multiplicar-lo per qT --> qT=angleQ*qT.
			if (rotaQ) {	// TO DO. PENDENT DE FER.

						}
			break;

		// Tecla HOME ('7')
		case GLFW_KEY_KP_7: //GLFW_KEY_HOME:
			TG.VRota.z += fact_Rota;
			if (TG.VRota.z >= 360.0) TG.VRota.z -= 360.0;
			// Entorn VGI. QUATERNIONS: Definir nou angle rotació X com a quaternió (angleQ) i multiplicar-lo per qT --> qT=angleQ*qT.
			if (rotaQ) {	// TO DO. PENDENT DE FER.

					}
			break;

		// Tecla END ('1')
		case GLFW_KEY_KP_1: //GLFW_KEY_END:
			TG.VRota.z -= fact_Rota;
			if (TG.VRota.z < 1.0) TG.VRota.z += 360.0;
			// Entorn VGI. QUATERNIONS: Definir nou angle rotació X com a quaternió (angleQ) i multiplicar-lo per qT --> qT=angleQ*qT.
			if (rotaQ) {	// TO DO. PENDENT DE FER.

						}
			break;

		// Tecla PgUp ('9')
		case GLFW_KEY_KP_9: //GLFW_KEY_PAGE_UP:
			fact_Rota /= 2;
			if (fact_Rota < 1.0) fact_Rota = 1.0;
			break;

		// Tecla PgDown ('3')
		case GLFW_KEY_KP_3:// GLFW_KEY_PAGE_DOWN:
			fact_Rota *= 2.0;
			if (fact_Rota > 90) fact_Rota = 90;
			break;

		// Entorn VGI. QUATERNIONS: Tecla F5 (Activar animació cossos rígids per QUATERNIONS) -> PENDENT DE FER
		case GLFW_KEY_F5:
			if (rotaQ) { // TO DO. PENDENT DE FER

					}
			break;

		// Entorn VGI. QUATERNIONS: Tecla Pausa (Activar/parar animació cossos rígids per QUATERNIONS) -> PENDENT DE FER
		case GLFW_KEY_PAUSE:
			if (rotaQ) {	// TO DO. PENDENT DE FER

						}
			break;

		// Entorn VGI. QUATERNIONS: Tecla End (Para animació i inicialitzar vector Keyframes - PENDENT DE FER
		case GLFW_KEY_END:
			if (rotaQ) {	// TO DO. PENDENT DE FER

						}
			break;

		// Entorn VGI. QUATERNIONS:  Tecla F3 (Accelerar animació cossos rígids per QUATERNIONS) - PENDENT DE FER (ADDICIONAL)
		case GLFW_KEY_F3:
			if (rotaQ) {	// TO DO. PENDENT DE FER (ADDICIONAL)

				}
			break;

		// Entorn VGI. QUATERNIONS: Tecla F4 (Desaccelerar animació cossos rígids per QUATERNIONS) - PENDENT DE FER (ADDICIONAL)
		case GLFW_KEY_F4:
			if (rotaQ) {	// TO Do. PENDENT DE FER (ADDICIONAL)

					}
			break;

		// Tecla Insert: Acumular transformacions Geomètriques (variable TG) i de pan en variables fixes (variable TGF)
		case GLFW_KEY_INSERT:
			// Entorn VGI. QUATERNIONS: Fixar els Quaternions en els keyframes (animació cossos rígids per QUATERNIONS)
			if (rotaQ) { // TO DO. PENDENT DE FER

			}
			else {
				TGF.VScal.x *= TG.VScal.x;		TGF.VScal.y *= TG.VScal.y; TGF.VScal.z *= TG.VScal.z;
				if (TGF.VScal.x > 8192.0) TGF.VScal.x = 8192.0;			if (TG.VScal.x < 0.125) TG.VScal.x = 0.125;
				if (TGF.VScal.y > 8192.0) TGF.VScal.y = 8192.0;			if (TG.VScal.y < 0.125) TG.VScal.y = 0.125;
				if (TGF.VScal.z > 8192.0) TGF.VScal.z = 8192.0;			if (TG.VScal.z < 0.125) TG.VScal.z = 0.125;
				TG.VScal.x = 1.0;				TG.VScal.y = 1.0;			TG.VScal.z = 1.0;
				TGF.VRota.x += TG.VRota.x;		TGF.VRota.y += TG.VRota.y;	TGF.VRota.z += TG.VRota.z;
				if (TGF.VRota.x >= 360.0) TGF.VRota.x -= 360.0; 		if (TGF.VRota.x < 0.0) TGF.VRota.x += 360.0;
				if (TGF.VRota.y >= 360.0) TGF.VRota.y -= 360.0;			if (TGF.VRota.y < 0.0) TGF.VRota.y += 360.0;
				if (TGF.VRota.z >= 360.0) TGF.VRota.z -= 360.0;			if (TGF.VRota.z < 0.0) TGF.VRota.z += 360.0;
				TG.VRota.x = 0.0;				TG.VRota.y = 0.0;			TG.VRota.z = 0.0;
				TGF.VTras.x += TG.VTras.x;		TGF.VTras.y += TG.VTras.y;	TGF.VTras.z += TG.VTras.z;
				if (TGF.VTras.x < -100000.0) TGF.VTras.x = 100000.0;	if (TGF.VTras.x > 10000.0) TGF.VTras.x = 100000.0;
				if (TGF.VTras.y < -100000.0) TGF.VTras.y = 100000.0;	if (TGF.VTras.y > 10000.0) TGF.VTras.y = 100000.0;
				if (TGF.VTras.z < -100000.0) TGF.VTras.z = 100000.0;	if (TGF.VTras.z > 10000.0) TGF.VTras.z = 100000.0;
				TG.VTras.x = 0.0;				TG.VTras.y = 0.0;			TG.VTras.z = 0.0;
				}
			break;

		// Tecla Delete: Esborrar les Transformacions Geomètriques Calculades
		case GLFW_KEY_DELETE:
			// Inicialitzar els valors de transformacions Geomètriques i de pan en variables fixes.
			TGF.VScal.x = 1.0;		TGF.VScal.y = 1.0;;	TGF.VScal.z = 1.0;
			TG.VScal.x = 1.0;		TG.VScal.y = 1.0;		TG.VScal.z = 1.0;
			TGF.VRota.x = 0.0;		TGF.VRota.y = 0.0;	TGF.VRota.z = 0.0;
			TG.VRota.x = 0.0;		TG.VRota.y = 0.0;		TG.VRota.z = 0.0;
			TGF.VTras.x = 0.0;		TGF.VTras.y = 0.0;	TGF.VTras.z = 0.0;
			TG.VTras.x = 0.0;		TG.VTras.y = 0.0;		TG.VTras.z = 0.0;
			break;

		// Modificar vector d'Escalatge per teclat
		// Tecla '+' (augmentar escalat)
		case GLFW_KEY_KP_ADD:
			if (escal) {
				TG.VScal.x = TG.VScal.x * 2.0;
				if (TG.VScal.x > 8192.0) TG.VScal.x = 8192.0;
				TG.VScal.y = TG.VScal.y * 2.0;
				if (TG.VScal.y > 8192.0) TG.VScal.y = 8192.0;
				TG.VScal.z = TG.VScal.z * 2.0;
				if (TG.VScal.z > 8192.0) TG.VScal.z = 8192.0;
				}
			break;

		// Tecla '-' (disminuir escalat)
		case GLFW_KEY_KP_SUBTRACT:
			if (escal) {
				TG.VScal.x = TG.VScal.x / 2.0;
				if (TG.VScal.x < 0.125) TG.VScal.x = 0.125;
				TG.VScal.y = TG.VScal.y / 2.0;
				if (TG.VScal.y < 0.125) TG.VScal.y = 0.125;
				TG.VScal.z = TG.VScal.z / 2.0;
				if (TG.VScal.z < 0.125) TG.VScal.z = 0.125;
				}
			break;

		// Tecla Espaiador
		case GLFW_KEY_SPACE:
			rota = !rota;
			trasl = !trasl;
			break;

		default:
			if (pan) Teclat_Pan(key, action);
				else if (camera == CAM_NAVEGA) Teclat_Navega(key, action);
						else if (!sw_color) Teclat_ColorFons(key, action);
								else Teclat_ColorObjecte(key, action);
			break;
		}
	}
}

// Teclat_TransTraslada: Teclat pels canvis del valor de traslació per X,Y,Z.
void Teclat_TransTraslada(int key, int action)
{
	GLdouble vdir[3] = { 0, 0, 0 };
	double modul = 0;

	if (action == GLFW_PRESS)
	{	switch (key)
		{
		// Tecla cursor amunt ('8') - (ASCII:104)
		case GLFW_KEY_UP:
			TG.VTras.x -= fact_Tras;
			if (TG.VTras.x < -100000.0) TG.VTras.x = 100000.0;
			break;

		// Tecla cursor avall ('2') - (ASCII:98)
		case GLFW_KEY_DOWN:
			TG.VTras.x += fact_Tras;
			if (TG.VTras.x > 10000.0) TG.VTras.x = 100000.0;
			break;

		// Tecla cursor esquerra ('4') - (ASCII:100)
		case GLFW_KEY_LEFT:
			TG.VTras.y -= fact_Tras;
			if (TG.VTras.y < -100000.0) TG.VTras.y = -100000.0;
			break;

		// Tecla cursor dret ('6') - (ASCII:102)
		case GLFW_KEY_RIGHT:
			TG.VTras.y += fact_Tras;
			if (TG.VTras.y > 100000.0) TG.VTras.y = 100000.0;
			break;

		// Tecla HOME ('7') - (ASCII:103)
		case GLFW_KEY_KP_7: //GLFW_KEY_HOME:
			TG.VTras.z += fact_Tras;
			if (TG.VTras.z > 100000.0) TG.VTras.z = 100000.0;
			break;

		// Tecla END ('1') - (ASCII:10#)
		case GLFW_KEY_KP_1: //GLFW_KEY_END:
			TG.VTras.z -= fact_Tras;
			if (TG.VTras.z < -100000.0) TG.VTras.z = -100000.0;
			break;

		// Tecla PgUp ('9') - (ASCII:105)
		case GLFW_KEY_KP_9: //GLFW_KEY_PAGE_UP:
			fact_Tras /= 2.0;
			if (fact_Tras < 1.0) fact_Tras = 1.0;
			break;

		// Tecla PgDown ('3') - (ASCII:99)
		case GLFW_KEY_KP_3: //GLFW_KEY_PAGE_DOWN:
			fact_Tras *= 2;
			if (fact_Tras > 100000.0) fact_Tras = 100000.0;
			break;

		// Modificar vector d'Escalatge per teclat (actiu amb Traslació)
		// Tecla '+' (augmentar escalat)
		case GLFW_KEY_KP_ADD:
			if (escal) {
				TG.VScal.x = TG.VScal.x * 2.0;
				if (TG.VScal.x > 8192.0) TG.VScal.x = 8192.0;
				TG.VScal.y = TG.VScal.y * 2.0;
				if (TG.VScal.y > 8192.0) TG.VScal.y = 8192.0;
				TG.VScal.z = TG.VScal.z * 2.0;
				if (TG.VScal.z > 8192.0) TG.VScal.z = 8192.0;
			}
			break;

		// Tecla '-' (disminuir escalat) . (ASCII:109)
		case GLFW_KEY_KP_SUBTRACT:
			if (escal) {
				TG.VScal.x = TG.VScal.x / 2.0;
				if (TG.VScal.x < 0.125) TG.VScal.x = 0.125;
				TG.VScal.y = TG.VScal.y / 2.0;
				if (TG.VScal.y < 0.125) TG.VScal.y = 0.125;
				TG.VScal.z = TG.VScal.z / 2.0;
				if (TG.VScal.z < 0.125) TG.VScal.z = 0.125;
			}
			break;

		// Tecla INSERT
		case GLFW_KEY_INSERT:
			// Acumular transformacions Geomètriques (variable TG) i de pan en variables fixes (variable TGF)
			TGF.VScal.x *= TG.VScal.x;		TGF.VScal.y *= TG.VScal.y; TGF.VScal.z *= TG.VScal.z;
			if (TGF.VScal.x > 8192.0) TGF.VScal.x = 8192.0;			if (TG.VScal.x < 0.125) TG.VScal.x = 0.125;
			if (TGF.VScal.y > 8192.0) TGF.VScal.y = 8192.0;			if (TG.VScal.y < 0.125) TG.VScal.y = 0.125;
			if (TGF.VScal.z > 8192.0) TGF.VScal.z = 8192.0;			if (TG.VScal.z < 0.125) TG.VScal.z = 0.125;
			TG.VScal.x = 1.0;				TG.VScal.y = 1.0;			TG.VScal.z = 1.0;
			TGF.VRota.x += TG.VRota.x;		TGF.VRota.y += TG.VRota.y;	TGF.VRota.z += TG.VRota.z;
			if (TGF.VRota.x >= 360.0) TGF.VRota.x -= 360.0; 		if (TGF.VRota.x < 0) TGF.VRota.x += 360.0;
			if (TGF.VRota.y >= 360.0) TGF.VRota.y -= 360.0;			if (TGF.VRota.y < 0.0) TGF.VRota.y += 360.0;
			if (TGF.VRota.z >= 360.0) TGF.VRota.z -= 360.0;			if (TGF.VRota.z < 0.0) TGF.VRota.z += 360,0;
			TG.VRota.x = 0.0;				TG.VRota.y = 0.0;			TG.VRota.z = 0.0;
			TGF.VTras.x += TG.VTras.x;		TGF.VTras.y += TG.VTras.y; TGF.VTras.z += TG.VTras.z;
			if (TGF.VTras.x < -100000.0) TGF.VTras.x = 100000.0;	if (TGF.VTras.x > 10000.0) TGF.VTras.x = 100000.0;
			if (TGF.VTras.y < -100000.0) TGF.VTras.y = 100000.0;	if (TGF.VTras.y > 10000.0) TGF.VTras.y = 100000.0;
			if (TGF.VTras.z < -100000.0) TGF.VTras.z = 100000.0;	if (TGF.VTras.z > 10000.0) TGF.VTras.z = 100000.0;
			TG.VTras.x = 0.0;				TG.VTras.y = 0.0;			TG.VTras.z = 0.0;
			break;

		// Tecla Delete: Esborrar les Transformacions Geomètriques Calculades
		case GLFW_KEY_DELETE:
			// Inicialitzar els valors de transformacions Geomètriques i de pan en variables fixes.
			TGF.VScal.x = 1.0;		TGF.VScal.y = 1.0;;		TGF.VScal.z = 1.0;
			TG.VScal.x = 1.0;		TG.VScal.y = 1.0;		TG.VScal.z = 1.0;
			TGF.VRota.x = 0.0;		TGF.VRota.y = 0.0;		TGF.VRota.z = 0.0;
			TG.VRota.x = 0.0;		TG.VRota.y = 0.0;		TG.VRota.z = 0.0;
			TGF.VTras.x = 0.0;		TGF.VTras.y = 0.0;		TGF.VTras.z = 0.0;
			TG.VTras.x = 0.0;		TG.VTras.y = 0.0;		TG.VTras.z = 0.0;
			break;

		// Tecla Espaiador
		case GLFW_KEY_SPACE:
			rota = !rota;
			trasl = !trasl;
			break;

		default:
			if (pan) Teclat_Pan(key, action);
				else if (camera == CAM_NAVEGA) Teclat_Navega(key, action);
						else if (!sw_color) Teclat_ColorFons(key, action);
								else Teclat_ColorObjecte(key, action);
			break;
		}
	}
}

// Teclat_Grid: Teclat pels desplaçaments dels gridXY, gridXZ i gridYZ.
void Teclat_Grid(int key, int action)
{
	if (action == GLFW_PRESS)
	{	switch (key)
		{
		// Key Up cursor ('8') - (ASCII:104)
		case GLFW_KEY_UP:
			hgrid.x -= PAS_GRID;
			break;

		// Key Down cursor ('2') - (ASCII:98)
		case GLFW_KEY_DOWN:
			hgrid.x += PAS_GRID;
			break;

		// Key Left cursor ('4') - (ASCII:100)
		case GLFW_KEY_LEFT:
			hgrid.y -= PAS_GRID;
			break;

		// Key Right cursor ('6') - (ASCII:102)
		case GLFW_KEY_RIGHT:
			hgrid.y += PAS_GRID;
			break;

			// Key HOME ('7') - (ASCII:103)
		case GLFW_KEY_HOME:
			hgrid.z += PAS_GRID;
			break;

			// Key END ('1') - (ASCII:97)
		case GLFW_KEY_END:
			hgrid.z -= PAS_GRID;
			break;

		// Key grid ('G') - (ASCII:'G')
		case GLFW_KEY_G:
			sw_grid = !sw_grid;
			break;

/*
			// Key grid ('g')
			case 'g':
			sw_grid = !sw_grid;
			break;
*/

		default:
			if (transf)
			{	if (rota) Teclat_TransRota(key, action);
					else if (trasl) Teclat_TransTraslada(key, action);
						else if (escal) Teclat_TransEscala(key, action);
			}
			else if (pan) Teclat_Pan(key, action);
					else if (camera == CAM_NAVEGA) Teclat_Navega(key, action);
							else if (!sw_color) Teclat_ColorFons(key, action);
									else Teclat_ColorObjecte(key, action);
			break;
		}
	}
}

// Teclat_PasCorbes: Teclat per incrementar-Decrementar el pas de dibuix de les corbes (pas_CS).
void Teclat_PasCorbes(int key, int action)
{
	if (action == GLFW_PRESS)
	{	switch (key)
		{
		// Tecla '+' (incrementar pas_CS)
		case GLFW_KEY_KP_ADD:
			pas_CS = pas_CS * 2.0;
			if (pas_CS > 0.5) pas_CS = 0.5;
			if (objecte == C_BSPLINE) {
				deleteVAOList(CRV_BSPLINE);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
				Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
			}
			else if (objecte == C_BEZIER) {
				deleteVAOList(CRV_BEZIER);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_BEZIER, load_Bezier_Curve_VAO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
				Set_VAOList(CRV_BEZIER, load_Bezier_Curve_EBO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
			}
			else if (objecte == C_LEMNISCATA) {
				deleteVAOList(CRV_LEMNISCATA3D);		//Eliminar VAO anterior.
				Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_EBO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.
			}
			else if (objecte == C_HERMITTE) {
				deleteVAOList(CRV_HERMITTE);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_HERMITTE, load_Hermitte_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_HERMITTE.
				Set_VAOList(CRV_HERMITTE, load_Hermitte_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_HERMITTE.
			}
			else if (objecte == C_CATMULL_ROM) {
				deleteVAOList(CRV_CATMULL_ROM);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_CATMULL_ROM, load_CatmullRom_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_CATMULL_ROM.
				Set_VAOList(CRV_CATMULL_ROM, load_CatmullRom_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_CATMULL_ROM.
			}
			break;

		// Tecla '-' (decrementar pas_CS)
		case GLFW_KEY_KP_SUBTRACT:
			pas_CS = pas_CS / 2;
			if (pas_CS < 0.0125) pas_CS = 0.00625;
			if (objecte == C_BSPLINE) {
				deleteVAOList(CRV_BSPLINE);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
				Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
			}
			else if (objecte == C_BEZIER) {
				deleteVAOList(CRV_BEZIER);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_BEZIER, load_Bezier_Curve_VAO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
				Set_VAOList(CRV_BEZIER, load_Bezier_Curve_EBO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
			}
			else if (objecte == C_LEMNISCATA) {
				deleteVAOList(CRV_LEMNISCATA3D);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_VAO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.
				Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_EBO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.	
			}
			else if (objecte == C_HERMITTE) {
				deleteVAOList(CRV_HERMITTE);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_HERMITTE, load_Hermitte_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_HERMITTE.
				Set_VAOList(CRV_HERMITTE, load_Hermitte_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_HERMITTE.
			}
			else if (objecte == C_CATMULL_ROM) {
				deleteVAOList(CRV_CATMULL_ROM);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_CATMULL_ROM, load_CatmullRom_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_CATMULL_ROM.
				Set_VAOList(CRV_CATMULL_ROM, load_CatmullRom_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_CATMULL_ROM.
			}
			break;

		// Tecla PgUp ('9') (incrementar pas_CS)
		case GLFW_KEY_KP_9:
			pas_CS = pas_CS * 2.0;
			if (pas_CS > 0.5) pas_CS = 0.5;
			if (objecte == C_BSPLINE) {
				deleteVAOList(CRV_BSPLINE);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
				Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
			}
			else if (objecte == C_BEZIER) {
				deleteVAOList(CRV_BEZIER);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_BEZIER, load_Bezier_Curve_VAO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
				Set_VAOList(CRV_BEZIER, load_Bezier_Curve_EBO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
			}
			else if (objecte == C_LEMNISCATA) {
				deleteVAOList(CRV_LEMNISCATA3D);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_EBO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.
				Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_EBO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.
			}
			else if (objecte == C_HERMITTE) {
				deleteVAOList(CRV_HERMITTE);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_HERMITTE, load_Hermitte_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_HERMITTE.
				Set_VAOList(CRV_HERMITTE, load_Hermitte_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_HERMITTE.
			}
			else if (objecte == C_CATMULL_ROM) {
				deleteVAOList(CRV_CATMULL_ROM);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_CATMULL_ROM, load_CatmullRom_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_CATMULL_ROM.
				Set_VAOList(CRV_CATMULL_ROM, load_CatmullRom_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_CATMULL_ROM.
			}
			break;

		// Tecla PgDown ('3') (decrementar pas_CS)
		case GLFW_KEY_KP_3:
			pas_CS = pas_CS / 2;
			if (pas_CS < 0.0125) pas_CS = 0.00625;
			if (objecte == C_BSPLINE) {
				deleteVAOList(CRV_BSPLINE);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
				Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
			}
			else if (objecte == C_BEZIER) {
				deleteVAOList(CRV_BEZIER);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_BEZIER, load_Bezier_Curve_VAO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
				Set_VAOList(CRV_BEZIER, load_Bezier_Curve_EBO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
			}
			else if (objecte == C_LEMNISCATA) {
				deleteVAOList(CRV_LEMNISCATA3D);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_VAO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.
				Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_EBO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.
			}
			else if (objecte == C_HERMITTE) {
				deleteVAOList(CRV_HERMITTE);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_HERMITTE, load_Hermitte_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_HERMITTE.
				Set_VAOList(CRV_HERMITTE, load_Hermitte_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_HERMITTE.
			}
			else if (objecte == C_CATMULL_ROM) {
				deleteVAOList(CRV_CATMULL_ROM);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_CATMULL_ROM, load_CatmullRom_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_CATMULL_ROM.
				Set_VAOList(CRV_CATMULL_ROM, load_CatmullRom_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_CATMULL_ROM.
			}
			break;

		default:
			if (transf)
			{	if (rota) Teclat_TransRota(key, action);
					else if (trasl) Teclat_TransTraslada(key, action);
							else if (escal) Teclat_TransEscala(key, action);
			}
			else if (pan) Teclat_Pan(key, action);
					else if (camera == CAM_NAVEGA) Teclat_Navega(key, action);
							else if (!sw_color) Teclat_ColorFons(key, action);
									else Teclat_ColorObjecte(key, action);
			break;
		}
	}
}
