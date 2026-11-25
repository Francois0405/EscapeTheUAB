#pragma once

#include "src/app/AppState.h"

const game::interaction::InteractionContext& interactionBindings();
bool playerHoldsObject();
InteractionTarget findCurrentInteractionTarget();
void updateCurrentInteractionTarget(bool permitido);
void beginThrowCharge();
void performInteraction();
void updateHeldObject(bool puedeJugar);
void cancelThrowChargeInput();
void throwHeldObject();
void updateThrowChargeInput(GLFWwindow* window, float delta);
