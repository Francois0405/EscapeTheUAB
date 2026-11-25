#include "src/app/AppState.h"
#include "src/app/DemoDoor.h"

// Se llama una vez por fotograma: avanza el reloj y calcula el ángulo con los keyframes.
void updateDemoDoor(float deltaSegundos)
{
	demoDoorPlayback.Actualizar(deltaSegundos);
	demoDoorAngle = animation::AnguloEnTiempo(demoDoorKeyframes, demoDoorPlayback.tiempo);
}

// Arranca la animación desde el principio (la puerta vuelve a estar cerrada).
void startDemoDoor()
{
	demoDoorPlayback.Iniciar(demoDoorKeyframes.back().tiempo);
}
