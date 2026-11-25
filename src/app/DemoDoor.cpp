#include "src/app/AppState.h"
#include "src/app/DemoDoor.h"

// Se llama una vez por fotograma: avanza el reloj y calcula el ángulo con los keyframes.
void actualizarPuertaDemo(float deltaSegundos)
{
	reproductorPuertaDemo.Actualizar(deltaSegundos);
	anguloPuertaDemo = animacion::AnguloEnTiempo(keyframesPuertaDemo, reproductorPuertaDemo.tiempo);
}

// Arranca la animación desde el principio (la puerta vuelve a estar cerrada).
void iniciarPuertaDemo()
{
	reproductorPuertaDemo.Iniciar(keyframesPuertaDemo.back().tiempo);
}
