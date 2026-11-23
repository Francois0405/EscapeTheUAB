#include "AnimacionHiriti.h"
#include "TemporizadorAmbiente.h"

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
	comprobar(!hiriti.ActualizarAgarrame(4.9f),
		"el sonido para agarrarlo espera cinco segundos");
	comprobar(hiriti.ActualizarAgarrame(0.1f),
		"el sonido para agarrarlo se activa al cumplirse cinco segundos");
	comprobar(!hiriti.ActualizarAgarrame(1.0f),
		"el sonido para agarrarlo solo se activa una vez");

	hiriti.Reiniciar();
	comprobar(hiriti.Actualizar(true, origen, haciaHiriti, centro, 0.2f),
		"reiniciar permite descubrir de nuevo al cargar otra escena");
	comprobar(hiriti.MarcarAgarrado(),
		"agarrar a Hiriti despues de descubrirlo activa el evento de exito");
	comprobar(!hiriti.ActualizarAgarrame(5.0f),
		"agarrar a Hiriti cancela el sonido pendiente");
	comprobar(!hiriti.MarcarAgarrado(),
		"el evento de agarrarlo solo se activa una vez");
	hiriti.Reiniciar();
	comprobar(!hiriti.MarcarAgarrado(),
		"no se registra el agarre antes de descubrir a Hiriti");
	hiriti.MarcarLanzadoPorJugador();
	comprobar(hiriti.ActualizarContacto(false) == -1,
		"sin contacto no se reproduce sonido de daño");
	comprobar(hiriti.ActualizarContacto(true) == 0,
		"el primer impacto tras lanzar a Hiriti usa la primera variante");
	comprobar(hiriti.ActualizarContacto(true) == -1,
		"un contacto sostenido no repite el sonido cada fotograma");
	comprobar(hiriti.ActualizarContacto(false) == -1 &&
		hiriti.ActualizarContacto(true) == 1,
		"el siguiente impacto tras separarse alterna a la otra variante");

	juego::TemporizadorAmbiente ambiente;
	comprobar(!ambiente.Actualizar(true, 29.0f) && ambiente.Actualizar(true, 1.0f),
		"el sonido ambiente se activa al cumplirse treinta segundos de PLAY");
	comprobar(!ambiente.Actualizar(false, 30.0f) && !ambiente.Actualizar(true, 29.0f),
		"salir de PLAY reinicia el temporizador ambiente");

	comprobar(!hiriti.Actualizar(true, origen, haciaHiriti,
		glm::vec3(0.0f, juego::AnimacionHiriti::AlcanceDescubrimiento + 1.0f, 0.0f), 0.2f),
		"no se descubre fuera del alcance");

	return fallos == 0 ? 0 : 1;
}
