#include "src/app/AppIncludes.h"
#include "src/legacy_vgi/VgiFileActions.h"

/* ------------------------------------------------------------------------- */
/*   RECURSOS DE MENU (persianes) DE L'APLICACIO:                            */
/*					1. ARXIUS												 */
/*					4. CÀMERA: Esfèrica (Mobil, Zoom, ZoomO, Satelit), Navega*/
/*					5. VISTA: Pan, Eixos i Grid							     */
/*					6. PROJECCIÓ                                             */
/*					7. OBJECTE					                             */
/*					8. TRANSFORMA											 */
/*					9. OCULTACIONS											 */
/*				   10. IL.LUMINACIÓ											 */
/*				   11. LLUMS												 */
/*				   12. SHADERS												 */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/*					1. ARXIUS 												 */
/* ------------------------------------------------------------------------- */

// Obrir fitxer Fractal
void OnArxiuObrirFractal()
{
// TODO: Agregue aquí su código de controlador de comandos

	nfdchar_t* nomFitxer = NULL;
	nfdresult_t result; // = NFD_OpenDialog(NULL, NULL, &nomFitxer);

// Entorn VGI: Obrir diàleg de lectura de fitxer (fitxers (*.MNT)
	result = NFD_OpenDialog(".mnt", NULL, &nomFitxer);

	if (result == NFD_OKAY) {
		puts("Fitxer Fractal Success!");
		puts(nomFitxer);

		objecte = O_FRACTAL;
// Entorn VGI: TO DO -> Llegir fitxer fractal (nomFitxer) i inicialitzar alçades

		free(nomFitxer);
	}

}

// OnArchivoObrirFitxerObj: Obrir fitxer en format gràfic OBJ
void OnArxiuObrirFitxerObj()
{
// TODO: Agregue aquí su código de controlador de comandos
	nfdchar_t* nomFitxer = NULL;
	nfdresult_t result = NFD_OpenDialog(NULL, NULL, &nomFitxer);

	if (result == NFD_OKAY) {
		// Entorn VGI: Variable de tipus nfdchar_t* 'nomFitxer' conté el nom del fitxer seleccionat
		puts("Success!");
		puts(nomFitxer);

		//if (ObOBJ != NULL) delete ObOBJ;
		objecte = OBJOBJ;	textura = true;		tFlag_invert_Y = false;

		if (ObOBJ == NULL) ObOBJ = ::new COBJModel;
		else {	// Si instància ja s'ha utilitzat en un objecte OBJ
			ObOBJ->netejaVAOList_OBJ();		// Netejar VAO, EBO i VBO
			ObOBJ->netejaTextures_OBJ();	// Netejar buffers de textures
		}

		//int error = ObOBJ->LoadModel(nomfitx);			// Carregar objecte OBJ amb textura com a varis VAO's
		int error = ObOBJ->LoadModel(nomFitxer);			// Carregar objecte OBJ amb textura com a varis VAO's

		//	Pas de paràmetres textura al shader
		glUniform1i(glcache::UniformLocation(shader_programID, "textur"), textura);
		glUniform1i(glcache::UniformLocation(shader_programID, "flag_invert_y"), tFlag_invert_Y);
		free(nomFitxer);
	}
}

// Obrir fitxer que conté paràmetres Font de Llum (fitxers .lght)
void OnArxiuObrirFitxerFontLlum()
{
// TODO: Agregue aquí su código de controlador de comandos
	nfdchar_t* nomFitxer = NULL;
	nfdresult_t result = NFD_OpenDialog(NULL, NULL, &nomFitxer);

	if (result == NFD_OKAY) {
		// Entorn VGI: Variable de tipus nfdchar_t* 'nomFitxer' conté el nom del fitxer seleccionat
		puts("Success!");
		puts(nomFitxer);

		bool err = llegir_FontLlum(nomFitxer); // Llegir Fitxer de Paràmetres Font de Llum
		}
}

// llegir_FontLlum: Llegir fitxer .lght que conté paràmetres de la font de lluym i-èssima. 
//				Retorna booleà a TRUE si s'ha fet la lectura correcte, FALSE en cas contrari.
//bool llegir_FontLlum(CString nomf)
bool llegir_FontLlum(char* nomf)
{
	int i, j;
	FILE* fd;

//	ifstream f("altinicials.dat",ios::in);
//    f>>i; f>>j;
	if ((fd = fopen(nomf, "rt")) == NULL)
		{	fprintf(stderr, "%s", "ERROR: File .lght was not opened");
			return false;
	}
	fscanf(fd, "%i \n", &i);
	if (i < 0 || i >= NUM_MAX_LLUMS) return false;
	else {
		fscanf(fd, "%i \n", &j); // Lectura llum encesa
		if (j == 0) llumGL[i].encesa = false; else llumGL[i].encesa = true;
		fscanf(fd, "%lf %lf %lf %lf\n", &llumGL[i].posicio.x, &llumGL[i].posicio.y, &llumGL[i].posicio.z, &llumGL[i].posicio.w);
		fscanf(fd, "%lf %lf %lf \n", &llumGL[i].difusa.r, &llumGL[i].difusa.g, &llumGL[i].difusa.b);
		fscanf(fd, "%lf %lf %lf \n", &llumGL[i].especular.r, &llumGL[i].especular.g, &llumGL[i].especular.b);
		fscanf(fd, "%lf %lf %lf \n", &llumGL[i].atenuacio.a, &llumGL[i].atenuacio.b, &llumGL[i].atenuacio.c);
		fscanf(fd, "%i \n", &j); // Lectura booleana font de llum restringida.
		if (j == 0) llumGL[i].restringida = false; else llumGL[i].restringida = true;
		fscanf(fd, "%lf %lf %lf \n", &llumGL[i].spotdirection.x, &llumGL[i].spotdirection.y, &llumGL[i].spotdirection.z);
		fscanf(fd, "%f \n", &llumGL[i].spotcoscutoff);
		fscanf(fd, "%f \n", &llumGL[i].spotexponent);
		}
	fclose(fd);

	return true;
}

// Obrir fitxers del SkyBox
void OnArxiuObrirSkybox()
{
	// TODO: Agregue aquí su código de controlador de comandos
	nfdchar_t* folderPath = NULL;
	nfdresult_t result = NFD_OpenDialog(NULL, NULL, &folderPath);
	std::vector<std::string> faces;

/*
// TODO: Agregue aquí su código de controlador de comandos
	CString folderPath;

// Càrrega VAO Skybox Cube
	if (skC_VAOID.vaoId == 0) skC_VAOID = loadCubeSkybox_VAO();
	Set_VAOList(CUBE_SKYBOX, skC_VAOID);

// Càrrega del fitxer right (+X <--> posx <--> right)
	CString facesCS = folderPath;
	facesCS += "\\right.jpg";
	// Convert a TCHAR string to a LPCSTR
	CT2CA pszConvertedAnsiString(facesCS);
// construct a std::string using the LPCSTR input
	std::string facesS1(pszConvertedAnsiString);
	faces.push_back(facesS1);

// Càrrega del fitxer left (-X <--> negx <--> left)
	facesCS = folderPath;
	facesCS += "\\left.jpg";
// Convert a TCHAR string to a LPCSTR
	CT2CA pszConvertedAnsiString2(facesCS);
// Construct a std::string using the LPCSTR input
	std::string facesS2(pszConvertedAnsiString2);
	faces.push_back(facesS2);

// Càrrega del fitxer top (+Y <--> posy <--> top)
	facesCS = folderPath;
	facesCS += "\\top.jpg";
// Convert a TCHAR string to a LPCSTR
	CT2CA pszConvertedAnsiString3(facesCS);
// Construct a std::string using the LPCSTR input
	std::string facesS3(pszConvertedAnsiString3);
	faces.push_back(facesS3);

// Càrrega del fitxer bottom (-Y <--> negy <--> bottom)
	facesCS = folderPath;
	facesCS += "\\bottom.jpg";
// Convert a TCHAR string to a LPCSTR
	CT2CA pszConvertedAnsiString4(facesCS);
// Construct a std::string using the LPCSTR input
	std::string facesS4(pszConvertedAnsiString4);
	faces.push_back(facesS4);
// Càrrega del fitxer front (+Z <--> posz <--> front)
	facesCS = folderPath;
	facesCS += "\\front.jpg";
// Convert a TCHAR string to a LPCSTR
	CT2CA pszConvertedAnsiString5(facesCS);
// Construct a std::string using the LPCSTR input
	std::string facesS5(pszConvertedAnsiString5);
	faces.push_back(facesS5);

// Càrrega del fitxer back (-Z <--> negz <--> back)
	facesCS = folderPath;
	facesCS += "\\back.jpg";
// Convert a TCHAR string to a LPCSTR
	CT2CA pszConvertedAnsiString6(facesCS);
// construct a std::string using the LPCSTR input
	std::string facesS6(pszConvertedAnsiString6);
	faces.push_back(facesS6);

//
//	if (!cubemapTexture)
//	{	// load Skybox textures
//		// -------------
//		std::vector<std::string> faces =
//		{ "./assets/textures/skybox/right.jpg",
//		"./assets/textures/skybox/left.jpg",
//		"./assets/textures/skybox/top.jpg",
//		"./assets/textures/skybox/bottom.jpg",
//		"./assets/textures/skybox/front.jpg",
//		"./assets/textures/skybox/back.jpg"
//	};

	cubemapTexture = loadCubemap(faces);
//	}

*/
}

// -------------------- FUNCIONS CORBES SPLINE i BEZIER

// llegir_ptsC: Llegir punts de control de corba (spline o Bezier) d'un fitxer .crv. 
//				Retorna el nombre de punts llegits en el fitxer.
//int llegir_pts(CString nomf)
int llegir_ptsC(const char* nomf)
{
	int i, j;
	FILE* fd;

// Inicialitzar vector punts de control de la corba spline
	for (i = 0; i < MAX_PATCH_CORBA; i = i++)
	{	PC_t[i].x = 0.0;
		PC_t[i].y = 0.0;
		PC_t[i].z = 0.0;
	}

//	ifstream f("altinicials.dat",ios::in);
//    f>>i; f>>j;
	if ((fd = fopen(nomf, "rt")) == NULL)
	{	//LPCWSTR texte1 = reinterpret_cast<LPCWSTR> ("ERROR:");
		//LPCWSTR texte2 = reinterpret_cast<LPCWSTR> ("File .crv was not opened");
		//MessageBox(texte1, texte2, MB_OK);
		fprintf(stderr, "ERROR: File .crv was not opened");
		return false;
	}
	fscanf(fd, "%d \n", &i);
	if (i == 0) return false;
	else {	for (j = 0; j < i; j = j++)
			{	//fscanf(fd, "%f", &corbaSpline[j].x);
				//fscanf(fd, "%f", &corbaSpline[j].y);
				//fscanf(fd, "%f \n", &corbaSpline[j].z);
				fscanf(fd, "%lf %lf %lf \n", &PC_t[j].x, &PC_t[j].y, &PC_t[j].z);
			}
		}
	fclose(fd);

	return i;
}
