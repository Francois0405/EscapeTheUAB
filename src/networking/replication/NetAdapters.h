// NetAdaptadores.h
// Adaptador de las puertas GLB del motor (glb::EscenaAnimada) a la interfaz de red IPuertasEscena.
// Solo usa ObjetosAnimadosGLB.h (sin OpenGL): el ID de red de cada puerta es el hash de su id de escena.
#pragma once

#include "src/networking/replication/NetGameLogic.h"
#include "src/animation/GlbAnimatedObjects.h"

namespace netgame
{
	class GlbDoors final : public ISceneDoors
	{
	public:
		explicit GlbDoors(glb::AnimatedScene& escena) : escena_(escena) {}
		size_t Count() const override { return escena_.objetos.size(); }
		std::string Id(size_t i) const override { return i < escena_.objetos.size() ? escena_.objetos[i].id : std::string(); }
		bool IsOpen(size_t i) const override
		{
			if (i >= escena_.objetos.size()) return false;
			const auto e = escena_.objetos[i].estado;
			return e == glb::DoorState::Open || e == glb::DoorState::Opening;
		}
		void SetOpen(size_t i, bool abrir) override
		{
			if (i < escena_.objetos.size() && IsOpen(i) != abrir) glb::ToggleDoor(escena_.objetos[i]);
		}
		glm::vec3 Point(size_t i) const override
		{
			return i < escena_.objetos.size() ? glb::InteractionPoint(escena_.objetos[i]) : glm::vec3(0.0f);
		}
		// Posicion de una puerta en la lista del motor a partir del puntero que devuelve la busqueda de interaccion.
		bool IndexOf(const glb::AnimatedObject* o, size_t& idx) const
		{
			if (!o || escena_.objetos.empty() || o < &escena_.objetos.front() || o > &escena_.objetos.back()) return false;
			idx = static_cast<size_t>(o - &escena_.objetos.front()); return true;
		}
	private:
		glb::AnimatedScene& escena_;
	};
}
