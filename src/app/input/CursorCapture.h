#pragma once

#include "src/app/AppState.h"

bool fpsModeActive();
void setCursorCapture(GLFWwindow* window, bool capturar);
void syncFpsCursor(GLFWwindow* window);
bool fpsInputActive();
