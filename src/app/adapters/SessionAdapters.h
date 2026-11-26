#pragma once

#include "src/app/AppIncludes.h"

const netgame::session::SessionContext& sessionBindings();
bool captureNetworkSceneState(std::vector<netgame::SceneObjectState>& salida);
void startNetwork(const netgame::NetworkConfig& cfg);
void stopNetwork();
void updateNetwork(double ahora, bool jugando);
