// VistaVR.h
// -----------------------------------------------------------------------------
// Matemática de la vista VR (OpenXR -> motor). Solo usa GLM: ni OpenGL ni OpenXR, así que se prueba
// sin visor ni ventana (tests/VistaVRTests.cpp).
//
// CONVENIOS
//   OpenXR : +X derecha, +Y arriba, -Z hacia delante (mano derecha). Unidades: metros.
//   Motor  : Z es el eje VERTICAL (arriba). La cámara FPS mira hacia 'adelante' (horizontal) y
//            la derecha es adelante x Z. Unidades del motor: 'escala' unidades por metro (1 = metros).
//
// ANCLAJE ("rig")
//   El origen del espacio de referencia de OpenXR se coloca en el cuerpo del jugador y se gira para que
//   el "adelante" de OpenXR coincida con el "adelante" de la cámara FPS (ratón). El visor se mueve dentro
//   de ese anclaje: la cabeza real mueve la cámara, pero NUNCA la cápsula de colisiones del jugador.
//     * Espacio STAGE (suelo): el origen es el suelo -> se ancla en los PIES del jugador.
//     * Espacio LOCAL (inicio de sesión, la cabeza arranca en el origen) -> se ancla en los OJOS.
// -----------------------------------------------------------------------------
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace vr
{
	// Pose en el espacio de OpenXR (posición en metros + orientación).
	struct PoseXR
	{
		glm::vec3 posicion{ 0.0f };
		glm::quat orientacion{ 1.0f, 0.0f, 0.0f, 0.0f };   // w, x, y, z (identidad)
	};

	// Campo de visión de un ojo como lo da OpenXR (XrFovf): ángulos en RADIANES desde el eje de la vista.
	// 'izquierda' y 'abajo' son negativos en un campo normal. Puede ser asimétrico.
	struct CampoDeVision
	{
		float izquierda = 0.0f, derecha = 0.0f, arriba = 0.0f, abajo = 0.0f;
	};

	// Dónde está el mundo VR dentro del motor.
	struct Anclaje
	{
		glm::vec3 origen{ 0.0f };                 // punto del motor que corresponde al origen del espacio OpenXR
		glm::vec3 adelante{ 0.0f, 1.0f, 0.0f };   // dirección horizontal (unitaria) que OpenXR llama "hacia delante"
		float escala = 1.0f;                      // unidades del motor por metro
	};

	// 'adelante' = dirección de la cámara FPS (puede tener componente vertical; se ignora). Si es casi vertical
	// o nula, se usa +Y para no producir una base degenerada.
	inline glm::vec3 AdelanteHorizontal(const glm::vec3& direccion)
	{
		glm::vec3 plano(direccion.x, direccion.y, 0.0f);
		const float largo = glm::length(plano);
		if (!(largo > 1e-4f)) return glm::vec3(0.0f, 1.0f, 0.0f);
		return plano / largo;
	}

	// Anclaje a partir del jugador. 'pies' = posición de sus pies; 'espacioSuelo' = true si el espacio de
	// referencia es STAGE (el origen está en el suelo), false si es LOCAL (el origen está a la altura de los ojos).
	inline Anclaje AnclajeDelJugador(const glm::vec3& pies, const glm::vec3& direccionCamara, bool espacioSuelo,
		float alturaOjos, float escala = 1.0f)
	{
		Anclaje a;
		a.origen = espacioSuelo ? pies : pies + glm::vec3(0.0f, 0.0f, alturaOjos);
		a.adelante = AdelanteHorizontal(direccionCamara);
		a.escala = escala;
		return a;
	}

	// Columnas: ejes X,Y,Z de OpenXR expresados en el motor:  X_xr = derecha, Y_xr = arriba (Z), Z_xr = -adelante.
	// Es una rotación propia (determinante +1).
	inline glm::mat3 BaseMotorDesdeXR(const glm::vec3& adelanteUnitario)
	{
		const glm::vec3 arriba(0.0f, 0.0f, 1.0f);
		const glm::vec3 derecha = glm::cross(adelanteUnitario, arriba);
		return glm::mat3(derecha, arriba, -adelanteUnitario);
	}

	// Pose del ojo en el MUNDO del motor (matriz modelo de la cámara: sus ejes locales son los de OpenXR).
	inline glm::mat4 MundoDeOjo(const PoseXR& pose, const Anclaje& anclaje)
	{
		const glm::mat3 base = BaseMotorDesdeXR(anclaje.adelante);
		const glm::mat3 rotacion = base * glm::mat3_cast(glm::normalize(pose.orientacion));
		const glm::vec3 posicion = anclaje.origen + base * (pose.posicion * anclaje.escala);
		glm::mat4 m(rotacion);
		m[3] = glm::vec4(posicion, 1.0f);
		return m;
	}

	// Matriz de vista (mundo -> ojo) lista para el shader (viewMatrix).
	inline glm::mat4 VistaDeOjoVR(const PoseXR& pose, const Anclaje& anclaje)
	{
		return glm::inverse(MundoDeOjo(pose, anclaje));
	}

	// Posición del ojo en el motor (para iluminación especular, depuración...).
	inline glm::vec3 PosicionDeOjo(const PoseXR& pose, const Anclaje& anclaje)
	{
		return glm::vec3(MundoDeOjo(pose, anclaje)[3]);
	}

	// Proyección en perspectiva (profundidad OpenGL -1..1) con el campo de visión, posiblemente asimétrico, de OpenXR.
	inline glm::mat4 ProyeccionDesdeCampo(const CampoDeVision& fov, float cerca, float lejos)
	{
		const float tanI = std::tan(fov.izquierda), tanD = std::tan(fov.derecha);
		const float tanA = std::tan(fov.arriba), tanB = std::tan(fov.abajo);
		const float ancho = tanD - tanI, alto = tanA - tanB;

		glm::mat4 p(0.0f);   // glm es column-major: p[columna][fila]
		p[0][0] = 2.0f / ancho;
		p[1][1] = 2.0f / alto;
		p[2][0] = (tanD + tanI) / ancho;
		p[2][1] = (tanA + tanB) / alto;
		p[2][2] = -(lejos + cerca) / (lejos - cerca);
		p[2][3] = -1.0f;
		p[3][2] = -2.0f * lejos * cerca / (lejos - cerca);
		return p;
	}
}
