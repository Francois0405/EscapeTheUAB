#include "src/app/AppState.h"
#include "src/app/VrToggle.h"

void toggleOpenXr()
{
	if (openXrManager.HasResources()) openXrManager.Deactivate();
	else openXrManager.SetActive(mainWindow, glMajorVersion, glMinorVersion);
}
