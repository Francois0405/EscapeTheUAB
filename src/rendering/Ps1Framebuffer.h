#pragma once

// Low resolution render target that gives PLAY mode its PS1 look. Needs a current OpenGL context.
struct Ps1Framebuffer
{
	static constexpr int kWidth = 960;
	static constexpr int kHeight = 540;

	int width = kWidth;
	int height = kHeight;
	unsigned int fbo = 0;
	unsigned int color = 0;
	unsigned int depth = 0;

	// false si el framebuffer no queda completo.
	bool Create();
	// Recrea el destino solo si cambia el tamano; si falla vuelve a 960x540.
	void Resize(int newWidth, int newHeight);
	void Destroy();
};
