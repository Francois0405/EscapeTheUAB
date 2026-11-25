#pragma once

class COBJModel;
struct CColor;
struct CPunt3D;
struct INSTANCIA;

// Borrows the VGI menu state owned by main.cpp.
struct LegacyVgiGeometryControls
{
	// Object selection and resources
	char& objecte;
	const CColor& col_obj;
	unsigned int* const texturesID;	// NUM_MAX_TEXTURES entries
	COBJModel* const& ObOBJ;

	// Curve data and the shared .crv reader.
	bool (&sw_material)[5];
	int& npts_T;
	CPunt3D* const PC_t;				// MAX_PATCH_CORBA entries
	const double& pas_CS;
	bool& sw_Punts_Control;
	bool& dibuixa_TriedreFrenet;
	bool& dibuixa_TriedreDarboux;
	int (&llegir_ptsC)(const char*);

	// Interactive transforms and their mouse controls
	bool& transf;
	bool& trasl;
	bool& rota;
	bool& escal;
	double& fact_Tras;
	double& fact_Rota;
	INSTANCIA& TG;
	const bool& transX;
	const bool& transY;
	const bool& transZ;
	bool& mobil;
	bool& zzoom;
	bool& pan;

	// Visibility switches shared with rendering controls
	bool& front_faces;
	bool& test_vis;
	bool& oculta;

	void OnObjecteCap();
	void OnObjecteCub();
	void OnObjecteCubRGB();
	void OnObjecteEsfera();
	void OnObjecteTetera();
	void OnObjecteArc();
	void OnObjecteTie();
	void OnObjecteCorbaBezier();
	void OnObjecteCorbaLemniscata();
	void OnObjecteCorbaHermitte();
	void OnObjecteCorbaCatmullRom();
	void OnObjecteCorbaBSpline();
	void OnObjectePuntsControl();
	void OnCorbesTriedreFrenet();
	void OnCorbesTriedreDarboux();
	void OnObjecteMatriuPrimitives();
	void OnObjecteMatriuPrimitivesVAO();

	void OnTransformaTraslacio();
	void OnTransformaOrigenTraslacio();
	void OnTransformaRotacio();
	void OnTransformaOrigenRotacio();
	void OnTransformaEscalat();
	void OnTransformaOrigenEscalat();
	void OnTransformaMobilX();
	void OnTransformaMobilY();
	void OnTransformaMobilZ();

	void OnOcultacionsFrontFaces();
	void OnOcultacionsTestvis();
	void OnOcultacionsZBuffer();
};
