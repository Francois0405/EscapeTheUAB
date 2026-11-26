// VistaVRTests.cpp
// Pruebas de la matemática VR (VistaVR.h) sin visor, ventana ni OpenGL.
#include "src/vr/VrView.h"

#include <cmath>
#include <iostream>

using namespace vr;

namespace
{
	int fallos = 0;
	void comprobar(bool condicion, const char* caso)
	{
		if (!condicion) { std::cerr << "FALLO: " << caso << '\n'; ++fallos; }
	}
	bool casi(const glm::vec3& a, const glm::vec3& b, float eps = 1e-4f) { return glm::length(a - b) < eps; }
	glm::vec3 transforma(const glm::mat4& m, const glm::vec3& v) { return glm::vec3(m * glm::vec4(v, 1.0f)); }
}

int main()
{
	const float PI = 3.14159265358979f;

	// 1) La base es una rotación propia y lleva los ejes de OpenXR a los del motor (Z vertical).
	{
		const glm::mat3 base = EngineBasisFromXr(glm::vec3(0, 1, 0));   // se mira hacia +Y
		comprobar(std::abs(glm::determinant(base) - 1.0f) < 1e-5f, "la base tiene determinante +1 (no refleja)");
		comprobar(casi(base * glm::vec3(1, 0, 0), glm::vec3(1, 0, 0)), "XR +X (derecha) -> motor +X cuando se mira a +Y");
		comprobar(casi(base * glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)), "XR +Y (arriba) -> motor +Z (vertical)");
		comprobar(casi(base * glm::vec3(0, 0, -1), glm::vec3(0, 1, 0)), "XR -Z (delante) -> motor +Y (adelante)");

		const glm::mat3 baseX = EngineBasisFromXr(glm::vec3(1, 0, 0));   // se mira hacia +X: la derecha es -Y
		comprobar(casi(baseX * glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)), "mirando a +X, la derecha es -Y");
		comprobar(casi(baseX * glm::vec3(0, 0, -1), glm::vec3(1, 0, 0)), "mirando a +X, XR -Z -> +X");
	}

	// 2) Anclaje: STAGE en los pies, LOCAL en los ojos; la componente vertical de la cámara se ignora.
	{
		const glm::vec3 pies(3, 4, 0.5f);
		const Anchor suelo = PlayerAnchor(pies, glm::vec3(0, 5, 3), true, 1.7f);
		comprobar(casi(suelo.origen, pies), "STAGE: el origen esta en los pies");
		comprobar(casi(suelo.adelante, glm::vec3(0, 1, 0)), "la direccion vertical se descarta y se normaliza");
		const Anchor local = PlayerAnchor(pies, glm::vec3(1, 0, 0), false, 1.7f);
		comprobar(casi(local.origen, glm::vec3(3, 4, 2.2f)), "LOCAL: el origen esta a la altura de los ojos");
		comprobar(casi(HorizontalForward(glm::vec3(0, 0, 1)), glm::vec3(0, 1, 0)), "camara vertical -> +Y (sin base degenerada)");
		comprobar(casi(HorizontalForward(glm::vec3(0.0f)), glm::vec3(0, 1, 0)), "vector nulo -> +Y");
	}

	// 3) La cabeza real mueve la camara: posicion en el motor = origen + base * posicionXR.
	{
		Anchor a;
		a.origen = glm::vec3(10, 0, 0);
		a.adelante = glm::vec3(0, 1, 0);
		XrPose pose;
		pose.position = glm::vec3(0.2f, 1.6f, -0.5f);   // 20 cm a la derecha, 1,6 m de altura, 50 cm hacia delante
		comprobar(casi(EyePosition(pose, a), glm::vec3(10.2f, 0.5f, 1.6f)), "posicion del ojo en el motor (Z vertical)");

		a.escala = 2.0f;   // un mundo con 2 unidades por metro
		comprobar(casi(EyePosition(pose, a), glm::vec3(10.4f, 1.0f, 3.2f)), "la escala multiplica los desplazamientos de la cabeza");
	}

	// 4) Orientacion: girar la cabeza a la izquierda (+90 grados sobre el eje vertical de OpenXR).
	{
		Anchor a;
		a.adelante = glm::vec3(0, 1, 0);
		XrPose pose;
		pose.orientacion = glm::angleAxis(PI / 2.0f, glm::vec3(0, 1, 0));
		const glm::mat4 mundo = EyeWorld(pose, a);
		const glm::vec3 delanteOjo = glm::vec3(mundo * glm::vec4(0, 0, -1, 0));   // hacia donde mira el ojo
		comprobar(casi(delanteOjo, glm::vec3(-1, 0, 0)), "girar a la izquierda mirando a +Y -> se mira hacia -X");
		comprobar(casi(glm::vec3(mundo * glm::vec4(0, 1, 0, 0)), glm::vec3(0, 0, 1)), "arriba de la cabeza sigue siendo +Z");
	}

	// 5) Vista: lleva el ojo al origen y un punto situado delante queda en -Z de la vista.
	{
		Anchor a;
		a.origen = glm::vec3(5, 5, 0);
		a.adelante = glm::vec3(1, 0, 0);
		XrPose pose;
		pose.position = glm::vec3(0, 1.6f, 0);
		const glm::mat4 vista = VrEyeViewMatrix(pose, a);
		comprobar(casi(transforma(vista, EyePosition(pose, a)), glm::vec3(0.0f)), "el ojo queda en el origen de la vista");
		const glm::vec3 delante = EyePosition(pose, a) + glm::vec3(2, 0, 0);
		comprobar(casi(transforma(vista, delante), glm::vec3(0, 0, -2)), "un punto 2 m delante queda en z = -2");
		const glm::vec3 derecha = EyePosition(pose, a) + glm::vec3(0, -1, 0);   // mirando a +X la derecha es -Y
		comprobar(casi(transforma(vista, derecha), glm::vec3(1, 0, 0)), "un punto a la derecha queda en x = +1");
		const glm::vec3 arriba = EyePosition(pose, a) + glm::vec3(0, 0, 1);
		comprobar(casi(transforma(vista, arriba), glm::vec3(0, 1, 0)), "un punto por encima queda en y = +1");
	}

	// 6) Proyeccion: un FOV simetrico coincide con glm::perspective; uno asimetrico respeta sus bordes.
	{
		const float semi = PI / 4.0f;   // 45 grados a cada lado -> 90 grados totales
		FieldOfView sim{ -semi, semi, semi, -semi };
		const glm::mat4 mia = ProjectionFromFov(sim, 0.1f, 100.0f);
		const glm::mat4 ref = glm::perspective(PI / 2.0f, 1.0f, 0.1f, 100.0f);
		bool igual = true;
		for (int c = 0; c < 4; ++c) for (int f = 0; f < 4; ++f) igual = igual && std::abs(mia[c][f] - ref[c][f]) < 1e-4f;
		comprobar(igual, "FOV simetrico == glm::perspective");

		// Asimetrico: 50 grados a la izquierda, 40 a la derecha, 45 arriba, 35 abajo (como un visor real).
		const float g = PI / 180.0f;
		FieldOfView asi{ -50 * g, 40 * g, 45 * g, -35 * g };
		const glm::mat4 p = ProjectionFromFov(asi, 0.05f, 50.0f);
		auto ndc = [&](float anguloH, float anguloV) {   // punto a 1 m con esos angulos respecto al eje
			const glm::vec4 c = p * glm::vec4(std::tan(anguloH), std::tan(anguloV), -1.0f, 1.0f);
			return glm::vec2(c.x / c.w, c.y / c.w);
		};
		comprobar(std::abs(ndc(-50 * g, 0).x + 1.0f) < 1e-4f, "borde izquierdo del FOV -> NDC x = -1");
		comprobar(std::abs(ndc(40 * g, 0).x - 1.0f) < 1e-4f, "borde derecho del FOV -> NDC x = +1");
		comprobar(std::abs(ndc(0, 45 * g).y - 1.0f) < 1e-4f, "borde superior del FOV -> NDC y = +1");
		comprobar(std::abs(ndc(0, -35 * g).y + 1.0f) < 1e-4f, "borde inferior del FOV -> NDC y = -1");
		const glm::vec4 cerca = p * glm::vec4(0, 0, -0.05f, 1.0f), lejos = p * glm::vec4(0, 0, -50.0f, 1.0f);
		comprobar(std::abs(cerca.z / cerca.w + 1.0f) < 1e-4f && std::abs(lejos.z / lejos.w - 1.0f) < 1e-4f,
			"plano cercano -> -1 y plano lejano -> +1");
	}

	if (fallos == 0) std::cout << "VistaVRTests: todo correcto\n";
	return fallos == 0 ? 0 : 1;
}
