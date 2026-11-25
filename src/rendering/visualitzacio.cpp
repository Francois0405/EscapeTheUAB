//******** PRACTICA VISUALITZACI� GR�FICA INTERACTIVA (Escola Enginyeria - UAB)
//******** Entorn b�sic VS2026 MONOFINESTRA amb OpenGL 4.6, interf�cie GLFW 3.4, ImGui i llibreries GLM
//******** Ferran Poveda, Marc Vivet, Carme Juli�, D�bora Gil, Enric Mart� (Setembre 2026)
// visualitzacio.cpp : Funcions de visualitzaci� i pantalla
// FUNCIONS:	- Iluminaci� (Iluminacio)
//				- Visualitzaci� Ortogr�fica (ProjeccioOrto i Ortografica)
//				- Visualitzaci� Perspectiva amb coord. esf�riques (ProjeccioPerspectiva(),Vista_Esferica())
//              - Visualitzaci� Perspectiva amb navegaci� per tecles cursor (ProjeccioPerspectiva(),Vista_Navega())
//				- Dibuixar els eixos (deixos)
//				- Incorporar plans de Grid2D (XY, XZ, YZ) i un Grid 3D (grid*())
//				- Dibuixar fons: blanc (FonsW) i negre (Fons)
//				- Carregar imatges de formats compatibles amb DevIL/OpenIL com a textura (loadIMA())
//				- Inicialitzar imatges textura per l'objecte Truck (Init_Textures())
//

#include "framework.h"
#include "src/rendering/RenderMetrics.h"

// Entorn V3D. QUATERNIONS: Include per a la definici� del tipus GL_QUAT i crida a les funcions de quatern.
#include "src/rendering/quatern.h"

#include "src/rendering/visualitzacio.h"
#include "src/scene/escena.h"

#include <algorithm>
#include <string>

// Iluminaci�: Configurar iluminaci� de l'escena
void Iluminacio(GLint sh_programID, char ilumin, bool ifix, bool ilu2sides, bool ll_amb, LLUM* lumin, char obj, bool frnt_fcs,
	bool bc_lin, int step)
{

// Variables per a configurar par�metres de les fonts de llum
	GLfloat especular[] = { 0.0,0.0,0.0,1.0 };
	GLfloat ambientg[] = { 0.26f, 0.27f, 0.30f, 1.0f };

// Definici� de llum ambient segons booleana ll_amb
//	if (ll_amb) glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambientg);	// Codi OpenGL 1.1
//		else glLightModelfv(GL_LIGHT_MODEL_AMBIENT,especular);		// Codi OpenGL 1.1
	if (ll_amb) glUniform4f(glGetUniformLocation(sh_programID, "LightModelAmbient"), ambientg[0], ambientg[1], ambientg[2], ambientg[3]);
	else glUniform4f(glGetUniformLocation(sh_programID, "LightModelAmbient"), especular[0], especular[1], especular[2], especular[3]);

// Passar llum fixe a Coordenades M�n o llum lligada a c�mera
	glUniform1i(glGetUniformLocation(sh_programID, "fixedLight"), ifix);
	// Subir todas las fuentes con un bucle: el rango debe coincidir con MaxLights en los shaders.
	for (int i = 0; i < NUM_MAX_LLUMS; ++i)
	{
		const LLUM& luz = lumin[i];
		const std::string prefijo = "LightSource[" + std::to_string(i) + "].";
		auto ubicacion = [&](const char* campo) { return glGetUniformLocation(sh_programID, (prefijo + campo).c_str()); };
		const GLfloat exterior = std::clamp(luz.spotcoscutoff, -0.999f, 0.999f);
		const GLfloat interior = luz.spotInnerCosCutoff > exterior
			? std::min(luz.spotInnerCosCutoff, 0.9999f)
			: std::cos(std::acos(exterior) * 0.72f);
		glUniform4f(ubicacion("position"), luz.posicio.x, luz.posicio.y, luz.posicio.z, luz.posicio.w);
		glUniform4f(ubicacion("diffuse"), luz.difusa.r, luz.difusa.g, luz.difusa.b, luz.difusa.a);
		glUniform4f(ubicacion("specular"), luz.especular.r, luz.especular.g, luz.especular.b, luz.especular.a);
		glUniform3f(ubicacion("attenuation"), luz.atenuacio.a, luz.atenuacio.b, luz.atenuacio.c);
		glUniform1i(ubicacion("restricted"), luz.restringida ? GL_TRUE : GL_FALSE);
		glUniform3f(ubicacion("spotDirection"), luz.spotdirection.x, luz.spotdirection.y, luz.spotdirection.z);
		glUniform1f(ubicacion("spotCosCutoff"), exterior);
		glUniform1f(ubicacion("spotInnerCosCutoff"), interior);
		glUniform1f(ubicacion("spotExponent"), luz.spotexponent);
		glUniform1i(ubicacion("sw_light"), luz.encesa ? GL_TRUE : GL_FALSE);
		glUniform1i(ubicacion("areaSource"), luz.areaSource ? GL_TRUE : GL_FALSE);
		glUniform3f(ubicacion("areaAxisU"), luz.areaAxisU.x, luz.areaAxisU.y, luz.areaAxisU.z);
		glUniform3f(ubicacion("areaAxisV"), luz.areaAxisV.x, luz.areaAxisV.y, luz.areaAxisV.z);
		glUniform2f(ubicacion("areaHalfSize"), luz.areaHalfSizeU, luz.areaHalfSizeV);
	}
// Selecci� del model d'iluminaci� segons variable ilumin
	switch(ilumin)
	{
	case PUNTS:
		glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
		break;

	case FILFERROS:
		glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
		break;

    case PLANA:
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		// PR�CTICA FRACTAL: Crida a la funci� pel c�lcul dels vectors normals normals a les cares si l'objecte �s un fractal
   
		// Il.luminaci� per cares planes
		glShadeModel(GL_FLAT); 
		break;

	case SUAU:
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		// PR�CTICA FRACTAL: Crida a la funci� pel c�lcul dels vectors normals normals a les cares si l'objecte �s un fractal

		// Il.luminaci� suau 
		glShadeModel(GL_SMOOTH);
		break;
	}
}


// -------- Entorn VGI: ORTOGR�FICA (Funcions Projeccio_Orto i Vista_Ortografica)

// Projeccio_Orto: Definici� Viewport i glOrtho 
// ---- Entorn VGI: ATENCI�!!. CAL DEFINIR PAR�METRES DE LA FUNCI�
glm::mat4 Projeccio_Orto()
{   
	glm::dmat4 MatriuProjeccio(1.0);
// ---- Entorn VGI: ATENCI�!!. ESPECIFICACIO FUNCIONS PER A DEFINIR VIEWPORT i PROJECCIO ORTOGR�FICA
//			        AMB ELS SEUS PAR�METRES. CONTINGUT SEMBLANT A FUNCI� Projeccio_Perspectiva().
	return MatriuProjeccio;
}

// Vista_Ortogr�fica: Ilumina i dibuixa l'escena. Crida a la funci� gluLookAt segons la variable prj 
//				(planta, al�at, perfil o axonometrica).
glm::mat4 Vista_Ortografica(GLuint sh_programID, int prj,GLdouble Raux,CColor col_fons,CColor col_object,char objecte,GLdouble mida,int step,
				bool frnt_fcs, bool oculta, bool testv, bool bck_ln, 
				char iluminacio, bool llum_amb, LLUM* lumi, bool ifix, bool il2sides, 
				bool eix, CMask3D reixa, CPunt3D hreixa)
{
	glm::mat4 MatriuVista(1.0);

// Iluminacio movent-se amb la camara (abans gluLookAt)
	if (!ifix) Iluminacio(sh_programID, iluminacio, ifix, il2sides, llum_amb, lumi, objecte, frnt_fcs, bck_ln, step);

// Implementaci� de planta,al�at,perfil i isom�trica 
// ---- Entorn VGI: ATENCI�!!. ESPECIFICACIO DE LA C�MERA
//								Cal definir la c�mera (glm::lookAt() ) en funci� del
//								tipus de projecci� definit a la variable prj.

// Pas Matriu a shader
	glUniformMatrix4fv(glGetUniformLocation(sh_programID, "viewMatrix"), 1, GL_FALSE, &MatriuVista[0][0]);

// Neteja dels buffers de color i profunditat
	Fons(col_fons);

// Iluminacio fixe respecte la camara (despr�s gluLookAt)
	if (ifix) Iluminacio(sh_programID, iluminacio, ifix, il2sides, llum_amb, lumi, objecte, frnt_fcs, bck_ln, step);

// Test de Visibilitat
	if (testv) glEnable(GL_CULL_FACE);
		else glDisable(GL_CULL_FACE);

// Ocultacions (Z-buffer)
	if (oculta) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);

// Dibuix de les cares back com a l�nies en Il.luminacio PLANA i SUAU
	if (bck_ln) glPolygonMode(GL_BACK, GL_LINE);
	
	return MatriuVista;
}

// -------- Entorn VGI: PERSPECTIVA (Funcions Projeccio_Perspectiva i Vista_Esferica)

// Projeccio_Perspectiva: Definici� Viewport i gluPerspective
glm::mat4 Projeccio_Perspectiva(GLuint sh_programID, int minx,int miny,GLsizei w,GLsizei h,double zoom)
{
	glm::mat4 MatriuProjeccio(1.0);

// Definici� Viewport
	glViewport(minx,miny,w,h);
	if (h==0) h=1;

// PROJECCIO PERSPECTIVA.Definim volum de visualitzaci� adaptant-lo 
//							a les mides actuals de la finestra windows. Amb glm::perspective()
	if (w >= h) MatriuProjeccio = glm::perspective(glm::radians(60.0), 1.0 * w / h, p_near, p_far);
		else MatriuProjeccio = glm::perspective(glm::radians(60.0), 1.0 * w / h, p_near, p_far);

// Pas Matriu a shader
	glUniformMatrix4fv(glGetUniformLocation(sh_programID, "projectionMatrix"), 1, GL_FALSE, &MatriuProjeccio[0][0]);

	return MatriuProjeccio;
}

// Vista_Esferica: Definici� gluLookAt amb possibilitat de moure el punt de vista interactivament amb el ratol�, 
//					ilumina i dibuixa l'escena
glm::mat4 Vista_Esferica(GLuint sh_programID,CEsfe3D opv,char VPol,bool pant,CPunt3D tr,CPunt3D trF,
				 CColor col_fons,CColor col_object,char objecte,double mida,int step, 
				 bool frnt_fcs, bool oculta, bool testv, bool bck_ln, 
				 char iluminacio, bool llum_amb, LLUM* lumi, bool ifix, bool il2sides,
				 bool eix, CMask3D reixa, CPunt3D hreixa)
{
	GLdouble cam[3] = { 0.0,0.0,0.0 }, up[3] = { 0.0,0.0,0.0 };
	glm::mat4 MatriuVista(1.0);
	glm::mat4 TransMatrix(1.0);	// Matrius Traslaci�

// Rectificar par�metre R si negatiu.
	if (opv.R < 1.0) opv.R = 1.0;

// Conversi� angles radians -> graus
	opv.alfa=opv.alfa*PI/180;
	opv.beta=opv.beta*PI/180;

// Neteja dels buffers de color i profunditat
	Fons(col_fons);

// Posici� c�mera i vector cap amunt
	if (VPol==POLARZ) { cam[0]=opv.R*cos(opv.beta)*cos(opv.alfa);
						cam[1]=opv.R*sin(opv.beta)*cos(opv.alfa);
						cam[2]=opv.R*sin(opv.alfa);		
						up[0]=-cos(opv.beta)*sin(opv.alfa);
						up[1]=-sin(opv.beta)*sin(opv.alfa);
						up[2]=cos(opv.alfa);	}
		else if (VPol==POLARY) {	cam[0]=opv.R*sin(opv.beta)*cos(opv.alfa);
									cam[1]=opv.R*sin(opv.alfa);
									cam[2]=opv.R*cos(opv.beta)*cos(opv.alfa);		
									up[0]=-sin(opv.beta)*sin(opv.alfa);
									up[1]=cos(opv.alfa);
									up[2]=-cos(opv.beta)*sin(opv.alfa);		}
			else {	cam[0]=opv.R*sin(opv.alfa);
					cam[1]=opv.R*cos(opv.beta)*cos(opv.alfa);
					cam[2]=opv.R*sin(opv.beta)*cos(opv.alfa);		
					up[0]=cos(opv.alfa);
					up[1]=-cos(opv.beta)*sin(opv.alfa);
					up[2]=-sin(opv.beta)*sin(opv.alfa);		}

// Iluminacio movent-se amb la camara (abans glLookAt)
	if (!ifix) Iluminacio(sh_programID,iluminacio, ifix, il2sides, llum_amb, lumi, objecte, frnt_fcs, bck_ln, step);

// Opci� pan: despla�ament del Centre de l'esfera (pant=1)
	if (pant) TransMatrix = glm::translate(TransMatrix, vec3(tr.x, tr.y, tr.z));	//glTranslatef(tr.x, tr.y, tr.z);
	TransMatrix = glm::translate(TransMatrix, vec3(trF.x, trF.y, trF.z));	// Traslaci� fixada amb la INSERT dins l'opci� pan

// Especificaci� del punt de vista
   //gluLookAt(cam[0],cam[1],cam[2],0.,0.,0.,up[0],up[1],up[2]);
   MatriuVista = glm::lookAt(
	   glm::vec3(cam[0], cam[1], cam[2]),	// Camera is here
	   glm::vec3(0, 0, 0),					// and looks here
	   glm::vec3(up[0], up[1], up[2])		// Head is up (set to 0,-1,0 to look upside-down)
	   );

// Concatenar matrius Traslaci� amb la de c�mera per al pan
	MatriuVista = TransMatrix * MatriuVista;

// Pas Matriu Vista a shader
   glUniformMatrix4fv(glGetUniformLocation(sh_programID, "viewMatrix"), 1, GL_FALSE, &MatriuVista[0][0]);

// Iluminacio fixe respecte la camara (despr�s glLookAt)
   if (ifix) Iluminacio(sh_programID,iluminacio, ifix, il2sides, llum_amb, lumi, objecte, frnt_fcs, bck_ln, step);

// Test de Visibilitat
	if (testv) glEnable(GL_CULL_FACE);
		else glDisable(GL_CULL_FACE);

// Ocultacions (Z-buffer)
	if (oculta) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);

	return MatriuVista;
}


//Vista_Navega: Definici� gluLookAt directament per par�metre, sense esf�riques.
//              amb possibilitat de moure el punt de vista interactivament amb les
//				tecles de cursor per poder navegar.
glm::mat4 Vista_Navega(GLuint sh_programID, CPunt3D pv, GLdouble n[3],GLdouble v[3],bool pant,CPunt3D tr,CPunt3D trF,
				  CColor col_fons,CColor col_object,char objecte,bool color, int step,
				  bool frnt_fcs, bool oculta, bool testv, bool bck_ln, 
				  char iluminacio, bool llum_amb, LLUM* lumi, bool ifix, bool il2sides,
				  bool eix, CMask3D reixa, CPunt3D hreixa)
{   double altfar=0;
	glm::mat4 MatriuVista(1.0), TransMatrix(1.0);

// Neteja dels buffers de color i profunditat
	Fons(col_fons);

// Iluminacio movent-se amb la camara (abans glLookAt)
	if (!ifix) Iluminacio(sh_programID,iluminacio, ifix, il2sides, llum_amb, lumi, objecte, frnt_fcs, bck_ln, step);

// Opci� pan: despla�ament del Centre de l'esfera (pant=true)
	if (pant) TransMatrix = glm::translate(glm::mat4(), vec3(tr.x, tr.y, tr.z)); // glTranslatef(tr.x, tr.y, tr.z);
	TransMatrix = glm::translate(TransMatrix, vec3(trF.x, trF.y, trF.z)); //glTranslatef(trF.x,trF.y,trF.z);	// Traslaci� fixada amb la INSERT dins l'opci� pan

// Especificaci� del punt de vista (C�MERA)
	//gluLookAt(pv.x,pv.y,pv.z,n[0],n[1],n[2],v[0],v[1],v[2]);
	MatriuVista = glm::lookAt(
		glm::vec3(pv.x, pv.y, pv.z), // Camera is here
		glm::vec3(n[0], n[1], n[2]), // and looks here
		glm::vec3(v[0], v[1], v[2])  // Head is up (set to 0,-1,0 to look upside-down)
		);

// Concatenar matrius Traslaci� amb la de c�mera per al pan
	MatriuVista = TransMatrix * MatriuVista;

// Pas Matriu a shader
	glUniformMatrix4fv(glGetUniformLocation(sh_programID, "viewMatrix"), 1, GL_FALSE, &MatriuVista[0][0]);

// Iluminacio fixe respecte la camara (despr�s glLookAt)
	if (ifix) Iluminacio(sh_programID,iluminacio, ifix, il2sides, llum_amb, lumi, objecte, frnt_fcs, bck_ln, step);

// Test de Visibilitat
	if (testv) glEnable(GL_CULL_FACE);
		else glDisable(GL_CULL_FACE);

// Ocultacions (Z-buffer)
	if (oculta) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);

	return MatriuVista;
}


//Vista_Geode: Definici� gluLookAt a partir de coordenades esf�riques, per� situant la c�mera a l'origen (0,0,0)
//              i colocant el punt on mira cap el punt (R,alfa,beta) amb la possibilitat de moure aquest darrer punt
//				interactivament amb les	tecles de cursor per a veure una c�pula.
glm::mat4 Vista_Geode(GLuint sh_programID, CEsfe3D opv, char VPol, bool pant, CPunt3D tr, CPunt3D trF,
	CColor col_fons, CColor col_object, char objecte, double mida, int step,
	bool frnt_fcs, bool oculta, bool testv, bool bck_ln,
	char iluminacio, bool llum_amb, LLUM* lumi, bool ifix, bool il2sides,
	bool eix, CMask3D reixa, CPunt3D hreixa)
{
	GLdouble cam[3] = {0.0, 0.0, 0.0}, camN[3] = { 0.0, 0.0, 0.0 }, up[3]{ 0.0, 0.0, 0.0 };
	glm::mat4 MatriuVista(1.0);
	glm::mat4 TransMatrix(1.0);	// Matrius Traslaci�

// Rectificar par�metre R si negatiu.
	if (opv.R < 1.0) opv.R = 1.0;

// Conversi� angles radians -> graus
	opv.alfa = opv.alfa * PI / 180;
	opv.beta = opv.beta * PI / 180;

// Neteja dels buffers de color i profunditat
	Fons(col_fons);

	// Posici� c�mera i vector cap amunt en GEODE
	if (VPol == POLARZ) {
		cam[0] = opv.R * cos(opv.beta) * cos(opv.alfa);
		cam[1] = opv.R * sin(opv.beta) * cos(opv.alfa);
		cam[2] = opv.R * sin(opv.alfa);
		// Vector camp on mira (cap a (R,alfa,beta)
		camN[0] = cos(opv.beta) * cos(opv.alfa);
		camN[1] = sin(opv.beta) * cos(opv.alfa);
		camN[2] = sin(opv.alfa);
		// Vector tangent (diferencial respecte alfa)
		up[0] = -cos(opv.beta) * sin(opv.alfa);
		up[1] = -sin(opv.beta) * sin(opv.alfa);
		up[2] = cos(opv.alfa);
		}
	else if (VPol == POLARY) {
		cam[0] = opv.R * sin(opv.beta) * cos(opv.alfa);
		cam[1] = opv.R * sin(opv.alfa);
		cam[2] = opv.R * cos(opv.beta) * cos(opv.alfa);
		// Vector camp on mira (cap a (R,alfa,beta)
		camN[0] = sin(opv.beta) * cos(opv.alfa);
		camN[1] = sin(opv.alfa);
		camN[2] = cos(opv.beta) * cos(opv.alfa);
		// Vector tangent (diferencial respecte alfa)
		up[0] = -sin(opv.beta) * sin(opv.alfa);
		up[1] = cos(opv.alfa);
		up[2] = -cos(opv.beta) * sin(opv.alfa);
		}
	else {
		cam[0] = opv.R * sin(opv.alfa);
		cam[1] = opv.R * cos(opv.beta) * cos(opv.alfa);
		cam[2] = opv.R * sin(opv.beta) * cos(opv.alfa);
		// Vector camp on mira (cap a (R,alfa,beta)
		camN[0] = sin(opv.alfa);
		camN[1] = cos(opv.beta) * cos(opv.alfa);
		camN[2] = sin(opv.beta) * cos(opv.alfa);
		// Vector tangent (diferencial respecte alfa)
		up[0] = cos(opv.alfa);
		up[1] = -cos(opv.beta) * sin(opv.alfa);
		up[2] = -sin(opv.beta) * sin(opv.alfa);
		}

// Iluminacio movent-se amb la camara (abans gluLookAt)
	if (!ifix) Iluminacio(sh_programID, iluminacio, ifix, il2sides, llum_amb, lumi, objecte, frnt_fcs, bck_ln, step);

// Opci� pan: despla�ament del Centre de l'esfera (pant=1)
	if (pant) TransMatrix = glm::translate(TransMatrix, vec3(tr.x, tr.y, tr.z));	//glTranslatef(tr.x, tr.y, tr.z);
	TransMatrix = glm::translate(TransMatrix, vec3(trF.x, trF.y, trF.z));	// Traslaci� fixada amb la INSERT dins l'opci� pan

// Especificaci� de la c�mera
	MatriuVista = glm::lookAt(
		glm::vec3(cam[0], cam[1], cam[2]), // Camera is here
		glm::vec3(cam[0] + camN[0], cam[1] + camN[1], cam[2] + camN[2]), // and looks here
		glm::vec3(up[0], up[1], up[2])  // Head is up (set to 0,-1,0 to look upside-down)
	);

// Concatenar matrius Traslaci� amb la de c�mera per al pan
	MatriuVista = TransMatrix * MatriuVista;

// Pas Matriu Vista a shader
	glUniformMatrix4fv(glGetUniformLocation(sh_programID, "viewMatrix"), 1, GL_FALSE, &MatriuVista[0][0]);

// Iluminacio fixe respecte la camara (despr�s gluLookAt)
	if (ifix) Iluminacio(sh_programID, iluminacio, ifix, il2sides, llum_amb, lumi, objecte, frnt_fcs, bck_ln, step);

// Test de Visibilitat
	if (testv) glEnable(GL_CULL_FACE);
	else glDisable(GL_CULL_FACE);

// Ocultacions (Z-buffer)
	if (oculta) glEnable(GL_DEPTH_TEST);
	else glDisable(GL_DEPTH_TEST);

	return MatriuVista;
}

// instancia: Aplica Transformacions Geometriques d'instanciaci�, segons els parametres 
//            definits a la persiana Transformacions
glm::mat4 instancia(bool TR, INSTANCIA tg, INSTANCIA tgF)
{
	glm::mat4 MatriuTG(1.0), MatriuT(1.0);

// Transformacions geometriques sobre objecte (Traslacio, Rotacions i Escalatge) amb conversi� angles Euler.
	if (TR) MatriuTG = glm::translate(MatriuTG, glm::vec3(tg.VTras.x, tg.VTras.y, tg.VTras.z));
	MatriuTG = glm::translate(MatriuTG, glm::vec3(tgF.VTras.x, tgF.VTras.y, tgF.VTras.z));	// Traslacio fixada amb la INSERT dins l'opcio Transformacions

// Convertir angles Euler tg i tgF en Matrius
	if (TR) {	MatriuT = glm::eulerAngleX(tg.VRota.x * PI / 180.f); // Passar angle en radians
				MatriuTG = MatriuTG * MatriuT;
			}
	MatriuT = glm::eulerAngleX(tgF.VRota.x * PI / 180.f); // Passar angle en radians
	MatriuTG = MatriuTG * MatriuT;
	if (TR) {	MatriuT = glm::eulerAngleY(tg.VRota.y * PI / 180.f); // Passar angle en radians
				MatriuTG = MatriuTG * MatriuT;
			}
	MatriuT = glm::eulerAngleY(tgF.VRota.y * PI / 180.f); // Passar angle en radians
	MatriuTG = MatriuTG * MatriuT;
	if (TR) {	MatriuT = glm::eulerAngleZ(tg.VRota.z * PI / 180.f); // Passar angle en radians
				MatriuTG = MatriuTG * MatriuT;
			}
	MatriuT = glm::eulerAngleX(tgF.VRota.z * PI / 180.f); // Passar angle en radians
	MatriuTG = MatriuTG * MatriuT;

	if (TR) MatriuTG = glm::scale(MatriuTG, vec3(tg.VScal.x, tg.VScal.y, tg.VScal.z));
	MatriuTG = glm::scale(MatriuTG, vec3(tgF.VScal.x, tgF.VScal.y, tgF.VScal.z));	// Escalatge fixada amb la INSERT dins l'opcio Transformacions

// Transformacions geometriques sobre objecte (Traslacio, Rotacions i Escalatge) amb Quaternions
	if (TR) MatriuTG = glm::translate(MatriuTG, glm::vec3(tg.VTras.x, tg.VTras.y, tg.VTras.z));
	MatriuTG = glm::translate(MatriuTG, glm::vec3(tgF.VTras.x, tgF.VTras.y, tgF.VTras.z));	// Traslacio fixada amb la INSERT dins l'opcio Transformacions

	return MatriuTG;
}


// deixos: Creaci� del VBO per al dibuix dels eixos coordenats
GLuint deixos()
{
	GLuint vaoId = 0;	// VAO Index
	GLuint vboId = 0;	// VBO Index

// vertex coords array for renderMetrics::DrawArrays() =====================================
// The World Coordinates Axes (WCA) are 3 lines, one over X axis (red), one over Y axis (green), one over Z axis(blue),
// the WCA cube consists of 6 vertices (3 axes * 4 vertices = 6 vertices). And, each
// vertex is 3 components (x,y,z) of doubles, therefore, the size of vertex
// array is 18 doubles (6 * 3 = 18). The size of each double is 4 bytes (72 * 4 = 288 bytes)
	GLfloat vertices[] = { 0.0f, 0.0f, 0.0f,  300.0f,   0.0f,   0.0f,  // X-Axis
						   0.0f, 0.0f, 0.0f,    0.0f, 300.0f,   0.0f,	 // Y-Axis
						   0.0f, 0.0f, 0.0f,    0.0f,   0.0f, 300.0f}; // Z-Axis

	GLfloat colors[] = { 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,   // Red color (X-Axis)
						 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,   // Green color (Y-Axis)
						 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f }; // Blue color (Z-Axis)

// Doblar el gruix de la linia dels eixos.
	//glLineWidth(2.0);

// Create Vertex Array Object (VAO) for 3D Model Cube
	glGenVertexArrays(1, &vaoId);

// Create vertex buffer objects for 3D Model attributes in the VAO
	glGenBuffers(1, &vboId);

// Bind our Vertex Array Object as the current used object
	glBindVertexArray(vaoId);

// Bind first VBO as being active buffer and storing vertex attributes (coordinates)
	glBindBuffer(GL_ARRAY_BUFFER, vboId);							// Activar VBO v�rtexs

	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices) + sizeof(colors), 0, GL_STATIC_DRAW);	// Allocate data to VBO starting from 0 offest

// Position Vertex attributes
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), &vertices);	// Copy geometry data to VBO starting from 0 offest
	glEnableVertexAttribArray(0);										// Enable attribute index 0 as being used (position)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);	// Specify that our coordinate data is going into attribute index 0 and contains 3 double

// Normal vertex Attributes
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(vertices), sizeof(colors), &colors); // Copy color data to VBO starting from 0 offest
	glEnableVertexAttribArray(1);										// Enable attribute index 1 as being used (normals)
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)sizeof(vertices));	// Specify that our color data is going into attribute index 0 and contains 3 double

//Unbind the registered VBO as current VBO
	glBindBuffer(GL_ARRAY_BUFFER, 0);

// Unbind VAO, to prevent bugs
	glBindVertexArray(0);

/*
// Eix X (vermell)
	glColor3f(1.0,0.0,0.0);
	glBegin(GL_LINES);
	 glVertex3f(0.0,0.0,0.0);
	 glVertex3f(300.0,0.0,0.0);
	glEnd();

// Eix Y (verd)
    glColor3f(0.0,1.0,0.0);
	glBegin(GL_LINES);
	 glVertex3f(0.0,0.0,0.0);
	 glVertex3f(0.0,300.0,0.0);
	glEnd();

// Eix Z (blau) 
	glColor3f(0.0,1.0,1.0);
	glBegin(GL_LINES);
	 glVertex3f(0.0,0.0,0.0);
	 glVertex3f(0.0,0.0,300.0);
	glEnd();
*/
	return vaoId;
}

// draw_Eixos: Dibuix del VBO dels eixos coordenats
void draw_Eixos(GLuint vaoId)
{
// Doblar el gruix de la linia dels eixos.
//	glEnable(GL_LINE_WIDTH);
//	glLineWidth(2.0);

	glBindVertexArray(vaoId);
	renderMetrics::DrawArrays(GL_LINES, 0, 6);
	glBindVertexArray(0);

// Restaurar el gruix de la linia dels eixos
//	glLineWidth(1.0);
//	glDisable(GL_LINE_WIDTH);
}


// ------------------- GRID (gridXY, gridYZ, gridXZ, gridXYZ) -------------------------

void draw_Grid(CMask3D quadricula, CPunt3D hquadricula)
{
// Switch on transparency to draw the grid over the scene
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

// Grids drawing
	if (quadricula.w) gridXYZ(GRID_SIZE);
	else  {	if (quadricula.x) gridXY(GRID_SIZE, hquadricula.z);
			if (quadricula.y) gridXZ(GRID_SIZE, hquadricula.y);
			if (quadricula.z) gridYZ(GRID_SIZE, hquadricula.x);
			}

// Switch off transparency
	glDisable(GL_BLEND);
}


// gridXY: Construeix un grid XY de tamany size amb pas 1 situat una al�ada h de l'eiz Z.
void gridXY(int size, GLdouble h)
{
	int i;
	//CColor col;

// VAO
	GLuint vaoId = 0, vboId = 0;
	std::vector <double> vertices, colors;		// Definici� vectors din�mics per a vertexs i colors 
	//vertices.resize(0);		colors.resize(0);	// Reinicialitzar vectors
	vertices.clear();		colors.clear();		// Reinicialitzar vectors

// Eix X
	//glColor4f(1.0f, 0.0f, 0.0f, 0.5f);
	for (i = -size; i < size + 1; i++)
		{	//glBegin(GL_LINES);
			  colors.push_back(1.0);		colors.push_back(0.0);			colors.push_back(0.0);	colors.push_back(1.0);
			  //glVertex3f(i, -size, h);	// glVertex3f(i, -0.5f, 0.0f);
			  vertices.push_back(-i);		vertices.push_back(-size);		vertices.push_back(h);
			  
			  colors.push_back(1.0);			colors.push_back(0.0);		colors.push_back(0.0);	colors.push_back(1.0);
			  //glVertex3f(i, +size, h);	// glVertex3f(i, +0.5f, 0.0f);
			  vertices.push_back(-i);		vertices.push_back(+size);		vertices.push_back(h);

			//glEnd();
		}

// Eix Y
	//glColor4f(0.0f, 1.0, 0.0f, 0.5f);
	for (i = -size; i < size + 1; i++)
		{	//glBegin(GL_LINES);
			  colors.push_back(0.0);		colors.push_back(1.0);		colors.push_back(0.0);	colors.push_back(1.0);
			  //glVertex3f(-size, i, h);	// glVertex3f(-0.5f, i, 0.0f);
			  vertices.push_back(-size);	vertices.push_back(i);		vertices.push_back(h);

			  colors.push_back(0.0);		colors.push_back(1.0);		colors.push_back(0.0);	colors.push_back(1.0);
			  //glVertex3f(+size, i, h);	// glVertex3f(+0.5f, i, 0.0f);
			  vertices.push_back(+size);	vertices.push_back(i);		vertices.push_back(h);
			//glEnd();
		}

// ----------------------- VBO
	std::vector <int>::size_type nv = vertices.size();	// Tamany del vector vertices en elements.
	std::vector <int>::size_type nc = colors.size();	// Tamany del vector colors en elements.

	draw_GL_LINES_VAO(vertices, colors);
}

// gridXZ: Construeix un grid YZ de tamany size amb pas 1 situat una al�ada h de l'eiz Z.
void gridXZ(int size, GLdouble h)
{
	int i;
	//CColor col;

// VAO
	//GLuint vaoId = 0, vboId = 0;
	std::vector <double> vertices, colors;		// Definici� vectors din�mics per a vertexs i colors 
	//vertices.resize(0);		colors.resize(0);	// Reinicialitzar vectors
	vertices.clear();		colors.clear();		// Reinicialitzar vectors


// Eix X
	//glColor4f(1.0f, 0.0, 0.0f, 0.5f);
	//col.r = 1.0f;	col.g = 0.0f;	col.b = 0.0f;	col.a = 0.5f;
	//SeleccionaColor(ref_mat, sw_mat, col);
	for (i = -size; i < size + 1; i++)
		{	//glBegin(GL_LINES);
			  colors.push_back(1.0);		colors.push_back(0.0);	colors.push_back(0.0);	colors.push_back(1.0);
			  // glVertex3f(i, h, -size);
			  vertices.push_back(i);		vertices.push_back(h);	vertices.push_back(-size);

			  colors.push_back(1.0);		colors.push_back(0.0);	colors.push_back(0.0);	colors.push_back(1.0);
			  //glVertex3f(i, h, +size); 
			  vertices.push_back(i);	vertices.push_back(h);		vertices.push_back(+size);
			//glEnd();
		}

// Eix Z
	//glColor4f(0.0f, 1.0, 1.0f, 0.5f);
	//col.r = 0.0f;	col.g = 1.0f;	col.b = 1.0f;	col.a = 0.5f;
	//SeleccionaColor(ref_mat, sw_mat, col);
	for (i = -size; i < size + 1; i++)
		{	//glBegin(GL_LINES);
			  colors.push_back(0.0);		colors.push_back(1.0);	colors.push_back(1.0); colors.push_back(1.0);
			  //glVertex3f(-size, h, i);
			  vertices.push_back(-size);	vertices.push_back(h);	vertices.push_back(i);

			  colors.push_back(0.0);		colors.push_back(1.0);	colors.push_back(1.0);	colors.push_back(1.0);
			  //glVertex3f(size, h, i);
			  vertices.push_back(size);	vertices.push_back(h);		vertices.push_back(i);
			//glEnd();
	}

// ----------------------- VBO
	std::vector <int>::size_type nv = vertices.size();	// Tamany del vector vertices en elements.
	std::vector <int>::size_type nc = colors.size();	// Tamany del vector colors en elements.

	draw_GL_LINES_VAO(vertices, colors);
}


// gridYZ: Construeix un grid YZ de tamany size amb pas 1 situat una al�ada h de l'eiz Z.
void gridYZ(int size, GLdouble h)
{
	int i;
	//CColor col;
	//bool ref_mat;
	//bool sw_mat[4];

// VAO, VBO
	//GLuint vaoId = 0, vboId = 0;
	std::vector <double> vertices, colors;		// Definici� vectors din�mics per a vertexs i colors 
	//vertices.resize(0);		colors.resize(0);	// Reinicialitzar vectors
	vertices.clear();		colors.clear();		// Reinicialitzar vectors

	//ref_mat = false;
	//sw_mat[0] = false;	sw_mat[1] = true;	sw_mat[2] = true;	sw_mat[3] = false;

// Eix Y
	//glColor4f(0.0f, 1.0, 0.0f, 0.5f);
	//col.r = 0.0f;	col.g = 1.0f;	col.b = 0.0f;	col.a = 0.5f;
	//SeleccionaColor(ref_mat, sw_mat, col);
	for (i = -size; i < size + 1; i++)
		{	//glBegin(GL_LINES);
		      colors.push_back(0.0);		colors.push_back(1.0);			colors.push_back(0.0);	colors.push_back(1.0);
			  //glVertex3f(h, i, -size);	// glVertex3f(0.0f, i, -0.5f);
			  vertices.push_back(h);		vertices.push_back(i);			vertices.push_back(-size);
			  
			  colors.push_back(0.0);		colors.push_back(1.0);			colors.push_back(0.0);	colors.push_back(1.0);
			  //glVertex3f(h, i, +size);	// glVertex3f(0.0f, i, 0.5f);
			  vertices.push_back(h);		vertices.push_back(i);			vertices.push_back(+size);

			//glEnd();
		}
/*
// Eix Z
	//glColor4f(0.0f, 1.0, 1.0f, 0.5f);
	//col.r = 0.0f;	col.g = 1.0f;	col.b = 1.0f;	col.a = 0.5f;
	//SeleccionaColor(ref_mat, sw_mat, col);
	for (i = -size; i < size + 1; i++)
		{	//glBegin(GL_LINES);
			  colors.push_back(0.0);		colors.push_back(1.0);			colors.push_back(1.0);	colors.push_back(1.0);
			  //glVertex3f(h, -size, i);	// glVertex3f(0.0, -0.5f, i);
			  vertices.push_back(h);		vertices.push_back(-size);		vertices.push_back(i);
			  
			  colors.push_back(0.0);		colors.push_back(1.0);			colors.push_back(1.0);	colors.push_back(1.0);
			  //glVertex3f(h, +size, i);	// glVertex3f(0.0f, 0.5f, i);
			  vertices.push_back(h);		vertices.push_back(+size);		vertices.push_back(i);
			//glEnd();
	}
*/
// ----------------------- VBO
	std::vector <int>::size_type nv = vertices.size();	// Tamany del vector vertices en elements.
	std::vector <int>::size_type nc = colors.size();	// Tamany del vector colors en elements.

	draw_GL_LINES_VAO(vertices, colors);
}


// gridXYZ: Construeix un grid 3D de tamany size amb pas 1.
void gridXYZ(int size)
{
	int i, j;
	//CColor col;
	//bool ref_mat;
	//bool sw_mat[4];

// VAO, VBO
	GLuint vaoId = 0, vboId = 0;
	std::vector <double> vertices, colors;		// Definici� vectors din�mics per a vertexs i colors 
	//vertices.resize(0);		colors.resize(0);	// Reinicialitzar vectors
	vertices.clear();		colors.clear();		// Reinicialitzar vectors

	//ref_mat = false;
	//sw_mat[0] = false;	sw_mat[1] = true;	sw_mat[2] = true;	sw_mat[3] = false;

	for (j = -size; j < size; j++)
	{
// Eix X
		//glColor4f(1.0f, 0.0, 0.0f, 0.5f);
		//col.r = 1.0f;	col.g = 0.0f;	col.b = 0.0f;	col.a = 0.5f;
		//SeleccionaColor(ref_mat, sw_mat, col);
		for (i = -size; i <= size; i++)
			{	//glBegin(GL_LINES);
				  colors.push_back(1.0);		colors.push_back(0.0);			colors.push_back(0.0);	colors.push_back(1.0);
				  //glVertex3f(i, -size, j);	// glVertex3f(i, -0.5f, z);
				  vertices.push_back(i);		vertices.push_back(-size);		vertices.push_back(j);

				  colors.push_back(1.0);		colors.push_back(0.0);			colors.push_back(0.0);	colors.push_back(1.0);
				  //glVertex3f(i, +size, j);	// glVertex3f(i, +0.5f, z);
				  vertices.push_back(i);		vertices.push_back(+size);		vertices.push_back(j);
				//glEnd();
			}

// Eix Y
		//glColor4f(0.0f, 1.0, 0.0f, 0.5f);
		//col.r = 0.0f;	col.g = 1.0f;	col.b = 0.0f;	col.a = 0.5f;
		//SeleccionaColor(ref_mat, sw_mat, col);
		for (i = -size; i <= size; i++)
			{	//glBegin(GL_LINES);
				  colors.push_back(0.0);		colors.push_back(1.0);		colors.push_back(0.0);	colors.push_back(1.0);
				  //glVertex3f(-size, i, j);	// glVertex3f(-0.5f, i, z);
				  vertices.push_back(-size);	vertices.push_back(i);		vertices.push_back(j);

				  colors.push_back(0.0);		colors.push_back(1.0);		colors.push_back(0.0);	colors.push_back(1.0);
				  //glVertex3f(+size, i, j);	// glVertex3f(+0.5f, i, z);
				  vertices.push_back(+size);	vertices.push_back(i);		vertices.push_back(j);
				//glEnd();
			}
	}

	draw_GL_LINES_VAO(vertices, colors);
}


// Fons: Dibuixa el fons variable sefons el color int_fons
void Fons(CColor int_fons)
{
	glClearColor(int_fons.r,int_fons.g,int_fons.b,1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0.f,0.f,0.f,1.f);
	glClear(GL_DEPTH_BUFFER_BIT);
   glFlush();
}

// FonsN: Dibuixa el fons negre
void FonsN()
{
	glClearColor(0.f,0.f,0.f,1.f);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
   glFlush();
}

// FonsB: Dibuixa el fons blanc
void FonsB()
{
	glClearColor(1.f,1.f,1.f,1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0.f,0.f,0.f,1.f);
	glClear(GL_DEPTH_BUFFER_BIT);
   glFlush();
}

// TEXTURES------------------------------------------------------
// loadIMA_SOIL: This function receives as input the image filename and an 
// integer identifier (0 for the first texture) and creates an OpenGL 
// texture which is stored in the global array 'textures'
// It uses SOIL library functions to load the image
// Par�metres:
//		- filename: Fitxer que cont� la imatge de qualsevol format:
//					BMP, JPG, TIFF, TGA, GIF, i d'altres suportats per SOIL
//		- Retorna: Identificador dins la taula textures on volem
//                assignar la imatge
GLint loadIMA_SOIL(const char * filename)
{	FILE *file = NULL;
	int err;
	GLuint textureID = 0;

// Open the image file for reading
//  file=fopen(nomf,"r");					// Funci� Visual Studio 6.0
	err = fopen_s(&file, filename, "rb");		// Funci� Visual 2010

// If the file is empty (or non existent) print an error and return false
// if (file == NULL)
	if (err != 0)
	{	//	printf("Could not open file '%s'.\n",filename) ;
		return false;
	}

// Close the image file
	fclose(file);

//Generate texture ID and load texture data 
	glGenTextures(1, &textureID);
	int width, height;
	//unsigned char* image = SOIL_load_image(filename, &width, &height, 0, SOIL_LOAD_RGB);	// Imatge 3 canals (RGB)
	unsigned char* image = SOIL_load_image(filename, &width, &height, 0, SOIL_LOAD_RGBA);	// Imatge 4 canals (RGBA)

// Assign texture to ID
	glBindTexture(GL_TEXTURE_2D, textureID);

	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);	// Imatge 3 canals (RGB)
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);	// Imatge 4 canals (RGBA)

	glGenerateMipmap(GL_TEXTURE_2D);

// Parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	SOIL_free_image_data(image);

// If execution arrives here it means that all went well. Return true
	return textureID;
}

// ----------------- CUBEMAP TEXTURE
// loads a cubemap texture from 6 individual texture faces
// order:
// +X (right)
// -X (left)
// +Y (top)
// -Y (bottom)
// +Z (front) 
// -Z (back)
// -------------------------------------------------------
unsigned int loadCubemap(std::vector<std::string> faces)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int width, height; // nrChannels;
	for (unsigned int i = 0; i < faces.size(); i++)
	{
		//unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
		unsigned char* data = SOIL_load_image(faces[i].c_str(), &width, &height, 0, SOIL_LOAD_RGB);
		if (data)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
			//stbi_image_free(data);
			SOIL_free_image_data(data);
		}
		else
		{	//std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
			fprintf(stderr, "Cubemap texture failed to load at path: %s \n", faces[i].c_str());
			//stbi_image_free(data);
			SOIL_free_image_data(data);
			return 0;
		}
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return textureID;
}


// SetTextureParameters: Funci� que configura par�metres de l'objecte textura IDText 
// Par�metres:
//		- num: N�mero de bloc de textura en el shader
//		- IDText: Identificador de l'objecte textura a configurar
//		- extensT: Ext�n la imatge textura fora dels l�mits normalitzats (0,1) [TRUE] o no [FALSE]
//		- filterT: Aplica filtre linieal a la imatge textura [TRUE] o pren p�xel m�s proper [FALSE]
//		- blendT: Aplica la funci� de blending tenint en compte lumin�ncia [TRUE] o nom�s la intensitat de la imatge textura [FALSE]
//		- genMipmpT: Implementa mipmaps sobre la imatge textura [TRUE] o no [FALSE]
void SetTextureParameters(GLint num, GLint IDText, bool extensT, bool filterT, bool blendT, bool genMipmpT)
{
	glActiveTexture(GL_TEXTURE0 + num);		// Activaci� del bloc de texture en el shader (texture<num>).
	glBindTexture(GL_TEXTURE_2D, IDText);

// Funci� de Blending de textura
	//if (blendT) glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	//else glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

// Extensi� de textures
	if (extensT) {	// Configuraci� textura per a que es repereixi m�s enll� de coordenades (0,0)-(1,1)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}
	else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}

// Filtratge de textures
	if (filterT) {	// Filtratge de textures (GL_LINEAR)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		if (genMipmpT) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		else glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else {	// Filtratge de textures (GL_NEAREST)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		if (genMipmpT) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		else glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}

// Activaci� de MipMaps de la imatge textura
	if (genMipmpT) {	// Activaci� MIPMAP en textures
		glGenerateMipmap(GL_TEXTURE_2D);
	}
}

