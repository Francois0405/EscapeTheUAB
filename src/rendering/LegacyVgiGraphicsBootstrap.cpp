#include "src/rendering/LegacyVgiGraphicsBootstrap.h"

#include "imgui.h"
#include "src/rendering/shader.h"
#include "src/rendering/visualitzacio.h"

#include <cmath>
#include <cstdio>
#include <vector>

void LegacyVgiGraphicsBootstrap::Run()
{
	InitViewAndRenderState();
	InitLights();
	LoadShadersAndSkybox();
	InitInteractionAndObjectState();

// Entorn VGI: // Inicialtzar llista de VAO's.
	initVAOList();
}

void LegacyVgiGraphicsBootstrap::InitViewAndRenderState()
{
	int i;

	// Entorn VGI: Variable de control per a Status Bar (consola)
	statusB = false;

// Entorn VGI: Variables de control per Menú CÀMERA: Esfèrica, Navega, Mòbil, Zoom, Satelit, Polars...
	camera = CAM_ESFERICA;	oCamera = 0;
	mobil = true;			zzoom = true;		zzoomO = false;		satelit = false;

// Entorn VGI: Variables de control de l'opció CAMERA->ESFÈRICA?
	cam_Esferica[0] = 15.0f;	cam_Esferica[1] = 0.0f;			cam_Esferica[2] = 0.0f;
	OPV.R = cam_Esferica[0];	OPV.alfa = cam_Esferica[1];		OPV.beta = cam_Esferica[2];		// Origen PV en esfèriques
	Vis_Polar = POLARZ;
	ViewMatrix = glm::mat4(1.0);		// Inicialitzar a identitat
	oPolars = -1;

// Entorn VGI: Variables de control de l'opció CÀMERA->NAVEGA?
	n[0] = -5.5;	n[1] = 4.0;		n[2] = -1.35;
	opvN.x = -6.5;	opvN.y = 4.0;	opvN.z = -1.35;
	angleZ = 0.0;

// Entorn VGI: Variables de control de l'opció CÀMERA->GEODE?
	OPV_G.R = 15.0;		OPV_G.alfa = 0.0;	OPV_G.beta = 0.0;	// Origen PV en esfèriques per a Vista_Geode

// Entorn VGI: Variables de control per Menú VISTA: Pantalla Completa, Pan, dibuixar eixos i grids
	fullscreen = false;	// La finestra s'inicia en mode finestra; F11 alterna pantalla completa
	pan = false;
	eixos = true;	eixos_programID = 0;  eixos_Id = 0;
	sw_grid = false;
	grid.x = false;	grid.y = false;		grid.z = false;		grid.w = false;
	hgrid.x = 0.0;	hgrid.y = 0.0;		hgrid.z = 0.0;		hgrid.w = 0.0;

// Entorn VGI: Variables opció VISTA->PAN?
	fact_pan = 1;
	tr_cpv.x = 0;	tr_cpv.y = 0;	tr_cpv.z = 0;		tr_cpvF.x = 0;	tr_cpvF.y = 0;	tr_cpvF.z = 0;

// Entorn VGI: Variables de control per les opcions de menú Projecció, Objecte
	projeccio = CAP;	// projeccio = PERSPECT;
	oProjeccio = 3;		// ImGui: Variable que controla desplegable TIPUS PROJECCIO d'ImGui (opció Perspectiva)
	ProjectionMatrix = glm::mat4(1.0);	// Inicialitzar matriu a identitat
	objecte = CAP;		// objecte = TETERA;
	oObjecte = 0;		// ImGui: Variable que controla desplegable TIPUS OBJECTE d'ImGui (opció cap objecte)

// Entorn VGI: Variables de control Skybox Cube
	SkyBoxCube = false;
	skC_programID = 0;		//shader_SkyBoxC = NULL;
	skC_VAOID.vaoId = 0;	skC_VAOID.vboId = 0;	skC_VAOID.nVertexs = 0;
	cubemapTexture = 0;		//shaderSkyBox = NULL;

// Entorn VGI: Variables de control del menú TRANFORMA
	transf = false;		trasl = false;		rota = false;		escal = false;
	fact_Tras = 1;		fact_Rota = 90;
	TG.VTras.x = 0.0;	TG.VTras.y = 0.0;	TG.VTras.z = 0;	TGF.VTras.x = 0.0;	TGF.VTras.y = 0.0;	TGF.VTras.z = 0;
	TG.VRota.x = 0;		TG.VRota.y = 0;		TG.VRota.z = 0;	TGF.VRota.x = 0;	TGF.VRota.y = 0;	TGF.VRota.z = 0;
	TG.VScal.x = 1;		TG.VScal.y = 1;		TG.VScal.z = 1;	TGF.VScal.x = 1;	TGF.VScal.y = 1;	TGF.VScal.z = 1;
	transX = false;		transY = false;		transZ = false;
	GTMatrix = glm::mat4(1.0);		// Inicialitzar matriu a identitat

// Entorn VGI: Variables de control per les opcions de menú OCULTACIONS
	front_faces = true;	test_vis = false;	oculta = false;		back_line = false;

// Entorn VGI: Variables de control del menú ILUMINACIÓ
	ifixe = false;					ilum2sides = false;				ilumina = FILFERROS;
	oIlumina = 1;					// Variable que controla desplegable TIPUS ILUMINACIO d'ImGui (opció Filferros)
	// Reflexions actives: Ambient [1], Difusa [2] i Especular [3]. No actives: Emission [0].
	sw_material[0] = false;			sw_material[1] = true;			sw_material[2] = true;			sw_material[3] = true;	sw_material[4] = true;
	sw_material_old[0] = false;		sw_material_old[1] = true;		sw_material_old[2] = true;		sw_material_old[3] = true;	sw_material_old[4] = true;
	textura = false;				t_textura = CAP;				textura_map = true;
	for (i = 0; i < NUM_MAX_TEXTURES; i++) texturesID[i] = 0;
	tFlag_invert_Y = false;
}

void LegacyVgiGraphicsBootstrap::InitLights()
{
	int i;

// ----------------- Entorn VGI: Variables de control del menú LLUMS
	// Entorn VGI: Inicialització variables Llums
	llum_ambient = true;		llumGL[0].encesa = true;
	for (i = 1; i < NUM_MAX_LLUMS; i++) llumGL[i].encesa = false;
	for (i = 0; i < NUM_MAX_LLUMS; i++)
		{	llumGL[i].posicio.x = 0.0;		llumGL[i].posicio.y = 0.0;		llumGL[i].posicio.z = 0.0;		llumGL[i].posicio.w = 1.0;
			llumGL[i].difusa.r = 1.0f;		llumGL[i].difusa.g = 1.0f;		llumGL[i].difusa.b = 1.0f;		llumGL[i].difusa.a = 1.0f;
			llumGL[i].especular.r = 1.0f;	llumGL[i].especular.g = 1.0f;	llumGL[i].especular.b = 1.0f;	llumGL[i].especular.a = 1.0f;
			llumGL[i].atenuacio.a = 0.0;	llumGL[i].atenuacio.b = 0.0;	llumGL[i].atenuacio.c = 1.0;
			llumGL[i].restringida = false;
			llumGL[i].spotdirection.x = 0.0;	llumGL[i].spotdirection.y = 0.0;	llumGL[i].spotdirection.z = 0.0; llumGL[i].spotdirection.w = 0.0;
			llumGL[i].spotcoscutoff = 0.0;		llumGL[i].spotInnerCosCutoff = 0.0;		llumGL[i].spotexponent = 0.0;
			llumGL[i].areaSource = false;
			llumGL[i].areaHalfSizeU = llumGL[i].areaHalfSizeV = 0.0f;
		}

// ---------------- LLUM #0 - (+Z) no restringida, amb atenuació constant (a,b,c) = (0,0,1)
// Posició de la font de llum (x,y,z)=(0,200,0):
	llumGL[0].posicio.x = 0.0;			llumGL[0].posicio.y = 0.0;			llumGL[0].posicio.z = 200.0;	llumGL[0].posicio.w = 1.0;

// Intensitats difusa i especular de la font de llum (r,g,b) = (1,1,1):
	llumGL[0].difusa.r = 1.0f;			llumGL[0].difusa.g = 1.0f;			llumGL[0].difusa.b = 1.0f;		llumGL[0].difusa.a = 1.0f;
	llumGL[0].especular.r = 1.0f;		llumGL[0].especular.g = 1.0f;		llumGL[0].especular.b = 1.0f;	llumGL[0].especular.a = 1.0f;

// Coeficients factor atenuació f_att=1/(ad2+bd+c). Llum sense atenuació per distància (a,b,c)=(0,0,1):
	llumGL[0].atenuacio.a = 0.0;		llumGL[0].atenuacio.b = 0.0;		llumGL[0].atenuacio.c = 1.0;

// Paràmetres font de llum restringida:
	llumGL[0].restringida = false;
	llumGL[0].spotdirection.x = 0.0;	llumGL[0].spotdirection.y = 0.0;	llumGL[0].spotdirection.z = -1.0;
	llumGL[0].spotcoscutoff = cos(25.0 * PI / 180);		llumGL[0].spotexponent = 1.0;		// llumGL[0].spotexponent = 45.0; Model de Warn (10, 500)

// Activació font de llum: ENCESA
	llumGL[0].encesa = true;

// ---------------- LLUM #1 - (+X) no restringida, amb atenuació constant (a,b,c) = (0,0,1)
// Posició de la font de llum (x,y,z)=(75,0,0):
	llumGL[1].posicio.x = 75.0;			llumGL[1].posicio.y = 0.0;			llumGL[1].posicio.z = 0.0;		llumGL[1].posicio.w = 1.0;

// Intensitats difusa i especular de la font de llum (r,g,b) = (1,1,1):
	llumGL[1].difusa.r = 1.0f;			llumGL[1].difusa.g = 1.0f;			llumGL[1].difusa.b = 1.0f;		llumGL[1].difusa.a = 1.0f;
	llumGL[1].especular.r = 1.0f;		llumGL[1].especular.g = 1.0f;		llumGL[1].especular.b = 1.0f;	llumGL[1].especular.a = 1.0f;

// Coeficients factor atenuació f_att=1/(ad2+bd+c). Llum sense atenuació per distància (a,b,c)=(0,0,1):
	llumGL[1].atenuacio.a = 0.0;		llumGL[1].atenuacio.b = 0.0;		llumGL[1].atenuacio.c = 1.0;

// Paràmetres font de llum restringida:
	llumGL[1].restringida = false;
	llumGL[1].spotdirection.x = 0.0;	llumGL[1].spotdirection.y = 0.0;	llumGL[1].spotdirection.z = 0.0;
	llumGL[1].spotcoscutoff = 0.0;		llumGL[1].spotexponent = 0.0;

// Activació font de llum: APAGADA
	llumGL[1].encesa = false;

// ---------------- LLUM #2 - (+Y) no restringida, amb atenuació constant (a,b,c) = (0,0,1)
// Posició de la font de llum (x,y,z)=(0,75,0):
	llumGL[2].posicio.x = 0.0;			llumGL[2].posicio.y = 75.0;			llumGL[2].posicio.z = 0.0;		llumGL[2].posicio.w = 1.0;

// Intensitats difusa i especular de la font de llum (r,g,b) = (1,1,1):
	llumGL[2].difusa.r = 1.0f;			llumGL[2].difusa.g = 1.0f;			llumGL[2].difusa.b = 1.0f;		llumGL[2].difusa.a = 1.0f;
	llumGL[2].especular.r = 1.0f;		llumGL[2].especular.b = 1.0f;		llumGL[2].especular.b = 1.0f;	llumGL[2].especular.a = 1.0f;

// Coeficients factor atenuació f_att=1/(ad2+bd+c). Llum amb atenuació per distància (a,b,c)=(0,0.025,1):
	llumGL[2].atenuacio.a = 0.0;		llumGL[2].atenuacio.b = 0.0;		llumGL[2].atenuacio.c = 1.0;

// Paràmetres font de llum restringida:
	llumGL[2].restringida = false;
	llumGL[2].spotdirection.x = 0.0;	llumGL[2].spotdirection.y = -1.0;	llumGL[2].spotdirection.z = 0.0;
	llumGL[2].spotcoscutoff = cos(2.5 * PI / 180);							llumGL[2].spotexponent = 1.0;

// Activació font de llum: APAGADA
	llumGL[2].encesa = false;

// ---------------- LLUM #3 - (Y=X), restringida amb 25 graus obertura i exponent 45, amb atenuació constant (a,b,c) = (0,0,1)
// Posició de la font de llum (x,y,z)=(75,75,75):
	llumGL[3].posicio.x = 75.0;			llumGL[3].posicio.y = 75.0;			llumGL[3].posicio.z = 75.0;		llumGL[3].posicio.w = 1.0;

// Intensitats difusa i especular de la font de llum (r,g,b) = (0,1,0):
	llumGL[3].difusa.r = 0.0f;			llumGL[2].difusa.g = 1.0f;			llumGL[3].difusa.b = 0.0f;		llumGL[3].difusa.a = 1.0f;
	llumGL[3].especular.r = 0.0f;		llumGL[2].especular.g = 1.0f;		llumGL[3].especular.b = 0.0f;	llumGL[3].especular.a = 1.0f;

// Coeficients factor atenuació f_att=1/(ad2+bd+c). Llum sense atenuació per distància (a,b,c)=(0,0,1):
	llumGL[3].atenuacio.a = 0.0;		llumGL[3].atenuacio.b = 0.0;		llumGL[3].atenuacio.c = 1.0;

// Paràmetres font de llum restringida:
	llumGL[3].restringida = true;
	llumGL[3].spotdirection.x = -1.0;	llumGL[3].spotdirection.y = -1.0;	llumGL[3].spotdirection.z = -1.0;
	llumGL[3].spotcoscutoff = cos(25.0 * PI / 180);							llumGL[3].spotexponent = 45.0;

// Activació font de llum: APAGADA
	llumGL[3].encesa = false;

// ---------------- LLUM #4 - (-Z), no restringida, amb atenuació constant (a,b,c) = (0,0,1)
// Posició de la font de llum (x,y,z)=(0,0,-75):
	llumGL[4].posicio.x = 0.0;			llumGL[4].posicio.y = 0.0;			llumGL[4].posicio.z = -75.0;	llumGL[4].posicio.w = 1.0;

// Intensitats difusa i especular de la font de llum (r,g,b) = (1,1,1):
	llumGL[4].difusa.r = 1.0f;			llumGL[4].difusa.g = 1.0f;			llumGL[4].difusa.b = 1.0f;		llumGL[4].difusa.a = 1.0f;
	llumGL[4].especular.r = 1.0f;		llumGL[4].especular.g = 1.0f;		llumGL[4].especular.b = 1.0f;	llumGL[4].especular.a = 1.0f;

// Coeficients factor atenuació f_att=1/(ad2+bd+c). Llum sense atenuació per distància (a,b,c)=(0,0,1):
	llumGL[4].atenuacio.a = 0.0;		llumGL[4].atenuacio.b = 0.0;		llumGL[4].atenuacio.c = 1.0;

// Paràmetres font de llum restringida:
	llumGL[4].restringida = false;
	llumGL[4].spotdirection.x = 0.0;	llumGL[4].spotdirection.y = 0.0;	llumGL[4].spotdirection.z = -1.0;
	llumGL[4].spotcoscutoff = cos(5 * PI / 180);							llumGL[4].spotexponent = 30.0;

// Activació font de llum: APAGADA
	llumGL[4].encesa = false;

// ---------------- LLUM #5 - (-X,-Y,-Z), direccional, no restringida, amb atenuació constant (a,b,c) = (0,0,1)
// Vector de la font de llum direccional (x,y,z)=(-1,-1,-1):
	llumGL[5].posicio.x = -1.0;			llumGL[5].posicio.y = -1.0;			llumGL[5].posicio.z = -1.0;		llumGL[5].posicio.w = 0.0;

// Intensitats difusa i especular de la font de llum (r,g,b) = (1,0,0):
	llumGL[5].difusa.r = 1.0f;			llumGL[5].difusa.g = 0.0f;			llumGL[5].difusa.b = 0.0f;		llumGL[5].difusa.a = 1.0f;
	llumGL[5].especular.r = 1.0f;		llumGL[5].especular.g = 0.0f;		llumGL[5].especular.b = 0.0f;	llumGL[5].especular.a = 1.0f;

// Coeficients factor atenuació f_att=1/(ad2+bd+c). Llum sense atenuació per distància (a,b,c)=(0,0,1):
	llumGL[5].atenuacio.a = 0.0;		llumGL[5].atenuacio.b = 0.0;		llumGL[5].atenuacio.c = 1.0;

// Paràmetres font de llum restringida:
	llumGL[5].restringida = false;
	llumGL[5].spotdirection.x = 0.0;	llumGL[5].spotdirection.y = 0.0;	llumGL[5].spotdirection.z = 0.0;
	llumGL[5].spotcoscutoff = 0.0;		llumGL[5].spotexponent = 0.0;

// Activació font de llum: APAGADA
	llumGL[5].encesa = false;

// ---------------- LLUM #6 - Llum Vaixell, configurada a la funció vaixell() en escena.cpp.
// Posició de la font de llum (x,y,z)=(-75,75,75):
	llumGL[6].posicio.x = -75.0;		llumGL[6].posicio.y = 75.0;			llumGL[6].posicio.z = 75.0;		llumGL[6].posicio.w = 1.0;

// Intensitats difusa i especular de la font de llum (r,g,b) = (1,1,1):
	llumGL[6].difusa.r = 1.0f;			llumGL[6].difusa.g = 1.0f;			llumGL[6].difusa.b = 1.0f;		llumGL[6].difusa.a = 1.0f;
	llumGL[6].especular.r = 1.0f;		llumGL[6].especular.g = 1.0f;		llumGL[6].especular.b = 1.0f;	llumGL[6].especular.a = 1;

// Coeficients factor atenuació f_att=1/(ad2+bd+c). Llum sense atenuació per distància (a,b,c)=(0,0,1):
	llumGL[6].atenuacio.a = 0.0;		llumGL[6].atenuacio.b = 0.0;		llumGL[6].atenuacio.c = 1.0;

// Paràmetres font de llum restringida:
	llumGL[6].restringida = false;
	llumGL[6].spotdirection.x = 1.0;	llumGL[6].spotdirection.y = -1.0;	llumGL[6].spotdirection.z = -1.0;
	llumGL[6].spotcoscutoff = sqrt(2.0) / 2.0;		llumGL[6].spotexponent = 10.0;

// Activació font de llum: APAGADA
	llumGL[6].encesa = false;

// ---------------- LLUM #7 - Llum Far, configurada a la funció faro() en escena.cpp.
// Posició de la font de llum (x,y,z)=(50,50,-50), però cap posició definida, definida en funció faro() en escena.cpp:
	llumGL[7].posicio.x = 50.0;			llumGL[7].posicio.y = 50.0;			llumGL[7].posicio.z = -50.0;	llumGL[7].posicio.w = 1.0;

	// Intensitats difusa i especular de la font de llum (r,g,b) = (0,1,1):
	llumGL[7].difusa.r = 0.0f;			llumGL[7].difusa.g = 1.0f;			llumGL[7].difusa.b = 1.0f;		llumGL[7].difusa.a = 1.0f;
	llumGL[7].especular.r = 0.0f;		llumGL[7].especular.g = 1.0f;		llumGL[7].especular.b = 1.0f;	llumGL[7].especular.a = 1;

	// Coeficients factor atenuació f_att=1/(ad2+bd+c). Llum sense atenuació per distància (a,b,c)=(0,0,1):
	llumGL[7].atenuacio.a = 0.0;		llumGL[7].atenuacio.b = 0.0;		llumGL[7].atenuacio.c = 1.0;

	// Paràmetres font de llum restringida:
	llumGL[7].restringida = true;
	llumGL[7].spotdirection.x = -1.0;	llumGL[7].spotdirection.y = -1.0;	llumGL[7].spotdirection.z = 1.0;
	llumGL[7].spotcoscutoff = 0.5;		llumGL[7].spotexponent = 5.0;

// Activació font de llum: APAGADA
	llumGL[7].encesa = false;
// ---------------- FI DEFINICIÓ LLUMS
	sw_llambient = false;
	col_llambient.r = 1.0;	col_llambient.g = 1.0;	col_llambient.b = 1.0; col_llambient.a = 1.0;
}

void LegacyVgiGraphicsBootstrap::LoadShadersAndSkybox()
{
// Entorn VGI: Variables de control del menú SHADERS
	shader = CAP_SHADER;
	oShader = 1;				// Variable que controla desplegable SHADER d'ImGui (opció Gouraud)
	shader_programID = 0;
	shaderLighting.releaseAllShaders();

// Entorn VGI: Càrrega SHADERS ---------------------------------------------------
// Càrrega Shader Eixos
	fprintf(stderr, "Eixos: \n");
	if (!eixos_programID) eixos_programID = shaderEixos.loadFileShaders("./assets/shaders/eixos.vert", "./assets/shaders/eixos.frag");

// VAO dels eixos: es crea aqui perque OnPaint() nomes el creava a la branca 'default', que mai
// s'executa (els shaders ja es carreguen en aquesta inicialitzacio). Sense VAO es dibuixava amb
// el VAO 0 i Core Profile generava GL_INVALID_OPERATION (1282) "Array object is not active".
	if (!eixos_Id) eixos_Id = deixos();

// Entorn VGI: Càrrega Shader Skybox
	fprintf(stderr, "SkyBox: \n");
	if (!skC_programID) skC_programID = shader_SkyBoxC.loadFileShaders("./assets/shaders/skybox.VERT", "./assets/shaders/skybox.FRAG");

// Entorn VGI: Càrrega VAO Skybox Cube
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

// Càrrega Shader de Gouraud
	shader_programID = 0;
	fprintf(stderr, "Gouraud_shdrML: \n");
	if (!shader_programID) shader_programID = shaderLighting.loadFileShaders("./assets/shaders/gouraud_shdrML.vert", "./assets/shaders/gouraud_shdrML.frag");
	shader = GOURAUD_SHADER;
// Entorn VGI: FI Càrrega SHADERS ---------------------------------------------------
}

void LegacyVgiGraphicsBootstrap::InitInteractionAndObjectState()
{
	int i;

// Entorn VGI: Variables de control dels botons de mouse
	m_PosEAvall.x = 0;			m_PosEAvall.y = 0;			m_PosDAvall.x = 0;			m_PosDAvall.y = 0;
	m_ButoEAvall = false;		m_ButoDAvall = false;
	m_EsfeEAvall.R = 0.0;		m_EsfeEAvall.alfa = 0.0;	m_EsfeEAvall.beta = 0.0;
	m_EsfeIncEAvall.R = 0.0;	m_EsfeIncEAvall.alfa = 0.0;	m_EsfeIncEAvall.beta = 0.0;

// Entorn VGI: Variables que controlen paràmetres visualització: Mides finestra Windows i PV (càmera)
	w = 640;			h = 480;			// Mides de la finestra Windows (w-amplada,h-alçada)
	width_old = 640;	height_old = 480;	// Mides de la resolució actual de la pantalla (finestra Windows)
	w_old = 640;		h_old = 480;		// Mides de la finestra Windows (w-amplada,h-alçada) per restaurar Finestra des de fullscreen

// Entorn VGI: Color de fons i de l'objecte
	fonsR = true;		fonsG = true;		fonsB = true;
	c_fons.r = clear_colorB.x;		c_fons.g = clear_colorB.y;		c_fons.b = clear_colorB.z;			c_fons.b = clear_colorB.w;
	sw_color = false;
	clear_colorB = ImVec4(0.0f, 0.0f, 0.0f, 1.00f);	// ImGui: Color de fons
	clear_colorO = ImVec4(1.0f, 1.0f, 1.0f, 1.00f);	// ImGui: Color de l'objecte.
	col_obj.r = clear_colorO.x;	col_obj.g = clear_colorO.y;	col_obj.b = clear_colorO.z;		col_obj.a = clear_colorO.w;

// Entorn VGI: Objecte OBJ
	ObOBJ = NULL;
	vao_OBJ.vaoId = 0;		vao_OBJ.vboId = 0;		vao_OBJ.nVertexs = 0;

// Entorn VGI: OBJECTE --> Corba B-Spline i Bezier
	npts_T = 0;
	for (i = 0; i < MAX_PATCH_CORBA; ++i)
		{	PC_t[i].x = 0.0;
			PC_t[i].y = 0.0;
			PC_t[i].z = 0.0;
		}
	pas_Corba = PAS_BSPLINE;	pas_CS = PAS_BSPLINE;
	sw_Punts_Control = false;

// TRIEDRE DE FRENET / DARBOUX: VT: vector Tangent, VNP: Vector Normal Principal, VBN: vector BiNormal
	dibuixa_TriedreFrenet = false;		dibuixa_TriedreDarboux = false;
	VT = { 0.0, 0.0, 1.0 };		VNP = { 1.0, 0.0, 0.0 };	VBN = { 0.0, 1.0, 0.0 };

// Entorn VGI: Variables del Timer
	t = 0;			anima = false;

// Entorn VGI : QUATERNIONS: Animació de cossos rígids per QUATERNIONS
	animaQ = false;		rotaQ = false;
/*
	for (i = 0; i < MAX_KEYFRAMES_Q; i++)
		{	keyf_Rota[i].x = 0.0f;	keyf_Rota[i].y = 0.0f;	keyf_Rota[i].z = 0.0f;
			//			keyf_Tras[i].x = 0.0f;	keyf_Tras[i].y = 0.0f;	keyf_Tras[i].z = 0.0f;
			keyf_Scal[i].x = 1.0f;	keyf_Scal[i].y = 1.0f;	keyf_Scal[i].z = 1.0f;
			keyf_Quat[i].w = 0.0f;	keyf_Quat[i].y = 0.0f;	keyf_Quat[i].y = 0.0f; keyf_Quat[i].z = 0.0f;
		}
	keyf_Tras[0].x = 10.0f;		keyf_Tras[0].y = -20.0f;	keyf_Tras[0].z = 30.0f;
	keyf_Tras[1].x = 0.0f;		keyf_Tras[1].y = 0.0f;		keyf_Tras[1].z = 0.0f;
	keyf_Tras[2].x = -10.0f;	keyf_Tras[2].y = 20.0f;		keyf_Tras[2].z = 30.0f;
	eix_Rota.x = 0;		eix_Rota.y = 0;		eix_Rota.z = 0;		angle_Rota = 0;
	for (i = 0; i < 16; i++) QMatrix[i] = 0;	QMatrix[0] = QMatrix[5] = QMatrix[10] = QMatrix[15] = 1.0;
	int_Tras.x = 0;		int_Tras.y = 0;		int_Tras.z = 0;
	int_Scal.x = 1;		int_Scal.y = 1;		int_Scal.z = 1;
	np_InQ = 0;
	keyf_RotaI.x = 0.0;	keyf_RotaI.y = 0.0; keyf_RotaI.z = 0.0;
	keyf_RotaF.x = 0.0;	keyf_RotaF.y = 0.0; keyf_RotaF.z = 0.0;
	keyf_RotaT.x = 0.0;	keyf_RotaT.y = 0.0; keyf_RotaT.z = 0.0;
	qI.w = 1.0;			qI.x = 0.0;				qI.y = 0.0;				qI.z = 0.0;
	qF.w = 1.0;			qF.x = 0.0;				qF.y = 0.0;				qF.z = 0.0;
	qT.w = 1.0;			qT.x = 0.0;				qT.y = 0.0;				qT.z = 0.0;
	qT_L.w = 1.0;		qT_L.x = 0.0;			qT_L.y = 0.0;			qT_L.z = 0.0;
	qT_S.w = 1.0;		qT_S.x = 0.0;			qT_S.y = 0.0;			qT_S.z = 0.0;
	qI_E.w = 1.0;		qI_E.x = 0.0;			qI_E.y = 0.0;			qI_E.z = 0.0;
	qF_E.w = 1.0;		qF_E.x = 0.0;			qF_E.y = 0.0;			qF_E.z = 0.0;
	qT_E.w = 1.0;		qT_E.x = 0.0;			qT_E.y = 0.0;			qT_E.z = 0.0;
	lerp = false;		slerp = false;
*/

// Entorn VGI: Variables de l'objecte FRACTAL
	t_fractal = CAP;	soroll = 'C';
	pas = 64;			pas_ini = 64;
	sw_il = true;		palcolFractal = false;
/*
	zzoomF = false;		paletaFractal = CAP_PALETA;
	vaoFractal.vaoId = 0;		vaoFractal.vboId = 0;		vaoFractal.nVertexs = 0;
	vaoMar.vaoId = 0;			vaoMar.vboId = 0;			vaoMar.nVertexs = 0;
*/

// Entorn VGI: ROBOT ARTICULAT
	animaR = false;		sw_robot = false;	sh_rmov = false;
/*
	angles_R[0] = 0.0;	angles_R[1] = 0.0;
	angles_R[2] = 0.0;	angles_R[3] = 0.0;	angles_R[4] = 0.0;
	angles_R[5] = 0.0;
	sh_rmov = true;

	pos_RI[0] = 0.0;		pos_RI[1] = 0.0;
	pos_RI[2] = 0.0;		pos_RI[3] = 0.0;		pos_RI[4] = 0.0;
	pos_RI[5] = 0.0;

	pos_RF[0] = 0.0;		pos_RF[1] = 0.0;
	pos_RF[2] = 0.0;		pos_RF[3] = 0.0;		pos_RF[4] = 0.0;
	pos_RF[5] = 0.0;

	for (i = 0; i < MAX_KEYFRAMES; i = i++)
		for (j = 0; j < 6; j = j++)		keyf_R[i][j] = 0.0f;
	np_In = 0;				num_frame = 0;			incr_frame = 1.0;
*/

// Entorn VGI: Altres variables
	mida = 1.0;			nom = "";		buffer = "";
}
