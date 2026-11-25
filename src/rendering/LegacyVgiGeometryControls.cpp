#include "src/rendering/LegacyVgiGeometryControls.h"

#include "file_dialog_compat.h"
#include "framework.h"
#include "src/scene/escena.h"

/* ------------------------------------------------------------------------- */
/*					7. OBJECTE					                             */
/* ------------------------------------------------------------------------- */

// OBJECTE: Cap objecte
void LegacyVgiGeometryControls::OnObjecteCap()
{
// TODO: Agregue aquí su código de controlador de comandos
	objecte = CAP;

	netejaVAOList();						// Neteja Llista VAO.

// Entorn VGI: Eliminar buffers de textures previs del vector texturesID[].
	for (int i = 0; i < NUM_MAX_TEXTURES; i++) {
		if (texturesID[i]) {	glDeleteTextures(1, &texturesID[i]);
								texturesID[i] = 0;
							}
		}

// Entorn VGI: Alliberar memòria i textures objecte OBJ, si creades.
	if (ObOBJ != NULL)
		{	ObOBJ->netejaVAOList_OBJ();		// Netejar VAO, EBO i VBO
			ObOBJ->netejaTextures_OBJ();	// Netejar buffers de textures
		}

}

// OBJECTE: Cub
void LegacyVgiGeometryControls::OnObjecteCub()
{
// TODO: Agregue aquí su código de controlador de comandos

	objecte = CUB;

//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)

	netejaVAOList();											// Neteja Llista VAO.

// Posar color objecte (col_obj) al vector de colors del VAO.
	SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

//Set_VAOList(GLUT_CUBE, loadglutSolidCube_VAO(1.0));	// Genera VAO de cub mida 1 i el guarda a la posició GLUT_CUBE.
	Set_VAOList(GLUT_CUBE, loadglutSolidCube_EBO(1.0));		// Genera EBO de cub mida 1 i el guarda a la posició GLUT_CUBE.
}


// OBJECTE: Cub RGB
void LegacyVgiGeometryControls::OnObjecteCubRGB()
{
// TODO: Agregue aquí su código de controlador de comandos
	objecte = CUB_RGB;

//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)


	netejaVAOList();						// Neteja Llista VAO.

	//Set_VAOList(GLUT_CUBE_RGB, loadglutSolidCubeRGB_VAO(1.0));	// Genera VAO de cub mida 1 i el guarda a la posició GLUT_CUBE_RGB.
	Set_VAOList(GLUT_CUBE_RGB, loadglutSolidCubeRGB_EBO(1.0));	// Genera EBO de cub mida 1 i el guarda a la posició GLUT_CUBE_RGB.
}


// OBJECTE Esfera
void LegacyVgiGeometryControls::OnObjecteEsfera()
{
// TODO: Agregue aquí su código de controlador de comandos
	objecte = ESFERA;

//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)


	netejaVAOList();						// Neteja Llista VAO.

// Posar color objecte (col_obj) al vector de colors del VAO.
	SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

	//Set_VAOList(GLU_SPHERE, loadgluSphere_VAO(1.0, 30,30)); // // Genera VAO d'esfera radi 1 i el guarda a la posició GLUT_CUBE_RGB.
	Set_VAOList(GLU_SPHERE, loadgluSphere_EBO(1.0, 30, 30));
}


// OBJECTE Tetera
void LegacyVgiGeometryControls::OnObjecteTetera()
{
// TODO: Agregue aquí su código de controlador de comandos
	objecte = TETERA;

//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)

	netejaVAOList();						// Neteja Llista VAO.

// Posar color objecte (col_obj) al vector de colors del VAO.
	SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

	//if (Get_VAOId(GLUT_TEAPOT) != 0) deleteVAOList(GLUT_TEAPOT);
	Set_VAOList(GLUT_TEAPOT, loadglutSolidTeapot_VAO()); //Genera VAO tetera mida 1 i el guarda a la posició GLUT_TEAPOT.
}


// --------------- OBJECTES COMPLEXES

// OBJECTE ARC
void LegacyVgiGeometryControls::OnObjecteArc()
{
	CColor color_Mar;

	color_Mar.r = 0.5;	color_Mar.g = 0.4; color_Mar.b = 0.9; color_Mar.a = 1.0;
	// TODO: Agregue aquí su código de controlador de comandos
	objecte = ARC;

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
}


// OBJECTE Tie
void LegacyVgiGeometryControls::OnObjecteTie()
{
// TODO: Agregue aquí su código de controlador de comandos
	objecte = TIE;

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
}

// ----------------- OBJECTES CORBES BEZIER, LEMNISCATA i B-SPLINE

// OBJECTE Corba Bezier
void LegacyVgiGeometryControls::OnObjecteCorbaBezier()
{
// TODO: Agregue aquí su código de controlador de comandos
	nfdchar_t* nomFitxer = NULL;
	nfdresult_t result = NFD_OpenDialog("crv", NULL, &nomFitxer);

	objecte = C_BEZIER;		sw_material[4] = true;

	if (result == NFD_OKAY) {
		// Entorn VGI: Variable de tipus nfdchar_t* 'nomFitxer' conté el nom del fitxer seleccionat
		puts("Success!");
		puts(nomFitxer);

		npts_T = llegir_ptsC(nomFitxer);	// Llegir Fitxer amb els punts de Control de la corba.

		//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

		//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)

		// Càrrega dels VAO's per a construir la corba Bezier
		netejaVAOList();						// Neteja Llista VAO.

		// Posar color objecte (col_obj) al vector de colors del VAO.
		SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

		// Definir Esfera EBO per a indicar punts de control de la corba
		Set_VAOList(GLU_SPHERE, loadgluSphere_EBO(5.0, 20, 20));	// Genera esfera i la guarda a la posició GLUT_CUBE.

		// Definir Corba Bezier com a VAO
		//Set_VAOList(CRV_BEZIER, load_Bezier_Curve_VAO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
		Set_VAOList(CRV_BEZIER, load_Bezier_Curve_EBO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
		free(nomFitxer);
	}
}


// OBJECTE Corba Lemniscata 3D
void LegacyVgiGeometryControls::OnObjecteCorbaLemniscata()
{
// TODO: Agregue aquí su código de controlador de comandos

	objecte = C_LEMNISCATA;		sw_material[4] = true;

//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)

// Càrrega dels VAO's per a construir la corba Bezier
	netejaVAOList();						// Neteja Llista VAO.

// Posar color objecte (col_obj) al vector de colors del VAO.
	SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

// Definr Corba Lemniscata 3D com a VAO
	//Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_VAO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.
	Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_EBO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.
}


// OBJECTE Corba Hermitte
void LegacyVgiGeometryControls::OnObjecteCorbaHermitte()
{
// TODO: Agregue aquí su código de controlador de comandos
	nfdchar_t* nomFitxer = NULL;
	nfdresult_t result = NFD_OpenDialog("crv", NULL, &nomFitxer);

	objecte = C_HERMITTE;	sw_material[4] = true;

	if (result == NFD_OKAY) {
		// Entorn VGI: Variable de tipus nfdchar_t* 'nomFitxer' conté el nom del fitxer seleccionat
		puts("Success!");
		puts(nomFitxer);

		npts_T = llegir_ptsC(nomFitxer);

		//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

		//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)

		// Càrrega dels VAO's per a construir la corba BSpline
		netejaVAOList();						// Neteja Llista VAO.

		// Posar color objecte (col_obj) al vector de colors del VAO.
		SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

		// Definir Esfera EBO per a indicar punts de control de la corba
		Set_VAOList(GLU_SPHERE, loadgluSphere_EBO(5.0, 20, 20));	// Guarda (vaoId, vboId, nVertexs) a la posició GLUT_CUBE.

		// Definir Corba HERMITTE com a VAO o EBO
				//Set_VAOList(CRV_HERMITTE, load_Hermitte_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_HERMITTE.
		Set_VAOList(CRV_HERMITTE, load_Hermitte_Curve_EBO(npts_T, PC_t, pas_CS));	// Genera corba i la guarda a la posició CRV_HERMITTE.
		free(nomFitxer);
	}
}


// OBJECTE Corba Catmull Rom (interpolació per punts)
void LegacyVgiGeometryControls::OnObjecteCorbaCatmullRom()
{
// TODO: Agregue aquí su código de controlador de comandos
	nfdchar_t* nomFitxer = NULL;
	nfdresult_t result = NFD_OpenDialog("crv", NULL, &nomFitxer);

	objecte = C_CATMULL_ROM;	sw_material[4] = true;

	if (result == NFD_OKAY) {
		// Entorn VGI: Variable de tipus nfdchar_t* 'nomFitxer' conté el nom del fitxer seleccionat
		puts("Success!");
		puts(nomFitxer);

		npts_T = llegir_ptsC(nomFitxer);

		//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

		//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)


		// Càrrega dels VAO's per a construir la corba BSpline
		netejaVAOList();						// Neteja Llista VAO.

		// Posar color objecte (col_obj) al vector de colors del VAO.
		SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

		// Definir Esfera EBO per a indicar punts de control de la corba
		Set_VAOList(GLU_SPHERE, loadgluSphere_EBO(5.0, 20, 20));	// Guarda (vaoId, vboId, nVertexs) a la posició GLUT_CUBE.

		// Definir Corba CATMULL ROM com a VAO o EBO
		//Set_VAOList(CRV_CATMULL_ROM, load_CatmullRom_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_CATMULL_ROM.
		Set_VAOList(CRV_CATMULL_ROM, load_CatmullRom_Curve_EBO(npts_T, PC_t, pas_CS));	// Genera corba i la guarda a la posició CRV_CATMULL_ROM.
		free(nomFitxer);
		}
}


// OBJECTE Corba B-Spline
void LegacyVgiGeometryControls::OnObjecteCorbaBSpline()
{
// TODO: Agregue aquí su código de controlador de comandos
	nfdchar_t* nomFitxer = NULL;
	nfdresult_t result = NFD_OpenDialog("crv", NULL, &nomFitxer);

	objecte = C_BSPLINE;	sw_material[4] = true;

	if (result == NFD_OKAY) {
		// Entorn VGI: Variable de tipus nfdchar_t* 'nomFitxer' conté el nom del fitxer seleccionat
		puts("Success!");
		puts(nomFitxer);

		npts_T = llegir_ptsC(nomFitxer);

		//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

		//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)

		// Càrrega dels VAO's per a construir la corba BSpline
		netejaVAOList();						// Neteja Llista VAO.

		// Posar color objecte (col_obj) al vector de colors del VAO.
		SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

		// Definir Esfera EBO per a indicar punts de control de la corba
		Set_VAOList(GLU_SPHERE, loadgluSphere_EBO(5.0, 20, 20));	// Guarda (vaoId, vboId, nVertexs) a la posició GLUT_CUBE.

		// Definr Corba BSpline com a VAO
			//Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
		Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
		free(nomFitxer);
		}
}


// OBJECTE Punts de Control: Activació de la visualització dels Punts de control de les Corbes (OPCIÓ BOOLEANA)
void LegacyVgiGeometryControls::OnObjectePuntsControl()
{
// TODO: Agregue aquí su código de controlador de comandos
	if (objecte != C_BEZIER || objecte != C_BSPLINE || objecte != C_LEMNISCATA || objecte != C_HERMITTE || objecte != C_CATMULL_ROM)
		sw_Punts_Control = !sw_Punts_Control;
}


// OBJECTE --> CORBES: Activar o desactivar visualització Triedre de Frenet de les corbes
//					Lemniscata, Bezier i BSpline (OPCIÓ BOOLEANA)
void LegacyVgiGeometryControls::OnCorbesTriedreFrenet()
{
// TODO: Agregue aquí su código de controlador de comandos
	if (objecte != C_BEZIER || objecte != C_BSPLINE || objecte != C_LEMNISCATA || objecte != C_HERMITTE || objecte != C_CATMULL_ROM)
		dibuixa_TriedreFrenet = !dibuixa_TriedreFrenet;
}


// OBJECTE --> CORBES: Activar o desactivar visualització Triedre de Darboux de les corbes Loxodroma (OPCIÓ BOOLEANA)
void LegacyVgiGeometryControls::OnCorbesTriedreDarboux()
{
// TODO: Agregue aquí su código de controlador de comandos
	if (objecte != C_BEZIER || objecte != C_BSPLINE || objecte != C_LEMNISCATA || objecte != C_HERMITTE || objecte != C_CATMULL_ROM)
		dibuixa_TriedreDarboux = !dibuixa_TriedreDarboux;
}


// OBJECTE Matriu Primitives
void LegacyVgiGeometryControls::OnObjecteMatriuPrimitives()
{
// TODO: Agregue aquí su código de controlador de comandos
	objecte = MATRIUP;

//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)
}


// OBJECTE Matriu Primitives VAO
void LegacyVgiGeometryControls::OnObjecteMatriuPrimitivesVAO()
{
// TODO: Agregue aquí su código de controlador de comandos
	objecte = MATRIUP_VAO;

//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)

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

/* ------------------------------------------------------------------------- */
/*					8. TRANSFORMA											 */
/* ------------------------------------------------------------------------- */

// TRANSFORMA: TRASLACIÓ
void LegacyVgiGeometryControls::OnTransformaTraslacio()
{
// TODO: Agregue aquí su código de controlador de comandos
	trasl = !trasl;		//rota = false;
	if (trasl) rota = false;
	transf = trasl || rota || escal;
}


void LegacyVgiGeometryControls::OnTransformaOrigenTraslacio()
{
// TODO: Agregue aquí su código de controlador de comandos
	if (trasl)
	{	fact_Tras = 1;
		TG.VTras.x = 0.0;	TG.VTras.y = 0.0;	TG.VTras.z = 0;
	}
}


// TRANSFORMA: ROTACIÓ
void LegacyVgiGeometryControls::OnTransformaRotacio()
{
// TODO: Agregue aquí su código de controlador de comandos
	rota = !rota;		//trasl = false;
	if (rota) trasl = false;
	transf = trasl || rota || escal;
}

void LegacyVgiGeometryControls::OnTransformaOrigenRotacio()
{
// TODO: Agregue aquí su código de controlador de comandos
	if (rota)
		{	fact_Rota = 90;
			TG.VRota.x = 0;		TG.VRota.y = 0;		TG.VRota.z = 0;
		}
}


// TRANSFORMA: ESCALAT
void LegacyVgiGeometryControls::OnTransformaEscalat()
{
// TODO: Agregue aquí su código de controlador de comandos
	escal = !escal;
	transf = trasl || rota || escal;
}


void LegacyVgiGeometryControls::OnTransformaOrigenEscalat()
{
// TODO: Agregue aquí su código de controlador de comandos
	if (escal) { TG.VScal.x = 1;	TG.VScal.y = 1;	TG.VScal.z = 1; }
}


// TRANSFOMA: Mòbil Eix X? (opció booleana).
void LegacyVgiGeometryControls::OnTransformaMobilX()
{
// TODO: Agregue aquí su código de controlador de comandos
	if (transf)
		{	//transX = !transX;
			if (transX) {	mobil = false;	zzoom = false;
							pan = false;	//navega = false;
						}
				else if ((!transY) && (!transZ)) {	mobil = true;
													zzoom = true;
												}
		}
}

// TRANSFOMA: Mòbil Eix Y? (opció booleana).
void LegacyVgiGeometryControls::OnTransformaMobilY()
{
// TODO: Agregue aquí su código de controlador de comandos
	if (transf)
	{	//transY = !transY;
		if (transY) {	mobil = false;	zzoom = false;
						pan = false;	//navega = false;
					}
		else if ((!transX) && (!transZ)) {	mobil = true;
											zzoom = true;
										}
	}
}


// TRANSFOMA: Mòbil Eix Z? (opció booleana).
void LegacyVgiGeometryControls::OnTransformaMobilZ()
{
// TODO: Agregue aquí su código de controlador de comandos
	if (transf)
	{	//transZ = !transZ;
		if (transZ) {	mobil = false;	zzoom = false;
						pan = false;	//navega = false;
					}
		else if ((!transX) && (!transY)) {	mobil = true;
											zzoom = true;
											}
	}
}


/* ------------------------------------------------------------------------- */
/*					9. OCULTACIONS											 */
/* ------------------------------------------------------------------------- */

void LegacyVgiGeometryControls::OnOcultacionsFrontFaces()
{
// TODO: Agregue aquí su código de controlador de comandos
	front_faces = !front_faces;
}


// OCULTACIONS: Test de Visibilitat? (opció booleana).
void LegacyVgiGeometryControls::OnOcultacionsTestvis()
{
// TODO: Agregue aquí su código de controlador de comandos
	test_vis = !test_vis;
}


// OCULTACIONS: Z-Buffer? (opció booleana).
void LegacyVgiGeometryControls::OnOcultacionsZBuffer()
{
// TODO: Agregue aquí su código de controlador de comandos
	oculta = !oculta;
}
