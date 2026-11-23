#include "AnimacionHiriti.h"

#include <iostream>

namespace
{
	int fallos = 0;

	void comprobar(bool condicion, const char* caso)
	{
		if (!condicion)
		{
			std::cerr << "FALLO: " << caso << '\n';
			++fallos;
		}
	}
}

int main()
{
	const glm::vec3 origen(0.0f);
	const glm::vec3 centro(0.0f, 5.0f, 0.0f);
	const glm::vec3 haciaHiriti(0.0f, 1.0f, 0.0f);
	juego::AnimacionHiriti hiriti;

	comprobar(!hiriti.Actualizar(false, origen, haciaHiriti, centro, 0.2f),
		"no se descubre mientras no se permite descubrir");
	comprobar(!hiriti.Actualizar(true, origen, glm::vec3(1.0f, 0.0f, 0.0f), centro, 0.2f),
		"mirar a otro lado no descubre a Hiriti");
	comprobar(!hiriti.Actualizar(true, origen, glm::vec3(0.0f), centro, 0.2f),
		"una direccion nula no descubre a Hiriti");
	comprobar(hiriti.Actualizar(true, origen, haciaHiriti, centro, 0.2f),
		"la primera mirada descubre a Hiriti");
	comprobar(!hiriti.Actualizar(true, origen, haciaHiriti, centro, 0.2f),
		"el evento de descubrimiento solo ocurre una vez");

	hiriti.Reiniciar();
	comprobar(hiriti.Actualizar(true, origen, haciaHiriti, centro, 0.2f),
		"reiniciar permite descubrir de nuevo al cargar otra escena");
	comprobar(!hiriti.Actualizar(true, origen, haciaHiriti,
		glm::vec3(0.0f, juego::AnimacionHiriti::AlcanceDescubrimiento + 1.0f, 0.0f), 0.2f),
		"no se descubre fuera del alcance");

	return fallos == 0 ? 0 : 1;
}
