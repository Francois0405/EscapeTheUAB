#pragma once

// Viewport callbacks and the per-frame draw path: desktop projections, the editor view, the F2
// stereo test view and the OpenXR eye loop. main.cpp owns every piece of state and lends it through
// this context; nothing here may include main.h (it defines globals).
// Member names match the main.cpp globals and functions they are bound to.
// w and h are references on purpose: the main loop sets them to the PS1 FBO size around OnPaint.
#include "framework.h"
#include <functional>

class Shader;
namespace editor { class SceneEditor; }
namespace vr { class OpenXrManager; struct VrEye; struct XrPose; }

struct RuntimeRenderer
{
	// Window and framebuffer sizes
	int& w;
	int& h;
	int& width_old;
	int& height_old;

	// Projection and camera
	const char& projeccio;
	const char& camera;
	const char& Vis_Polar;
	const CEsfe3D& OPV;
	const CEsfe3D& OPV_G;
	const CPunt3D& opvN;
	GLdouble (&n)[3];				// OnPaint resets it for the spherical camera.
	const bool& pan;
	const CPunt3D& tr_cpv;
	const CPunt3D& tr_cpvF;
	glm::mat4& ViewMatrix;
	glm::mat4& ProjectionMatrix;

	// Scene appearance, visibility and lighting
	const CColor& c_fons;
	const CColor& col_obj;
	const char& objecte;
	const GLdouble& mida;
	const int& pas;
	const bool& front_faces;
	const bool& oculta;
	const bool& test_vis;
	const bool& back_line;
	const char& ilumina;
	const bool& llum_ambient;
	LLUM* llumGL;					// NUM_MAX_LLUMS entries
	const bool& ifixe;
	const bool& ilum2sides;
	const bool& eixos;
	const CMask3D& grid;
	const CPunt3D& hgrid;
	const bool& statusB;
	const bool& modoEstereoPrueba;

	// Shader programs and axes VAO
	GLuint& shader_programID;
	GLuint& eixos_programID;
	GLuint& eixos_Id;
	Shader& shaderEixos;
	Shader& shaderLighting;

	// Editor and VR
	const editor::SceneEditor& editorEscenas;
	vr::OpenXrManager& gestorOpenXR;
	vr::XrPose& ultimaCabezaVR;		// head pose of the last VR frame, read by networking
	bool& hayCabezaVR;

	// Services that stay in main.cpp
	std::function<void()> configura_Escena;
	std::function<void()> dibuixa_Escena;
	std::function<void()> Barra_Estat;
	std::function<glm::dvec3()> desplazamientoVisualDeCamara;
	std::function<glm::vec3()> piesJugador;
	std::function<float()> alturaOjosJugador;

	void OnSize(GLFWwindow* window, int width, int height);
	void OnFramebufferSize(GLFWwindow* window, int width, int height);
	void dibujarEscenaEstereo(int anchoVentana, int altoVentana);
	bool dibujarEscenaVR();
	void OnPaint(GLFWwindow* window);

private:
	void copiarOjoAVentana(const vr::VrEye& ojo);
};
