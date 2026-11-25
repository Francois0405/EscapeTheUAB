#pragma once

#include "src/app/AppState.h"

gameplay::player::PlayerContext& contextoJugador();
void fijarCamaraAlSpawn(const SceneSpawn& spawn);
void aplicarPresetJuego();
void inclinarJugadorFPS(double deltaY);
bool hayTeclaDeMovimiento(GLFWwindow* window);
glm::vec3 posicionPiesDesdeCamara();
void origenYDireccionDeLaMirada(glm::vec3& origen, glm::vec3& direccion);
bool crearJugadorFisico();
void moverJugadorFPS(GLFWwindow* window, double velocidadJugador);
void sincronizarJugadorYCamara();
void actualizarMovimientoMovil(GLFWwindow* window, float deltaTime);
void actualizarBalanceoVisual(float delta);
glm::dvec3 desplazamientoVisualDeCamara();
