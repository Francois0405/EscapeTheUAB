#pragma once

// Viewport callbacks and the per-frame draw path: desktop projections, the editor view, the F2
// stereo test view and the OpenXR eye loop. src/app owns every piece of state and lends it through
// this context; nothing here may include the application state headers.
// Member names match the src/app globals and functions they are bound to.
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
	const bool& stereoTestMode;

	// Shader programs and axes VAO
	GLuint& shader_programID;
	GLuint& eixos_programID;
	GLuint& eixos_Id;
	Shader& shaderEixos;
	Shader& shaderLighting;

	// Editor and VR
	const editor::SceneEditor& sceneEditor;
	vr::OpenXrManager& openXrManager;
	vr::XrPose& lastVrHead;		// head pose of the last VR frame, read by networking
	bool& hasVrHead;

	// Services that stay in src/app
	std::function<void()> configura_Escena;
	std::function<void()> dibuixa_Escena;
	std::function<void()> Barra_Estat;
	std::function<glm::dvec3()> cameraVisualOffset;
	std::function<glm::dvec3()> cameraSanityLookOffset;   // giro de camara por baja cordura (solo vista de escritorio)
	std::function<glm::vec3()> piesJugador;
	std::function<float()> alturaOjosJugador;

	void OnSize(GLFWwindow* window, int width, int height);
	void OnFramebufferSize(GLFWwindow* window, int width, int height);
	void drawStereoScene(int anchoVentana, int altoVentana);
	bool drawVrScene();
	void OnPaint(GLFWwindow* window);

private:
	void copyEyeToWindow(const vr::VrEye& ojo);
};
