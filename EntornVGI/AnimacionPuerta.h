// AnimacionPuerta.h
// -----------------------------------------------------------------------------
// FASE 1: ejemplo mínimo de animación por fotogramas clave (keyframes).
//
// Idea en 4 pasos:
//   1. Una animación es una lista de keyframes: "en el segundo T, el ángulo vale A".
//   2. Cada fotograma sabemos cuánto tiempo lleva sonando la animación.
//   3. Buscamos entre qué dos keyframes estamos y mezclamos sus valores (glm::mix).
//   4. Con el ángulo resultante construimos la matriz del objeto girando
//      alrededor de la BISAGRA (no alrededor del centro).
//
// Todo está en este .h (funciones "inline") para no tocar CMake ni el .vcxproj.
// Sistema de coordenadas del juego: Z es "arriba" (igual que en Blender).
// -----------------------------------------------------------------------------
#ifndef ANIMACION_PUERTA_H
#define ANIMACION_PUERTA_H

#include <vector>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace animacion
{
	// Un fotograma clave: "en el segundo 'tiempo', el ángulo es 'anguloGrados'".
	struct KeyframeAngulo
	{
		float tiempo;        // segundos desde el inicio de la animación
		float anguloGrados;  // giro alrededor del eje vertical (Z)
	};

	// Devuelve el ángulo que corresponde a un instante, interpolando entre keyframes.
	// Los keyframes deben estar ordenados por tiempo (de menor a mayor).
	inline float AnguloEnTiempo(const std::vector<KeyframeAngulo>& keyframes, float tiempo)
	{
		if (keyframes.empty()) return 0.0f;

		// Antes del primer keyframe o después del último: nos quedamos con el extremo.
		if (tiempo <= keyframes.front().tiempo) return keyframes.front().anguloGrados;
		if (tiempo >= keyframes.back().tiempo)  return keyframes.back().anguloGrados;

		// Buscamos el par (anterior, siguiente) que rodea al tiempo actual.
		for (size_t i = 1; i < keyframes.size(); ++i)
		{
			const KeyframeAngulo& anterior = keyframes[i - 1];
			const KeyframeAngulo& siguiente = keyframes[i];
			if (tiempo > siguiente.tiempo) continue;

			// fraccion = 0 en 'anterior', 1 en 'siguiente'. Interpolación lineal.
			const float fraccion = (tiempo - anterior.tiempo) / (siguiente.tiempo - anterior.tiempo);
			return glm::mix(anterior.anguloGrados, siguiente.anguloGrados, fraccion);
		}
		return keyframes.back().anguloGrados;
	}

	// Lleva la cuenta del tiempo: empieza, avanza con el delta de cada fotograma y termina.
	struct Reproductor
	{
		float tiempo = 0.0f;           // segundos transcurridos
		float duracion = 0.0f;         // segundos totales de la animación
		bool reproduciendo = false;

		void Iniciar(float duracionSegundos)
		{
			tiempo = 0.0f;
			duracion = duracionSegundos;
			reproduciendo = true;
		}

		// Se llama una vez por fotograma con el tiempo que ha pasado desde el anterior.
		void Actualizar(float deltaSegundos)
		{
			if (!reproduciendo) return;
			tiempo += deltaSegundos;
			if (tiempo >= duracion)
			{
				tiempo = duracion;      // nos quedamos justo en el final
				reproduciendo = false;  // y avisamos de que ya terminó
			}
		}
	};

	// Matriz de una puerta que gira alrededor de su bisagra.
	//
	// El modelo es un cubo de -1 a 1 (assets/models/test.obj), así que el cubo
	// original mide 2x2x2 y su centro es el (0,0,0).
	//
	//   mitadTamano     : medio ancho, medio grosor y media altura de la puerta (en metros).
	//   posicionBisagra : punto del mundo donde está la bisagra, a ras de suelo.
	//   anguloGrados    : giro actual alrededor del eje Z.
	//
	// Con la puerta cerrada, la hoja se extiende desde la bisagra hacia +Y.
	//
	// Se lee de derecha a izquierda (así se aplican las transformaciones a los vértices):
	//   1. escalar el cubo al tamaño de la puerta.
	//   2. desplazarlo para que la BISAGRA quede en el origen (0,0,0)
	//   3. girar: como la bisagra está en el origen, gira alrededor de ella.
	//   4. llevar la bisagra a su sitio en el mundo.
	inline glm::mat4 MatrizPuertaConBisagra(
		const glm::vec3& mitadTamano,
		const glm::vec3& posicionBisagra,
		float anguloGrados)
	{
		// Tras escalar, la bisagra (en el borde y=-1 del cubo, a ras de suelo z=-1)
		// está en (0, -mitad.y, -mitad.z). Sumamos lo contrario para llevarla al origen.
		const glm::vec3 desplazamientoBisagra(0.0f, mitadTamano.y, mitadTamano.z);

		glm::mat4 matriz(1.0f);
		matriz = glm::translate(matriz, posicionBisagra);                                   // 4
		matriz = glm::rotate(matriz, glm::radians(anguloGrados), glm::vec3(0, 0, 1));       // 3
		matriz = glm::translate(matriz, desplazamientoBisagra);                             // 2
		matriz = glm::scale(matriz, mitadTamano);                                           // 1
		return matriz;
	}
}

#endif
