#pragma once

// Render-control callbacks: lighting modes, materials, textures, light slots, shader
// selection and the binary shader cache. main.cpp owns every piece of state and lends it
// through this context; nothing here may include main.h (it defines globals).
// Member names match the main.cpp globals they are bound to.
#include <functional>

class Shader;
struct LLUM;

struct RuntimeRenderControls
{
	// Lighting and visibility
	bool& ifixe;
	bool& ilum2sides;
	char& ilumina;
	int& oIlumina;
	bool& test_vis;
	bool& oculta;

	// Materials and light slots
	bool* sw_material;				// 5 entries
	bool& sw_il;
	const bool& llum_ambient;
	LLUM* llumGL;					// NUM_MAX_LLUMS entries

	// Textures
	bool& textura;
	char& t_textura;
	bool& tFlag_invert_Y;
	unsigned int* texturesID;		// NUM_MAX_TEXTURES entries

	// Shader program
	char& shader;
	unsigned int& shader_programID;
	Shader& shaderLighting;
	bool& oclusoresPhongInvalidos;

	// Service that stays in main.cpp (the render path uses it too).
	std::function<void(unsigned int programa, bool ambienteActivo)> aplicarIntensidadMaterial;

	void OnIluminacioLlumfixe();
	void OnIluminacio2Sides();
	void OnIluminacioPunts();
	void OnIluminacioFilferros();
	void OnIluminacioPlana();
	void OnIluminacioSuau();
	void OnMaterialReflmaterial();
	void OnMaterialEmissio();
	void OnMaterialAmbient();
	void OnMaterialDifusa();
	void OnMaterialEspecular();
	void OnIluminacioTextures();
	void OnIluminacioTexturaFitxerimatge();
	void OnIluminacioTexturaFlagInvertY();
	void OnLlumsLlumAmbient();
	void OnLlumsLlum0();
	void OnLlumsLlum1();
	void OnLlumsLlum2();
	void OnLlumsLlum3();
	void OnLlumsLlum4();
	void OnLlumsLlum5();
	void OnLlumsLlum6();
	void OnLlumsLlum7();
	void OnShaderFlat();
	void OnShaderGouraud();
	void OnShaderPhong();
	void OnShaderLoadFiles();
	void OnShaderPBinaryWrite();
	void OnShaderPBinaryRead();
};
