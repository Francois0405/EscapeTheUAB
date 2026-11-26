#include "src/rendering/RetroPostFX.h"

#include <GL/glew.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace
{
	GLuint CompileStage(GLenum tipo, const char* ruta)
	{
		std::ifstream archivo(ruta, std::ios::binary);
		if (!archivo)
		{
			std::cerr << "[retro] No se encuentra " << ruta << '\n';
			return 0;
		}
		const std::string fuente((std::istreambuf_iterator<char>(archivo)), std::istreambuf_iterator<char>());
		const char* texto = fuente.c_str();
		const GLuint shader = glCreateShader(tipo);
		glShaderSource(shader, 1, &texto, nullptr);
		glCompileShader(shader);
		GLint correcto = GL_FALSE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &correcto);
		if (correcto) return shader;
		char registro[1024] = {};
		glGetShaderInfoLog(shader, sizeof(registro), nullptr, registro);
		std::cerr << "[retro] Error al compilar " << ruta << ": " << registro << '\n';
		glDeleteShader(shader);
		return 0;
	}
}

bool RetroPostFX::Create(const char* vertexPath, const char* fragmentPath)
{
	Destroy();
	const GLuint vertice = CompileStage(GL_VERTEX_SHADER, vertexPath);
	const GLuint fragmento = CompileStage(GL_FRAGMENT_SHADER, fragmentPath);
	if (vertice != 0 && fragmento != 0)
	{
		program = glCreateProgram();
		glAttachShader(program, vertice);
		glAttachShader(program, fragmento);
		glLinkProgram(program);
		GLint correcto = GL_FALSE;
		glGetProgramiv(program, GL_LINK_STATUS, &correcto);
		if (!correcto)
		{
			char registro[1024] = {};
			glGetProgramInfoLog(program, sizeof(registro), nullptr, registro);
			std::cerr << "[retro] Error al enlazar el posprocesado: " << registro << '\n';
			glDeleteProgram(program);
			program = 0;
		}
		else glGenVertexArrays(1, &vao);   // el perfil core exige un VAO aunque no haya atributos
	}
	if (vertice != 0) glDeleteShader(vertice);
	if (fragmento != 0) glDeleteShader(fragmento);
	return Ready();
}

void RetroPostFX::Destroy()
{
	if (program != 0) glDeleteProgram(program);
	if (vao != 0) glDeleteVertexArrays(1, &vao);
	program = vao = 0;
}

void RetroPostFX::Draw(unsigned int sceneTexture, retro::Size internal, int windowWidth, int windowHeight,
	const retro::Settings& settings) const
{
	if (!Ready() || windowWidth <= 0 || windowHeight <= 0) return;

	GLint programaAnterior = 0, vaoAnterior = 0, unidadAnterior = 0, texturaAnterior = 0;
	GLint modoPoligono[2] = { GL_FILL, GL_FILL };
	glGetIntegerv(GL_CURRENT_PROGRAM, &programaAnterior);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vaoAnterior);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &unidadAnterior);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &texturaAnterior);
	glGetIntegerv(GL_POLYGON_MODE, modoPoligono);
	const GLboolean profundidad = glIsEnabled(GL_DEPTH_TEST);
	const GLboolean recorte = glIsEnabled(GL_CULL_FACE);
	const GLboolean mezcla = glIsEnabled(GL_BLEND);
	const GLboolean tijera = glIsEnabled(GL_SCISSOR_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, windowWidth, windowHeight);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glUseProgram(program);
	glBindTexture(GL_TEXTURE_2D, sceneTexture);
	const auto lugar = [this](const char* nombre) { return glGetUniformLocation(program, nombre); };
	glUniform1i(lugar("uScene"), 0);
	glUniform2i(lugar("uInternalSize"), internal.width, internal.height);
	glUniform1i(lugar("uLevels"), (1 << settings.bitsPerChannel) - 1);
	glUniform1i(lugar("uDitherMode"), static_cast<int>(settings.dither));
	glUniform1i(lugar("uPattern"), static_cast<int>(settings.pattern));
	glUniform1f(lugar("uDitherStrength"), settings.ditherStrength);
	glUniform1f(lugar("uSaturation"), settings.saturation);
	glUniform1f(lugar("uContrast"), settings.contrast);
	glUniform1f(lugar("uExposureEV"), settings.exposureEV);
	glUniform1f(lugar("uGrain"), settings.grain);
	glUniform1f(lugar("uVignette"), settings.vignette);
	glUniform1f(lugar("uGrading"), settings.grading);
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindVertexArray(static_cast<GLuint>(vaoAnterior));
	glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texturaAnterior));
	glActiveTexture(static_cast<GLenum>(unidadAnterior));
	glUseProgram(static_cast<GLuint>(programaAnterior));
	glPolygonMode(GL_FRONT_AND_BACK, static_cast<GLenum>(modoPoligono[0]));
	if (profundidad) glEnable(GL_DEPTH_TEST);
	if (recorte) glEnable(GL_CULL_FACE);
	if (mezcla) glEnable(GL_BLEND);
	if (tijera) glEnable(GL_SCISSOR_TEST);
}
