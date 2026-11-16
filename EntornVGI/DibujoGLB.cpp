// DibujoGLB.cpp
// -----------------------------------------------------------------------------
// OpenGL de ModeloGLB. Una "primitiva" se sube a la GPU una sola vez (VAO + VBO + EBO)
// y después se dibuja cada fotograma con la matriz que toque.
// -----------------------------------------------------------------------------
#include "framework.h"
#include "DibujoGLB.h"
#include <vector>

namespace glb
{
	namespace
	{
		// Cada vértice ocupa 6 números seguidos: posición (x,y,z) y normal (x,y,z).
		std::vector<float> IntercalarPosicionesYNormales(const PrimitivaGLB& primitiva)
		{
			std::vector<float> datos;
			datos.reserve(primitiva.posiciones.size() * 6);
			for (size_t i = 0; i < primitiva.posiciones.size(); ++i)
			{
				datos.push_back(primitiva.posiciones[i].x);
				datos.push_back(primitiva.posiciones[i].y);
				datos.push_back(primitiva.posiciones[i].z);
				datos.push_back(primitiva.normales[i].x);
				datos.push_back(primitiva.normales[i].y);
				datos.push_back(primitiva.normales[i].z);
			}
			return datos;
		}

		void SubirPrimitiva(PrimitivaGLB& primitiva)
		{
			const std::vector<float> vertices = IntercalarPosicionesYNormales(primitiva);

			glGenVertexArrays(1, &primitiva.vao);
			glGenBuffers(1, &primitiva.vbo);
			glGenBuffers(1, &primitiva.ebo);

			glBindVertexArray(primitiva.vao);   // el VAO recuerda la configuración de abajo

			glBindBuffer(GL_ARRAY_BUFFER, primitiva.vbo);
			glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, primitiva.ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, primitiva.indices.size() * sizeof(unsigned int),
				primitiva.indices.data(), GL_STATIC_DRAW);

			const GLsizei tamanoVertice = 6 * sizeof(float);
			glEnableVertexAttribArray(0);   // 0 = posición
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, tamanoVertice, (void*)0);
			glEnableVertexAttribArray(2);   // 2 = normal
			glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, tamanoVertice, (void*)(3 * sizeof(float)));

			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		// Envía el color del material al shader (igual que hace COBJModel con sus materiales).
		void ActivarMaterial(unsigned int programa, const glm::vec4& color)
		{
			glUniform1i(glGetUniformLocation(programa, "textur"), GL_FALSE);
			glUniform4f(glGetUniformLocation(programa, "material.ambient"), color.r, color.g, color.b, color.a);
			glUniform4f(glGetUniformLocation(programa, "material.diffuse"), color.r, color.g, color.b, color.a);
			glUniform4f(glGetUniformLocation(programa, "material.specular"), 0.1f, 0.1f, 0.1f, 1.0f);
			glUniform1f(glGetUniformLocation(programa, "material.shininess"), 20.0f);
			// El shader lee el color del vértice solo si no usa el material; damos uno por si acaso.
			glVertexAttrib4f(1, color.r, color.g, color.b, color.a);
			glVertexAttrib2f(3, 0.0f, 0.0f);
		}
	}

	void SubirModeloAGPU(ModeloGLB& modelo)
	{
		for (MallaGLB& malla : modelo.mallas)
			for (PrimitivaGLB& primitiva : malla.primitivas)
				if (primitiva.vao == 0) SubirPrimitiva(primitiva);
	}

	void LiberarModeloDeGPU(ModeloGLB& modelo)
	{
		for (MallaGLB& malla : modelo.mallas)
			for (PrimitivaGLB& primitiva : malla.primitivas)
			{
				if (primitiva.vao == 0) continue;
				glDeleteVertexArrays(1, &primitiva.vao);
				glDeleteBuffers(1, &primitiva.vbo);
				glDeleteBuffers(1, &primitiva.ebo);
				primitiva.vao = primitiva.vbo = primitiva.ebo = 0;
			}
	}

	void DibujarModeloGLB(const ModeloGLB& modelo, const PoseGLB& pose, unsigned int programa, const glm::mat4& matrizBase)
	{
		// Matriz final de cada nodo (se lee de derecha a izquierda):
		//   1. mundo del nodo : su posición dentro de la jerarquía del archivo (Y arriba)
		//   2. Y-arriba -> Z-arriba : cambio de ejes de glTF a nuestro juego
		//   3. matrizBase     : dónde colocamos el modelo en la habitación
		const glm::mat4 cambioDeEjes = matrizBase * MatrizYArribaAZArriba();

		for (size_t i = 0; i < modelo.nodos.size(); ++i)
		{
			const int indiceMalla = modelo.nodos[i].indiceMalla;
			if (indiceMalla < 0) continue;

			const glm::mat4 matriz = cambioDeEjes * MatrizMundoDelNodo(modelo, pose, static_cast<int>(i));
			glUniformMatrix4fv(glGetUniformLocation(programa, "modelMatrix"), 1, GL_FALSE, &matriz[0][0]);

			for (const PrimitivaGLB& primitiva : modelo.mallas[indiceMalla].primitivas)
			{
				if (primitiva.vao == 0) continue;   // no se subió a la GPU
				ActivarMaterial(programa, primitiva.colorBase);
				glBindVertexArray(primitiva.vao);
				glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(primitiva.indices.size()), GL_UNSIGNED_INT, (void*)0);
			}
		}
		glBindVertexArray(0);
	}

	void SubirEscenaAnimadaAGPU(EscenaAnimada& escena)
	{
		for (auto& entrada : escena.modelos) SubirModeloAGPU(entrada.second);   // un modelo = una subida, aunque tenga varias instancias
	}

	void LiberarEscenaAnimadaDeGPU(EscenaAnimada& escena)
	{
		for (auto& entrada : escena.modelos) LiberarModeloDeGPU(entrada.second);
	}

	void DibujarObjetosAnimados(const EscenaAnimada& escena, unsigned int programa)
	{
		for (const ObjetoAnimado& objeto : escena.objetos)
			DibujarModeloGLB(*objeto.modelo, objeto.pose, programa, MatrizBaseDelObjeto(objeto));
	}
}
