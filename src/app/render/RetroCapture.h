#pragma once

// Diagnostico opcional: con ENTORNVGI_RETRO_CAPTURE=<carpeta> el juego entra en partida, recorre
// los presets desde la misma camara, guarda capturas PNG y metricas.csv y se cierra.
// Sin la variable no hace nada. No guarda los ajustes y silencia el audio.
struct GLFWwindow;

bool retroCaptureEnabled();
// Antes de construir la interfaz del fotograma.
void retroCaptureBeforeUi(GLFWwindow* window);
// Despues de dibujar ImGui y antes de intercambiar buffers.
void retroCaptureAfterUi(GLFWwindow* window);
