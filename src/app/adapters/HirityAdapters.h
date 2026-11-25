#pragma once

#include "src/app/AppIncludes.h"

const game::hiriti::HiritiContext& hiritiBindings();
void playHiritiSoundCue(const std::string& ruta, float volumenBase = 1.0f);
void updateHiritiBehaviour(bool puedeDescubrir, bool puedeProcesarColisiones, float delta);
void updateAmbientSound(bool activo, float delta);
