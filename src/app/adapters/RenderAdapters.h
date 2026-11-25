#pragma once

#include "src/app/AppState.h"

RuntimeRenderer& runtimeRenderer();
void OnSize(GLFWwindow* window, int width, int height);
void OnFramebufferSize(GLFWwindow* window, int width, int height);
bool drawVrScene();
void OnPaint(GLFWwindow* window);
void configura_Escena();
void dibuixa_Escena();
