#include "src/app/AppState.h"
#include "src/legacy_vgi/VgiMenus.h"
#include "src/app/adapters/InteractionAdapters.h"
#include "src/app/adapters/SceneAdapters.h"
#include "src/app/input/CursorCapture.h"

// Entorn VGI: Funció que mostra el menú principal de l'Entorn VGI amb els seus desplegables.
void MostraEntornVGIWindow(bool* p_open)
{	// Exceptionally add an extra assert here for people confused about initial Dear ImGui setup
	// Most functions would normally just crash if the context is missing.
	IM_ASSERT(ImGui::GetCurrentContext() != NULL && "Missing dear imgui context. Refer to examples app!");

// Examples Apps (accessible from the "Examples" menu)
//static bool show_window_about = false;

	if (show_window_about)       ShowAboutWindow(&show_window_about);

// We specify a default position/size in case there's no data in the .ini file.
// We only do it to make the demo applications a little more welcoming, but typically this isn't required.
	const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 650, main_viewport->WorkPos.y + 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);

	ImGuiWindowFlags window_flags = 0;
	// Main body of the Demo window starts here.
	if (!ImGui::Begin("EntornVGI Menu", p_open, window_flags))
	{	// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

// Most "big" widgets share a common width settings by default. See 'Demo->Layout->Widgets Width' for details.
// e.g. Use 2/3 of the space for widgets and 1/3 for labels (right align)
//ImGui::PushItemWidth(-ImGui::GetWindowWidth() * 0.35f);
// e.g. Leave a fixed amount of width for labels (by passing a negative value), the rest goes to widgets.
	ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

// Menu Bar
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("Arxius"))
		{
			//IMGUI_DEMO_MARKER("Menu/File");
			ShowArxiusOptions();
			ImGui::EndMenu();
		}
		if (modoJuegoFPS() && ImGui::BeginMenu("Juego"))
		{
			if (ImGui::MenuItem(
				estadoActual == ESTADO_PAUSA ? "Reanudar juego FPS" : "Pausar juego FPS"))
			{
				if (estadoActual == ESTADO_JUGANDO)
				{
					estadoActual = ESTADO_PAUSA;
					cancelarCargaLanzamiento();
				}
				else if (estadoActual == ESTADO_PAUSA)
				{
					estadoActual = ESTADO_JUGANDO;
					fpsTienePosicionAnterior = false;
					mouseDeltaX = mouseDeltaY = 0.0;
				}

				fpsPausado = (estadoActual != ESTADO_JUGANDO);
				sincronizarCursorFPS(window);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Sobre EntornVGI"))
			{	//IMGUI_DEMO_MARKER("Menu/Examples");
				//ShowAboutOptions(&show_window_about);
				ImGui::EndMenu();
			}
		ImGui::EndMenuBar();
	}

// End of ShowEntornVGIWindow()
	ImGui::PopItemWidth();
	ImGui::End();
}

// Entorn VGI: Funció que mostra els menús popUp "Arxius" per a obrir fitxers i el pop Up "About".
void ShowArxiusOptions()
{
//IMGUI_DEMO_MARKER("Examples/Menu");
	ImGui::MenuItem("(Arxius menu)", NULL, false, false);
	if (ImGui::MenuItem("New")) {}
	if (ImGui::MenuItem("Cargar habitacion JSON"))
		cargarYAplicarEscena("assets/scenes/habitacion_beta.json");
	if (ImGui::MenuItem("Cargar ejemplo fisica JSON"))
		cargarYAplicarEscena("assets/scenes/ejemplo_fisica.json");

	if (ImGui::MenuItem("Obrir Fitxer OBJ", "(<Shift>+#)")) {
		nfdchar_t* nomFitxer = NULL;
		nfdresult_t result = NFD_OpenDialog(NULL, NULL, &nomFitxer);

		if (result == NFD_OKAY) {
			puts("Success!");
			puts(nomFitxer);

			objecte = OBJOBJ;	textura = true;		tFlag_invert_Y = false; oObjecte = 9;
			//char* nomfitx = nomfitxer;
			if (ObOBJ == NULL) ObOBJ = ::new COBJModel;
			else { // Si instància ja s'ha utilitzat en un objecte OBJ
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

	if (ImGui::MenuItem("Obrir Fractal", "(<Shift>+#)")) {
		nfdchar_t* nomFitxer = NULL;
		nfdresult_t result = NFD_OpenDialog(NULL, NULL, &nomFitxer);

		if (result == NFD_OKAY) {	puts("Success!");
									puts(nomFitxer);
									free(nomFitxer);
								}
		else if (result == NFD_CANCEL) puts("User pressed cancel.");
		else printf("Error: %s\n", NFD_GetError());
	}

	if (ImGui::BeginMenu("Open Recent"))
	{	ImGui::MenuItem("fish_hat.c");
		ImGui::MenuItem("fish_hat.inl");
		ImGui::MenuItem("fish_hat.h");
		if (ImGui::BeginMenu("More.."))
		{	ImGui::MenuItem("Hello");
			ImGui::MenuItem("Sailor");
			if (ImGui::BeginMenu("Recurse.."))
			{	ShowArxiusOptions();
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("Save", "(<Shift>+#))")) {}
	if (ImGui::MenuItem("Save As..")) {}

	if (ImGui::BeginMenu("Disabled", false)) // Disabled
		{	IM_ASSERT(0);
		}
	if (ImGui::MenuItem("Checked", NULL, true)) {}
	ImGui::Separator();
	if (ImGui::MenuItem("Quit", "Alt+F4"))
		glfwSetWindowShouldClose(window, GL_TRUE);
}

// Entorn VGI: Funció que mostra la finestra "About" de l'aplicació.
void ShowAboutWindow(bool* p_open)
{
// For the demo: add a debug button _BEFORE_ the normal log window contents
// We take advantage of a rarely used feature: multiple calls to Begin()/End() are appending to the _same_ window.
// Most of the contents of the window will be added by the log.Draw() call.
	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("About", p_open);
	//IMGUI_DEMO_MARKER("Window/About");
	ImGui::Text("VISUALITZACIO GRAFICA INTERACTIVA (Escola d'Enginyeria - UAB");
	ImGui::Text("Entorn Grafic VS2026 amb interficie GLFW 3.4, ImGui i OpenGL 4.6 per a practiques i ABP");
	ImGui::Text("AUTOR: Enric Marti Godia");
	ImGui::Text("Copyright (C) 2026");
	if (ImGui::Button("Acceptar"))
		show_window_about = false;
	ImGui::End();
}

// Entorn VGI: Funció que retorna opció de menú TIPUS CAMERA segons variable camera (si modificada per teclat)
int shortCut_Camera()
{	int auxCamera;

// Entorn VGI: Gestió opcions desplegable TIPUS CAMERA segons el valor de la variable selected
	switch (camera)
	{	case CAM_ESFERICA:	// Càmera ESFÈRICA
			auxCamera = 0;
			break;
		case CAM_NAVEGA:	// Càmera NAVEGA
			auxCamera = 1;
			break;
		case CAM_GEODE:		// Càmera GEODE
			auxCamera = 2;
			break;
		default:			// Opció CÀMERA <Altres Càmeres>
			auxCamera = -1;
			break;
	}
	return auxCamera;
}

// Entorn VGI: Funció que retorna opció de menú TIPUS CAMERA segons variable camera (si modificada per teclat)
int shortCut_Polars()
{
	int auxPolars;

// Entorn VGI: Gestió opcions desplegable TIPUS CAMERA segons el valor de la variable selected
	switch (Vis_Polar)
	{	case POLARX:	// Polars Eix X
			auxPolars = 0;
			break;
		case POLARY:	// Polars Eix Y
			auxPolars = 1;
			break;
		case POLARZ:	// Polars Eix Z
			auxPolars = 2;
			break;
		default:			// Opció CÀMERA <Altres Càmeres>
			auxPolars = -1;
			break;
	}
	return auxPolars;
}

// Entorn VGI: Funció que retorna opció de menú TIPUS PROJECCIO segons variable projecte (si modificada per teclat)
int shortCut_Projeccio()
{
	int auxProjeccio;

// Entorn VGI: Gestió opcions desplegable TIPUS PROJECCIO segons el valor de la variable selected
	switch (projeccio)
	{	case CAP:	// Projeccio CAP
			auxProjeccio = 0;
			break;
		case AXONOM:		// Projeccio AXONOMÈTRICA
			auxProjeccio = 1;
			break;
		case ORTO:			// Projeccio ORTOGRÀFICA
			auxProjeccio = 2;
			break;
		case PERSPECT:		// Projeccio PERSPECTIVA
			auxProjeccio = 3;
			break;
		default:			// Opció PROJECCIÓ <Altres Projeccions>
			auxProjeccio = 0;
			break;
	}
	return auxProjeccio;
}

// Entorn VGI: Funció que retorna opció de menú Objecte segons variable objecte (si modificada per teclat)
int shortCut_Objecte()
{
	int auxObjecte;

// Entorn VGI: Gestió opcions desplegable OBJECTES segons el valor de la variable selected
	switch (objecte)
	{	case CAP:			// Objecte CAP
		auxObjecte = 0;
		break;
		case CUB:			// Objecte CUB
			auxObjecte = 1;
			break;
		case CUB_RGB:		// Objecte CUB_RGB
			auxObjecte = 2;
			break;
		case ESFERA:		// Objecte ESFERA
			auxObjecte = 3;
			break;
		case TETERA:		// Objecte TETERA
			auxObjecte = 4;
			break;
		case ARC:			// Objecte ARC
			auxObjecte = 5;
			break;
		case MATRIUP:		// Objecte MATRIU PRIMITIVES
			auxObjecte = 6;
			break;
		case MATRIUP_VAO:	// Objecte MATRIU PRIMITIVES VAO
			auxObjecte = 7;
			break;
		case TIE:			// Objecte TIE
			auxObjecte = 8;
			break;
		case C_BEZIER:		// Objecte CORBA BEZIER
			auxObjecte = 10;
			break;
		case C_BSPLINE:		// Objecte CORBA B-SPLINE
			auxObjecte = 11;
			break;
		case C_LEMNISCATA:	// Objecte CORBA LEMNISCATA
			auxObjecte = 12;
			break;
		case C_HERMITTE:	// Objecte CORBA LEMNISCATA
			auxObjecte = 13;
			break;
		case C_CATMULL_ROM:	// Objecte CORBA LEMNISCATA
			auxObjecte = 14;
			break;
		case OBJOBJ:	// Objecte Arxiu OBJ
			auxObjecte = 9;
			break;
		default:			// Opció OBJECTE <Altres Objectes>
			auxObjecte = 0;
			break;
	}
	return auxObjecte;
}

// Entorn VGI: Funció que retorna opció de menú TIPUS ILUMINACIO segons variable ilumina (si modificada per teclat)
int shortCut_Iluminacio()
{
	int auxIlumina;

	// Entorn VGI: Gestió opcions desplegable OBJECTES segons el valor de la variable selected
	switch (ilumina)
	{	case PUNTS:		// Ilumninacio PUNTS
		auxIlumina = 0;
		break;
		case FILFERROS:	// Iluminacio FILFERROS
			auxIlumina = 1;
			break;
		case PLANA:		// Iluminacio PLANA
			auxIlumina = 2;
			break;
		case SUAU:		// Iluminacio SUAU
			auxIlumina = 3;
			break;
		default:		 // Opció Iluminacio <Altres Iluminacio>
			auxIlumina = 0;
			break;
	}
	return auxIlumina;
}

// Entorn VGI: Funció que retorna opció de menú TIPUS SHADER segons variable shader (si modificada per teclat)
int shortCut_Shader()
{
	int auxShader;

// Entorn VGI: Gestió opcions desplegable OBJECTES segons el valor de la variable selected
	switch (shader)
	{	case FLAT_SHADER:		// Shader FLAT_SHADER
			auxShader = 0;
			break;
		case GOURAUD_SHADER:	// Shader GOURAUD_SHADER
			auxShader = 1;
			break;
		case PHONG_SHADER:		// Shader PHONG
			auxShader = 2;
			break;
		case FILE_SHADER:		// Shader FILE_SHADER
			auxShader = 3;
			break;
		default:		 // Opció Iluminacio <Altres Iluminacio>
			auxShader = 0;
			break;
	}
	return auxShader;
}
