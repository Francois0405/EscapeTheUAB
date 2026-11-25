#pragma once

#include "src/app/AppState.h"

glm::mat4 matrizLocalMovilActual();
void actualizarLinternaMovil(const glm::mat4& matrizMovilMundo);
bool matrizMandoIzquierdoMundo(glm::mat4& matrizMundo);
void dibujarRayoMandoVR();
