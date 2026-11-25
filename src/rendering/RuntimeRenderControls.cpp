#include "file_dialog_compat.h"
#include "framework.h"
#include "src/rendering/RuntimeRenderControls.h"
#include "src/rendering/shader.h"
#include "src/rendering/UniformCache.h"
#include "src/rendering/visualitzacio.h"

/* ------------------------------------------------------------------------- */
/*					10. IL.LUMINACIÓ										 */
/* ------------------------------------------------------------------------- */

// IL.LUMINACIÓ Font de llum fixe? (opció booleana).
void RuntimeRenderControls::OnIluminacioLlumfixe()
{
// TODO: Agregue aquí su código de controlador de comandos
	ifixe = !ifixe;
}


// IL.LUMINACIÓ: Mantenir iluminades les Cares Front i Back
void RuntimeRenderControls::OnIluminacio2Sides()
{
// TODO: Agregue aquí su código de controlador de comandos
	ilum2sides = !ilum2sides;
}


// ILUMINACIÓ PUNTS
void RuntimeRenderControls::OnIluminacioPunts()
{
// TODO: Agregue aquí su código de controlador de comandos
	ilumina = PUNTS;
	test_vis = false;		oculta = false;
}


// ILUMINACIÓ FILFERROS
void RuntimeRenderControls::OnIluminacioFilferros()
{
// TODO: Agregue aquí su código de controlador de comandos
	ilumina = FILFERROS;
	test_vis = false;		oculta = false;
}


// ILUMINACIÓ PLANA
void RuntimeRenderControls::OnIluminacioPlana()
{
// TODO: Agregue aquí su código de controlador de comandos
	test_vis = false;		oculta = true;
	if (ilumina != PLANA)
		{	ilumina = PLANA;	shader = FLAT_SHADER; oIlumina = 2;
			// Elimina shader anterior
			glcache::InvalidateAll(); phongOccludersDirty = true; shaderLighting.DeleteProgram();
			// Càrrega Flat shader
			shader_programID = shaderLighting.loadFileShaders("./assets/shaders/flat_shdrML.vert", "./assets/shaders/flat_shdrML.frag");
		}
}


// ILUMINACIÓ SUAU
void RuntimeRenderControls::OnIluminacioSuau()
{
// TODO: Agregue aquí su código de controlador de comandos
	test_vis = false;		oculta = true;
	if (ilumina != SUAU)
		{	ilumina = SUAU;		shader = GOURAUD_SHADER;
			// Elimina shader anterior
			glcache::InvalidateAll(); phongOccludersDirty = true; shaderLighting.DeleteProgram();
			// Càrrega Flat shader
			shader_programID = shaderLighting.loadFileShaders("./assets/shaders/gouraud_shdrML.vert", "./assets/shaders/gouraud_shdrML.frag");
		}
}


// ILUMINACIÓ->REFLECTIVITAT MATERIAL / COLOR: Activació i desactivació de la reflectivitat pròpia del material com a color.
void RuntimeRenderControls::OnMaterialReflmaterial()
{
// TODO: Agregue aquí su código de controlador de comandos
	sw_material[4] = !sw_material[4];	sw_il = true;

	// Pas màscara llums
	glUniform1i(glcache::UniformLocation(shader_programID, "sw_material"), sw_material[4]);
}


// ILUMINACIÓ->REFLECTIVITAT MATERIAL EMISSIÓ: Activació i desactivació de la reflectivitat pròpia del material.
void RuntimeRenderControls::OnMaterialEmissio()
{
// TODO: Agregue aquí su código de controlador de comandos
	sw_material[0] = !sw_material[0];

// Pas màscara llums
	applyMaterialIntensity(shader_programID, llum_ambient);
}


// ILUMINACIÓ->REFLECTIVITAT MATERIAL AMBIENT: Activació i desactivació de la reflectivitat ambient del material.
void RuntimeRenderControls::OnMaterialAmbient()
{
// TODO: Agregue aquí su código de controlador de comandos
	sw_material[1] = !sw_material[1];

// Pas màscara llums
	applyMaterialIntensity(shader_programID, llum_ambient);
}


// ILUMINACIÓ->REFLECTIVITAT MATERIAL DIFUSA: Activació i desactivació de la reflectivitat difusa del materials.
void RuntimeRenderControls::OnMaterialDifusa()
{
// TODO: Agregue aquí su código de controlador de comandos
	sw_material[2] = !sw_material[2];

// Pas màscara llums
	applyMaterialIntensity(shader_programID, llum_ambient);
}


// ILUMINACIÓ->REFLECTIVITAT MATERIAL ESPECULAR: Activació i desactivació de la reflectivitat especular del material.
void RuntimeRenderControls::OnMaterialEspecular()
{
// TODO: Agregue aquí su código de controlador de comandos
	sw_material[3] = !sw_material[3];

// Pas màscara llums
	applyMaterialIntensity(shader_programID, llum_ambient);
}


// ILUMINACIÓ: Textures?: Activació (TRUE) o desactivació (FALSE) de textures.
void RuntimeRenderControls::OnIluminacioTextures()
{
// TODO: Agregue aquí su código de controlador de comandos
	textura = !textura;

//	Pas de textura al shader
	glUniform1i(glcache::UniformLocation(shader_programID, "texture"), textura);
}


// ILUMINACIÓ --> TEXTURA: Càrrega fitxer textura per llibreria SOIL
void RuntimeRenderControls::OnIluminacioTexturaFitxerimatge()
{
// TODO: Agregue aquí su código de controlador de comandos
	nfdchar_t* nomFitxer = NULL;
	nfdresult_t result = NFD_OpenDialog(NULL, NULL, &nomFitxer);

	t_textura = TEXTURA_FITXERIMA;		tFlag_invert_Y = true;
	textura = true;

	if (result == NFD_OKAY)
		{	// Entorn VGI: Variable de tipus nfdchar_t* 'nomFitxer' conté el nom del fitxer seleccionat
			puts("Success!");
			puts(nomFitxer);

			// Entorn VGI: Eliminar buffers de textures previs del vector texturesID[].
			for (int i = 0; i < NUM_MAX_TEXTURES; i++)
				{	if (texturesID[i])
					{	//err = glIsTexture(texturesID[i]);
						glDeleteTextures(1, &texturesID[i]);
						//err = glIsTexture(texturesID[i]);
						texturesID[i] = 0;
					}
				}

			// EntornVGI: Carregar fitxer textura i definir buffer de textura.Identificador guardat a texturesID[0].
			texturesID[0] = loadIMA_SOIL(nomFitxer);

			//	Pas de textura al shader
			glUniform1i(glcache::UniformLocation(shader_programID, "texture0"), GLint(0));
			glUniform1i(glcache::UniformLocation(shader_programID, "flag_invert_y"), tFlag_invert_Y);
			free(nomFitxer);
		}
}


// ILUMINACIÓ --> TEXTURA: FLAG_INVERT_Y Inversió coordenada t de textura (1-cty) per a textures SOIL (TRUE)
//			o no (FALSE) per a objectes 3DS i OBJ.
void RuntimeRenderControls::OnIluminacioTexturaFlagInvertY()
{
// TODO: Agregue aquí su código de controlador de comandos
	if (textura) tFlag_invert_Y = !tFlag_invert_Y;

//	Pas de paràmetres textura al shader
	glUniform1i(glcache::UniformLocation(shader_programID, "flag_invert_y"), tFlag_invert_Y);
}

/* ------------------------------------------------------------------------- */
/*					11. LLUMS												 */
/* ------------------------------------------------------------------------- */

// LLUMS: Activació / Desactivació llum ambient
void RuntimeRenderControls::OnLlumsLlumAmbient()
{
// TODO: Agregue aquí su código de controlador de comandos
	//llum_ambient = !llum_ambient;
	sw_il = true;

// Pas màscara llums al shader
	applyMaterialIntensity(shader_programID, llum_ambient); // actualiza el vector bvec4 completo
}

// LLUMS: Activació /Desactivació llum 0 (GL_LIGHT0)
void RuntimeRenderControls::OnLlumsLlum0()
{
// TODO: Agregue aquí su código de controlador de comandos
	//llumGL[0].encesa = !llumGL[0].encesa;
	sw_il = true;

// Pas màscara llums
	glUniform1i(glcache::UniformLocation(shader_programID, "sw_lights[0]"), llumGL[0].encesa);
}


// LLUMS-->ON/OFF: Activació /Desactivació llum 1 (GL_LIGHT1)
void RuntimeRenderControls::OnLlumsLlum1()
{
// TODO: Agregue aquí su código de controlador de comandos
	//llumGL[1].encesa = !llumGL[1].encesa;
	sw_il = true;

// Pas màscara llums
	glUniform1i(glcache::UniformLocation(shader_programID, "sw_lights[1]"), llumGL[1].encesa);
}


// LLUMS-->ON/OFF: Activació /Desactivació llum 2 (GL_LIGHT2)
void RuntimeRenderControls::OnLlumsLlum2()
{
// TODO: Agregue aquí su código de controlador de comandos
	//llumGL[2].encesa = !llumGL[2].encesa;
	sw_il = true;

// Pas màscara llums
	glUniform1i(glcache::UniformLocation(shader_programID, "sw_lights[2]"), llumGL[2].encesa);
}


// LLUMS-->ON/OFF: Activació /Desactivació llum 3 (GL_LIGHT3)
void RuntimeRenderControls::OnLlumsLlum3()
{
// TODO: Agregue aquí su código de controlador de comandos
	//llumGL[3].encesa = !llumGL[3].encesa;
	sw_il = true;

// Pas màscara llums
	glUniform1i(glcache::UniformLocation(shader_programID, "sw_lights[3]"), llumGL[3].encesa);
}


// LLUMS-->ON/OFF: Activació /Desactivació llum 4 (GL_LIGHT4)
void RuntimeRenderControls::OnLlumsLlum4()
{
// TODO: Agregue aquí su código de controlador de comandos
	//llumGL[4].encesa = !llumGL[4].encesa;
	sw_il = true;

// Pas màscara llums
	glUniform1i(glcache::UniformLocation(shader_programID, "sw_lights[4]"), llumGL[4].encesa);
}

// LLUMS-->ON/OFF: Activació /Desactivació llum 5 (GL_LIGHT5)
void RuntimeRenderControls::OnLlumsLlum5()
{
// TODO: Agregue aquí su código de controlador de comandos
	//llumGL[5].encesa = !llumGL[5].encesa;
	sw_il = true;

// Pas màscara llums
	glUniform1i(glcache::UniformLocation(shader_programID, "sw_lights[5]"), llumGL[5].encesa);
}

// LLUMS-->ON/OFF: Activació /Desactivació llum 6 (GL_LIGHT6)
void RuntimeRenderControls::OnLlumsLlum6()
{
// TODO: Agregue aquí su código de controlador de comandos
	//llumGL[6].encesa = !llumGL[6].encesa;
	sw_il = true;

// Pas màscara llums
	glUniform1i(glcache::UniformLocation(shader_programID, "sw_lights[6]"), llumGL[6].encesa);
}

// LLUMS-->ON/OFF: Activació /Desactivació llum 7 (GL_LIGHT7)
void RuntimeRenderControls::OnLlumsLlum7()
{
// TODO: Agregue aquí su código de controlador de comandos
	//llumGL[7].encesa = !llumGL[7].encesa;
	sw_il = true;

// Pas màscara llums
	glUniform1i(glcache::UniformLocation(shader_programID, "sw_lights[7]"), llumGL[7].encesa);
}

/* ------------------------------------------------------------------------- */
/*					12. SHADERS												 */
/* ------------------------------------------------------------------------- */

// SHADER FLAT
void RuntimeRenderControls::OnShaderFlat()
{
// TODO: Agregue aquí su código de controlador de comandos
	GLuint newShaderID = 0;

	shader = FLAT_SHADER;	ilumina = PLANA;
	test_vis = false;		oculta = true;

// Càrrega Shader de Flat
	fprintf(stderr, "Flat Shader: \n");

// Elimina shader anterior
	glcache::InvalidateAll(); phongOccludersDirty = true; shaderLighting.DeleteProgram();	shader_programID = 0;
// Càrrega Flat shader
	newShaderID = shaderLighting.loadFileShaders("./assets/shaders/flat_shdrML.vert", "./assets/shaders/flat_shdrML.frag");

// Càrrega shaders dels fitxers
	if (newShaderID) {	shader_programID = newShaderID;
						// Activa shader.
						glUseProgram(shader_programID); // shaderLighting.use();
					 }
	else fprintf(stderr, "%s", "GLSL_Error. Fitxers .vert o .frag amb errors"); // AfxMessageBox(_T("GLSL_Error. Fitxers .vert o .frag amb errors"));
}

// SHADER GOURAUD
void RuntimeRenderControls::OnShaderGouraud()
{
// TODO: Agregue aquí su código de controlador de comandos
	GLuint newShaderID = 0;

	shader = GOURAUD_SHADER;	ilumina = SUAU;
	test_vis = false;			oculta = true;

// Càrrega Shader de Gouraud
	fprintf(stderr, "Gouraud Shader: \n");

// Elimina shader anterior
	glcache::InvalidateAll(); phongOccludersDirty = true; shaderLighting.DeleteProgram();	shader_programID = 0;
// Càrrega Gouraud shader
	newShaderID = shaderLighting.loadFileShaders("./assets/shaders/gouraud_shdrML.vert", "./assets/shaders/gouraud_shdrML.frag");

// Càrrega shaders dels fitxers
	if (newShaderID) {	shader_programID = newShaderID;
						// Activa shader.
						glUseProgram(shader_programID); // shaderLighting.use();
					 }
	else fprintf(stderr, "%s", "GLSL_Error. Fitxers .vert o .frag amb errors"); // AfxMessageBox(_T("GLSL_Error. Fitxers .vert o .frag amb errors"));
}


// SHADER PHONG
void RuntimeRenderControls::OnShaderPhong()
{
	GLuint newShaderID = 0;

// TODO: Agregue aquí su código de controlador de comandos
	shader = PHONG_SHADER;	ilumina = SUAU;
	test_vis = false;		oculta = true;

// Càrrega Shader de Phong
	fprintf(stderr, "Phong Shader: \n");

// Elimina shader anterior
	glcache::InvalidateAll(); phongOccludersDirty = true; shaderLighting.DeleteProgram();	shader_programID = 0;
// Càrrega Phong Shader
	newShaderID = shaderLighting.loadFileShaders("./assets/shaders/phong_shdrML.vert", "./assets/shaders/phong_shdrML.frag");

// Càrrega shaders dels fitxers
	if (newShaderID) {	shader_programID = newShaderID;
						// Activa shader.
						glUseProgram(shader_programID); // shaderLighting.use();
					 }
	else fprintf(stderr, "%s", "GLSL_Error. Fitxers .vert o .frag amb errors"); // AfxMessageBox(_T("GLSL_Error. Fitxers .vert o .frag amb errors"));
}


// SHADERS: Càrrega Fitxers Shader (.vert, .frag)
void RuntimeRenderControls::OnShaderLoadFiles()
{
// TODO: Agregue aquí su código de controlador de comandos
	GLuint newShaderID = 0;

	nfdchar_t* nomVertS = NULL;
	nfdchar_t* nomFragS = NULL;

	shader = FILE_SHADER;	ilumina = SUAU;
	test_vis = false;		oculta = true;

// Càrrega fitxer VERT
// Entorn VGI: Obrir diàleg de lectura de fitxer (fitxers (*.VERT)
	nfdresult_t resultVS = NFD_OpenDialog("vert,vrt,vs", NULL, &nomVertS);

	if (resultVS == NFD_OKAY)
		{	puts("Fitxer .vert llegit!");
			puts(nomVertS);
		}
		else if (resultVS == NFD_CANCEL)
			{	puts("User pressed cancel.");
				return;
			}
			else {	printf("Error: %s\n", NFD_GetError());
					return;
				 }

// Càrrega fitxer FRAG
// Entorn VGI: Obrir diàleg de lectura de fitxer (fitxers (*.FRAG)
	nfdresult_t resultFS = NFD_OpenDialog("frag,frg,fs", NULL, &nomFragS);

	if (resultVS == NFD_OKAY)
		{	puts("Fitxer .FRAG llegit!");
			puts(nomFragS);
		}
		else if (resultVS == NFD_CANCEL)
			{	puts("User pressed cancel.");
				return;
			}
			else {	printf("Error: %s\n", NFD_GetError());
					return;
				}

// Entorn VGI: Carrega dels shaders
// Elimina shader anterior
	glcache::InvalidateAll(); phongOccludersDirty = true; shaderLighting.DeleteProgram();	shader_programID = 0;
	newShaderID = shaderLighting.loadFileShaders(nomVertS, nomFragS);

// Càrrega shaders dels fitxers
	if (newShaderID)
		{	shader_programID = newShaderID;
			// Activa shader.
			glUseProgram(shader_programID); // shaderLighting.use();
		}
	else fprintf(stderr, "%s", "GLSL_Error. Fitxers .vert o .frag amb errors"); // AfxMessageBox(_T("GLSL_Error. Fitxers .vert o .frag amb errors"));
}


// Escriure Binary Program actual en fitxer .bin
void RuntimeRenderControls::OnShaderPBinaryWrite()
{
// TODO: Agregue aquí su código de controlador de comandos
	nfdchar_t* nomPBinary = NULL;

// Càrrega fitxer .BIN
// Entorn VGI: Obrir diàleg d'escriptura de fitxer (fitxers (*.bin)
	nfdresult_t resultBS = NFD_OpenDialog(NULL, NULL, &nomPBinary);

	if (resultBS == NFD_OKAY)
	{	// Entorn VGI: Variable de tipus nfdchar_t* 'nomFitxer' conté el nom del fitxer seleccionat
		puts("Success!");
		puts(nomPBinary);

		// Entorn VGI: To retrieve the compiled Binary Program shader code and write it to a file
		GLint formats = 0;
		glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &formats);
		GLint* binaryFormats = new GLint[formats];
		glGetIntegerv(GL_PROGRAM_BINARY_FORMATS, binaryFormats);

		GLint length = 0;
		glGetProgramiv(shader_programID, GL_PROGRAM_BINARY_LENGTH, &length);

		// Retrieve the binary code
		std::vector<GLubyte> buffer(length);
		GLenum* Formats = 0;
		glGetProgramBinary(shader_programID, length, NULL, (GLenum*)Formats, buffer.data());

		// Write the binary to a binary file
		FILE* sb;
		sb = fopen(nomPBinary, "wb");
		fwrite(buffer.data(), length, 1, sb);
		fclose(sb);

		// MISSATGE DE FITXER BEN GRAVAT o MAL GRAVAT
		//AfxMessageBox(_T("Fitxer ben gravat"));
		fprintf(stderr, "%s \n", "Fitxer ben gravat");
	}
}


// Llegir Binary Program de fitxer .bin i instalar i definir com actual.
void RuntimeRenderControls::OnShaderPBinaryRead()
{
// TODO: Agregue aquí su código de controlador de comandos
	nfdchar_t* nomPBinary = NULL;
	FILE* fd;

	shader = PROG_BINARY_SHADER;		ilumina = SUAU;
	test_vis = false;					oculta = true;

	// Càrrega fitxer .BIN
	// Entorn VGI: Obrir diàleg de lectura de fitxer (fitxers (*.bin)
	nfdresult_t resultBS = NFD_OpenDialog(NULL, NULL, &nomPBinary);

	if (resultBS == NFD_OKAY)
	{	// Entorn VGI: Variable de tipus nfdchar_t* 'nomFitxer' conté el nom del fitxer seleccionat
		puts("Success!");
		puts(nomPBinary);

		// Entorn VGI: To read de Shader Program from a file and install it
		GLint filelength = 0;
		GLenum format = 0;

		/* Retrieve the binary code per a obtenir valor variable format
			std::vector<GLubyte> buff(filelength);
			GLint longitut = 0;
			glGetProgramBinary(shader_programID, longitut, NULL, &format, buff.data());
		*/

		// Entorn VGI: Read from a binary file
		FILE* sb;
		sb = fopen(nomPBinary, "rb");
		if (!sb) {
			fprintf(stderr, "%s \n", "GLSL_Error. Unable to open file"); // AfxMessageBox(_T("GLSL_Error. Unable to open file"));
			return;
			}

		// Get file length
		fseek(sb, 0, SEEK_END);
		filelength = ftell(sb);
		fseek(sb, 0, SEEK_SET);

		std::vector<GLubyte> buffer(filelength + 1); // Allocatem buffer amb mida de Binary Program
		fclose(sb);

		sb = fopen(nomPBinary, "rb");
		fread(buffer.data(), filelength, 1, sb);
		fclose(sb);

		// Install shader binary
		GLint formats = 0;
		glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &formats);

		GLuint shader_BinProgramID = glCreateProgram();
		glProgramBinary(shader_BinProgramID, formats, buffer.data(), filelength);

		//glLinkProgram(shader_BinProgramID); // Linkedició del program.
		// Check for success/failure
		GLint status;
		glGetProgramiv(shader_BinProgramID, GL_LINK_STATUS, &status);
		if (status == GL_FALSE) {
			// Llista error de linkedició del Shader Program
			GLint maxLength = 0;

			glGetProgramiv(shader_BinProgramID, GL_INFO_LOG_LENGTH, &maxLength);
			// The maxLength includes the NULL character
			std::vector<GLchar> errorLog(maxLength);
			glGetProgramInfoLog(shader_BinProgramID, maxLength, &maxLength, &errorLog[0]);

			// AfxMessageBox(_T("Error Linkedicio Shader Binary Program"));
			fprintf(stderr, "%s \n", "Error Linkedicio Shader Binary Program");
			//fprintf(stderr, "%s \n", "Error Linkedicio Shader Program");

			// Volcar missatges error a fitxer GLSL_Error.LINK
			if ((fd = fopen("GLSL_Error.LINK", "w")) == NULL)
			{	//AfxMessageBox(_T("GLSL_Error.LINK was not opened"));
				fprintf(stderr, "%s \n", "GLSL_Error.LINK was not opened");
			}
			for (int i = 0; i <= maxLength; i = i++) fprintf(fd, "%c", errorLog[i]);
			fclose(fd);
			glDeleteProgram(shader_BinProgramID);		// Don't leak the program.
			}
		else {
			//shaderLighting.DeleteProgram();	// Eliminar shader anterior
			glcache::InvalidateAll(); phongOccludersDirty = true;
			shader_programID = shader_BinProgramID; // Assignar nou Binary Program com l'actual.
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			glUseProgram(shader_programID);			// Activa shader llegit.
			}
	}
}
