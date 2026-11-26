#pragma once

#include "src/rendering/RetroSettings.h"

// Pase de posprocesado de la escena (cuantizacion y dithering). Necesita un contexto OpenGL actual.
// Si el shader no esta disponible, Ready() es false y el juego usa el blit original.
class RetroPostFX
{
public:
	bool Create(const char* vertexPath, const char* fragmentPath);
	void Destroy();
	bool Ready() const { return program != 0; }

	// Dibuja sceneTexture sobre el framebuffer 0 cubriendo la ventana y restaura el estado OpenGL.
	void Draw(unsigned int sceneTexture, retro::Size internal, int windowWidth, int windowHeight,
		const retro::Settings& settings) const;

private:
	unsigned int program = 0;
	unsigned int vao = 0;
};
