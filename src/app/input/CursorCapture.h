#pragma once

#include "src/app/AppIncludes.h"

bool fpsModeActive();
void setCursorCapture(GLFWwindow* window, bool capturar);
void syncFpsCursor(GLFWwindow* window);
bool fpsInputActive();
