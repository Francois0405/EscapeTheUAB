#include "src/app/AppState.h"
#include "src/app/VrToggle.h"

void alternarOpenXR()
{
	if (gestorOpenXR.TieneRecursos()) gestorOpenXR.Desactivar();
	else gestorOpenXR.Activar(ventanaPrincipal, versionGLMayor, versionGLMenor);
}
