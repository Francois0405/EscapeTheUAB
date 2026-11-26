#pragma once

// One-time initialization of the legacy VGI state and GL resources (the former body of InitGL).
// src/app owns every piece of state and lends it through this context; nothing here may include
// the application state headers. Member names match the src/app symbols they are bound to.
// Requires a current OpenGL context with GLEW already initialized.
#include "framework.h"

#include <string>

class COBJModel;
class Shader;
struct ImVec4;

struct LegacyVgiGraphicsBootstrap
{
	// Status bar and camera
	bool& statusB;
	char& camera;
	int& oCamera;
	bool& mobil;
	bool& zzoom;
	bool& zzoomO;
	bool& satelit;
	float (&cam_Esferica)[3];
	CEsfe3D& OPV;
	char& Vis_Polar;
	glm::mat4& ViewMatrix;
	int& oPolars;
	GLdouble (&n)[3];
	CPunt3D& opvN;
	double& angleZ;
	CEsfe3D& OPV_G;

	// View: full screen, pan, axes and grids
	bool& fullscreen;
	bool& pan;
	bool& eixos;
	GLuint& eixos_programID;
	GLuint& eixos_Id;
	bool& sw_grid;
	CMask3D& grid;
	CPunt3D& hgrid;
	double& fact_pan;
	CPunt3D& tr_cpv;
	CPunt3D& tr_cpvF;

	// Projection, object and skybox
	char& projeccio;
	int& oProjeccio;
	glm::mat4& ProjectionMatrix;
	char& objecte;
	int& oObjecte;
	bool& SkyBoxCube;
	GLuint& skC_programID;
	CVAO& skC_VAOID;
	GLuint& cubemapTexture;

	// Transforms and visibility
	bool& transf;
	bool& trasl;
	bool& rota;
	bool& escal;
	double& fact_Tras;
	double& fact_Rota;
	INSTANCIA& TG;
	INSTANCIA& TGF;
	bool& transX;
	bool& transY;
	bool& transZ;
	glm::mat4& GTMatrix;
	bool& front_faces;
	bool& test_vis;
	bool& oculta;
	bool& back_line;

	// Illumination, materials, textures and lights
	bool& ifixe;
	bool& ilum2sides;
	char& ilumina;
	int& oIlumina;
	bool (&sw_material)[5];
	bool (&sw_material_old)[5];
	bool& textura;
	char& t_textura;
	bool& textura_map;
	GLuint (&texturesID)[NUM_MAX_TEXTURES];
	bool& tFlag_invert_Y;
	bool& llum_ambient;
	LLUM (&llumGL)[NUM_MAX_LLUMS];
	bool& sw_llambient;
	CColor& col_llambient;

	// Shaders
	char& shader;
	int& oShader;
	GLuint& shader_programID;
	Shader& shaderLighting;
	Shader& shaderEixos;
	Shader& shader_SkyBoxC;

	// Mouse, window sizes and colors
	CPoint& m_PosEAvall;
	CPoint& m_PosDAvall;
	bool& m_ButoEAvall;
	bool& m_ButoDAvall;
	CEsfe3D& m_EsfeEAvall;
	CEsfe3D& m_EsfeIncEAvall;
	int& w;
	int& h;
	int& width_old;
	int& height_old;
	int& w_old;
	int& h_old;
	bool& fonsR;
	bool& fonsG;
	bool& fonsB;
	CColor& c_fons;
	ImVec4& clear_colorB;
	bool& sw_color;
	ImVec4& clear_colorO;
	CColor& col_obj;

	// OBJ object, curves and Frenet frame
	COBJModel*& ObOBJ;
	CVAO& vao_OBJ;
	int& npts_T;
	CPunt3D (&PC_t)[MAX_PATCH_CORBA];
	GLdouble& pas_Corba;
	GLdouble& pas_CS;
	bool& sw_Punts_Control;
	bool& dibuixa_TriedreFrenet;
	bool& dibuixa_TriedreDarboux;
	CPunt3D& VT;
	CPunt3D& VNP;
	CPunt3D& VBN;

	// Timer, animation, fractal, robot and misc
	double& t;
	bool& anima;
	bool& animaQ;
	bool& rotaQ;
	char& t_fractal;
	char& soroll;
	int& pas;
	int& pas_ini;
	bool& sw_il;
	bool& palcolFractal;
	bool& animaR;
	bool& sw_robot;
	bool& sh_rmov;
	GLdouble& mida;
	std::string& nom;
	std::string& buffer;

	// Runs the whole initialization in its original order.
	void Run();

private:
	void InitViewAndRenderState();
	void InitLights();
	void LoadShadersAndSkybox();
	void InitInteractionAndObjectState();
};
