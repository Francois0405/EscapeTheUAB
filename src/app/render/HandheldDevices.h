#pragma once

#include "src/app/AppIncludes.h"

glm::mat4 currentPhoneLocalMatrix();
void updatePhoneFlashlight(const glm::mat4& matrizMovilMundo);
bool leftControllerWorldMatrix(glm::mat4& matrizMundo);
void drawVrControllerRay();
