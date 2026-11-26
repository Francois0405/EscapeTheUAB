#include "src/ui/StatusBar.h"

#include <cmath>
#include <cstdio>

namespace runtimeui
{
	namespace
	{
		// Every status line takes exactly one C string, as the original fprintf calls did.
		void Append(std::string& output, const char* format, const char* value)
		{
			const int length = std::snprintf(nullptr, 0, format, value);
			if (length <= 0) return;
			const std::size_t offset = output.size();
			output.resize(offset + static_cast<std::size_t>(length) + 1);
			std::snprintf(output.data() + offset, static_cast<std::size_t>(length) + 1, format, value);
			output.resize(offset + static_cast<std::size_t>(length));
		}
	}

	std::string FormatStatusBar(const StatusBarState& state)
	{
		const auto& [nom, projeccio, camera, objecte, Vis_Polar, OPV, OPV_G, opvN, pan, tr_cpv,
			sw_grid, sw_color, transf, rota, trasl, escal, fonsR, fonsG, fonsB, TG, c_fons, col_obj, pas] = state;
		std::string output;

	std::string buffer, sss;
	CEsfe3D OPVAux = { 0.0, 0.0, 0.0 };
	double PVx, PVy, PVz;

// Status Bar fitxer fractal
	if (nom != "") Append(output, "Fitxer: %s \n", nom.c_str());

// Càlcul dels valors per l'opció Vista->Navega
	if (projeccio != CAP && projeccio != ORTO) {
		if (camera == CAM_ESFERICA)
		{	// Càmera Esfèrica
			OPVAux.R = OPV.R; OPVAux.alfa = OPV.alfa; OPVAux.beta = OPV.beta;
		}
		else if (camera == CAM_NAVEGA)
		{	// Càmera Navega
			OPVAux.R = sqrt(opvN.x * opvN.x + opvN.y * opvN.y + opvN.z * opvN.z);
			OPVAux.alfa = (asin(opvN.z / OPVAux.R) * 180) / PI;
			OPVAux.beta = (atan(opvN.y / opvN.x)) * 180 / PI;
		}
		else {	// Càmera Geode
			OPVAux.R = OPV_G.R; OPVAux.alfa = OPV_G.alfa; OPVAux.beta = OPV_G.beta;
			}
	}
	else {	OPVAux.R = OPV.R; OPVAux.alfa = OPV.alfa; OPVAux.beta = OPV.beta;
		 }

// Status Bar R Origen Punt de Vista
	if (projeccio == CAP) buffer = "       ";
		else if (projeccio == ORTO) buffer = " ORTO   ";
				else if (camera == CAM_NAVEGA) buffer = " NAV   ";
						else buffer = std::to_string(OPVAux.R);
// Refrescar posició R Status Bar
	Append(output, "R=: %s", buffer.c_str());

// Status Bar angle alfa Origen Punt de Vista
	if (projeccio == CAP) buffer = "       ";
		else if (projeccio == ORTO) buffer = "ORTO   ";
				else if (camera == CAM_NAVEGA) buffer = " NAV   ";
						else buffer = std::to_string(OPVAux.alfa);
// Refrescar posició angleh Status Bar
	Append(output, " a=: %s", buffer.c_str());

// Status Bar angle beta Origen Punt de Vista
	if (projeccio == CAP) buffer = "       ";
			else if (projeccio == ORTO) buffer = "ORTO   ";
					else if (camera == CAM_NAVEGA) buffer = " NAV   ";
							else buffer = std::to_string(OPVAux.beta);
// Refrescar posició anglev Status Bar
	Append(output, " ß=: %s  ", buffer.c_str());

// Transformació PV de Coord. esfèriques (R,anglev,angleh) --> Coord. cartesianes (PVx,PVy,PVz)
	if (camera == CAM_NAVEGA) { PVx = opvN.x; PVy = opvN.y; PVz = opvN.z; }
	else {if (Vis_Polar == POLARZ)
			{	PVx = OPVAux.R * cos(OPVAux.beta * PI / 180) * cos(OPVAux.alfa * PI / 180);
				PVy = OPVAux.R * sin(OPVAux.beta * PI / 180) * cos(OPVAux.alfa * PI / 180);
				PVz = OPVAux.R * sin(OPVAux.alfa * PI / 180);
				if (camera == CAM_GEODE)
					{	// Vector camp on mira (cap a (R,alfa,beta)
						PVx = PVx + cos(OPVAux.beta * PI / 180) * cos(OPVAux.alfa * PI / 180);
						PVy = PVy + sin(OPVAux.beta * PI / 180) * cos(OPVAux.alfa * PI / 180);
						PVz = PVz + sin(OPVAux.alfa * PI / 180);
					}
			}
		else if (Vis_Polar == POLARY)
			{	PVx = OPVAux.R * sin(OPVAux.beta * PI / 180) * cos(OPVAux.alfa * PI / 180);
				PVy = OPVAux.R * sin(OPVAux.alfa * PI / 180);
				PVz = OPVAux.R * cos(OPVAux.beta * PI / 180) * cos(OPVAux.alfa * PI / 180);
				if (camera == CAM_GEODE)
					{	// Vector camp on mira (cap a (R,alfa,beta)
						PVx = PVx + sin(OPVAux.beta * PI / 180) * cos(OPVAux.alfa * PI / 180);
						PVy = PVy + sin(OPVAux.alfa * PI / 180);
						PVz = PVz + cos(OPVAux.beta * PI / 180) * cos(OPVAux.alfa * PI / 180);
					}
			}
		else {	PVx = OPVAux.R * sin(OPVAux.alfa * PI / 180);
				PVy = OPVAux.R * cos(OPVAux.beta * PI / 180) * cos(OPVAux.alfa * PI / 180);
				PVz = OPVAux.R * sin(OPVAux.beta * PI / 180) * cos(OPVAux.alfa * PI / 180);
				if (camera == CAM_GEODE)
					{	// Vector camp on mira (cap a (R,alfa,beta)
						PVx = PVx + sin(OPVAux.alfa * PI / 180);
						PVy = PVy + cos(OPVAux.beta * PI / 180) * cos(OPVAux.alfa * PI / 180);
						PVz = PVz + sin(OPVAux.beta * PI / 180) * cos(OPVAux.alfa * PI / 180);
					}
			}
		}

// Status Bar PVx
	if (projeccio == CAP) buffer = "       ";
		else if (pan) buffer = std::to_string(tr_cpv.x);
				else buffer = std::to_string(PVx);
	//sss = _T("PVx=") + buffer;
// Refrescar posició PVx Status Bar
	Append(output, "PVx= %s", buffer.c_str());

	// Status Bar PVy
	if (projeccio == CAP) buffer = "       ";
	else if (pan) buffer = std::to_string(tr_cpv.y);
	else buffer = std::to_string(PVy);
// Refrescar posició PVy Status Bar
	Append(output, " PVy= %s", buffer.c_str());

	// Status Bar PVz
	if (projeccio == CAP) buffer = "       ";
		else if (pan) buffer = std::to_string(tr_cpv.z);
				else buffer = std::to_string(PVz);
// Refrescar posició PVz Status Bar
	Append(output, " PVz= %s \n", buffer.c_str());

// Status Bar per indicar el modus de canvi de color (FONS o OBJECTE)
	sss = " ";
	if (sw_grid) sss = "GRID ";
		else if (pan) sss = "PAN ";
				else if (camera == CAM_NAVEGA) sss = "NAV ";
						else if (sw_color) sss = "OBJ ";
								else sss = "FONS ";
// Refrescar posició Transformacions en Status Bar
	Append(output, "%s ", sss.c_str());

// Status Bar per indicar tipus de Transformació (TRAS, ROT, ESC)
	sss = " ";
	if (transf) {
		if (rota) sss = "ROT";
		else if (trasl) sss = "TRA";
		else if (escal) sss = "ESC";
		}
	else if ((!sw_grid) && (!pan) && (camera != CAM_NAVEGA))
		{	// Components d'intensitat de fons que varien per teclat
		if ((fonsR) && (fonsG) && (fonsB)) sss = " RGB";
			else if ((fonsR) && (fonsG)) sss = " RG ";
			else if ((fonsR) && (fonsB)) sss = " R   B";
			else if ((fonsG) && (fonsB)) sss = "   GB";
			else if (fonsR) sss = " R  ";
			else if (fonsG) sss = "   G ";
			else if (fonsB) sss = "      B";
		}
// Refrescar posició Transformacions en Status Bar
	Append(output, "%s ", sss.c_str());

// Status Bar dels paràmetres de Transformació, Color i posicions de Robot i Cama
	sss = " ";
	if (transf)
	{	if (rota)
		{	buffer = std::to_string(TG.VRota.x);
			sss = " " + buffer + " ";

			buffer = std::to_string(TG.VRota.y);
			sss = sss + buffer + " ";

			buffer = std::to_string(TG.VRota.z);
			sss = sss + buffer;
		}
		else if (trasl)
		{	buffer = std::to_string(TG.VTras.x);
			sss = " " + buffer + " ";

			buffer = std::to_string(TG.VTras.y);
			sss = sss + buffer + " ";

			buffer = std::to_string(TG.VTras.z);
			sss = sss + buffer;
		}
		else if (escal)
		{	buffer = std::to_string(TG.VScal.x);
			sss = " " + buffer + " ";

			buffer = std::to_string(TG.VScal.y);
			sss = sss + buffer + " ";

			buffer = std::to_string(TG.VScal.x);
			sss = sss + buffer;
		}
	}
	else if ((!sw_grid) && (!pan) && (camera != CAM_NAVEGA))
	{	if (!sw_color)
		{	// Color fons
			buffer = std::to_string(c_fons.r);
			sss = " " + buffer + " ";

			buffer = std::to_string(c_fons.g);
			sss = sss + buffer + " ";

			buffer = std::to_string(c_fons.b);
			sss = sss + buffer;
		}
		else
		{	// Color objecte
			buffer = std::to_string(col_obj.r);
			sss = " " + buffer + " ";

			buffer = std::to_string(col_obj.g);
			sss = sss + buffer + " ";

			buffer = std::to_string(col_obj.b);
			sss = sss + buffer;
		}
	}

// Refrescar posició PVz Status Bar
	Append(output, "%s \n", sss.c_str());

// Status Bar per indicar el pas del Fractal
	if (objecte == O_FRACTAL)
	{	buffer = std::to_string(pas);
		//sss = "Pas=" + buffer;
		Append(output, "Pas= %s \n", buffer.c_str());
	}
	else {	sss = "          ";
			Append(output, "%s \n", sss.c_str());
		}

		return output;
	}
}
