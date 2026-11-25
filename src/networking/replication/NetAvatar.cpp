// NetAvatar.cpp
#include "framework.h"
#include "src/networking/replication/NetAvatar.h"

namespace netjuego
{
	namespace
	{
		GLuint vao = 0, vbo = 0;
		void CrearBuffersSiHaceFalta()
		{
			if (vao != 0) return;
			glGenVertexArrays(1, &vao);
			glGenBuffers(1, &vbo);
			glBindVertexArray(vao);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glEnableVertexAttribArray(0);   // posicion (3) + color (4), como espera eixos.vert
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), reinterpret_cast<void*>(0));
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
			glBindVertexArray(0);
		}
	}

	void DibujarAvataresRemotos(unsigned int programaEjes, const glm::mat4& proyeccion, const glm::mat4& vista,
		const std::vector<net::PoseEntry>& remotos, bool dibujarObjeto, const glm::vec3& posObjeto, bool objetoMio, bool objetoOcupado)
	{
		if (programaEjes == 0 || (remotos.empty() && !dibujarObjeto)) return;
		static const glm::vec4 colores[] = { {0.2f, 1.0f, 0.9f, 1}, {1.0f, 0.6f, 0.1f, 1}, {1.0f, 0.3f, 0.8f, 1}, {0.7f, 1.0f, 0.2f, 1} };
		std::vector<float> v;
		for (const net::PoseEntry& p : remotos) ConstruirAvatar(v, p, colores[p.peer % 4]);
		if (dibujarObjeto)
		{
			const glm::vec4 c = objetoMio ? glm::vec4(0.3f, 1.0f, 0.3f, 1) : (objetoOcupado ? glm::vec4(1.0f, 0.4f, 0.2f, 1) : glm::vec4(1.0f, 1.0f, 0.2f, 1));
			AnadirCajaLineas(v, posObjeto, glm::quat(1, 0, 0, 0), glm::vec3(0.15f), c);
			AnadirCajaLineas(v, posObjeto, glm::quat(1, 0, 0, 0), glm::vec3(0.075f), c);
		}
		if (v.empty()) return;

		CrearBuffersSiHaceFalta();
		glUseProgram(programaEjes);
		glUniformMatrix4fv(glGetUniformLocation(programaEjes, "projectionMatrix"), 1, GL_FALSE, &proyeccion[0][0]);
		glUniformMatrix4fv(glGetUniformLocation(programaEjes, "viewMatrix"), 1, GL_FALSE, &vista[0][0]);
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_DYNAMIC_DRAW);
		glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(v.size() / 7));   // con profundidad: las paredes los tapan
		glBindVertexArray(0);
	}

	void LiberarDibujoAvatares()
	{
		if (vbo != 0) glDeleteBuffers(1, &vbo);
		if (vao != 0) glDeleteVertexArrays(1, &vao);
		vao = vbo = 0;
	}
}
