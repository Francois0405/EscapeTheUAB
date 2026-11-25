#pragma once

#include "src/app/AppState.h"

const netgame::session::SessionContext& contextoSesionRed();
bool capturarEstadoEscenaRed(std::vector<netgame::SceneObjectState>& salida);
void iniciarRed(const netgame::NetworkConfig& cfg);
void detenerRed();
void actualizarRed(double ahora, bool jugando);
