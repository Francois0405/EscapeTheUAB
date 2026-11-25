#include "src/app/AppState.h"
#include "src/ui/InteractionHudTexture.h"

void loadInteractionHandTexture()
{
	std::error_code ec;
	const std::filesystem::path rutaAbs = std::filesystem::absolute("assets/ui/hand.png", ec).lexically_normal();
	if (ec || !std::filesystem::is_regular_file(rutaAbs, ec))
	{
		std::fprintf(stderr, "No se encuentra assets/ui/hand.png desde la raiz de recursos '%s'. "
			"Comprueba que EntornVGI_runtime_resources se haya ejecutado.\n",
			std::filesystem::current_path(ec).string().c_str());
		return;
	}

	int ancho = 0, alto = 0, canales = 0;
	unsigned char* pixels = SOIL_load_image(rutaAbs.string().c_str(), &ancho, &alto, &canales, SOIL_LOAD_RGBA);
	if (pixels == nullptr)
	{
		std::fprintf(stderr, "No se pudo cargar el icono de interaccion '%s': %s\n",
			rutaAbs.string().c_str(), SOIL_last_result());
		return;
	}
	if (ancho <= 0 || alto <= 0)
	{
		std::fprintf(stderr, "El icono de interaccion tiene dimensiones invalidas (%d x %d): %s\n",
			ancho, alto, rutaAbs.string().c_str());
		SOIL_free_image_data(pixels);
		return;
	}

	for (int i = 0; i < ancho * alto; ++i)
	{
		const unsigned char opacidad = static_cast<unsigned char>(
			(static_cast<unsigned int>(pixels[i * 4]) * pixels[i * 4 + 3] + 127u) / 255u);
		pixels[i * 4] = pixels[i * 4 + 1] = pixels[i * 4 + 2] = 255;
		pixels[i * 4 + 3] = opacidad;
	}

	glGenTextures(1, &interactionHandTexture);
	if (interactionHandTexture != 0)
	{
		glBindTexture(GL_TEXTURE_2D, interactionHandTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ancho, alto, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	else
		std::fprintf(stderr, "No se pudo crear la textura del HUD de interaccion.\n");
	SOIL_free_image_data(pixels);
}
