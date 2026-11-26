#include "src/app/AppIncludes.h"
#include "src/app/state/SceneState.h"
#include "src/app/state/RenderState.h"
#include "src/app/state/PlayerState.h"
#include "src/app/state/GameplayState.h"
#include "src/app/state/NetworkState.h"
#include "src/app/state/VrState.h"
#include "src/app/ui/UiFrame.h"
#include "src/app/DemoDoor.h"
#include "src/app/VrToggle.h"
#include "src/app/adapters/SessionAdapters.h"
#include "src/app/adapters/InteractionAdapters.h"
#include "src/app/input/CursorCapture.h"
#include "src/legacy_vgi/VgiMainWindow.h"

// State used only by this file.
namespace
{
	float masterVolumePercent = 100.0f;
}

// Ventanas ImGui heredadas del entorno VGI (demo, EntornVGI y "Status Menu").
void drawLegacyImGuiWindows()
{
	if (!fpsModeActive() || showDebugPanels)
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
	const auto fpsMode = [] { return fpsModeActive(); };
	const runtimeui::FrameContext contexto{
		// HUD
		{ fpsMode, [] { return fpsInputActive(); }, [] { return sceneEditor.IsEnabled(); }, [] { return playerHoldsObject(); },
			[] { return currentGameState == GAME_STATE_PLAYING && fpsModeActive() && !sceneEditor.IsEnabled() &&
				!showDebugPanels && !stereoTestMode && !openXrManager.IsSessionRunning(); },
			staminaBar, playerMovement, showStaminaPercent,
			throwChargeActive, throwChargeTime, kMaxThrowChargeDuration, physicsScene,
			[]() -> const char* { return currentInteractionTarget.accion == InteractionAction::None
				? nullptr : currentInteractionTarget.texto; },
			interactionHandTexture, retroSettings.pickupNotice },
		// Menus
		{ window, currentGameState, menuBackgroundTexture, masterVolumePercent, audioManager,
			[] { fpsHasPreviousPosition = false; mouseDeltaX = mouseDeltaY = 0.0; } },
		// Multijugador
		{ networkGame, pendingNetworkConfig, showNetworkPanel,
			[](const netgame::NetworkConfig& cfg) { startNetwork(cfg); }, [] { stopNetwork(); } },
		// Objetos dinamicos (F4)
		{ networkGame, physicsScene, sceneObjects, hiritiFollowPlayerEnabled, hiritiAnimator },
		// Paneles de depuracion (F1)
		{ showPhone, phonePosition, phoneRotation, phoneScale, idleSwayIntensity, walkSwayIntensity, mouseSwayIntensity, deltaFPS,
			playerMovement, cameraBob, showStaminaPercent, showFpsCounter, optimizedRendering, lastGpuMs, lightingDiagnosticMode,
			demoDoorPlayback, demoDoorAngle, showColliders, hiritiFollowPlayerEnabled, hiritiFollowStopDistance, hiritiFollowMaxSpeed,
			stereoTestMode, openXrManager,
			physicsWorld, physicsScene, fallingCubeDemo, networkGame, sceneObjects, animatedObjects,
			[] { startDemoDoor(); }, [] { toggleOpenXr(); }, retroSettings },
		// Subtitulos
		{ subtitleManager, retroSettings, [](unsigned int id) { return audioManager.isPlaying(id); },
			[](unsigned int id) { return audioManager.playbackCursorSeconds(id); } },
		(c_fons.r < 0.5) || (c_fons.g < 0.5) || (c_fons.b < 0.5),
		showDebugPanels, showFpsCounter,
		drawLegacyImGuiWindows,
		[](int ancho, int alto) { sceneEditor.DrawEditorInterface(editor::EditorSceneContext{ sceneObjects, sceneAnimatedData, animatedObjects }, ancho, alto); }
	};
	runtimeui::DrawRuntimeUiFrame(contexto);
}
