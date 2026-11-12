//******** PRACTICA VISUALITZACI� GR�FICA INTERACTIVA (Escola Enginyeria - UAB)
//******** Entorn b�sic VS2026 MONOFINESTRA amb OpenGL 4.6, interf�cie GLFW, ImGui i llibreries GLM
//******** Ferran Poveda, Marc Vivet, Carme Juli�, D�bora Gil, Enric Mart� (Setembre 2026)
// main.h : interface of the CEntornVGI class
//
/////////////////////////////////////////////////////////////////////////////

// Entorn VGI: Llibreries i constants Constants de l'aplicaci� EntornVGI
#include "framework.h"

// Entorn VGI: OBJECTE OBJ. Include per la definici� de l'objecte Obj_OBJ
#include "objLoader.h"	

//-------------- Entorn VGI: Variables globals de main

// Retrieving main monitor
	GLFWmonitor* primary;

// To get current video mode of a monitor
	const GLFWvidmode* mode;

// Entorn VGI: Finestra principal 
	GLFWwindow* window;

// Entorn VGI: Variable de control per a Status Bar (consola) 
	bool statusB;	// Opci� booleana per a mostrar Status Bar per consola [0:NO,1:SI]

// EntornVGI.ImGui: Gesti� de finestres de men� ImGui
	bool show_demo_window = false;
	bool show_another_window = false;
	bool show_EntornVGI_window = false;
	bool show_window_about = false;

// Entorn VGI: Variables de control per Men� CAMERA: Esf�rica, Navega, M�bil, Zoom, Satelit, Polars... 
	char camera;	// Variable que controla el tipus de c�mera segons valors definits en constants.h
	static int oCamera;	// Variable que controla desplegable CAMERA d'ImGui
	bool mobil;		// Opci� canvi de Punt de Vista interactiu (mobil) [0:NO,1:SI]
	bool zzoom;		// Opci� Zoom interactiu (zoom) [0:NO,1:SI]
	bool zzoomO;	// Opci� Zoom en Projecci� Ortogr�fica adaptant Volum Visualitzaci� [0:NO,1:SI]
	bool satelit;	// Opci� de navegaci� animada al volt de l'objecte centrat definint increment per mouse (SAT�LIT)

// Entorn VGI: Variables de control de l'opci� CAMERA->ESF�RICA?
	float cam_Esferica[3];		// ImGui: Coordenades Esf�riques C�mera
	CEsfe3D OPV;				// Par�metres camera en coord. esf�riques (R,alfa,beta)
	char Vis_Polar;				// Variable que controla orientaci� dels eixos en Visualitzaci� Interactiva (POLARZ,POLARY,POLARX)
	glm::mat4 ViewMatrix(1.0);	// Matriu de C�mera a passar al shader
	static int oPolars ;		// ImGui: Variable que controla Botons POLARS d'ImGui

// Entorn VGI: Variables de control de l'opci� C�MERA->NAVEGA?
	GLdouble n[3];				// Punt cap on mira.
	CPunt3D opvN;				// Definici� Or�gen Punt de Vista (en coordenades m�n)
	double angleZ;				// angle de rotaci� de la traject�ria.

// Entorn VGI: Variables de control de l'opci� C�MERA->GEODE?
	CEsfe3D OPV_G;			// Par�metres camera en coord. esf�riques (R,alfa,beta) per a Vista Geode

// Entorn VGI: Variables de control per Men� VISTA: fullsecreen, pan, dibuixar eixos i grid. 
	bool fullscreen;	// Opci� booleana per Pantalla Completal [0:NO,1:SI]
	bool pan;			// Opcio Pan. Despla�ament del centre de l'esfera de Vista [0:NO,1:SI]
	bool eixos;			// Opci� per dibuixar els eixos X,Y,Z (Eixos?) [0:NO,1:SI]
	Shader shaderEixos;	// Ientificador pel shader d'eixos.
	GLuint eixos_programID;
	GLuint eixos_Id;	// Identificador del VBO per a dibuixar els eixos.
	bool sw_grid;		// Opci� per activar comandes teclat per moure grids [TRUE] o no [FALSE]
	CMask3D grid;		// Opci� per a dibuixar grids (.x: grid XY, .y:grid XZ, .z: grid YZ, .w:grid XYZ)
	CPunt3D hgrid;		// Al�ada de cada grid (.x: grid XY, .y:grid XZ, .z: grid YZ, .w:grid XYZ)

// Entorn VGI: Variables de l'opci� VISTA->PAN?
	double fact_pan;	// Factor de despla�ament de la c�mara (opci� pan).
	CPunt3D tr_cpv;		// Vector de Traslaci� del Centre del Punt de Vista actiu dins opci� pan.
	CPunt3D tr_cpvF;	// Vector de Traslaci� del Centre del Punt de Vista (fixat amb INSERT dins opci� pan) 
						//       i actiu fora l'opci� pan.

// Entorn VGI: Variables de control per les opcions de men� PROJECCI�, OBJECTE
	char projeccio;					// Men� Projecci�: Tipus de projeccio
	static int oProjeccio;			// ImGui: Variable que controla desplegable TIPUS PROJECCIO d'ImGui
	glm::mat4 ProjectionMatrix(1.0);// Matriu de Projecci� a passar al shader
	char objecte;					// Men� Objecte: Tipus d'Objecte
	static int oObjecte;			// ImGui: Variable que controla desplegable TIPUS OBJECTE d'ImGui

// Entorn VGI: Variables de control Skybox Cube
	bool SkyBoxCube;			// Booleana que controla si es visualitza Skybox [TRUE] o no [FALSE].
	Shader shader_SkyBoxC;		// Shader Skybox Cub
	GLuint skC_programID;		// Identificador program Skybox Cube
	CVAO skC_VAOID;				// Identificador VAO List (vaoId, vboId, nVertexs) per a Skybox Cube
	GLuint cubemapTexture;		// Identificador textura cubemap.
	Shader shaderSkyBox;		// Shader SkyBox

// Entorn VGI: Variables de control del men� TRANSFORMA
	bool transf;	// Booleana per activar Transformacions (true) o no (false).
	bool trasl;		// Booleana d'activaci� de la Traslaci� (true) o no (false).
	bool rota;		// Booleana d'activaci� de la Rotaci� (true) o no (false).
	bool escal;		// Booleana d'activaci� de l'Escalatge (true) o no (false).
	double fact_Tras, fact_Rota;	// Increments de Traslaci� i Rotaci�.
	INSTANCIA TG;	// Estructura que cont� TG del men� Transforma actiu dins qualsevol opci� de Transforma 
					//      (Traslaci� Rotaci� i Escalatge).
	INSTANCIA TGF;	// Estructura que cont� TG del men� Transforma fixat amb INSERT dins les opcions de Transforma
					//     i actiu fora l'opci� Transforma.
	bool transX;	// Opci� Mobil Eix X?: TG interactives per l'eix X via mouse [F:NO,T:SI].
	bool transY;	// Opci� Mobil Eix Y?: TG interactives per l'eix Y via mouse [F:NO,T:SI].
	bool transZ;	// Opci� Mobil Eix Z?: TG interactives per l'eix Z via mouse [F:NO,T:SI].
	glm::mat4 GTMatrix; // Matriu de Transformacions Geom�triques a passar al shader

// Entorn VGI: Variables de control per les opcions de men� OCULTACIONS
	bool front_faces;// Men� ocultacions: Determina si les cares visibles s�n les frontals [true-defecte] o les back [false] pel Test de Visibilitat.
	bool test_vis;  // Men� ocultacions: Activa [true] o desactiva [false] Test Visibilitat.
	bool oculta;    // Men� ocultacions: Activa [true] o desactiva [false] Ocultacions (Z-buffer).
	bool back_line; // Men� ocultacions: Activa [true] o desactiva [false] dibuix cares back com a l�nies.

// Entorn VGI: Variables de control del men� ILUMINACI�
	bool ifixe;         // Iluminaci� fixe independent del PV (ifixe=1) 
						// o depenent (mobil) respecte el PV (casc miner) (ifixe=0)
	bool ilum2sides;	// Iluminaci� de les cares FRONT i BACK [TRUE] o nom�s les FRONT [FALSE]
	char ilumina;		// Tipus d'il.luminaci� [FILFERROS 'f',PLANA 'c',SUAU 'c']
	static int oIlumina = 1;	// Variable que controla desplegable TIPUS ILUMINACIO d'ImGui
	bool sw_material[5];// Variable que controla els coeficients de reflectivitat del material [TRUE] o no [FALSE]:
						// [0]: emission, [1]: ambient, [2]: difusa, [3] especular.
	bool sw_material_old[5]; // Variable que guarda els valors de sw_material mentre no hi ha reflectivitat de material (refl_material=false).
	bool textura;       // Control de textures desactivades [0:NO] o activades [1:SI]
	char t_textura;		// Tipus de textures (predefinides o per fitxer)
	bool textura_map;	// Mapping de textura modulat amb la llum [TRUE] o calcat [FALSE]
	GLuint texturesID[NUM_MAX_TEXTURES];// Vector d'identificadors de textura de l'entorn. Si no hi ha textura activa, agafa el valor -1.
										// 0: Textura general, assignada a la lectura de fitxer.
										// 1-6: Textures de l'objecte Truck
										// 7: Textura pel Fractal
										// 8-9: Lliures
	bool tFlag_invert_Y; // Booleana que activa la inversi� coordenada textura t (o Y) a 1.0-cty segons llibreria SOIL (TRUE) o no (FALSE).

// Entorn VGI: Variables de control del men� LLUMS
	bool llum_ambient;		// Booleana que controla la llum ambient (SI/NO).
	LLUM llumGL[NUM_MAX_LLUMS];		// Vector de llums d'OpenGL
	bool sw_llambient;		// Booleana que controla modus de configurar el color de la llum ambient [TRUE] o no [FALSE]
	CColor col_llambient;	// Color llum ambient.

// Entorn VGI: Variables de control del men� SHADER
	char shader;				// Tipus de shader [FLAT, GOURAUD, PHONG, FILE, PROG_BINARY_SHADER]
	static int oShader;			// Variable que controla desplegable SHADER d'ImGui
	GLuint shader_programID;	// Shader Program que cont� el Vertex i Fragment program.
	Shader shaderLighting;		// Shader que implementa els codis GLSL d'il.luminaci�.
	
// Entorn VGI: Variables de control dels botons de mouse
	CPoint m_PosEAvall, m_PosDAvall;	// Coordenades del cursor quan el boto esquerre(E) o dret(D) del 
										//    mouse ha estat clicat.
	bool m_ButoEAvall, m_ButoDAvall;	// TRUE si el boto esquerre(E) o dret(D) del mouse esta clicat.
	CEsfe3D m_EsfeEAvall;				// Coordenades Esf�riques del PV (OPV) quan el boto esquerre(E) o dret(D) del 
										//		mouse ha estat clicat.
	CEsfe3D m_EsfeIncEAvall;			// Increment de despla�ament en coordenades Esf�riques del PV (OPV).

// Entorn VGI: Variables que controlen par�metres visualitzaci�: Mides finestra Windows i PV (c�mera)
	int w, h;				// Mides de la finestra Windows (w-amplada,h-al�ada)
	int width_old, height_old;	// Mides de la resoluci� actual de la pantalla (finestra Windows)
	int w_old, h_old;		// Mides de la finestra Windows (w-amplada,h-al�ada) per restaurar Finestra des de fullscreen

// Entorn VGI: Color de fons i de l'objecte
	bool fonsR, fonsG, fonsB;	// Booleanes per controlar variaci� de color per teclat.
	CColor c_fons;				// Intensitat de color de fons.
	bool sw_color;				// Booleana que controla el modus de configurar color de l'objecte per teclat [TRUE] o no [FALSE]
	ImVec4 clear_colorB;		// ImGui: Color de fons
	ImVec4 clear_colorO;		// ImGui: Color de l'objecte.
	CColor col_obj;				// Color de l'objecte simple.

// Entorn VGI: Objecte OBJ:
	COBJModel* ObOBJ;		// Variable d'objecte format OBJ (*.OBJ)
	CVAO vao_OBJ;			// Identificador VAO per a objecte OBJ

// Entorn VGI: OBJECTE --> Corbes Bezier, BSpline, Catmull-Rom i Loxodroma
	int npts_T;							// N�mero de punts de control de la corba en el Vector corbaSpline (<=MAX_PATH_SPLINE)
	CPunt3D PC_t[MAX_PATCH_CORBA];		// Vector que enmagatzema Punts de Control Corba Spline
	GLdouble pas_Corba;					// Increment del par�metre t per al dibuix de les corbes.
	GLdouble pas_CS;					// Increment del par�metre t per al dibuix de corbes i superficies.
	bool sw_Punts_Control;				// Booleana que activa o desactiva la visualitzaci� dels punts de control de la corba o de la superficie

// Entorn VGI. TRIEDRE DE FRENET / DARBOUX: VT: vector Tangent, VNP: Vector Normal Principal, VBN: vector BiNormal
	bool dibuixa_TriedreFrenet;			// Booleana que controla dibuix de Triedre de Frenet per a cada punt de la Corba [TRUE-dibuixa, FALSE-no dibuixa]
	bool dibuixa_TriedreDarboux;		// Booleana que controla dibuix de Triedre de Darboux per a cada punt de la Corba Loxodroma[TRUE-dibuixa, FALSE-no dibuixa]
	CPunt3D VT, VNP, VBN;				// TRIEDRE de FRENET: VT: Vector Tangent, VNP: Vector Normal Principal, VBN: Vector BiNormal.

// Entorn VGI: Variables del Timer
	double t;		// Par�metre t pel Timer.
	bool anima;		// Booleana que controla si l'animaci� �s activa (TRUE) o no (FALSE)
					//    dins la funci� de control del rellotge OnTimer.

// Entorn VGI : QUATERNIONS: Animaci� de cossos r�gids per QUATERNIONS
	bool animaQ;			// Booleana que controla si l'animaci� r�gida �s activa 
	bool rotaQ;				// Booleana que verifica si les rotacions es fan per Quaternions.
/*
	CPunt3D keyf_Rota[MAX_KEYFRAMES_Q]; // Estructura que guarda fins a 3 keyframes d'orientaci� de l'objecte r�gid
										//	en angles d'Euler. Es graben amb la tecla INSERT.
	GL_Quat keyf_Quat[MAX_KEYFRAMES_Q]; // Estructura que guarda fins a 3 keyframes d'orientaci� de l'objecte r�gid
										//	en Quaternions. Es graben amb la tecla INSERT.
	CPunt3D keyf_Tras[MAX_KEYFRAMES_Q]; // Estructura que guarda fins a 3 keyframes de TRASLACI� de
										//	l'objecte r�gid. Es graben amb la tecla INSERT.
	CPunt3D keyf_Scal[MAX_KEYFRAMES_Q]; // Estructura que guarda fins a 3 keyframes d'ESCALATGE de
										//	l'objecte r�gid. Es graben amb la tecla INSERT.
	CPunt3D eix_Rota;		// Eix de rotaci� interpolat resultat dels quaternions.
	GLdouble angle_Rota;		// Angle de rotaci� interpolat dels quaternions.
	GLdouble QMatrix[16];	// Matriu de rotaci� corresponent a un quaterni�.
	CPunt3D int_Tras;		// Valors de Traslaci� interpolats en moviment rigid.
	CPunt3D int_Scal;		// Valors d'Escalat interpolats en moviment r�gid.
	int np_InQ;				// Controla el n�mero de keyframes entrats.
	bool animaQ;			// Booleana que controla si l'animaci� r�gida �s activa
							//   (TRUE) o no (FALSE).
	CPunt3D keyf_RotaI, keyf_RotaF, keyf_RotaT;	// Angles Euler inicial  (keyf_RotaI), final (keyf_RotaF) i
												// intermig (keyf_RotaT) del moviment d'objectes r�gids.
	GL_Quat qT;					// Quaterni� que acumula ordenadament les rotacions aplicades sobre l'objecte en moviment d'objectes r�gids.
	GL_Quat qI, qF;				// Quaternions inicial (qI), final (qF) del moviment d'objectes r�gids.
	GL_Quat qT_L, qT_S;			// Quaternions intermitjos per interpolaci� linial (qT_L) i esf�rica (qT_S) del moviment d'objectes r�gids.
	GL_Quat qI_E, qF_E, qT_E;	// Quaternions inicial (qI_E), final (qF_E) i intermig (qT_E) convertits i interpolats dels angles d'Euler.
	bool lerp, slerp;			// Booleanes que activen la interpolaci� linial (lerp) i
								//     esf�rica (slerp).
*/

// Entorn VGI: Variables de l'objecte FRACTAL
	char t_fractal;		// Tipus de fractal.
	char soroll;		// Men� Fractals: Tipus de soroll
	int pas, pas_ini;	// Resoluci� del fractal inicial (pas_ini) i de visualitzaci� (pas).
	bool sw_il;			// Booleana que controla si cal compilar el fractal (sw_il=1) o no (sw_il=0)
	bool palcolFractal;	// Booleana que activa coloraci� del fractal segons paleta de colors [TRUE] o no [FALSE].
/*
	bool zzoomF;		// variable que activa [TRUE] o desactiva [FALSE] el Zoom Fractal.
	char paletaFractal;	// Variable que cont� la paleta de colors a aplicar el fractal.
	CVAO vaoFractal;	// Variable VAO per al Fractal
	int nvertexsFractal; // Nombre de v�rtexs del Fractal per a VAO.
	CVAO vaoMar;		// Variable VAO per al mar del Fractal
*/

// Entorn VGI: ROBOT ARTICULAT
	bool animaR;		// Booleana que controla si l'animaci� del robot �s activa (TRUE) o no (FALSE).
	bool sw_robot;		// Switch que activa (TRUE) o desactiva (FALSE) moviments de robot per teclat.
	bool sh_rmov;		// Shift que intercanvia els moviments del robot per teclat. 
/*
	double angles_R[6]; // Vector d'angles que indiquen la posici� del bra� del robot / Synkope
						// [0]: Rotaci� Z del bra�.
						// [1]: Rotaci� X del bra�.
						// [2]: Rotaci� Z del canell.
						// [3]: Rotaci� X del canell.
						// [4]: Rotaci� Y del canell.
						// [5]: Tancament de la ma.
	bool sh_rmov;	// Shift que intercanvia els moviments del robot per teclat.
						//   (TRUE: moviments bra� i m�, FALSE: moviments canell)
	double pos_RI[6], pos_RF[6];	// Angles que defineixen dos keyframes: posici� inicial (pos_RI)
									// i final del robot per fer l'animaci�. Es graben amb la
									// tecla INSERT.
	double keyf_R[MAX_KEYFRAMES][6];	// Estructura que guarda fins a 5 keyframes de la cama articulada (0: Esquerra, 1: Dreta).
										//    Es graben amb la tecla INSERT.
	int np_In;				// Controla el n�mero de punts de l'animaci� entrats
	GLdouble num_frame;		// N�mero de frame de l'animaci�.
	GLdouble incr_frame;	// Increment positiu/negatiu per assegurar continuitat a l'animaci�.
*/


// Entorn VGI: Altres variables
	GLdouble mida;		// Factor d'escala per calcular Volum de Visualitzaci� de l'objecte que encaixi.
	std::string nom;	// Nom de fitxer.
	std::string buffer; // Buffer que magatzema string caracters corresponent a variables double a printar en Status Bar (funci� Barra_Estat).
//-------------- Entorn VGI: Fi De Variables globals de main

//-------------- Entorn VGI: Declaraci� funcions main

// Inicialitzaci� variables de control
	void InitGL();	
	

// Entorn VGI: Control de l'EVENT ONSIZE
	void OnSize(GLFWwindow* window, int width, int height);

	void OnPaint(GLFWwindow* window);
	void configura_Escena();
	void dibuixa_Escena();
	void Barra_Estat();

// EntornVGI: Funcions de mostrar Finestres ImGui
	void draw_Menu_ImGui();
	void MostraEntornVGIWindow(bool* p_open);
	void ShowArxiusOptions();
	void ShowAboutWindow(bool* p_open);
	int shortCut_Camera();
	int shortCut_Polars();
	int shortCut_Projeccio();
	int shortCut_Objecte();
	int shortCut_Iluminacio();
	int shortCut_Shader();
	void ShowEntornVGIWindow(bool* p_open);

// EntornVGI: RECURSOS DE MENU (persianes) DE L'APLICACIO:
	void OnArxiuObrirFractal();
	void OnArxiuObrirFitxerObj();
	void OnArxiuObrirFitxerFontLlum();
	bool llegir_FontLlum(char* nomf);
	void OnArxiuObrirSkybox();
	// Desplegable CAMERA
	void OnCameraEsferica();
	void OnCameraOrigenEsferica();
	void OnVistaMobil();
	void OnVistaZoom();
	void OnVistaZoomOrto();
	void OnVistaSatelit();
	void OnVistaPolarsX();
	void OnVistaPolarsY();
	void OnVistaPolarsZ();
	void OnCameraNavega();
	void OnCameraOrigenNavega();
	void OnCameraGeode();
	void OnCameraOrigenGeode();
	// Desplegable VISTA
	void OnVistaFullscreen();
	void OnVistaPan();
	void OnVistaOrigenPan();
	void OnVistaEixos();
	void OnVistaSkyBox();
	// Desplegable PROJECCIO
	void OnProjeccioPerspectiva();
	void OnProjeccioOrtografica();
	void OnProjeccioAxonometrica();
	// Desplegable OBJECTE
	void OnObjecteCap();
	void OnObjecteCub();
	void OnObjecteCubRGB();
	void OnObjecteEsfera();
	void OnObjecteTetera();
	void OnObjecteArc();
	void OnObjecteTie();
	void OnObjecteCorbaBezier();
	void OnObjecteCorbaLemniscata();
	void OnObjecteCorbaHermitte();
	void OnObjecteCorbaCatmullRom();
	void OnObjecteCorbaBSpline();
	void OnObjectePuntsControl();
	void OnCorbesTriedreFrenet();
	void OnCorbesTriedreDarboux();
	void OnObjecteMatriuPrimitives();
	void OnObjecteMatriuPrimitivesVAO();
	// Desplegable TRANSFORMA
	void OnTransformaTraslacio();
	void OnTransformaOrigenTraslacio();
	void OnTransformaRotacio();
	void OnTransformaOrigenRotacio();
	void OnTransformaEscalat();
	void OnTransformaOrigenEscalat();
	void OnTransformaMobilX();
	void OnTransformaMobilY();
	void OnTransformaMobilZ();
	// Desplegable OCULTACIONS
	void OnOcultacionsFrontFaces();
	void OnOcultacionsTestvis();
	void OnOcultacionsZBuffer();
	// Desplegable ILUMINACI�
	void OnIluminacioLlumfixe();
	void OnIluminacio2Sides();
	void OnIluminacioPunts();
	void OnIluminacioFilferros();
	void OnIluminacioPlana();
	void OnIluminacioSuau();
	void OnMaterialReflmaterial();
	void OnMaterialEmissio();
	void OnMaterialAmbient();
	void OnMaterialDifusa();
	void OnMaterialEspecular();
	void OnIluminacioTextures();
	void OnIluminacioTexturaFitxerimatge();
	void OnIluminacioTexturaFlagInvertY();
	// Desplegable LLUMS
	void OnLlumsLlumAmbient();
	void OnLlumsLlum0();
	void OnLlumsLlum1();
	void OnLlumsLlum2();
	void OnLlumsLlum3();
	void OnLlumsLlum4();
	void OnLlumsLlum5();
	void OnLlumsLlum6();
	void OnLlumsLlum7();
	// Desplegable SHADERS
	void OnShaderFlat();
	void OnShaderGouraud();
	void OnShaderPhong();
	void OnShaderLoadFiles();
	void OnShaderPBinaryWrite();
	void OnShaderPBinaryRead();

// Entorn VGI: Control de l'EVENT TECLAT
	void OnKeyDown(GLFWwindow* window, int key, int scancode, int action, int mods);

// Entorn VGI: Funcions de tractament de teclat en diferents modus
	void Teclat_Shift(int key, GLFWwindow* window);
	void Teclat_Ctrl(int key);
	void Teclat_ColorObjecte(int key, int action);
	void Teclat_ColorFons(int key, int action);
	void Teclat_Navega(int key, int action);
	void Teclat_Pan(int key, int action);
	void Teclat_TransEscala(int key, int action);
	void Teclat_TransRota(int key, int action);
	void Teclat_TransTraslada(int key, int action);
	void Teclat_Grid(int key, int action);
	void Teclat_PasCorbes(int key, int action);

// Entorn VGI: Control de l'EVENT MOUSE
	void OnMouseButton(GLFWwindow* window, int button, int action, int mods);
	void OnMouseMove(GLFWwindow* window, double xpos, double ypos);
	void OnMouseWheel(GLFWwindow* window, double xoffset, double yoffset);


// Entorn VGI: Control de l'EVENT TIMER
	void OnTimer();

// ---------------- Entorn VGI: Funcions locals a main.cpp
	int Log2(int num);							// Log2: C�lcul del log base 2 de num
	int llegir_ptsC(const char* nomf);				// Llegir Punts Control Corbes Bezier i B-Spline
	void OnFull_Screen(GLFWmonitor* monitor, GLFWwindow* window);
// ---------------- Entorn VGI: Tractament d'errors
	void error_callback(int code, const char* description);
	void APIENTRY glDebugOutput(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
		const GLchar* message, const void* userParam);
