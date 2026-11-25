#include "src/app/AppState.h"
#include "src/app/VrToggle.h"

void toggleOpenXr()
{
	if (openXrManager.TieneRecursos()) openXrManager.Desactivar();
	else openXrManager.Activar(mainWindow, glMajorVersion, glMinorVersion);
}
