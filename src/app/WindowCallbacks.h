#pragma once

#include "src/app/AppIncludes.h"

void OnWindowFocus(GLFWwindow* window, int focused);
void OnWindowCloseRequest(GLFWwindow* window);
void OnFull_Screen(GLFWmonitor* monitor, GLFWwindow* window);
void error_callback(int code, const char* description);
void APIENTRY glDebugOutput(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
