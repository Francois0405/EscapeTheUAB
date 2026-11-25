#pragma once

#include "src/app/AppState.h"

bool diagnosticoAgarreActivo();
net::GrabDiagnosticRole rolDiagnosticoAgarre();
void persistirDiagnosticoAgarre(const std::string& registro);
void emitirDiagnosticoAgarre(const std::string& registro);
void trazarAgarre(const char* evento, uint32_t id, net::PeerId peer, net::PeerId propietario, const glm::vec3& posicion, const glm::vec3& punto, const char* detalle);
