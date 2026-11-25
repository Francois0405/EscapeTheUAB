#pragma once

// Camera, view and projection menu callbacks. main.cpp owns every piece of state and lends it
// through this context; nothing here may include main.h (it defines globals).
// Member names match the main.cpp symbols they are bound to.
class Shader;
struct CEsfe3D;
struct CPunt3D;
struct CVAO;
struct LLUM;
struct SceneSpawn;

struct LegacyVgiViewControls
{
	// Camera mode and interactive point of view
	char& camera;
	CEsfe3D& OPV;
	CEsfe3D& OPV_G;
	char& Vis_Polar;
	bool& mobil;
	bool& zzoom;
	bool& zzoomO;
	bool& satelit;
	CEsfe3D& m_EsfeIncEAvall;
	const bool& anima;

	// Navigation mode
	CPunt3D& opvN;
	double (&n)[3];
	double& angleZ;
	const SceneSpawn& spawnEscenaActiva;
	bool (&crearJugadorFisico)();

	// View: full screen, pan, axes and mouse transforms
	const bool& fullscreen;
	bool& pan;
	double& fact_pan;
	CPunt3D& tr_cpv;
	CPunt3D& tr_cpvF;
	bool& transX;
	bool& transY;
	bool& transZ;
	bool& eixos;
	LLUM* const llumGL;				// NUM_MAX_LLUMS entries

	// Skybox resources
	Shader& shader_SkyBoxC;
	unsigned int& skC_programID;
	CVAO& skC_VAOID;
	unsigned int& cubemapTexture;

	// Projection
	char& projeccio;

	void OnCameraEsferica();
	void OnCameraOrigenEsferica();
	void OnVistaMobil();
	void OnVistaZoom();
	void OnVistaZoomOrto();
	void OnVistaSatelit();
	void OnVistaPolarsX();
	void OnVistaPolarsY();
	void OnVistaPolarsZ();
	void OnCameraNavega();
	void OnCameraOrigenNavega();
	void OnCameraGeode();
	void OnCameraOrigenGeode();
	void OnVistaFullscreen();
	void OnVistaPan();
	void OnVistaOrigenPan();
	void OnVistaEixos();
	void OnVistaSkyBox();
	void OnProjeccioPerspectiva();
	void OnProjeccioOrtografica();
	void OnProjeccioAxonometrica();
};
