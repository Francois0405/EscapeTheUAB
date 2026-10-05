//******** PRACTICA VISUALITZACIÓ GRÀFICA INTERACTIVA (Escola Enginyeria - UAB)
//******** Entorn bàsic VS2026 MONOFINESTRA amb OpenGL 4.6, interfície GLFW, ImGui i llibreries GLM
//******** Ferran Poveda, Marc Vivet, Carme Julià, Débora Gil, Enric Martí (Setembre 2026)
// main.h : interface of the CEntornVGI class
//
/////////////////////////////////////////////////////////////////////////////


// Entorn VGI: Llibreries i constants Constants de l'aplicació EntornVGI
#include "framework.h"

// Entorn VGI: OBJECTE OBJ. Include per la definició de l'objecte Obj_OBJ
#include "objLoader.h"	
#include <memory>

//-------------- Entorn VGI: Variables globals de main

enum class SceneObjectType
{
	Cube,
	Obj
};

struct SceneObject
{
	std::string name;
	SceneObjectType type;
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;
	glm::vec4 color;
	std::unique_ptr<COBJModel> model;
};

std::vector<SceneObject> sceneObjects;
int selectedSceneObject = -1;
glm::vec3 freeCameraPosition(24.0f, -32.0f, 16.0f);
float freeCameraYaw = 127.0f;
float freeCameraPitch = -22.0f;
bool freeCameraMouseCaptured = false;
bool freeCameraFirstMouse = true;
double freeCameraLastMouseX = 0.0;
double freeCameraLastMouseY = 0.0;

// Retrieving main monitor
	GLFWmonitor* primary;

// To get current video mode of a monitor
	const GLFWvidmode* mode;

// Entorn VGI: Finestra principal 
	GLFWwindow* window;

// Entorn VGI: Variable de control per a Status Bar (consola) 
	bool statusB;	// Opció booleana per a mostrar Status Bar per consola [0:NO,1:SI]

// EntornVGI.ImGui: Gestió de finestres de menú ImGui
	bool show_demo_window = false;
	bool show_another_window = false;
	bool show_EntornVGI_window = false;
	bool show_window_about = false;

// Entorn V3D: Variables de control per Menú Càmera: Esfèrica, Navega, Mòbil, Zoom, Satelit, Polars... 
	char camera;	// Variable que controla el tipus de càmera segons valors definits en constants.h
	static int oCamera = 3;	// Variable que controla desplegable CAMERA d'ImGui
	bool mobil;		// Opció canvi de Punt de Vista interactiu (mobil) [0:NO,1:SI]
	bool zzoom;		// Opció Zoom interactiu (zoom) [0:NO,1:SI]
	bool zzoomO;	// Opció Zoom en Projecció Ortogràfica adaptant Volum Visualització [0:NO,1:SI]
	bool satelit;	// Opció de navegació animada al volt de l'objecte centrat definint increment per mouse (SATÈLIT)

// Entorn VGI: Variables de control de l'opció Càmera->Navega?
	GLdouble n[3];				// Punt cap on mira.
	CPunt3D opvN;				// Definició Orígen Punt de Vista (en coordenades món)
	double angleZ;				// angle de rotació de la trajectòria.
	glm::mat4 ViewMatrix(1.0);	// Matriu de Càmera a passar al shader

// Entorn VGI: Variables de control de l'opció Càmera->Geode?
	CEsfe3D OPV_G;			// Paràmetres camera en coord. esfèriques (R,alfa,beta) per a Vista Geode

// Entorn VGI: Variables de control per Menú Vista: fullsecreen, pan, dibuixar eixos i grid. 
	bool fullscreen;	// Opció booleana per Pantalla Completal [0:NO,1:SI]
	bool pan;			// Opcio Pan. Desplaçament del centre de l'esfera de Vista [0:NO,1:SI]
	bool eixos;			// Opció per dibuixar els eixos X,Y,Z (Eixos?) [0:NO,1:SI]
	Shader shaderEixos;	// Ientificador pel shader d'eixos.
	GLuint eixos_programID;
	GLuint eixos_Id;	// Identificador del VBO per a dibuixar els eixos.
	bool sw_grid;		// Opció per activar comandes teclat per moure grids [TRUE] o no [FALSE]
	CMask3D grid;		// Opció per a dibuixar grids (.x: grid XY, .y:grid XZ, .z: grid YZ, .w:grid XYZ)
	CPunt3D hgrid;		// Alçada de cada grid (.x: grid XY, .y:grid XZ, .z: grid YZ, .w:grid XYZ)

// Entorn VGI: Variables de l'opció Vista->Pan
	double fact_pan;	// Factor de desplaçament de la càmara (opció pan).
	CPunt3D tr_cpv;		// Vector de Traslació del Centre del Punt de Vista actiu dins opció pan.
	CPunt3D tr_cpvF;	// Vector de Traslació del Centre del Punt de Vista (fixat amb INSERT dins opció pan) 
						//       i actiu fora l'opció pan.

// Entorn VGI: Variables de control per les opcions de menú Projecció, Objecte
	char projeccio;					// Menú Projecció: Tipus de projeccio
	static int oProjeccio = 3;		// ImGui: Variable que controla desplegable TIPUS PROJECCIO d'ImGui
	glm::mat4 ProjectionMatrix(1.0);// Matriu de Projecció a passar al shader
	char objecte;					// Menú Objecte: Tipus d'Objecte
	static int oObjecte = 0;		// ImGui: Variable que controla desplegable TIPUS OBJECTE d'ImGui

// Entorn VGI: Variables de control Skybox Cube
	bool SkyBoxCube;			// Booleana que controla si es visualitza Skybox [TRUE] o no [FALSE].
	Shader shader_SkyBoxC;		// Shader Skybox Cub
	GLuint skC_programID;		// Identificador program Skybox Cube
	CVAO skC_VAOID;				// Identificador VAO List (vaoId, vboId, nVertexs) per a Skybox Cube
	GLuint cubemapTexture;		// Identificador textura cubemap.
	Shader shaderSkyBox;		// Shader SkyBox

// Entorn VGI: Variables de control del menú Transforma
	bool transf;	// Booleana per activar Transformacions (true) o no (false).
	bool trasl;		// Booleana d'activació de la Traslació (true) o no (false).
	bool rota;		// Booleana d'activació de la Rotació (true) o no (false).
	bool escal;		// Booleana d'activació de l'Escalatge (true) o no (false).
	double fact_Tras, fact_Rota;	// Increments de Traslació i Rotació.
	INSTANCIA TG;	// Estructura que conté TG del menú Transforma actiu dins qualsevol opció de Transforma 
					//      (Traslació Rotació i Escalatge).
	INSTANCIA TGF;	// Estructura que conté TG del menú Transforma fixat amb INSERT dins les opcions de Transforma
					//     i actiu fora l'opció Transforma.
	bool transX;	// Opció Mobil Eix X?: TG interactives per l'eix X via mouse [F:NO,T:SI].
	bool transY;	// Opció Mobil Eix Y?: TG interactives per l'eix Y via mouse [F:NO,T:SI].
	bool transZ;	// Opció Mobil Eix Z?: TG interactives per l'eix Z via mouse [F:NO,T:SI].
	glm::mat4 GTMatrix; // Matriu de Transformacions Geomètriques a passar al shader

// Entorn VGI: Variables de control per les opcions de menú Ocultacions
	bool front_faces;// Menú ocultacions: Determina si les cares visibles són les frontals [true-defecte] o les back [false] pel Test de Visibilitat.
	bool test_vis;  // Menú ocultacions: Activa [true] o desactiva [false] Test Visibilitat.
	bool oculta;    // Menú ocultacions: Activa [true] o desactiva [false] Ocultacions (Z-buffer).
	bool back_line; // Menú ocultacions: Activa [true] o desactiva [false] dibuix cares back com a línies.

// Entorn VGI: Variables de control del menú Iluminació	
	bool ifixe;         // Iluminació fixe independent del PV (ifixe=1) 
						// o depenent (mobil) respecte el PV (casc miner) (ifixe=0)
	bool ilum2sides;	// Iluminació de les cares FRONT i BACK [TRUE] o només les FRONT [FALSE]
	char ilumina;		// Tipus d'il.luminació [FILFERROS 'f',PLANA 'c',SUAU 'c']
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
	bool tFlag_invert_Y; // Booleana que activa la inversió coordenada textura t (o Y) a 1.0-cty segons llibreria SOIL (TRUE) o no (FALSE).

// Entorn VGI: Variables de control del menú Llums
	bool llum_ambient;		// Booleana que controla la llum ambient (SI/NO).
	LLUM llumGL[NUM_MAX_LLUMS];		// Vector de llums d'OpenGL
	bool sw_llambient;		// Booleana que controla modus de configurar el color de la llum ambient [TRUE] o no [FALSE]
	CColor col_llambient;	// Color llum ambient.

// Entorn VGI: Variables de control del menú Shaders
	char shader;				// Tipus de shader [FLAT, GOURAUD, PHONG, FILE, PROG_BINARY_SHADER]
	static int oShader = 1;		// Variable que controla desplegable SHADER d'ImGui
	GLuint shader_programID;	// Shader Program que conté el Vertex i Fragment program.
	Shader shaderLighting;		// Shader que implementa els codis GLSL d'il.luminació.
	
// Entorn VGI: Variables butons de mouse 
	CPoint m_PosEAvall, m_PosDAvall;	// Coordenades del cursor quan el boto esquerre(E) o dret(D) del 
										//    mouse ha estat clicat.
	bool m_ButoEAvall, m_ButoDAvall;	// TRUE si el boto esquerre(E) o dret(D) del mouse esta clicat.
	CEsfe3D m_EsfeEAvall;				// Coordenades Esfèriques del PV (OPV) quan el boto esquerre(E) o dret(D) del 
										//		mouse ha estat clicat.
	CEsfe3D m_EsfeIncEAvall;			// Increment de desplaçament en coordenades Esfèriques del PV (OPV).

// Entorn VGI: Variables que controlen paràmetres visualització: Mides finestra Windows i PV
	int w, h;				// Mides de la finestra Windows (w-amplada,h-alçada)
	int width_old, height_old;	// Mides de la resolució actual de la pantalla (finestra Windows)
	int w_old, h_old;		// Mides de la finestra Windows (w-amplada,h-alçada) per restaurar Finestra des de fullscreen
	CEsfe3D OPV;			// Paràmetres camera en coord. esfèriques (R,alfa,beta)
	float cam_Esferica[3] = { 15.0f, 0.0f, 0.0f }; // ImGui: Coordenades Esfèriques Càmera
	char Vis_Polar;			// Variable que controla orientació dels eixos en Visualització Interactiva (POLARZ,POLARY,POLARX)
	static int oPolars = 0;		// ImGui: Variable que controla Botons POLARS d'ImGui

// Entorn VGI: Color de fons i de l'objecte
	bool fonsR, fonsG, fonsB;	// Booleanes per controlar variació de color per teclat.
	CColor c_fons;				// Intensitat de color de fons.
	bool sw_color;				// Booleana que controla el modus de configurar color de l'objecte per teclat [TRUE] o no [FALSE]
	CColor col_obj;				// Color de l'objecte simple.
	ImVec4 clear_colorB = ImVec4(0.0f, 0.0f, 0.0f, 1.00f);	// ImGui: Color de fons
	ImVec4 clear_colorO = ImVec4(1.0f, 1.0f, 1.0f, 1.00f);	// ImGui: Color de l'objecte.

// Entorn VGI: Objecte OBJ:
	COBJModel* ObOBJ;		// Variable d'objecte format OBJ (*.OBJ)
	CVAO vao_OBJ;			// Identificador VAO per a objecte OBJ

// Entorn VGI: OBJECTE --> Corbes Bezier i BSpline
	int npts_T;							// Número de punts de control de la corba en el Vector corbaSpline (<=MAX_PATH_SPLINE)
	CPunt3D PC_t[MAX_PATCH_CORBA];		// Vector que enmagatzema Punts de Control Corba Spline
	GLdouble pas_Corba;					// Increment del paràmetre t per al dibuix de les corbes.
	GLdouble pas_CS;					// Increment del paràmetre t per al dibuix de corbes i superficies.
	bool sw_Punts_Control;				// Booleana que activa o desactiva la visualització dels punts de control de la corba o de la superficie

// Entorn VGI. TRIEDRE DE FRENET / DARBOUX: VT: vector Tangent, VNP: Vector Normal Principal, VBN: vector BiNormal
	bool dibuixa_TriedreFrenet;			// Booleana que controla dibuix de Triedre de Frenet per a cada punt de la Corba [TRUE-dibuixa, FALSE-no dibuixa]
	bool dibuixa_TriedreDarboux;		// Booleana que controla dibuix de Triedre de Darboux per a cada punt de la Corba Loxodroma[TRUE-dibuixa, FALSE-no dibuixa]
	CPunt3D VT, VNP, VBN;				// TRIEDRE de FRENET: VT: Vector Tangent, VNP: Vector Normal Principal, VBN: Vector BiNormal.

// Entorn VGI: Variables del Timer
	double t;		// Paràmetre t pel Timer.
	bool anima;		// Booleana que controla si l'animació és activa (TRUE) o no (FALSE)
					//    dins la funció de control del rellotge OnTimer.

// Entorn VGI: Variables de l'objecte FRACTAL
	char t_fractal;		// Tipus de fractal.
	char soroll;		// Menú Fractals: Tipus de soroll
	int pas, pas_ini;	// Resolució del fractal inicial (pas_ini) i de visualització (pas).
	bool sw_il;			// Booleana que controla si cal compilar el fractal (sw_il=1) o no (sw_il=0)
	bool palcolFractal;	// Booleana que activa coloració del fractal segons paleta de colors [TRUE] o no [FALSE].

// Entorn VGI: Altres variables
	GLdouble mida;		// Factor d'escala per calcular Volum de Visualització de l'objecte que encaixi.
	std::string nom;	// Nom de fitxer.
	std::string buffer; // Buffer que magatzema string caracters corresponent a variables double a printar en Status Bar (funció Barra_Estat).
//-------------- Entorn VGI: Fi De Variables globals de main

//-------------- Entorn VGI: Declaració funcions main

// Inicialització variables de control
	void InitGL();	
	
	void InitAPI();
	void GetGLVersion(int* major, int* minor);

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
	void OnCameraFly();
	void addSceneCube();
	void addSceneOBJ(const char* filename);
	void updateFreeCamera(float deltaTime);
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
	// Desplegable ILUMINACIÓ
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
	void OnKeyUp(GLFWwindow* window, int key, int scancode, int action, int mods);
	void OnTextDown(GLFWwindow* window, unsigned int codepoint);

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
	int Log2(int num);							// Log2: Càlcul del log base 2 de num
	int llegir_ptsC(const char* nomf);				// Llegir Punts Control Corbes Bezier i B-Spline
	void OnFull_Screen(GLFWmonitor* monitor, GLFWwindow* window);
// ---------------- Entorn VGI: Tractament d'errors
	void error_callback(int code, const char* description);
	GLenum glCheckError_(const char* file, int line);
	void APIENTRY glDebugOutput(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
		const GLchar* message, const void* userParam);
