#include "src/app/AppIncludes.h"
#include "src/legacy_vgi/VgiTimer.h"

/* ------------------------------------------------------------------------- */
void OnTimer()
{
// TODO: Agregue aquí su código de controlador de mensajes o llame al valor predeterminado
	if (anima) {
		// Codi de tractament de l'animació quan transcorren els ms. del crono.

		// Crida a OnPaint() per redibuixar l'escena
		//InvalidateRect(NULL, false);
	}
	else if (satelit) {	// OPCIÓ SATÈLIT: Increment OPV segons moviments mouse.
		if (camera == CAM_ESFERICA) 
		{	//OPV.R = OPV.R + m_EsfeIncEAvall.R;
			OPV.alfa = OPV.alfa + m_EsfeIncEAvall.alfa;
			while (OPV.alfa > 360.0) OPV.alfa = OPV.alfa - 360.0;
			while (OPV.alfa < 0.0) OPV.alfa = OPV.alfa + 360.0;
			OPV.beta = OPV.beta + m_EsfeIncEAvall.beta;
			while (OPV.beta > 360.0) OPV.beta = OPV.beta - 360.0;
			while (OPV.beta < 0.0) OPV.beta = OPV.beta + 360.0;
		}	
		else if (camera == CAM_GEODE) 
		{	//OPV.R = OPV.R + m_EsfeIncEAvall.R;
			OPV_G.alfa = OPV_G.alfa + m_EsfeIncEAvall.alfa * 0.5;
			while (OPV_G.alfa > 360.0) OPV_G.alfa = OPV_G.alfa - 360.0;
			while (OPV_G.alfa < 0) OPV_G.alfa = OPV_G.alfa + 360.0;
			OPV_G.beta = OPV_G.beta + m_EsfeIncEAvall.beta * 0.5;
			while (OPV_G.beta > 360.0) OPV_G.beta = OPV.beta - 360.0;
			while (OPV_G.beta < 0.0) OPV_G.beta = OPV_G.beta + 360.0;
			}

		// Crida a OnPaint() per redibuixar l'escena
		//OnPaint();
		}
// EntornVGI. QUATERNIONS: Animació Objecte Rígid per Quaternions
	else if (animaQ)
	{	// CAL FER: Animació sense fi a partir qI i qF amb interpolació lineal o esfèrica.

		// Crida a OnPaint() per redibuixar l'escena
		//InvalidateRect(NULL, false);
	}
// EntornVGI. ROBOT: Animació Robot Articulat
	else if (objecte == ROBOT && animaR)
	{	// CAL FER: Animació sense fi a partir de dues o tres posicios clau del robot.posicions

		// Crida a OnPaint() per redibuixar l'escena
		//InvalidateRect(NULL, false);
	}
}

// ---------------- Entorn VGI: Funcions locals a main.cpp

// Log2: Càlcul del log base 2 de num
int Log2(int num)
{
	int tlog;

	if (num >= 8192) tlog = 13;
	else if (num >= 4096) tlog = 12;
	else if (num >= 2048) tlog = 11;
	else if (num >= 1024) tlog = 10;
	else if (num >= 512) tlog = 9;
	else if (num >= 256) tlog = 8;
	else if (num >= 128) tlog = 7;
	else if (num >= 64) tlog = 6;
	else if (num >= 32) tlog = 5;
	else if (num >= 16) tlog = 4;
	else if (num >= 8) tlog = 3;
	else if (num >= 4) tlog = 2;
	else if (num >= 2) tlog = 1;
	else tlog = 0;

	return tlog;
}
