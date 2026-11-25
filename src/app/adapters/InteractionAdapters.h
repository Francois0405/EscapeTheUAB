#pragma once

#include "src/app/AppState.h"

const game::interaction::InteractionContext& contextoInteraccion();
bool jugadorTieneAgarre();
InteractionTarget buscarObjetivoInteraccion();
void actualizarObjetivoInteraccion(bool permitido);
void iniciarCargaLanzamiento();
void interactuar();
void actualizarAgarreDelJugador(bool puedeJugar);
void cancelarCargaLanzamiento();
void lanzarObjetoCargado();
void actualizarCargaLanzamiento(GLFWwindow* window, float delta);
