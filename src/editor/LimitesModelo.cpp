#include "LimitesModelo.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <fstream>
#include <limits>
#include <string>

namespace editor
{
	namespace
	{
		struct Acumulador
		{
			glm::vec3 minimo{ std::numeric_limits<float>::max() };
			glm::vec3 maximo{ std::numeric_limits<float>::lowest() };
			bool hay = false;
			void Anadir(const glm::vec3& p)
			{
				if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return;
				minimo = glm::min(minimo, p);
				maximo = glm::max(maximo, p);
				hay = true;
			}
			CajaLocal Caja() const
			{
				CajaLocal caja;
				if (hay) { caja.minimo = minimo; caja.maximo = maximo; caja.valida = true; }
				return caja;
			}
		};
	}

	CajaLocal LimitesDeOBJ(const std::filesystem::path& ruta)
	{
		std::ifstream archivo(ruta);
		if (!archivo) return {};
		Acumulador acumulador;
		std::string linea;
		while (std::getline(archivo, linea))
		{
			if (linea.size() < 7 || linea[0] != 'v' || (linea[1] != ' ' && linea[1] != '\t')) continue;   // "vt", "vn" no
			std::istringstream numeros(linea.substr(1));
			float x = 0, y = 0, z = 0;
			if (numeros >> x >> y >> z) acumulador.Anadir(glm::vec3(x, y, z));
		}
		return acumulador.Caja();
	}

	CajaLocal LimitesDeGLB(const glb::ModeloGLB& modelo)
	{
		const glb::PoseGLB pose = glb::CrearPoseDeReposo(modelo);
		const glm::mat4 aZArriba = glb::MatrizYArribaAZArriba();
		Acumulador acumulador;
		for (int i = 0; i < static_cast<int>(modelo.nodos.size()); ++i)
		{
			const int indiceMalla = modelo.nodos[static_cast<std::size_t>(i)].indiceMalla;
			if (indiceMalla < 0 || indiceMalla >= static_cast<int>(modelo.mallas.size())) continue;
			const glm::mat4 aMundo = aZArriba * glb::MatrizMundoDelNodo(modelo, pose, i);
			for (const glb::PrimitivaGLB& primitiva : modelo.mallas[static_cast<std::size_t>(indiceMalla)].primitivas)
				for (const glm::vec3& posicion : primitiva.posiciones)
					acumulador.Anadir(glm::vec3(aMundo * glm::vec4(posicion, 1.0f)));
		}
		return acumulador.Caja();
	}

	CajaLocal ConMitadMinima(CajaLocal caja, float mitadMinima)
	{
		if (!caja.valida) return caja;
		const glm::vec3 centro = caja.Centro();
		glm::vec3 mitad = glm::max(caja.MitadTamano(), glm::vec3(mitadMinima));
		caja.minimo = centro - mitad;
		caja.maximo = centro + mitad;
		return caja;
	}

	CajaLocal CajaEnMundo(const glm::mat4& modelo, const CajaLocal& caja)
	{
		Acumulador acumulador;
		for (int esquina = 0; esquina < 8; ++esquina)
		{
			const glm::vec3 p((esquina & 1) ? caja.maximo.x : caja.minimo.x,
				(esquina & 2) ? caja.maximo.y : caja.minimo.y,
				(esquina & 4) ? caja.maximo.z : caja.minimo.z);
			acumulador.Anadir(glm::vec3(modelo * glm::vec4(p, 1.0f)));
		}
		return acumulador.Caja();
	}

	bool InterseccionRayoCaja(const glm::vec3& origen, const glm::vec3& direccion, const glm::mat4& modelo,
		const CajaLocal& caja, float& distancia)
	{
		if (std::abs(glm::determinant(glm::mat3(modelo))) < 1.0e-12f) return false;
		const glm::mat4 inversa = glm::inverse(modelo);
		const glm::vec3 o = glm::vec3(inversa * glm::vec4(origen, 1.0f));
		const glm::vec3 d = glm::vec3(inversa * glm::vec4(direccion, 0.0f));   // t se conserva: la transformación es lineal
		float entrada = 0.0f, salida = std::numeric_limits<float>::max();
		for (int eje = 0; eje < 3; ++eje)
		{
			if (std::abs(d[eje]) < 1.0e-9f)
			{
				if (o[eje] < caja.minimo[eje] || o[eje] > caja.maximo[eje]) return false;
				continue;
			}
			float a = (caja.minimo[eje] - o[eje]) / d[eje];
			float b = (caja.maximo[eje] - o[eje]) / d[eje];
			if (a > b) std::swap(a, b);
			entrada = std::max(entrada, a);
			salida = std::min(salida, b);
			if (entrada > salida) return false;
		}
		distancia = entrada;
		return true;
	}
}
