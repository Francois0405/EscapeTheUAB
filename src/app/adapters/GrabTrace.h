#pragma once

#include "src/app/AppIncludes.h"

bool grabTraceEnabled();
net::GrabDiagnosticRole grabTraceRole();
void persistGrabTrace(const std::string& registro);
void emitGrabTrace(const std::string& registro);
void traceGrab(const char* evento, uint32_t id, net::PeerId peer, net::PeerId propietario, const glm::vec3& position, const glm::vec3& punto, const char* detalle);
