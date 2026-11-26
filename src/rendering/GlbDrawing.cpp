// DibujoGLB.cpp
// -----------------------------------------------------------------------------
// OpenGL de ModeloGLB. Una "primitiva" se sube a la GPU una sola vez (VAO + VBO + EBO)
// y después se dibuja cada fotograma con la matriz que toque.
// -----------------------------------------------------------------------------
#include "framework.h"
#include "src/rendering/GlbDrawing.h"
#include "src/rendering/UniformCache.h"
#include "src/rendering/FrustumCulling.h"
#include "src/rendering/RenderMetrics.h"
#include <vector>
#include <limits>

namespace glb
{
	namespace
	{
		// Cada vértice ocupa 6 números seguidos: posición (x,y,z) y normal (x,y,z).
		std::vector<float> IntercalarPosicionesYNormales(const GlbPrimitive& primitiva)
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

		void SubirPrimitiva(GlbPrimitive& primitiva)
		{
			if (!primitiva.tieneBounds && !primitiva.posiciones.empty())
			{
				primitiva.boundsMin = glm::vec3(std::numeric_limits<float>::max());
				primitiva.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
				for (const glm::vec3& position : primitiva.posiciones)
				{
					primitiva.boundsMin = glm::min(primitiva.boundsMin, position);
					primitiva.boundsMax = glm::max(primitiva.boundsMax, position);
				}
				primitiva.tieneBounds = true;
			}
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
			glUniform1i(glcache::UniformLocation(programa, "textur"), GL_FALSE);
			glUniform4f(glcache::UniformLocation(programa, "material.ambient"), color.r, color.g, color.b, color.a);
			glUniform4f(glcache::UniformLocation(programa, "material.diffuse"), color.r, color.g, color.b, color.a);
			glUniform4f(glcache::UniformLocation(programa, "material.specular"), 0.1f, 0.1f, 0.1f, 1.0f);
			glUniform1f(glcache::UniformLocation(programa, "material.shininess"), 20.0f);
			// El shader lee el color del vértice solo si no usa el material; damos uno por si acaso.
			glVertexAttrib4f(1, color.r, color.g, color.b, color.a);
			glVertexAttrib2f(3, 0.0f, 0.0f);
		}
	}

	void UploadModelToGpu(GlbModel& modelo)
	{
		for (GlbMesh& malla : modelo.mallas)
			for (GlbPrimitive& primitiva : malla.primitivas)
				if (primitiva.vao == 0) SubirPrimitiva(primitiva);
	}

	void ReleaseModelFromGpu(GlbModel& modelo)
	{
		for (GlbMesh& malla : modelo.mallas)
			for (GlbPrimitive& primitiva : malla.primitivas)
			{
				if (primitiva.vao == 0) continue;
				glDeleteVertexArrays(1, &primitiva.vao);
				glDeleteBuffers(1, &primitiva.vbo);
				glDeleteBuffers(1, &primitiva.ebo);
				primitiva.vao = primitiva.vbo = primitiva.ebo = 0;
			}
	}

	void DrawGlbModel(const GlbModel& modelo, const GlbPose& pose, unsigned int programa, const glm::mat4& matrizBase,
		const glm::mat4& viewProjection, bool aplicarCulling)
	{
		// Matriz final de cada nodo (se lee de derecha a izquierda):
		//   1. mundo del nodo : su posición dentro de la jerarquía del archivo (Y arriba)
		//   2. Y-arriba -> Z-arriba : cambio de ejes de glTF a nuestro juego
		//   3. matrizBase     : dónde colocamos el modelo en la habitación
		const glm::mat4 cambioDeEjes = matrizBase * YUpToZUpMatrix();

		for (size_t i = 0; i < modelo.nodos.size(); ++i)
		{
			const int meshIndex = modelo.nodos[i].meshIndex;
			if (meshIndex < 0) continue;

			const glm::mat4 matriz = cambioDeEjes * NodeWorldMatrix(modelo, pose, static_cast<int>(i));
			bool nodoVisible = !aplicarCulling;
			if (aplicarCulling)
			{
				// Compute bounds from the actual primitive vertices in its current node pose.
				// Empty or malformed geometry is kept visible to preserve correctness.
				bool tieneBounds = false;
				for (const GlbPrimitive& primitiva : modelo.mallas[meshIndex].primitivas)
				{
					if (!primitiva.tieneBounds) continue;
					tieneBounds = true;
					if (!BoxOutsideFrustum(viewProjection, matriz, primitiva.boundsMin, primitiva.boundsMax))
					{ nodoVisible = true; break; }
				}
				if (!tieneBounds) nodoVisible = true;
			}
			if (renderMetrics::enabled) { if (nodoVisible) ++renderMetrics::visibles; else ++renderMetrics::descartados; }
			if (!nodoVisible) continue;
			glUniformMatrix4fv(glcache::UniformLocation(programa, "modelMatrix"), 1, GL_FALSE, &matriz[0][0]);

			for (const GlbPrimitive& primitiva : modelo.mallas[meshIndex].primitivas)
			{
				if (primitiva.vao == 0) continue;   // no se subió a la GPU
				ActivarMaterial(programa, primitiva.colorBase);
				glBindVertexArray(primitiva.vao);
						renderMetrics::DrawElements(GL_TRIANGLES, static_cast<GLsizei>(primitiva.indices.size()), GL_UNSIGNED_INT, (void*)0);
			}
		}
		glBindVertexArray(0);
	}

	void UploadAnimatedSceneToGpu(AnimatedScene& escena)
	{
		for (auto& entrada : escena.modelos) UploadModelToGpu(entrada.second);   // un modelo = una subida, aunque tenga varias instancias
	}

	void ReleaseAnimatedSceneFromGpu(AnimatedScene& escena)
	{
		for (auto& entrada : escena.modelos) ReleaseModelFromGpu(entrada.second);
	}

	void DrawAnimatedObjects(const AnimatedScene& escena, unsigned int programa,
		const glm::mat4& viewProjection, bool aplicarCulling)
	{
		for (const AnimatedObject& objeto : escena.objetos)
			DrawGlbModel(*objeto.modelo, objeto.pose, programa, ObjectBaseMatrix(objeto), viewProjection, aplicarCulling);
	}
}
