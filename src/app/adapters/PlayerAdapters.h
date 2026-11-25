#pragma once

#include "src/app/AppState.h"

gameplay::player::PlayerContext& playerBindings();
void placeCameraAtSpawn(const SceneSpawn& spawn);
void applyStartupPreset();
void pitchFpsPlayer(double deltaY);
bool movementKeyPressed(GLFWwindow* window);
glm::vec3 feetPositionFromCamera();
void gazeOriginAndDirection(glm::vec3& origen, glm::vec3& direccion);
bool spawnPhysicsPlayer();
void moveFirstPersonPlayer(GLFWwindow* window, double velocidadJugador);
void syncPlayerWithCamera();
void updatePhoneMotion(GLFWwindow* window, float deltaTime);
void updateCameraBobbing(float delta);
glm::dvec3 cameraVisualOffset();
