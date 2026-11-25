#include "src/app/AppState.h"
#include "src/ui/UiFrame.h"
#include "src/app/DemoDoor.h"
#include "src/app/VrToggle.h"
#include "src/app/adapters/SessionAdapters.h"
#include "src/app/input/CursorCapture.h"
#include "src/legacy_vgi/VgiMainWindow.h"

// Ventanas ImGui heredadas del entorno VGI (demo, EntornVGI y "Status Menu").
void dibujarVentanasLegacyImGui()
{
	if (!modoJuegoFPS() || mostrarPanelesDebug)
	{

// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
	if (show_demo_window)
		ImGui::ShowDemoWindow(&show_demo_window);

// 2.a. Show another simple window.
	if (show_another_window)
	{
		ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
		ImGui::Text("Hello from another window!");
		if (ImGui::Button("Close Me"))
			show_another_window = false;
		ImGui::End();
	}

// 2.b. Show the EntornVGI window. Finestra amb totes les opcions de l'aplicació.
	if (show_EntornVGI_window)
		ShowEntornVGIWindow(&show_EntornVGI_window); //ShowEntornVGIWindow(&show_EntornVGI_window);

// 3. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
	{	static float f = 0.0f;
		static int counter = 0;
		static float PV[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

		ImGui::Begin("Status Menu");                          // Create a window called "Status Menu" and append into it.

		ImGui::Text("Finestres EntornVGI:");               // Display some text (you can use a format strings too)
		ImGui::SameLine();
		ImGui::Checkbox("EntornVGI Window", &show_EntornVGI_window);
		static bool preset = true;
		ImGui::Separator();
		ImGui::Spacing();

		// Transformació PV de Coord. esfèriques (R,anglev,angleh) --> Coord. cartesianes (PVx,PVy,PVz)
		if (camera == CAM_NAVEGA) { PV[0] = opvN.x; PV[1] = opvN.y; PV[2] = opvN.z; }
		else {
			cam_Esferica[0] = OPV.R;	cam_Esferica[1] = OPV.alfa; cam_Esferica[2] = OPV.beta;
			if (Vis_Polar == POLARZ)
				{	PV[0] = OPV.R * cos(OPV.beta * PI / 180) * cos(OPV.alfa * PI / 180);
					PV[1] = OPV.R * sin(OPV.beta * PI / 180) * cos(OPV.alfa * PI / 180);
					PV[2] = OPV.R * sin(OPV.alfa * PI / 180);
				}
			else if (Vis_Polar == POLARY)
				{	PV[0] = OPV.R * sin(OPV.beta * PI / 180) * cos(OPV.alfa * PI / 180);
					PV[1] = OPV.R * sin(OPV.alfa * PI / 180);
					PV[2] = OPV.R * cos(OPV.beta * PI / 180) * cos(OPV.alfa * PI / 180);
			}
			else {	PV[0] = OPV.R * sin(OPV.alfa * PI / 180);
					PV[1] = OPV.R * cos(OPV.beta * PI / 180) * cos(OPV.alfa * PI / 180);
					PV[2] = OPV.R * sin(OPV.beta * PI / 180) * cos(OPV.alfa * PI / 180);
				}
		}

		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("CAMERA:");
		ImGui::PopStyleColor();

		ImGui::InputFloat3("Esferiques (R,alfa,beta)", cam_Esferica);
		ImGui::InputFloat3("Cartesianes (PVx,PVy,PVz)", PV);
		//ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f

		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("COLORS:");
		ImGui::PopStyleColor();
		ImGui::ColorEdit3("Color de Fons", (float*)&clear_colorB); // Edit 3 floats representing a background color
		ImGui::ColorEdit3("Color d'Objecte", (float*)&clear_colorO); // Edit 3 floats representing a object color
		c_fons.r = clear_colorB.x;	c_fons.g = clear_colorB.y;	c_fons.b = clear_colorB.z;	c_fons.a = clear_colorB.w;
		col_obj.r = clear_colorO.x;	col_obj.g = clear_colorO.y;	col_obj.b = clear_colorO.z;		col_obj.a = clear_colorO.w;
		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("TRANSFORMA:");
		ImGui::PopStyleColor();
		float tras_ImGui[3] = { (float)TG.VTras.x,(float)TG.VTras.y,(float)TG.VTras.z };
		ImGui::InputFloat3("Traslacio (Tx,Ty,Tz)", tras_ImGui);
		float rota_ImGui[3] = { (float)TG.VRota.x,(float)TG.VRota.y,(float)TG.VRota.z };
		ImGui::InputFloat3("Rotacio (Rx,Ry,Rz)", rota_ImGui);
		float scal_ImGui[3] = { (float)TG.VScal.x,(float)TG.VScal.y,(float)TG.VScal.z };
		ImGui::InputFloat3("Escala (Sx, Sy, Sz)", scal_ImGui);

		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("ImGui:");               // Display some text (you can use a format strings too)
		ImGui::PopStyleColor();
		ImGui::Checkbox("Demo ImGui Window", &show_demo_window);      // Edit bools storing our window open/close state
		ImGui::SameLine();
		ImGui::Checkbox("Another ImGui Window", &show_another_window);
		ImGui::Spacing();

		ImGui::Text("imgui versions: (%s) (%d)", IMGUI_VERSION, IMGUI_VERSION_NUM);
		ImGui::Spacing();

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}
	}
}

void draw_Menu_ImGui()
{
	const auto fpsMode = [] { return modoJuegoFPS(); };
	const runtimeui::FrameContext contexto{
		// HUD
		{ fpsMode, [] { return entradaFPSActiva(); }, [] { return editorEscenas.Activo(); },
			barraEstamina, movimientoJugador, mostrarPorcentajeEstamina,
			cargaLanzamientoActiva, tiempoCargaLanzamiento, TiempoMaximoCargaLanzamiento, escenaFisica,
			[]() -> const char* { return objetivoInteraccionActual.accion == InteractionAction::Ninguna
				? nullptr : objetivoInteraccionActual.texto; },
			texturaManoInteraccion },
		// Menus
		{ window, estadoActual, texturaFondoMenu, volumenGeneralPorcentaje, audioManager,
			[] { fpsTienePosicionAnterior = false; mouseDeltaX = mouseDeltaY = 0.0; } },
		// Multijugador
		{ redJuego, configRedPendiente, mostrarPanelRed,
			[](const netgame::NetworkConfig& cfg) { iniciarRed(cfg); }, [] { detenerRed(); } },
		// Objetos dinamicos (F4)
		{ redJuego, escenaFisica, objetosEscena, hiritiFollowPlayerEnabled, animacionHiriti },
		// Paneles de depuracion (F1)
		{ mostrarMovil, movilPosicion, movilRotacion, movilEscala, intensidadIdle, intensidadWalking, intensidadMouse, deltaFPS,
			movimientoJugador, balanceoCamara, mostrarPorcentajeEstamina, mostrarContadorFPS, renderizadoOptimizado, ultimoGpuMs, modoDiagnosticoIluminacion,
			reproductorPuertaDemo, anguloPuertaDemo, mostrarColisionadores, hiritiFollowPlayerEnabled, hiritiFollowStopDistance, hiritiFollowMaxSpeed,
			modoEstereoPrueba, gestorOpenXR,
			mundoFisico, escenaFisica, demoFisica, redJuego, objetosEscena, objetosAnimados,
			[] { iniciarPuertaDemo(); }, [] { alternarOpenXR(); } },
		(c_fons.r < 0.5) || (c_fons.g < 0.5) || (c_fons.b < 0.5),
		mostrarPanelesDebug, mostrarContadorFPS,
		dibujarVentanasLegacyImGui,
		[](int ancho, int alto) { editorEscenas.DibujarInterfaz(editor::EditorSceneContext{ objetosEscena, datosAnimadosEscena, objetosAnimados }, ancho, alto); }
	};
	runtimeui::DrawRuntimeUiFrame(contexto);
}
