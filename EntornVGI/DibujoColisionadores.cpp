// DibujoColisionadores.cpp
#include "framework.h"
#include "DibujoColisionadores.h"

namespace fisica
{
	namespace
	{
		GLuint vao = 0;
		GLuint vbo = 0;

		glm::vec4 ColorDe(const CajaColision& caja)
		{
			switch (caja.tipo)
			{
			case TipoCuerpo::Estatico:   return glm::vec4(0.2f, 0.5f, 1.0f, 1.0f);
			case TipoCuerpo::Cinematico: return glm::vec4(1.0f, 0.6f, 0.1f, 1.0f);
			case TipoCuerpo::Dinamico:   return caja.dormido ? glm::vec4(0.1f, 0.6f, 0.2f, 1.0f)
			                                                 : glm::vec4(0.2f, 1.0f, 0.3f, 1.0f);
			}
			return glm::vec4(1.0f);
		}

		void CrearBuffersSiHaceFalta()
		{
			if (vao != 0) return;
			glGenVertexArrays(1, &vao);
			glGenBuffers(1, &vbo);
			glBindVertexArray(vao);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			// Cada vértice: posición (3 floats) + color (4 floats), como espera eixos.vert.
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), reinterpret_cast<void*>(0));
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
			glBindVertexArray(0);
		}
	}

	void DibujarColisionadores(unsigned int programaEjes, const glm::mat4& proyeccion, const glm::mat4& vista,
		const std::vector<CajaColision>& cajas)
	{
		if (cajas.empty() || programaEjes == 0) return;

		// Todas las aristas de todas las cajas en un solo buffer (se recalcula cada fotograma).
		std::vector<float> vertices;
		for (const CajaColision& caja : cajas)
		{
			const glm::vec4 color = ColorDe(caja);
			for (const glm::vec3& punto : AristasDeCaja(caja))
				vertices.insert(vertices.end(), { punto.x, punto.y, punto.z, color.r, color.g, color.b, color.a });
		}

		CrearBuffersSiHaceFalta();
		glUseProgram(programaEjes);
		glUniformMatrix4fv(glGetUniformLocation(programaEjes, "projectionMatrix"), 1, GL_FALSE, &proyeccion[0][0]);
		glUniformMatrix4fv(glGetUniformLocation(programaEjes, "viewMatrix"), 1, GL_FALSE, &vista[0][0]);

		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);

		const GLboolean teniaProfundidad = glIsEnabled(GL_DEPTH_TEST);
		glDisable(GL_DEPTH_TEST);   // se ven también a través de las paredes
		glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size() / 7));
		if (teniaProfundidad) glEnable(GL_DEPTH_TEST);
		glBindVertexArray(0);
	}

	void LiberarDibujoColisionadores()
	{
		if (vbo != 0) glDeleteBuffers(1, &vbo);
		if (vao != 0) glDeleteVertexArrays(1, &vao);
		vao = vbo = 0;
	}
}
