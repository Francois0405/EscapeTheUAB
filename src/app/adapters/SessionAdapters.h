#pragma once

#include "src/app/AppState.h"

const netjuego::session::SessionContext& contextoSesionRed();
bool capturarEstadoEscenaRed(std::vector<netjuego::EstadoObjetoEscena>& salida);
void iniciarRed(const netjuego::ConfigRed& cfg);
void detenerRed();
void actualizarRed(double ahora, bool jugando);
