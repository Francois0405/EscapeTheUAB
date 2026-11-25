#pragma once

#include "src/app/AppState.h"

const game::hiriti::HiritiContext& contextoHiriti();
void reproducirSonidoHiriti(const std::string& ruta, float volumenBase = 1.0f);
void actualizarHiriti(bool puedeDescubrir, bool puedeProcesarColisiones, float delta);
void actualizarAudioAmbiente(bool activo, float delta);
