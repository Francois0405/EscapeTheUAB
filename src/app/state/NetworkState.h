#pragma once

// Network state shared by the application glue. Defined in src/app/AppState.cpp.
#include "src/app/AppIncludes.h" // types: each slice compiles on its own
#include "src/networking/replication/NetGame.h"
#include "src/networking/replication/NetAdapters.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

extern netgame::NetworkGame networkGame;
extern bool showNetworkPanel;
extern std::unique_ptr<netgame::GlbDoors> networkDoors;
extern netgame::NetworkConfig pendingNetworkConfig;
