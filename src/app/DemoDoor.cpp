#include "src/app/AppIncludes.h"
#include "src/app/state/SceneState.h"
#include "src/app/DemoDoor.h"

// State used only by this file.
namespace
{
	std::vector<animation::AngleKeyframe> demoDoorKeyframes = { {0.0f, 0.0f}, {2.0f, 90.0f} };
}

// Se llama una vez por fotograma: avanza el reloj y calcula el ángulo con los keyframes.
void updateDemoDoor(float deltaSegundos)
{
	demoDoorPlayback.Update(deltaSegundos);
	demoDoorAngle = animation::AngleAtTime(demoDoorKeyframes, demoDoorPlayback.tiempo);
}

// Arranca la animación desde el principio (la puerta vuelve a estar cerrada).
void startDemoDoor()
{
	demoDoorPlayback.Start(demoDoorKeyframes.back().tiempo);
}
