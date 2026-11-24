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
	hiriti.MarcarLanzadoPorJugador(0.7f);
	comprobar(hiriti.ActualizarContacto(false) == -1,
		"sin contacto no se reproduce sonido de daño");
	comprobar(hiriti.ActualizarContacto(true) == 0,
		"una patada por encima del 50% reproduce el primer impacto");
	comprobar(hiriti.ActualizarContacto(true) == -1,
		"un contacto sostenido no repite el sonido cada fotograma");
	comprobar(hiriti.ActualizarContacto(false) == -1 &&
		hiriti.ActualizarContacto(true) == -1,
		"un solo lanzamiento no repite el sonido en impactos posteriores");
	hiriti.Reiniciar();
	hiriti.MarcarLanzadoPorJugador(0.3f);
	comprobar(hiriti.ActualizarContacto(true) == -1,
		"una patada con el 30% de potencia no reproduce audio de impacto");
	hiriti.Reiniciar();
	hiriti.MarcarLanzadoPorJugador(0.5f);
	comprobar(hiriti.ActualizarContacto(true) == -1,
		"una patada con exactamente el 50% de potencia no reproduce audio de impacto");

	hiriti.Reiniciar();
	hiriti.MarcarLanzadoPorJugador(0.7f);
	comprobar(hiriti.ActualizarContacto(true) == 0,
		"una patada con el 70% de potencia reproduce audio de impacto");

	juego::AnimacionHiriti seguimiento;
	comprobar(!seguimiento.ActualizarSuspensionSeguimiento(true, glm::vec3(0.0f), 0.1f),
		"el seguimiento se suspende mientras el jugador manipula a Hiriti");
	comprobar(!seguimiento.ActualizarSuspensionSeguimiento(false, glm::vec3(0.4f, 0.0f, 0.0f), 1.0f),
		"el seguimiento espera a que se calme el movimiento provocado por el jugador");
	comprobar(!seguimiento.ActualizarSuspensionSeguimiento(false, glm::vec3(0.0f), 0.5f),
		"el periodo de estabilizacion requiere mantenerse por debajo del umbral");
	comprobar(seguimiento.ActualizarSuspensionSeguimiento(false, glm::vec3(0.0f), 0.3f),
		"el seguimiento se reanuda tras estabilizarse Hiriti");

	const glm::vec3 haciaJugador(1.0f, 0.0f, 0.0f);
	const glm::vec3 sinAceleracion = juego::AnimacionHiriti::CalcularAceleracionSeguimiento(
		glm::vec3(0.0f), glm::vec3(1.7f, 0.0f, 0.0f), glm::vec3(0.0f),
		haciaJugador, 1.8f, 2.4f, 3.2f);
	comprobar(glm::length(sinAceleracion) < 1.0e-5f,
		"Hiriti no acelera al estar dentro de la distancia de parada");
	const glm::vec3 aceleracion = juego::AnimacionHiriti::CalcularAceleracionSeguimiento(
		glm::vec3(0.0f), glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(0.0f),
		haciaJugador, 1.8f, 2.4f, 3.2f);
	comprobar(aceleracion.x > 0.0f && std::fabs(aceleracion.y) < 1.0e-5f &&
		glm::length(aceleracion) <= 3.21f,
		"el seguimiento acelera hacia el jugador con aceleracion limitada");
	const glm::vec3 frenado = juego::AnimacionHiriti::CalcularAceleracionSeguimiento(
		glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
		haciaJugador, 1.8f, 2.4f, 3.2f);
	comprobar(frenado.x < 0.0f,
		"el seguimiento frena aunque Hiriti alcance la posicion del jugador");
	const std::array<float, 5> obstaculoFrontal = {
		0.2f, juego::AnimacionHiriti::AlcanceAnticipacionObstaculos,
		juego::AnimacionHiriti::AlcanceAnticipacionObstaculos,
		juego::AnimacionHiriti::AlcanceAnticipacionObstaculos,
		juego::AnimacionHiriti::AlcanceAnticipacionObstaculos
	};
	const glm::vec3 rodeo = juego::AnimacionHiriti::CalcularDireccionSeguimiento(
		glm::vec3(0.0f), glm::vec3(5.0f, 0.0f, 0.0f), obstaculoFrontal);
	comprobar(rodeo.x > 0.0f && std::fabs(rodeo.y) > 0.5f,
		"el seguimiento escoge una trayectoria lateral cuando el camino directo esta bloqueado");

	juego::AnimacionHiriti reaparicion;
	comprobar(!reaparicion.ActualizarTemporizadorReaparicion(true, false, 29.0f),
		"Hiriti no reaparece antes de que venza el cooldown de 30 segundos");
	comprobar(!reaparicion.ActualizarTemporizadorReaparicion(true, true, 1.0f),
		"la interaccion del jugador bloquea la reaparicion aunque venza el cooldown");
	comprobar(reaparicion.ActualizarTemporizadorReaparicion(true, false, 0.0f),
		"si ya esta lejos, reaparece en cuanto vence el cooldown sin esperar 30 segundos lejos");
	reaparicion.RegistrarReaparicion();
	comprobar(!reaparicion.ActualizarTemporizadorReaparicion(true, false, 29.0f) &&
		reaparicion.ActualizarTemporizadorReaparicion(true, false, 1.0f),
		"cada reaparicion inicia un nuevo cooldown de 30 segundos");
	reaparicion.Reiniciar();
	comprobar(!reaparicion.ActualizarTemporizadorReaparicion(false, false, 30.0f),
		"estar a 10 metros o menos impide reaparecer");
	reaparicion.Reiniciar();
	comprobar(!reaparicion.ActualizarTemporizadorReaparicion(true, false, 29.0f) &&
		reaparicion.ActualizarTemporizadorReaparicion(true, false, 1.0f),
		"reiniciar Hiriti limpia el cooldown antes de cambiar o reinicializar la escena");

	juego::TemporizadorAudioHiriti temporizadorAudio;
	comprobar(temporizadorAudio.Actualizar(true, 29.0f) == juego::TemporizadorAudioHiriti::Evento::Ninguno &&
		temporizadorAudio.Actualizar(true, 1.0f) == juego::TemporizadorAudioHiriti::Evento::Ambiente,
		"el sonido ambiente se activa al cumplirse treinta segundos de PLAY");
	comprobar(temporizadorAudio.Actualizar(true, 14.0f) == juego::TemporizadorAudioHiriti::Evento::Ninguno &&
		temporizadorAudio.Actualizar(true, 1.0f) == juego::TemporizadorAudioHiriti::Evento::Buscando,
		"el sonido searching alterna quince segundos despues del ambiente");
	comprobar(temporizadorAudio.Actualizar(true, 15.0f) == juego::TemporizadorAudioHiriti::Evento::Ambiente &&
		temporizadorAudio.Actualizar(true, 15.0f) == juego::TemporizadorAudioHiriti::Evento::Buscando,
		"cada categoria de sonido se repite con treinta segundos de diferencia");
	comprobar(temporizadorAudio.Actualizar(false, 30.0f) == juego::TemporizadorAudioHiriti::Evento::Ninguno &&
		temporizadorAudio.Actualizar(true, 29.0f) == juego::TemporizadorAudioHiriti::Evento::Ninguno &&
		temporizadorAudio.Actualizar(true, 1.0f) == juego::TemporizadorAudioHiriti::Evento::Ambiente,
		"salir de PLAY reinicia la secuencia empezando por ambiente");

	comprobar(!hiriti.Actualizar(true, origen, haciaHiriti,
		glm::vec3(0.0f, juego::AnimacionHiriti::AlcanceDescubrimiento + 1.0f, 0.0f), 0.2f),
		"no se descubre fuera del alcance");

	return fallos == 0 ? 0 : 1;
}
