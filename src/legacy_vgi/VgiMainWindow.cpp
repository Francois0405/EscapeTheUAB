#include "src/app/AppIncludes.h"
#include "src/app/state/RenderState.h"
#include "src/legacy_vgi/VgiMainWindow.h"
#include "src/legacy_vgi/VgiFileActions.h"
#include "src/legacy_vgi/VgiMaterial.h"
#include "src/legacy_vgi/VgiMenuCallbacks.h"
#include "src/legacy_vgi/VgiMenus.h"

// Demonstrate most Dear ImGui features (this is big function!)
// You may execute this function to experiment with the UI and understand what it does.
// You may then search for keywords in the code when you are interested by a specific feature.
void ShowEntornVGIWindow(bool* p_open)
{
	int i = 0; // Variable per a menús de selecció.
	static int selected = -1;

// Exceptionally add an extra assert here for people confused about initial Dear ImGui setup
// Most functions would normally just crash if the context is missing.
	IM_ASSERT(ImGui::GetCurrentContext() != NULL && "Missing dear imgui context. Refer to examples app!");

// Examples Apps(accessible from the "Examples" menu)
	static bool show_app_main_menu_bar = false;
	static bool show_app_documents = false;
	static bool show_app_console = false;
	static bool show_app_log = false;
	static bool show_app_layout = false;
	static bool show_app_property_editor = false;
	static bool show_app_long_text = false;
	static bool show_app_auto_resize = false;
	static bool show_app_constrained_resize = false;
	static bool show_app_simple_overlay = false;
	static bool show_app_fullscreen = false;
	static bool show_app_window_titles = false;
	static bool show_app_custom_rendering = false;

// Examples Apps (accessible from the "Examples" menu)
	//static bool show_window_about = false;

	if (show_window_about)       ShowAboutWindow(&show_window_about);

/*
	if (show_app_main_menu_bar)       ShowExampleAppMainMenuBar();
	if (show_app_documents)           ShowExampleAppDocuments(&show_app_documents);
	if (show_app_console)             ShowExampleAppConsole(&show_app_console);
	if (show_app_log)                 ShowExampleAppLog(&show_app_log);
	if (show_app_layout)              ShowExampleAppLayout(&show_app_layout);
	if (show_app_property_editor)     ShowExampleAppPropertyEditor(&show_app_property_editor);
	if (show_app_long_text)           ShowExampleAppLongText(&show_app_long_text);
	if (show_app_auto_resize)         ShowExampleAppAutoResize(&show_app_auto_resize);
	if (show_app_constrained_resize)  ShowExampleAppConstrainedResize(&show_app_constrained_resize);
	if (show_app_simple_overlay)      ShowExampleAppSimpleOverlay(&show_app_simple_overlay);
	if (show_app_fullscreen)          ShowExampleAppFullscreen(&show_app_fullscreen);
	if (show_app_window_titles)       ShowExampleAppWindowTitles(&show_app_window_titles);
	if (show_app_custom_rendering)    ShowExampleAppCustomRendering(&show_app_custom_rendering);
*/

// Dear ImGui Tools/Apps (accessible from the "Tools" menu)
	static bool show_app_metrics = false;
	static bool show_app_debug_log = false;
	static bool show_app_stack_tool = false;
	static bool show_app_about = false;
	static bool show_app_style_editor = false;

	if (show_app_metrics)
		ImGui::ShowMetricsWindow(&show_app_metrics);
	if (show_app_debug_log)
		ImGui::ShowDebugLogWindow(&show_app_debug_log);
	if (show_app_stack_tool)
		ImGui::ShowStackToolWindow(&show_app_stack_tool);
	if (show_app_about)
		ImGui::ShowAboutWindow(&show_app_about);
	if (show_app_style_editor)
	{	ImGui::Begin("Dear ImGui Style Editor", &show_app_style_editor);
		ImGui::ShowStyleEditor();
		ImGui::End();
	}

// Demonstrate the various window flags. Typically you would just use the default!
	static bool no_titlebar = false;
	static bool no_scrollbar = false;
	static bool no_menu = false;
	static bool no_move = false;
	static bool no_resize = false;
	static bool no_collapse = false;
	static bool no_close = false;
	static bool no_nav = false;
	static bool no_background = false;
	static bool no_bring_to_front = false;
	static bool unsaved_document = false;

	ImGuiWindowFlags window_flags = 0;
	if (no_titlebar)        window_flags |= ImGuiWindowFlags_NoTitleBar;
	if (no_scrollbar)       window_flags |= ImGuiWindowFlags_NoScrollbar;
	if (!no_menu)           window_flags |= ImGuiWindowFlags_MenuBar;
	if (no_move)            window_flags |= ImGuiWindowFlags_NoMove;
	if (no_resize)          window_flags |= ImGuiWindowFlags_NoResize;
	if (no_collapse)        window_flags |= ImGuiWindowFlags_NoCollapse;
	if (no_nav)             window_flags |= ImGuiWindowFlags_NoNav;
	if (no_background)      window_flags |= ImGuiWindowFlags_NoBackground;
	if (no_bring_to_front)  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	if (unsaved_document)   window_flags |= ImGuiWindowFlags_UnsavedDocument;
	if (no_close)           p_open = NULL; // Don't pass our bool* to Begin

// We specify a default position/size in case there's no data in the .ini file.
// We only do it to make the demo applications a little more welcoming, but typically this isn't required.
	const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 650, main_viewport->WorkPos.y + 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);

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

// Entorn VGI: Menu Bar (Pop Ups desplagables (Arxius, About)
	if (ImGui::BeginMenuBar())
	{	if (ImGui::BeginMenu("Arxius"))
			{	//IMGUI_DEMO_MARKER("Menu/File");
				//ShowExampleMenuFile();
				//ImGui::Text("Desplegable Arxius");
				ShowArxiusOptions();
				ImGui::EndMenu();
			}

		if (ImGui::BeginMenu("About"))
			{	//IMGUI_DEMO_MARKER("Menu/Examples");
				//ShowAboutOptions(&show_window_about);
				ImGui::MenuItem("Sobre EntornVGI", NULL, &show_window_about);
				ImGui::EndMenu();
			}
		ImGui::EndMenuBar();
	}

	ImGui::Text("Finestra de menus d'EntornVGI");
	ImGui::Spacing();

// Entorn VGI: DESPLEGABLES ---------------------------------------------------
// DESPLEGABLE CAMERA
//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("CAMERA"))
	{	// Entorn VGI. Mostrar Opcions desplegable TIPUS CAMERA
		//IMGUI_DEMO_MARKER("Widgets/Basic/RadioButton");

		ImGui::RadioButton("Esferica (<Shift>+L)", &oCamera, 0); ImGui::SameLine();
		static int clickCE = 0;
		if (ImGui::Button("Origen Esferica (<Shift>+O)")) clickCE++;
		if (ImGui::BeginTable("split", 3))
			{	ImGui::TableNextColumn(); ImGui::Checkbox("Mobil (<Shift>+M)", &mobil);
				ImGui::TableNextColumn(); ImGui::Checkbox("Zoom? (<Shift>+Z)", &zzoom);
				ImGui::TableNextColumn(); ImGui::Checkbox("Zoom Orto? (<Shift>+O)", &zzoomO);
				ImGui::Separator();
				ImGui::TableNextColumn(); ImGui::Checkbox("Satelit? (<Shift>+T)", &satelit);
				ImGui::EndTable();
			}
		ImGui::Separator();
		ImGui::Spacing();

		// EntornVGI: Si s'ha apretat el botó "Origen Esfèrica"
		if (clickCE)
			{	clickCE = 0;
				if (camera == CAM_ESFERICA) OnCameraOrigenEsferica();
			}

		static int clickCN = 0;
		ImGui::RadioButton("Navega (<Shift>+N)", &oCamera, 1); ImGui::SameLine();
		if (ImGui::Button("Origen Navega (<Shift>+Inici)")) clickCN++;
		ImGui::Separator();
		ImGui::Spacing();

		// EntornVGI: Si s'ha apretat el botó "Origen Navega"
		if (clickCN)
			{	clickCN = 0;
				if (camera == CAM_NAVEGA) OnCameraOrigenNavega();
			}

		static int clickCG = 0;
		ImGui::RadioButton("Geode (<Shift>+J)", &oCamera, 2); ImGui::SameLine();
		if (ImGui::Button("Origen Geode (<Shift>+Inici)")) clickCG++;

		// EntornVGI: Si s'ha apretat el botó "Origen Geode"
		if (clickCG)
			{	clickCG = 0;
				if (camera == CAM_GEODE) OnCameraOrigenGeode();
			}

		// Entorn VGI. Gestió opcions desplegable CAMERA segons el valor de la variable selected
		switch (oCamera)
		{	case 0: // Opció CAMERA Esfèrica
				if (camera != CAM_ESFERICA) OnCameraEsferica();
				break;
			case 1: // Opció CAMERA Navega
				if (camera != CAM_NAVEGA) OnCameraNavega();
				break;
			case 2:	// Opció CAMERA Geode
				if (camera != CAM_GEODE) OnCameraGeode();
				break;
			default:
				// Opció per defecte: CAMERA Esfèrica
				OnCameraEsferica();
				break;
		}

		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("POLARS");
		ImGui::PopStyleColor();
		
		// Entorn VGI. Mostrar Opcions desplegable POLARS
		//IMGUI_DEMO_MARKER("Widgets/Basic/RadioButton");
		oPolars = shortCut_Polars();	//static int oPolars = -1;
		ImGui::RadioButton("Polars X (<Shift>+X)", &oPolars, 0); ImGui::SameLine();
		ImGui::RadioButton("Polars Y (<Shift>+Y)", &oPolars, 1); ImGui::SameLine();
		ImGui::RadioButton("Polars Z (<Shift>+Z)", &oPolars, 2);

		// Entorn VGI. Gestió opcions desplegable OBJECTES segons el valor de la variable selected
		switch (oPolars)
		{	case 0: // Opció POLARS X
				if (Vis_Polar != POLARX) OnVistaPolarsX();
				break;
			case 1: // Opció POLARS Y
				if (Vis_Polar != POLARY) OnVistaPolarsY();
				break;
			case 2:	// Opció POLARS Z
				if (Vis_Polar != POLARZ) OnVistaPolarsZ();
				break;
			default:// Opció per defecte: POLARS Z
				OnVistaPolarsZ();
				break;
		}
	}

// DESPLEGABLE VISTA
//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("VISTA"))
	{	if (ImGui::BeginTable("split", 2))
			{	ImGui::TableNextColumn(); ImGui::Checkbox("Full Screen? (<Shift>+F)", &fullscreen);
				ImGui::TableNextColumn(); ImGui::Checkbox("Eixos? (<Shift>+E)", &eixos);
				ImGui::TableNextColumn(); ImGui::Checkbox("SkyBox? (<Shift>+S)", &SkyBoxCube);
				ImGui::EndTable();
			}
		ImGui::Spacing();
		ImGui::Separator();

		// Configurar opció Vista -> SkyBox?
		if (SkyBoxCube) {	// Càrrega Shader Skybox
							OnVistaSkyBox();
						}

		static int clicked = 0;
		if (ImGui::BeginTable("split", 2))
			{	ImGui::TableNextColumn(); ImGui::Checkbox("Pan? (<Shift>+P)", &pan);
				ImGui::TableNextColumn(); //ImGui::Checkbox("Origen Pan", &pan);
				//IMGUI_DEMO_MARKER("Widgets/Basic/Button");
				if (ImGui::Button("Origen Pan (<Shift>+Inici)")) clicked++;
				ImGui::EndTable();
			}
		ImGui::Spacing();
		ImGui::Separator();

		// Configurar opció Vista -> Pan?
		if (pan) OnVistaPan();
		if (clicked)
			{	if (pan) {	clicked = 0;
							OnVistaOrigenPan();
						}
			}
	}

	// DESPLEGABLE PROJECCIO
			//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("PROJECCIO"))
	{	//IMGUI_DEMO_MARKER("Widgets/Selectables/Single Selection PROJECCIO");
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("TIPUS PROJECCIO");
		ImGui::PopStyleColor();

		// Entorn VGI: Mostrar Opcions desplegable PROJECCIO
		//IMGUI_DEMO_MARKER("Widgets/Basic/RadioButton");

		ImGui::RadioButton("Axonometrica (<Shift>+A)", &oProjeccio, 1); ImGui::SameLine();
		ImGui::RadioButton("Ortografica (<Shift>+O)", &oProjeccio, 2); ImGui::SameLine();
		ImGui::RadioButton("Perspectiva (<Shift>+P)", &oProjeccio, 3);

		// Entorn VGI. Gestió opcions desplegable OBJECTES segons el valor de la variable selected
		switch (oProjeccio)
			{	case 1: // Opció PROJECCIÓ Axonomètrica
					if (projeccio != AXONOM) OnProjeccioAxonometrica();
					break;
				case 2: // Opció PROJECCIÓ Ortogràfica
					if (projeccio != ORTO) OnProjeccioOrtografica();
					break;
				case 3:	// Opció PROJECCIÓ Perspectiva
					if (projeccio != PERSPECT) OnProjeccioPerspectiva();
					break;
				default:
					// Opció per defecte: Projecció Perspectiva
					OnProjeccioPerspectiva();
					break;
				}
	}

// DESPLEGABLE VISTA
//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("OBJECTE"))
	{	// Entorn VGI. Mostrar Opcions desplegable OBJECTES
		//IMGUI_DEMO_MARKER("Widgets/Basic/RadioButton");

		// Using the _simplified_ one-liner Combo() api here
		// See "Combo" section for examples of how to use the more flexible BeginCombo()/EndCombo() api.
		//IMGUI_DEMO_MARKER("Widgets/Basic/Combo");
		const char* items[] = { "Cap(<Shift>+B)", "Cub (<Shift>+C)", "Cub RGB (<Shift>+D)", "Esfera (<Shift>+E)", "Tetera (<Shift>+T)",
			"Arc (<Shift>+R)", "Matriu Primitives (<Shift>+H)", "Matriu Primitives VAO (<Shift>+V)", "Tie (<Shift>+I)", "Arxiu OBJ",
			"Bezier (<Shift>+F5)", "B-spline (<Shift>+F6)", "Lemniscata (<Shift>+F7)", "Hermitte (<Shift>+F8)", "Catmull-Rom (<Shift>+F9)" };
		//static int item_current = 0;
		ImGui::Combo(" ", &oObjecte, items, IM_ARRAYSIZE(items));
		ImGui::Spacing();

		// Mostrar fram PARAMETRES CORBES si Objectes Corbes seleccionats
		if ((oObjecte > 9) && (oObjecte < 16)) {
			//IMGUI_DEMO_MARKER("Widgets/Selectables/Single Selection CORBES");
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
			ImGui::SeparatorText("PARAMETRES CORBES:");
			ImGui::PopStyleColor();

			if (ImGui::BeginTable("split", 2))
				{	ImGui::TableNextColumn(); ImGui::Checkbox("Triedre de Frenet? (<Shift>+F11)", &dibuixa_TriedreFrenet);
					ImGui::TableNextColumn(); ImGui::Checkbox("Triedre de Darboux? (<Shift>+F12)", &dibuixa_TriedreDarboux);
					ImGui::TableNextColumn(); ImGui::Checkbox("Punts de Control? (<Shift>+F10)", &sw_Punts_Control);
					ImGui::EndTable();
				}
			}

		// EntornVGI: Variables associades a Pop Up OBJECTE
		bool testA = false;
		int error = 0;

		// Entorn VGI. Gestió opcions desplegable OBJECTES segons el valor de la variable selected
		switch (oObjecte)
		{	case 0: // Opció OBJECTE Cap
				if (objecte != CAP) OnObjecteCap();
				break;
			case 1: // Opció OBJECTE Cub
				if (objecte != CUB) OnObjecteCub();
				break;
			case 2:	// Opció OBJECTE Cub RGB
				if (objecte != CUB_RGB) OnObjecteCubRGB();
				break;
			case 3: // Opció OBJECTE Esfera
				if (objecte != ESFERA) OnObjecteEsfera();
				break;
			case 4: // Opció OBJECTE Tetera
				if (objecte != TETERA) OnObjecteTetera();
				break;
			case 5: // Opció OBJECTE Arc
				if (objecte != ARC) OnObjecteArc();
				break;
			case 6: // Opció OBJECTE MatrVAOiu Primitives
				if (objecte != MATRIUP) OnObjecteMatriuPrimitives();
				break;
			case 7: // Opció OBJECTE Matriu Primitives VAO
				if (objecte != MATRIUP_VAO) OnObjecteMatriuPrimitivesVAO();
				break;
			case 8: // Opció OBJECTE Tie
				if (objecte != TIE) OnObjecteTie();
				break;
			case 9: // Opció OBJECTE Arxiu OBJ
				if (objecte != OBJOBJ) OnArxiuObrirFitxerObj();
				break;
			case 10: // Opció OBJECTE CORBA BEZIER
				if (objecte != C_BEZIER) OnObjecteCorbaBezier();
				break;
			case 11: // Opció OBJECTE CORBA B-SPLINE
				if (objecte != C_BSPLINE) OnObjecteCorbaBSpline();
				break;
			case 12: // Opció OBJECTE CORBA LEMNISCATA
				if (objecte != C_LEMNISCATA) OnObjecteCorbaLemniscata();
				break;
			case 13: // Opció OBJECTE CORBA HERMITTE
				if (objecte != C_HERMITTE) OnObjecteCorbaHermitte();
				break;
			case 14: // Opció OBJECTE CORBA CATMULL-ROM
				if (objecte != C_CATMULL_ROM) OnObjecteCorbaCatmullRom();
				break;
			default:
				// Opció per defecte: CAP
				OnObjecteCap();
				break;
		}
	}

// DESPLEGABLE VISTA
//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("TRANSFORMA"))
	{	//IMGUI_DEMO_MARKER("Widgets/Basic/RadioButton");
		static int clickTT = 0;
		ImGui::Checkbox("Traslacio (<Ctrl>+T)", &trasl); ImGui::SameLine();
		if (ImGui::Button("Origen Traslacio (<Ctrl>+O)")) clickTT++;
		ImGui::Spacing();

		// Si traslació activa, inhibir rotació.
		if (trasl) rota = false;
		transf = trasl || rota || escal;	// Actualitzar variable transf, si hi ha alguna transformació activa.

		// EntornVGI: Si s'ha apretat el botó "Origen Traslacio"
		if (clickTT)
			{	clickTT = 0;
				OnTransformaOrigenTraslacio();
			}

		static int clickTRota = 0;
		ImGui::Checkbox("Rotacio (<Ctrl>+R)", &rota); ImGui::SameLine();
		if (ImGui::Button("Origen Rotacio (<Ctrl>+O)")) clickTRota++;
		ImGui::Spacing();

		// Si rotació activa, inhibir traslació.
		if (rota) trasl = false;
		transf = trasl || rota || escal;	// Actualitzar variable transf, si hi ha alguna transformació activa.

		// EntornVGI: Si s'ha apretat el botó "Origen Rotacio"
		if (clickTRota)
			{	clickTRota = 0;
				OnTransformaOrigenRotacio();
			}

		static int clickTE = 0;
		ImGui::Checkbox("Escalat (<Ctrl>+S)", &escal); ImGui::SameLine();
		if (ImGui::Button("Origen Escalat (<Ctrl>+O)")) clickTE++;

		// Actualitzar variable transf, si hi ha alguna transformació activa.
		transf = trasl || rota || escal;

		// EntornVGI: Si s'ha apretat el botó "Origen Escalat"
		if (clickTE)
			{	clickTE = 0;
				OnTransformaOrigenEscalat();
			}

		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("MOBIL TRANSFORMA:");
		ImGui::PopStyleColor();

		// Guardar valors de les variables prèvies.
		bool transX_old = transX;
		bool transY_old = transY;
		bool transZ_old = transZ;

		ImGui::Checkbox("Mobil Eix X? (<Ctrl>+X)", &transX); ImGui::SameLine();
		ImGui::Checkbox("Mobil Eix y? (<Ctrl>+Y)", &transY); ImGui::SameLine();
		ImGui::Checkbox("Mobil Eix Z? (<Ctrl>+Z)", &transZ);

		if (transX) OnTransformaMobilX();
		if (transY) OnTransformaMobilY();
		if (transZ) OnTransformaMobilZ();
	}

// DESPLEGABLE VISTA
//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("OCULTACIONS"))
	{	//IMGUI_DEMO_MARKER("Widgets/Selectables/Single Selection OCULTACIONS");
		if (ImGui::BeginTable("split", 2))
			{	ImGui::TableNextColumn(); ImGui::Checkbox("Test Visibilitat? (<Ctrl>+C)", &test_vis);
				ImGui::TableNextColumn(); ImGui::Checkbox("Z-Buffer? (<Ctrl>+B)", &oculta);
				ImGui::EndTable();
			}
		ImGui::Separator();
		if (ImGui::BeginTable("split", 2))
			{	ImGui::TableNextColumn(); ImGui::Checkbox("Front Faces? (<Ctrl>+D)", &front_faces);
				//ImGui::TableNextColumn(); ImGui::Checkbox("Back Line? (<Ctrl>+B)", &back_line);
				ImGui::EndTable();
			}
	}

// DESPLEGABLE ILUMINACIO
//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("ILUMINACIO"))
	{	ImGui::Checkbox("Llum Fixe (T) / Llum lligada a camera (F) - (<Ctrl>+F)", &ifixe);
		ImGui::Spacing();

		// Entorn VGI. Mostrar Opcions desplegable TIPUS ILUMINACIO
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("TIPUS ILUMINACIO:");
		ImGui::PopStyleColor();

		// Entorn VGI. Llegir el valor actual de la variable objecte
		//int oldIlumina = shortCut_Iluminacio();	//static int oIlumina = 1;

		// Combo Boxes are also called "Dropdown" in other systems
		// Expose flags as checkbox for the demo
		static ImGuiComboFlags flags = (0 && ImGuiComboFlags_PopupAlignLeft && ImGuiComboFlags_NoPreview && ImGuiComboFlags_NoArrowButton);

		// Using the generic BeginCombo() API, you have full control over how to display the combo contents.
		// (your selection data could be an index, a pointer to the object, an id for the object, a flag intrusively
		// stored in the object itself, etc.)
		const char* items[] = { "Punts (<Ctrl>+F1)", "Filferros (<Ctrl>+F2)", "Plana (<Ctrl>+F3)",
			"Suau (<Ctrl>+F4)" };
		const char* combo_preview_value = items[oIlumina];  // Pass in the preview value visible before opening the combo (it could be anything)
		if (ImGui::BeginCombo("     ", combo_preview_value, flags))
		{	for (int n = 0; n < IM_ARRAYSIZE(items); n++)
			{	const bool is_selected = (oIlumina == n);
				if (ImGui::Selectable(items[n], is_selected)) oIlumina = n;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (is_selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		// Entorn VGI. Gestió opcions desplegable TIPUS ILUMINACIO segons el valor de la variable selected
		switch (oIlumina)
		{	case 0:
				// Opció ILUMINACIO Punts
				if (ilumina != PUNTS) OnIluminacioPunts();
				break;
			case 1:
				// Opció ILUMINACIO Filferros
				if (ilumina != FILFERROS) OnIluminacioFilferros();
				break;
			case 2:
				// Opció ILUMINACIO Plana
				if (ilumina != PLANA) OnIluminacioPlana();
				break;
			case 3:
				// Opció ILUMINACIO Suau
				if (ilumina != SUAU) OnIluminacioSuau();
				break;
			default:
				// Opció per defecte: FILFERROS
				OnIluminacioFilferros();
				break;
		}

		//IMGUI_DEMO_MARKER("Widgets/Selectables/Single Selection REFLEXIO MATERIAL");
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("REFLEXIO MATERIAL:");
		ImGui::PopStyleColor();
		ImGui::Checkbox("Material (T) / Color (F) - (<Ctrl>+F10)", &sw_material[4]);
		ImGui::Separator();
		ImGui::Spacing();

		// Pas de color material o color VAO al shader
		glUniform1i(glcache::UniformLocation(shader_programID, "sw_material"), sw_material[4]);

		if (ImGui::BeginTable("split", 2))
		{	ImGui::TableNextColumn(); ImGui::Checkbox("Emissio? (<Ctrl>+F6)", &sw_material[0]);
			ImGui::TableNextColumn(); ImGui::Checkbox("Ambient? (<Ctrl>+F7)", &sw_material[1]);
			ImGui::TableNextColumn(); ImGui::Checkbox("Difusa? (<Ctrl>+F8)", &sw_material[2]);
			ImGui::TableNextColumn(); ImGui::Checkbox("Especular? (<Ctrl>+F9)", &sw_material[3]);
			ImGui::EndTable();
		}

		// Pas de propietats de materials al shader
		applyMaterialIntensity(shader_programID, llum_ambient);

		//IMGUI_DEMO_MARKER("Widgets/Selectables/Single Selection TEXTURES");
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("TEXTURES:");
		ImGui::PopStyleColor();
		ImGui::Checkbox("Textura? (<Ctrl>+I)", &textura);
		ImGui::SameLine();

		static int clickITS = 0;
		if (ImGui::Button("Imatge Textura SOIL (<Ctrl>+J)")) clickITS++;
		// EntornVGI: Si s'ha apretat el botó "Image Textura SOIL"
		if (clickITS) {	clickITS = 0;
						// Mostrar diàleg de fitxer i carregar imatge com a textura.
						OnIluminacioTexturaFitxerimatge();
						}

		ImGui::Spacing();
		ImGui::Checkbox("Flag_Invert_Y? (<Ctrl>+K)", &tFlag_invert_Y);
		if (tFlag_invert_Y && shader_programID) glUniform1i(glcache::UniformLocation(shader_programID, "flag_invert_y"), tFlag_invert_Y);
	}

// DESPLEGABLE LLUMS
//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("LLUMS"))
	{	ImGui::Checkbox("Llum Ambient? (<Ctrl>+A)", &llum_ambient);
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::BeginTable("split", 2))
		{	ImGui::TableNextColumn();
			ImGui::Checkbox("Llum #0 (+Z) (<Ctrl>+0)?", &llumGL[0].encesa);
			ImGui::TableNextColumn();
			ImGui::Checkbox("Llum #1 (+X) (<Ctrl>+1)?", &llumGL[1].encesa);
			ImGui::TableNextColumn();
			ImGui::Checkbox("Llum #2 (+Y) (<Ctrl>+2)?", &llumGL[2].encesa);
			ImGui::TableNextColumn();
			ImGui::Checkbox("Llum #3 (Z=Y=X) (<Ctrl>+3)?", &llumGL[3].encesa);
			ImGui::TableNextColumn();
			ImGui::Checkbox("Llum #4 (-Z)?(<Ctrl>+4)", &llumGL[4].encesa);
			ImGui::TableNextColumn();
			ImGui::Checkbox("Llum #5?(<Ctrl>+5)", &llumGL[5].encesa);
			ImGui::TableNextColumn();
			ImGui::Checkbox("Llum #6?(<Ctrl>+6)", &llumGL[6].encesa);
			ImGui::TableNextColumn();
			ImGui::Checkbox("Llum #7?(<Ctrl>+7)", &llumGL[7].encesa);
			ImGui::EndTable();
		}
		ImGui::Checkbox("Lamparas de escena", &labLampsEnabled);
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted("Activa o apaga las luces declaradas en el JSON. La linterna F permanece independiente.");
			ImGui::EndTooltip();
		}

		// Activar llum ambient i llums (GL_LIGHT0-GL_LIGTH7)
		if (llum_ambient) OnLlumsLlumAmbient();

		if (llumGL[0].encesa)	OnLlumsLlum0();

		if (llumGL[1].encesa)	OnLlumsLlum1();

		if (llumGL[2].encesa)	OnLlumsLlum2();

		if (llumGL[3].encesa)	OnLlumsLlum3();

		if (llumGL[4].encesa)	OnLlumsLlum4();

		if (llumGL[5].encesa)	OnLlumsLlum5();

		if (llumGL[6].encesa)	OnLlumsLlum6();

		if (llumGL[7].encesa)	OnLlumsLlum7();
	}

// DESPLEGABLE SHADERS
//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("SHADERS"))
	{	// Combo Boxes are also called "Dropdown" in other systems
		// Expose flags as checkbox for the demo
		static ImGuiComboFlags flagsS = (0 && ImGuiComboFlags_PopupAlignLeft && ImGuiComboFlags_NoPreview && ImGuiComboFlags_NoArrowButton);

		// Using the generic BeginCombo() API, you have full control over how to display the combo contents.
		// (your selection data could be an index, a pointer to the object, an id for the object, a flag intrusively
		// stored in the object itself, etc.)
		const char* itemsS[] = { "Flat (<Ctrl>+L)", "Gouraud (<Ctrl>+G)", "Phong (<Ctrl>+P)",
			"Carregar fitxers Shader [.vert,.frag] (<Ctrl>+V)" };
		const char* combo_preview_value = itemsS[oShader];  // Pass in the preview value visible before opening the combo (it could be anything)
		if (ImGui::BeginCombo("Tipus de Shader", combo_preview_value, flagsS))
		{	for (int n = 0; n < IM_ARRAYSIZE(itemsS); n++)
			{	const bool is_selected = (oShader == n);
				if (ImGui::Selectable(itemsS[n], is_selected))
					oShader = n;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (is_selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		// Entorn VGI. Definició de nou Program que substituirà l'actual
		GLuint newShaderID = 0;
		// Entorn VGI. Gestió opcions desplegable TIPUS ILUMINACIO segons el valor de la variable selected
		switch (oShader)
		{	case 0: // Opció SHADER Flat
				if (shader != FLAT_SHADER)	OnShaderFlat();
				break;

			case 1:	// Opció SHADER Gouraud
				if (shader != GOURAUD_SHADER)	OnShaderGouraud();
				break;

			case 2:	// Opció SHADER Phong
				if (shader != PHONG_SHADER)	OnShaderPhong();
				break;

			case 3:	// Opció SHADER Carregar fitxers shader (.vert, .frag)
				if (shader != FILE_SHADER) OnShaderLoadFiles();
				break;

		}
	}

// DESPLEGABLE SHADERS
//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("ABOUT"))
	{	//IMGUI_DEMO_MARKER("Window/About");
		ImGui::Text("VISUALITZACIO GRAFICA INTERACTIVA (Escola d'Enginyeria - UAB");
		ImGui::Text("Entorn Grafic VS2022 amb interficie GLFW 3.4, ImGui i OpenGL 4.6 per a practiques i ABP");
		ImGui::Text("AUTOR: Enric Marti Godia");
		ImGui::Text("Copyright (C) 2026");
		ImGui::Separator();
	}

	// End of ShowDemoWindow()
	ImGui::PopItemWidth();
	ImGui::End();
}
