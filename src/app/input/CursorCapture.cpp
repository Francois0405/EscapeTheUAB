#include "src/app/AppIncludes.h"
#include "src/app/state/SceneState.h"
#include "src/app/state/PlayerState.h"
#include "src/app/state/VrState.h"
#include "src/app/input/CursorCapture.h"
#include "src/app/adapters/PlayerAdapters.h"

bool fpsModeActive() { return gameplay::player::isFpsMode(playerBindings()); }

void setCursorCapture(GLFWwindow* window, bool capturar)
{
	if (fpsCursorCaptured == capturar) return;
	fpsCursorCaptured = capturar;
	fpsHasPreviousPosition = false;
	glfwSetInputMode(window, GLFW_CURSOR, capturar ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
	// Al cambiar GLFW_CURSOR se reinicia el delta antes de aplicar giro FPS.
}

void syncFpsCursor(GLFWwindow* window)
{
	const bool capturar = fpsModeActive() && !sceneEditor.IsEnabled() && !fpsPaused && !showDebugPanels && windowFocused && fpsCaptureRequested;
	setCursorCapture(window, capturar);
}

// Estado del juego FPS que permite la entrada (no hay pausa, ni foco perdido, ni paneles de depuracion).
bool fpsInputActive()
{
	return fpsModeActive() && !sceneEditor.IsEnabled() && !fpsPaused && !showDebugPanels &&
		(windowFocused || openXrManager.IsSessionRunning());
}
