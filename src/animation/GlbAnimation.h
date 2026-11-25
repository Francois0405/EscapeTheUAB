// AnimacionGLB.h
// -----------------------------------------------------------------------------
// Matemática de las animaciones que vienen de Blender dentro de un .glb.
// No usa OpenGL ni cgltf: solo GLM. Por eso se puede probar sin abrir ventana.
//
// Recordatorio de la Fase 1 (AnimacionPuerta.h), ahora con datos reales:
//   - Una ANIMACIÓN es una lista de CANALES.
//   - Un CANAL anima UNA propiedad (posición, rotación o escala) de UN nodo.
//   - Un canal tiene KEYFRAMES: "en el instante T, el valor es V".
//   - Entre dos keyframes se INTERPOLA:
//        posición y escala -> glm::mix   (mezcla lineal)
//        rotación          -> glm::slerp (mezcla de cuaterniones por el camino corto)
//
// Convención glTF para los cuaterniones: el archivo guarda (x, y, z, w), pero
// glm::quat se construye como (w, x, y, z). Esa conversión solo se hace aquí.
// -----------------------------------------------------------------------------
#ifndef ANIMACION_GLB_H
#define ANIMACION_GLB_H

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace glb
{
	// Qué propiedad del nodo anima un canal.
	enum class AnimatedProperty { Posicion, Rotacion, Escala };

	// Cómo se pasa de un keyframe al siguiente.
	enum class InterpolationMode
	{
		Lineal,   // mezcla progresiva (mix / slerp)
		Escalon   // mantiene el valor anterior hasta llegar al siguiente keyframe
	};

	// Un canal: los keyframes de una propiedad de un nodo.
	// 'valores' usa vec4 para todo: posición y escala usan x,y,z; la rotación usa x,y,z,w.
	struct AnimationChannel
	{
		int indiceNodo = -1;
		AnimatedProperty propiedad = AnimatedProperty::Posicion;
		InterpolationMode interpolacion = InterpolationMode::Lineal;
		std::vector<float> tiempos;       // segundos, de menor a mayor
		std::vector<glm::vec4> valores;   // un valor por cada tiempo
	};

	// Una animación completa (por ejemplo "Abrir puerta").
	struct GlbAnimation
	{
		std::string nombre;
		float duracion = 0.0f;            // segundos; el primer keyframe está en t = 0
		std::vector<AnimationChannel> canales;
	};

	// Lleva la cuenta del tiempo de una animación: empieza, avanza y termina.
	// Puede reproducirse hacia ADELANTE (sentido = +1, abrir) o hacia ATRÁS (sentido = -1,
	// cerrar). Al terminar se queda en el último instante alcanzado: la puerta se queda
	// abierta (tiempo = duración) o cerrada (tiempo = 0).
	struct AnimationPlayer
	{
		float tiempo = 0.0f;
		float duracion = 0.0f;
		float sentido = 1.0f;          // +1 hacia delante, -1 hacia atrás
		bool reproduciendo = false;

		// Empieza desde el principio y hacia delante.
		void BeginPlayback(float duracionSegundos)
		{
			tiempo = 0.0f;
			duracion = duracionSegundos;
			sentido = 1.0f;
			reproduciendo = true;
		}

		// Continúa DESDE EL INSTANTE ACTUAL en el sentido indicado (+1 o -1).
		// Si se llama a mitad de una reproducción, invierte el movimiento sin saltos.
		void PlayAnimation(float nuevoSentido)
		{
			sentido = (nuevoSentido >= 0.0f) ? 1.0f : -1.0f;
			reproduciendo = true;
		}

		// Se llama una vez por fotograma con el tiempo transcurrido desde el anterior.
		void Advance(float deltaSegundos)
		{
			if (!reproduciendo) return;
			tiempo += deltaSegundos * sentido;

			if (sentido > 0.0f && tiempo >= duracion)
			{
				tiempo = duracion;      // llegó al final: abierta del todo
				reproduciendo = false;
			}
			else if (sentido < 0.0f && tiempo <= 0.0f)
			{
				tiempo = 0.0f;          // llegó al principio: cerrada del todo
				reproduciendo = false;
			}
		}
	};

	// Convierte el (x, y, z, w) del archivo en un glm::quat (que pide w, x, y, z).
	inline glm::quat QuaternionFromVec4(const glm::vec4& v)
	{
		return glm::quat(v.w, v.x, v.y, v.z);
	}

	// Busca el keyframe anterior al instante dado.
	// Devuelve i tal que tiempos[i] <= tiempo < tiempos[i+1]. Se asume i+1 válido.
	inline size_t FindPreviousKeyframe(const std::vector<float>& tiempos, float tiempo)
	{
		size_t i = 0;
		while (i + 2 < tiempos.size() && tiempos[i + 1] <= tiempo) ++i;
		return i;
	}

	// Mezcla dos valores de un canal: slerp para rotaciones, mix para lo demás.
	inline glm::vec4 BlendValues(AnimatedProperty propiedad, const glm::vec4& a, const glm::vec4& b, float fraccion)
	{
		if (propiedad == AnimatedProperty::Rotacion)
		{
			const glm::quat q = glm::slerp(QuaternionFromVec4(a), QuaternionFromVec4(b), fraccion);
			return glm::vec4(q.x, q.y, q.z, q.w);
		}
		return glm::mix(a, b, fraccion);
	}

	// Valor de un canal en un instante.
	//  - Antes del primer keyframe o después del último: se queda con el extremo.
	//  - Entre dos keyframes: interpola (o mantiene, si el canal es de tipo escalón).
	inline glm::vec4 ChannelValueAtTime(const AnimationChannel& canal, float tiempo)
	{
		if (canal.valores.empty()) return glm::vec4(0.0f);
		if (tiempo <= canal.tiempos.front()) return canal.valores.front();
		if (tiempo >= canal.tiempos.back()) return canal.valores.back();

		const size_t anterior = FindPreviousKeyframe(canal.tiempos, tiempo);
		const size_t siguiente = anterior + 1;
		if (canal.interpolacion == InterpolationMode::Escalon) return canal.valores[anterior];

		// fraccion = 0 en el keyframe anterior, 1 en el siguiente.
		const float duracionTramo = canal.tiempos[siguiente] - canal.tiempos[anterior];
		if (duracionTramo <= 0.0f) return canal.valores[siguiente];   // keyframes repetidos: evita dividir por cero
		const float fraccion = (tiempo - canal.tiempos[anterior]) / duracionTramo;
		return BlendValues(canal.propiedad, canal.valores[anterior], canal.valores[siguiente], fraccion);
	}
}

#endif
