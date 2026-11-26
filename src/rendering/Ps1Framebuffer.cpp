#include "src/legacy_vgi/framework.h"
#include "src/rendering/Ps1Framebuffer.h"

#include <iostream>

void Ps1Framebuffer::Create()
{
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	// Crear textura de color
	glGenTextures(1, &color);
	glBindTexture(GL_TEXTURE_2D, color);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	// CLAVE PARA PS1: GL_NEAREST evita el suavizado de los píxeles
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);

	// Crear Renderbuffer para profundidad (Depth)
	glGenRenderbuffers(1, &depth);
	glBindRenderbuffer(GL_RENDERBUFFER, depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cerr << "Error en el FBO de PS1\n";
	glBindFramebuffer(GL_FRAMEBUFFER, 0); // Volver al framebuffer por defecto
}

void Ps1Framebuffer::Resize(int newWidth, int newHeight)
{
	if (newWidth <= 0 || newHeight <= 0 || (newWidth == width && newHeight == height)) return;
	Destroy();
	width = newWidth;
	height = newHeight;
	Create();
}

void Ps1Framebuffer::Destroy()
{
	glDeleteFramebuffers(1, &fbo);
	glDeleteTextures(1, &color);
	glDeleteRenderbuffers(1, &depth);
}
