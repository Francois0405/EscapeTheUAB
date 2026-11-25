#include "src/app/AppIncludes.h"
#include "src/app/state/VrState.h"
#include "src/app/VrToggle.h"

void toggleOpenXr()
{
	if (openXrManager.HasResources()) openXrManager.Deactivate();
	else openXrManager.SetActive(mainWindow, glMajorVersion, glMinorVersion);
}
