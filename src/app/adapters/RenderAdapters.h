#pragma once

#include "src/app/AppState.h"

RuntimeRenderer& renderizadorRuntime();
void OnSize(GLFWwindow* window, int width, int height);
void OnFramebufferSize(GLFWwindow* window, int width, int height);
bool dibujarEscenaVR();
void OnPaint(GLFWwindow* window);
void configura_Escena();
void dibuixa_Escena();
