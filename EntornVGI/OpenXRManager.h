#pragma once
#include <GLFW/glfw3.h>

bool InicializarOpenXRInstance();
bool ObtenerSistemaOpenXR();
bool CrearSesionOpenXR(GLFWwindow* glfwWindow);