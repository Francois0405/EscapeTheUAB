#include "src/app/AppState.h"
#include "src/legacy_vgi/VgiStatusBar.h"

// Barra_Estat: Actualitza la barra d'estat (Status Bar) de l'aplicació en la consola amb els
//      valors R,A,B,PVx,PVy,PVz en Visualització Interactiva.
void Barra_Estat()
{
	const std::string estado = runtimeui::FormatStatusBar({ nom, projeccio, camera, objecte, Vis_Polar,
		OPV, OPV_G, opvN, pan, tr_cpv, sw_grid, sw_color, transf, rota, trasl, escal,
		fonsR, fonsG, fonsB, TG, c_fons, col_obj, pas });
	std::fputs(estado.c_str(), stderr);
}
