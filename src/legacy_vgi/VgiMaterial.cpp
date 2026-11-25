#include "src/app/AppState.h"
#include "src/legacy_vgi/VgiMaterial.h"

void applyMaterialIntensity(GLuint programa, bool ambienteActivo)
{
	if (programa == 0) return;
	const GLint localizacion = glcache::UniformLocation(programa, "sw_intensity");
	if (localizacion < 0) return;
	GLint programaAnterior = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &programaAnterior);
	if (static_cast<GLuint>(programaAnterior) != programa) glUseProgram(programa);
	glUniform4i(localizacion, sw_material[0] ? GL_TRUE : GL_FALSE,
		(sw_material[1] && ambienteActivo) ? GL_TRUE : GL_FALSE, sw_material[2] ? GL_TRUE : GL_FALSE,
		sw_material[3] ? GL_TRUE : GL_FALSE);
	if (static_cast<GLuint>(programaAnterior) != programa) glUseProgram(static_cast<GLuint>(programaAnterior));
}
