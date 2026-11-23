// NetAdaptadores.h
// Adaptador de las puertas GLB del motor (glb::EscenaAnimada) a la interfaz de red IPuertasEscena.
// Solo usa ObjetosAnimadosGLB.h (sin OpenGL): el ID de red de cada puerta es el hash de su id de escena.
#pragma once

#include "NetJuegoLogica.h"
#include "ObjetosAnimadosGLB.h"

namespace netjuego
{
	class PuertasGLB final : public IPuertasEscena
	{
	public:
		explicit PuertasGLB(glb::EscenaAnimada& escena) : escena_(escena) {}
		size_t Cantidad() const override { return escena_.objetos.size(); }
		std::string Id(size_t i) const override { return i < escena_.objetos.size() ? escena_.objetos[i].id : std::string(); }
		bool Abierta(size_t i) const override
		{
			if (i >= escena_.objetos.size()) return false;
			const auto e = escena_.objetos[i].estado;
			return e == glb::EstadoPuerta::Abierta || e == glb::EstadoPuerta::Abriendo;
		}
		void FijarAbierta(size_t i, bool abrir) override
		{
			if (i < escena_.objetos.size() && Abierta(i) != abrir) glb::AlternarPuerta(escena_.objetos[i]);
		}
		glm::vec3 Punto(size_t i) const override
		{
			return i < escena_.objetos.size() ? glb::PuntoDeInteraccion(escena_.objetos[i]) : glm::vec3(0.0f);
		}
		// Posicion de una puerta en la lista del motor a partir del puntero que devuelve la busqueda de interaccion.
		bool IndiceDe(const glb::ObjetoAnimado* o, size_t& idx) const
		{
			if (!o || escena_.objetos.empty() || o < &escena_.objetos.front() || o > &escena_.objetos.back()) return false;
			idx = static_cast<size_t>(o - &escena_.objetos.front()); return true;
		}
	private:
		glb::EscenaAnimada& escena_;
	};
}
