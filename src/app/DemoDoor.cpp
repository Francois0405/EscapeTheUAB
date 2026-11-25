#include "src/app/AppState.h"
#include "src/app/DemoDoor.h"

// Se llama una vez por fotograma: avanza el reloj y calcula el ángulo con los keyframes.
void updateDemoDoor(float deltaSegundos)
{
	demoDoorPlayback.Advance(deltaSegundos);
	demoDoorAngle = animation::AngleAtTime(demoDoorKeyframes, demoDoorPlayback.tiempo);
}

// Arranca la animación desde el principio (la puerta vuelve a estar cerrada).
void startDemoDoor()
{
	demoDoorPlayback.BeginPlayback(demoDoorKeyframes.back().tiempo);
}
