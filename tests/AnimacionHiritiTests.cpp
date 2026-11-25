#include "src/gameplay/hirity/HiritiAnimation.h"
#include "src/gameplay/hirity/HiritiSoundVolume.h"
#include "src/gameplay/hirity/HiritiAudioTimer.h"

#include <cmath>
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
	game::HiritiAnimator hiriti;

	comprobar(!hiriti.Advance(false, origen, haciaHiriti, centro, 0.2f),
		"no se descubre mientras no se permite descubrir");
	comprobar(!hiriti.Advance(true, origen, glm::vec3(1.0f, 0.0f, 0.0f), centro, 0.2f),
		"mirar a otro lado no descubre a Hiriti");
	comprobar(!hiriti.Advance(true, origen, glm::vec3(0.0f), centro, 0.2f),
		"una direccion nula no descubre a Hiriti");
	comprobar(hiriti.Advance(true, origen, haciaHiriti, centro, 0.2f),
		"la primera mirada descubre a Hiriti");
	comprobar(!hiriti.Advance(true, origen, haciaHiriti, centro, 0.2f),
		"el evento de descubrimiento solo ocurre una vez");
	comprobar(!hiriti.UpdateGrabMe(4.9f),
		"el sonido para agarrarlo espera cinco segundos");
	comprobar(hiriti.UpdateGrabMe(0.1f),
		"el sonido para agarrarlo se activa al cumplirse cinco segundos");
	comprobar(!hiriti.UpdateGrabMe(1.0f),
		"el sonido para agarrarlo solo se activa una vez");

	hiriti.ResetState();
	comprobar(hiriti.Advance(true, origen, haciaHiriti, centro, 0.2f),
		"reiniciar permite descubrir de nuevo al cargar otra escena");
	comprobar(hiriti.MarkGrabbed(),
		"agarrar a Hiriti despues de descubrirlo activa el evento de exito");
	comprobar(!hiriti.UpdateGrabMe(5.0f),
		"agarrar a Hiriti cancela el sonido pendiente");
	comprobar(!hiriti.MarkGrabbed(),
		"el evento de agarrarlo solo se activa una vez");
	game::HiritiAnimator agarreValidado;
	comprobar(agarreValidado.DiscoverByInteraction() && agarreValidado.MarkGrabbed(),
		"un agarre físico validado descubre a Hiriti aunque otra réplica no detectara su mirada");
	comprobar(!agarreValidado.MarkGrabbed(),
		"el estado remoto de Hiriti no duplica el evento de agarre");
	hiriti.ResetState();
	comprobar(!hiriti.MarkGrabbed(),
		"no se registra el agarre antes de descubrir a Hiriti");
	hiriti.MarkThrownByPlayer(0.7f);
	comprobar(hiriti.UpdateContact(false) == -1,
		"sin contacto no se reproduce sonido de daño");
	comprobar(hiriti.UpdateContact(true) == 0,
		"una patada por encima del 50% reproduce el primer impacto");
	comprobar(hiriti.UpdateContact(true) == -1,
		"un contacto sostenido no repite el sonido cada fotograma");
	comprobar(hiriti.UpdateContact(false) == -1 &&
		hiriti.UpdateContact(true) == -1,
		"un solo lanzamiento no repite el sonido en impactos posteriores");
	hiriti.ResetState();
	hiriti.MarkThrownByPlayer(0.3f);
	comprobar(hiriti.UpdateContact(true) == -1,
		"una patada con el 30% de potencia no reproduce audio de impacto");
	hiriti.ResetState();
	hiriti.MarkThrownByPlayer(0.5f);
	comprobar(hiriti.UpdateContact(true) == -1,
		"una patada con exactamente el 50% de potencia no reproduce audio de impacto");

	hiriti.ResetState();
	hiriti.MarkThrownByPlayer(0.7f);
	comprobar(hiriti.UpdateContact(true) == 0,
		"una patada con el 70% de potencia reproduce audio de impacto");

	game::HiritiAnimator seguimiento;
	comprobar(!seguimiento.UpdateFollowSuspension(true, glm::vec3(0.0f), 0.1f),
		"el seguimiento se suspende mientras el jugador manipula a Hiriti");
	comprobar(!seguimiento.UpdateFollowSuspension(false, glm::vec3(0.4f, 0.0f, 0.0f), 1.0f),
		"el seguimiento espera a que se calme el movimiento provocado por el jugador");
	comprobar(!seguimiento.UpdateFollowSuspension(false, glm::vec3(0.0f), 0.5f),
		"el periodo de estabilizacion requiere mantenerse por debajo del umbral");
	comprobar(seguimiento.UpdateFollowSuspension(false, glm::vec3(0.0f), 0.3f),
		"el seguimiento se reanuda tras estabilizarse Hiriti");

	const glm::vec3 haciaJugador(1.0f, 0.0f, 0.0f);
	const glm::vec3 sinAceleracion = game::HiritiAnimator::ComputeFollowAcceleration(
		glm::vec3(0.0f), glm::vec3(1.7f, 0.0f, 0.0f), glm::vec3(0.0f),
		haciaJugador, 1.8f, 2.4f, 3.2f);
	comprobar(glm::length(sinAceleracion) < 1.0e-5f,
		"Hiriti no acelera al estar dentro de la distancia de parada");
	const glm::vec3 aceleracion = game::HiritiAnimator::ComputeFollowAcceleration(
		glm::vec3(0.0f), glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(0.0f),
		haciaJugador, 1.8f, 2.4f, 3.2f);
	comprobar(aceleracion.x > 0.0f && std::fabs(aceleracion.y) < 1.0e-5f &&
		glm::length(aceleracion) <= 3.21f,
		"el seguimiento acelera hacia el jugador con aceleracion limitada");
	const glm::vec3 frenado = game::HiritiAnimator::ComputeFollowAcceleration(
		glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
		haciaJugador, 1.8f, 2.4f, 3.2f);
	comprobar(frenado.x < 0.0f,
		"el seguimiento frena aunque Hiriti alcance la posicion del jugador");
	const std::array<float, 5> obstaculoFrontal = {
		0.2f, game::HiritiAnimator::AlcanceAnticipacionObstaculos,
		game::HiritiAnimator::AlcanceAnticipacionObstaculos,
		game::HiritiAnimator::AlcanceAnticipacionObstaculos,
		game::HiritiAnimator::AlcanceAnticipacionObstaculos
	};
	const glm::vec3 rodeo = game::HiritiAnimator::ComputeFollowDirection(
		glm::vec3(0.0f), glm::vec3(5.0f, 0.0f, 0.0f), obstaculoFrontal);
	comprobar(rodeo.x > 0.0f && std::fabs(rodeo.y) > 0.5f,
		"el seguimiento escoge una trayectoria lateral cuando el camino directo esta bloqueado");

	game::HiritiAnimator reaparicion;
	comprobar(!reaparicion.UpdateRespawnTimer(true, false, 29.0f),
		"Hiriti no reaparece antes de que venza el cooldown de 30 segundos");
	comprobar(!reaparicion.UpdateRespawnTimer(true, true, 1.0f),
		"la interaccion del jugador bloquea la reaparicion aunque venza el cooldown");
	comprobar(reaparicion.UpdateRespawnTimer(true, false, 0.0f),
		"si ya esta lejos, reaparece en cuanto vence el cooldown sin esperar 30 segundos lejos");
	reaparicion.RegisterRespawn();
	comprobar(!reaparicion.UpdateRespawnTimer(true, false, 29.0f) &&
		reaparicion.UpdateRespawnTimer(true, false, 1.0f),
		"cada reaparicion inicia un nuevo cooldown de 30 segundos");
	reaparicion.ResetState();
	comprobar(!reaparicion.UpdateRespawnTimer(false, false, 30.0f),
		"estar a 10 metros o menos impide reaparecer");
	reaparicion.ResetState();
	comprobar(!reaparicion.UpdateRespawnTimer(true, false, 29.0f) &&
		reaparicion.UpdateRespawnTimer(true, false, 1.0f),
		"reiniciar Hiriti limpia el cooldown antes de cambiar o reinicializar la escena");

	game::HiritiAudioTimer temporizadorAudio;
	comprobar(temporizadorAudio.Advance(true, 29.0f) == game::HiritiAudioTimer::Evento::Ninguno &&
		temporizadorAudio.Advance(true, 1.0f) == game::HiritiAudioTimer::Evento::Ambiente,
		"el sonido ambiente se activa al cumplirse treinta segundos de PLAY");
	comprobar(temporizadorAudio.Advance(true, 14.0f) == game::HiritiAudioTimer::Evento::Ninguno &&
		temporizadorAudio.Advance(true, 1.0f) == game::HiritiAudioTimer::Evento::Buscando,
		"el sonido searching alterna quince segundos despues del ambiente");
	comprobar(temporizadorAudio.Advance(true, 15.0f) == game::HiritiAudioTimer::Evento::Ambiente &&
		temporizadorAudio.Advance(true, 15.0f) == game::HiritiAudioTimer::Evento::Buscando,
		"cada categoria de sonido se repite con treinta segundos de diferencia");
	comprobar(temporizadorAudio.Advance(false, 30.0f) == game::HiritiAudioTimer::Evento::Ninguno &&
		temporizadorAudio.Advance(true, 29.0f) == game::HiritiAudioTimer::Evento::Ninguno &&
		temporizadorAudio.Advance(true, 1.0f) == game::HiritiAudioTimer::Evento::Ambiente,
		"salir de PLAY reinicia la secuencia empezando por ambiente");

	comprobar(!hiriti.Advance(true, origen, haciaHiriti,
		glm::vec3(0.0f, game::HiritiAnimator::AlcanceDescubrimiento + 1.0f, 0.0f), 0.2f),
		"no se descubre fuera del alcance");

	const auto volumenCercano = [](float volumenBase, float distancia, float esperado)
	{
		return std::fabs(game::hiriti::AttenuateVolumeByDistance(volumenBase, distancia) - esperado) < 1.0e-5f;
	};
	comprobar(volumenCercano(1.0f, 0.0f, 1.0f) && volumenCercano(1.0f, 2.0f, 1.0f),
		"la voz de Hiriti suena a volumen completo hasta dos metros");
	comprobar(volumenCercano(1.0f, 11.0f, 0.575f),
		"la voz de Hiriti se atenua linealmente entre dos y veinte metros");
	comprobar(volumenCercano(1.0f, 20.0f, 0.15f) && volumenCercano(1.0f, 100.0f, 0.15f),
		"la voz de Hiriti conserva un volumen minimo desde veinte metros");
	comprobar(volumenCercano(1.6f, 11.0f, 0.92f),
		"la atenuacion es proporcional al volumen base de cada sonido");

	return fallos == 0 ? 0 : 1;
}
