#include "src/app/AppState.h"
#include "src/app/WindowCallbacks.h"
#include "src/app/adapters/InteractionAdapters.h"
#include "src/app/input/CursorCapture.h"

void OnWindowFocus(GLFWwindow* window, int focused)
{
	windowFocused = focused == GLFW_TRUE;
	if (!windowFocused)
	{
		cancelThrowChargeInput();
		setCursorCapture(window, false);
	}
	else
	{
		fpsHasPreviousPosition = false;
	}
}

void OnWindowCloseRequest(GLFWwindow* window)
{
	if (sceneEditor.HasUnsavedChanges())
	{
		glfwSetWindowShouldClose(window, GLFW_FALSE);
		sceneEditor.RequestCloseConfirmation();
	}
}

// Entorn VGI. OnFull_Screen: Funció per a pantalla completa
void OnFull_Screen(GLFWmonitor* monitor, GLFWwindow* window)
{
	static int windowX = 0;
	static int windowY = 0;
	static int windowWidth = 1280;
	static int windowHeight = 720;

	fullscreen = !fullscreen;

	if (fullscreen)
	{
		glfwGetWindowPos(window, &windowX, &windowY);
		glfwGetWindowSize(window, &windowWidth, &windowHeight);
		const GLFWvidmode* videoMode = glfwGetVideoMode(monitor);
		glfwSetWindowMonitor(window, monitor, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate);
	}
	else
	{
		glfwSetWindowMonitor(window, nullptr, windowX, windowY, windowWidth, windowHeight, GLFW_DONT_CARE);
	}
}

// -------------------- TRACTAMENT ERRORS
// error_callback: Displaia error que es pugui produir
void error_callback(int code, const char* description)
{
	//const char* descripcio;
	//int codi = glfwGetError(&descripcio);

 //   display_error_message(code, description);
	fprintf(stderr, "Codi Error: %i", code);
	fprintf(stderr, "Descripcio: %s\n", description);
}

void APIENTRY glDebugOutput(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
	const GLchar* message, const void* userParam)
{
	if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return; // ignore these non-significant error codes

	fprintf(stderr, "---------------\n");
	fprintf(stderr, "Debug message ( %3i %s \n", id, message);

	switch (source)
	{
	case GL_DEBUG_SOURCE_API:             fprintf(stderr, "Source: API \n"); break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   fprintf(stderr, "Source: Window System \n"); break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER: fprintf(stderr, "Source: Shader Compiler \n"); break;
	case GL_DEBUG_SOURCE_THIRD_PARTY:     fprintf(stderr, "Source: Third Party \n"); break;
	case GL_DEBUG_SOURCE_APPLICATION:     fprintf(stderr, "Source: Application \n"); break;
	case GL_DEBUG_SOURCE_OTHER:           fprintf(stderr, "Source: Other \n"); break;
	} //std::cout << std::endl;

	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR:               fprintf(stderr, "Type: Error\n"); break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: fprintf(stderr, "Type: Deprecated Behaviour\n"); break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  fprintf(stderr, "Type: Undefined Behaviour\n"); break;
	case GL_DEBUG_TYPE_PORTABILITY:         fprintf(stderr, "Type: Portability\n"); break;
	case GL_DEBUG_TYPE_PERFORMANCE:         fprintf(stderr, "Type: Performance\n"); break;
	case GL_DEBUG_TYPE_MARKER:              fprintf(stderr, "Type: Marker\n"); break;
	case GL_DEBUG_TYPE_PUSH_GROUP:          fprintf(stderr, "Type: Push Group\n"); break;
	case GL_DEBUG_TYPE_POP_GROUP:           fprintf(stderr, "Type: Pop Group\n"); break;
	case GL_DEBUG_TYPE_OTHER:               fprintf(stderr, "Type: Other\n"); break;
	} //std::cout << std::endl;

	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH:         fprintf(stderr, "Severity: high\n"); break;
	case GL_DEBUG_SEVERITY_MEDIUM:       fprintf(stderr, "Severity: medium\n"); break;
	case GL_DEBUG_SEVERITY_LOW:          fprintf(stderr, "Severity: low\n"); break;
	case GL_DEBUG_SEVERITY_NOTIFICATION: fprintf(stderr, "Severity: notification\n"); break;
	} //std::cout << std::endl;
	//std::cout << std::endl;
	fprintf(stderr, "\n");
}
