#include "src/app/AppIncludes.h"
#include "src/app/state/SceneState.h"
#include "src/app/state/RenderState.h"
#include "src/app/state/PlayerState.h"
#include "src/app/state/GameplayState.h"
#include "src/app/state/NetworkState.h"
#include "src/app/state/VrState.h"
#include "src/app/input/KeyboardInput.h"
#include "src/app/VrToggle.h"
#include "src/app/adapters/InteractionAdapters.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/app/input/CursorCapture.h"
#include "src/app/adapters/GrabTrace.h"
#include "src/legacy_vgi/VgiKeyboard.h"

/* ------------------------------------------------------------------------- */
/*                            FI MENUS ImGui                                 */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/*                           CONTROL DEL TECLAT                              */
/* ------------------------------------------------------------------------- */

// OnKeyDown: Funció de tractament de teclat (funció que es crida quan es prem una tecla)
//   PARÀMETRES:
//    - key: Codi del caracter seleccionat
//    - scancode: Nombre de vegades que s'ha apretat la tecla (acceleració)
//    - action: Acció de la tecla: GLFW_PRESS (si s'ha apretat), GLFW_REPEAT, si s'ha repetit pressió i GL_RELEASE, si es deixa d'apretar.
//    - mods: Variable que identifica si la tecla s'ha pulsat directa (mods=0), juntament amb la tecla Shift (mods=1) o la tecla Ctrl (mods=2).
void OnKeyDown(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	const double incr = 0.025f;
	double modul = 0;
	GLdouble vdir[3] = { 0, 0, 0 };

	// EntornVGI.ImGui: Test si events de mouse han sigut filtrats per ImGui (io.WantCaptureMouse)
	// (1) ALWAYS forward mouse data to ImGui! This is automatic with default backends. With your own backend:
	ImGuiIO& io = ImGui::GetIO();
	// F4 es global: el panel se puede recuperar desde PLAY, EDIT, pausa o VR,
	// incluso cuando los paneles de depuracion estan cerrados.
	if (key == GLFW_KEY_F4 && action == GLFW_PRESS && mods == 0 &&
		!io.WantTextInput)
	{
		showNetworkPanel = !showNetworkPanel;
		syncFpsCursor(window);
		return;
	}
	if (key == GLFW_KEY_E && action == GLFW_RELEASE && throwChargeActive)
	{
		if (fpsInputActive() && !io.WantTextInput && !io.WantCaptureKeyboard &&
			!io.WantCaptureMouse && !openXrManager.IsSessionRunning())
			throwHeldObject();
		else
			cancelThrowChargeInput();
		return;
	}

	// F6: alternar entre PLAY y EDIT.
	if (key == GLFW_KEY_F6 &&
		action == GLFW_PRESS &&
		mods == 0 &&
		!io.WantTextInput &&
		currentGameState != ESTADO_MENU_PRINCIPAL)
	{
		if (networkGame.IsOnline() && networkGame.IsClient())
		{
			sceneEditor.ReportStatus("Solo el host puede editar la escena compartida.", true);
			return;
		}
		sceneEditor.Shortcut(key, action, mods, false);
		syncFpsCursor(window);
		return;
	}

	if (sceneEditor.IsEnabled())
	{
		sceneEditor.Shortcut(key, action, mods, io.WantTextInput);
		syncFpsCursor(window);
		return;
	}

	// F: encender/apagar linterna solo durante el juego.
	if (key == GLFW_KEY_F &&
		action == GLFW_PRESS &&
		currentGameState == ESTADO_JUGANDO &&
		fpsInputActive() &&
		!io.WantTextInput &&
		!io.WantCaptureKeyboard)
	{
		phoneFlashlightEnabled = !phoneFlashlightEnabled;

		std::cout << "Linterna: "
				<< (phoneFlashlightEnabled ? "ON" : "OFF")
				<< std::endl;
		return;
	}

	if (fpsModeActive())
	{
	// ESC: pausar y reanudar.
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		if (currentGameState == ESTADO_JUGANDO)
		{
			currentGameState = ESTADO_PAUSA;
			cancelThrowChargeInput();
		}
		else if (currentGameState == ESTADO_PAUSA)
		{
			currentGameState = ESTADO_JUGANDO;
			fpsHasPreviousPosition = false;
			mouseDeltaX = mouseDeltaY = 0.0;
		}

		fpsPaused = (currentGameState != ESTADO_JUGANDO);
		syncFpsCursor(window);
		return;
	}

	// Aquí continúa el resto del código FPS.
	if (key == GLFW_KEY_M && action == GLFW_PRESS &&
		(mods & GLFW_MOD_CONTROL) && (mods & GLFW_MOD_SHIFT))
	{
		fpsCaptureRequested = !fpsCaptureRequested;
		syncFpsCursor(window);
		return;
	}
	if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
	{
		showDebugPanels = !showDebugPanels;
		if (showDebugPanels) show_EntornVGI_window = true;
		syncFpsCursor(window);
		return;
	}
	// Mantener E carga el lanzamiento; soltarla lanza el objeto sujeto.
	if (key == GLFW_KEY_E && action == GLFW_PRESS)
	{
		const bool gateJuego = glb::CanInteract(fpsPaused, windowFocused, io.WantTextInput);
		const bool gateInterfaz = !showDebugPanels && !io.WantCaptureKeyboard &&
			!io.WantCaptureMouse && !openXrManager.IsSessionRunning();
		if (grabTraceEnabled())
		{
			const InteractionTarget& objetivo = currentInteractionTarget;
			uint32_t idObjetivo = 0;
			if (objetivo.accion == InteractionAction::PickUp && objetivo.indiceDinamico >= 0 &&
				objetivo.indiceDinamico < static_cast<int>(sceneObjects.size()))
				idObjetivo = netgame::HashId(sceneObjects[objetivo.indiceDinamico].id);
			glm::vec3 origenRayo(0.0f), direccionRayo(0.0f);
			gazeOriginAndDirection(origenRayo, direccionRayo);
			float distanciaRayo = std::numeric_limits<float>::infinity();
			const fisica::IdCuerpo cuerpoRayo = physicsWorld.BodyAtRayEnd(
				origenRayo, direccionRayo,
				std::max(fisica::PhysicsScene::AlcanceDeAgarre, glb::RADIO_INTERACCION), &distanciaRayo);
			const int indiceCuerpoDinamico = cuerpoRayo == fisica::CuerpoInvalido
				? -1 : physicsScene->DynamicObjectOfBody(cuerpoRayo);
			const int indiceCuerpoAnimado = cuerpoRayo == fisica::CuerpoInvalido
				? -1 : physicsScene->AnimatedObjectOfBody(cuerpoRayo);
			std::string idCuerpoRayo = "none";
			if (indiceCuerpoDinamico >= 0 && indiceCuerpoDinamico < static_cast<int>(sceneObjects.size()))
				idCuerpoRayo = sceneObjects[indiceCuerpoDinamico].id;
			const char* rol = networkGame.IsHosting() ? "host" : networkGame.IsClient() ? "client" : "offline";
			std::ostringstream registro;
			registro << "[grab] input key=E role=" << rol
				<< " game=" << (currentGameState == ESTADO_JUGANDO)
				<< " allowed=" << (gateJuego && gateInterfaz)
				<< " paused=" << fpsPaused << " focused=" << windowFocused
				<< " text=" << io.WantTextInput << " captureKeyboard=" << io.WantCaptureKeyboard
				<< " captureMouse=" << io.WantCaptureMouse
				<< " debugPanels=" << showDebugPanels
				<< " xr=" << openXrManager.IsSessionRunning()
				<< " holding=" << playerHoldsObject()
				<< " action=" << static_cast<int>(objetivo.accion)
				<< " dynamicIndex=" << objetivo.indiceDinamico
				<< " targetId=" << idObjetivo
				<< " rayBody=" << cuerpoRayo << " rayDynamicIndex=" << indiceCuerpoDinamico
				<< " rayAnimatedIndex=" << indiceCuerpoAnimado << " rayObjectId=" << idCuerpoRayo
				<< " rayDistance=" << distanciaRayo
				<< " rayOrigin=(" << origenRayo.x << ',' << origenRayo.y << ',' << origenRayo.z
				<< ") rayDirection=(" << direccionRayo.x << ',' << direccionRayo.y << ',' << direccionRayo.z << ')';
			emitGrabTrace(registro.str());
		}
		if (gateJuego && gateInterfaz)
		{
			if (playerHoldsObject()) beginThrowCharge();
			else performInteraction();
		}
		return;
	}
	// F3: activar/desactivar OpenXR de forma EXPLÍCITA (independiente de F2).
	if (key == GLFW_KEY_F3 && action == GLFW_PRESS)
	{
		toggleOpenXr();
		return;
	}
	// F2: alternar la vista estereoscópica de prueba (dos ojos en pantalla partida).
	if (key == GLFW_KEY_F2 && action == GLFW_PRESS)
	{
		stereoTestMode = !stereoTestMode;
		std::cout << "Vista estereoscopica de prueba: " << (stereoTestMode ? "ON" : "OFF") << std::endl;
		return;
	}
	if (fpsPaused || !windowFocused) return;
	if (key == GLFW_KEY_W || key == GLFW_KEY_A || key == GLFW_KEY_S || key == GLFW_KEY_D) return;
	if (key == GLFW_KEY_PRINT_SCREEN && mods == 0 && action == GLFW_PRESS)
		statusB = !statusB;
	return;
}

// (2) ONLY forward mouse data to your underlying app/game.
	if (!io.WantCaptureKeyboard) { //<Tractament mouse de l'aplicació>}
		// EntornVGI: Si tecla pulsada és ESCAPE, tancar finestres i aplicació.
		if (mods == 0 && key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		{
			if (sceneEditor.HasUnsavedChanges()) sceneEditor.RequestCloseConfirmation();
			else glfwSetWindowShouldClose(window, GL_TRUE);
		}
		else if (mods == 0 && key == GLFW_KEY_PRINT_SCREEN && action == GLFW_PRESS) statusB = !statusB;
		else if ((mods == 1) && (action == GLFW_PRESS)) Teclat_Shift(key, window);	// Shorcuts Shift Key
		else if ((mods == 2) && (action == GLFW_PRESS)) Teclat_Ctrl(key);	// Shortcuts Ctrl Key
		else if ((objecte == C_BEZIER || objecte == C_BSPLINE || objecte == C_LEMNISCATA || objecte == C_HERMITTE
			|| objecte == C_CATMULL_ROM) && (action == GLFW_PRESS)) Teclat_PasCorbes(key, action);
		else if (camera == CAM_NAVEGA) Teclat_Navega(key, action);
		else if ((sw_grid) && ((grid.x) || (grid.y) || (grid.z))) Teclat_Grid(key, action);
		else if (((key == GLFW_KEY_G) && (action == GLFW_PRESS)) && ((grid.x) || (grid.y) || (grid.z))) sw_grid = !sw_grid;
		else if ((key == GLFW_KEY_O) && (action == GLFW_PRESS)) sw_color = true; // Activació color objecte
		else if ((key == GLFW_KEY_F) && (action == GLFW_PRESS)) sw_color = false; // Activació color objecte
		else if (pan) Teclat_Pan(key, action);
		else if (transf)
			{	if (rota) Teclat_TransRota(key, action);
					else if (trasl) Teclat_TransTraslada(key, action);
							else if (escal) Teclat_TransEscala(key, action);
			}
		else if (!sw_color) Teclat_ColorFons(key, action);
		else Teclat_ColorObjecte(key, action);
	}
	// Crida a OnPaint() per redibuixar l'escena
		//OnPaint(window);

/*
	if (key == GLFW_KEY_E && action == GLFW_PRESS)
			activate_airship();

	int state = glfwGetKey(window, GLFW_KEY_E);
	if (state == GLFW_PRESS) activate_airship();
*/
}
