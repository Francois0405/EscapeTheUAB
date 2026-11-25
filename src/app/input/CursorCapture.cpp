#include "src/app/AppState.h"
#include "src/app/input/CursorCapture.h"
#include "src/app/adapters/PlayerAdapters.h"

bool modoJuegoFPS() { return gameplay::player::isFpsMode(contextoJugador()); }

void fijarCapturaCursor(GLFWwindow* window, bool capturar)
{
	if (fpsCursorCapturado == capturar) return;
	fpsCursorCapturado = capturar;
	fpsTienePosicionAnterior = false;
	glfwSetInputMode(window, GLFW_CURSOR, capturar ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
	// Al cambiar GLFW_CURSOR se reinicia el delta antes de aplicar giro FPS.
}

void sincronizarCursorFPS(GLFWwindow* window)
{
	const bool capturar = modoJuegoFPS() && !editorEscenas.Activo() && !fpsPausado && !mostrarPanelesDebug && ventanaConFoco && fpsCapturaSolicitada;
	fijarCapturaCursor(window, capturar);
}

// Estado del juego FPS que permite la entrada (no hay pausa, ni foco perdido, ni paneles de depuracion).
bool entradaFPSActiva()
{
	return modoJuegoFPS() && !editorEscenas.Activo() && !fpsPausado && !mostrarPanelesDebug &&
		(ventanaConFoco || gestorOpenXR.SesionEnMarcha());
}
