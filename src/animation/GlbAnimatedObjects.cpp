// ObjetosAnimadosGLB.cpp
#include "src/animation/GlbAnimatedObjects.h"
#include <glm/gtc/matrix_transform.hpp>

namespace glb
{
	namespace
	{
		// Devuelve el modelo de ese archivo, cargándolo si aún no está. nullptr si falla.
		const GlbModel* ObtenerModelo(AnimatedScene& escena, const std::string& rutaGlb, std::string& error)
		{
			const auto existente = escena.modelos.find(rutaGlb);
			if (existente != escena.modelos.end()) return &existente->second;

			GlbModel nuevo;
			if (!LoadGlbModel(rutaGlb, nuevo, error)) return nullptr;
			return &escena.modelos.emplace(rutaGlb, std::move(nuevo)).first->second;
		}

		bool TieneAnimacion(const AnimatedObject& objeto)
		{
			return objeto.modelo != nullptr && objeto.animationIndex >= 0 &&
				objeto.animationIndex < static_cast<int>(objeto.modelo->animaciones.size());
		}

		// Cuando el reloj se detiene, "Abriendo" pasa a "Abierta" y "Cerrando" a "Cerrada".
		void PasarAlEstadoFinalSiHaTerminado(AnimatedObject& objeto)
		{
			if (objeto.reproductor.reproduciendo) return;
			if (objeto.estado == DoorState::Opening) objeto.estado = DoorState::Open;
			if (objeto.estado == DoorState::Closing) objeto.estado = DoorState::Closed;
		}

		// Coseno del ángulo entre la dirección de mirada y la dirección hacia un punto.
		// 1 = justo enfrente, 0 = a 90 grados, negativo = a la espalda.
		float CosenoHaciaElPunto(const glm::vec3& origen, const glm::vec3& direccionUnitaria, const glm::vec3& punto)
		{
			const glm::vec3 haciaElPunto = punto - origen;
			const float distancia = glm::length(haciaElPunto);
			if (distancia < 1.0e-6f) return 1.0f;   // el punto está justo en el origen
			return glm::dot(direccionUnitaria, haciaElPunto / distancia);
		}
	}

	bool AddAnimatedObject(AnimatedScene& escena, const std::string& id, const std::string& rutaGlb,
		const glm::vec3& position, float yawDegrees, std::string& error)
	{
		if (FindAnimatedObject(escena, id) != nullptr)
		{
			error = "ya existe un objeto animado con el id '" + id + "'";
			return false;
		}
		const GlbModel* modelo = ObtenerModelo(escena, rutaGlb, error);
		if (modelo == nullptr) return false;

		AnimatedObject objeto;
		objeto.id = id;
		objeto.modelo = modelo;
		objeto.position = position;
		objeto.yawDegrees = yawDegrees;
		objeto.pose = CreateRestPose(*modelo);   // cada instancia empieza con su pose propia
		objeto.centroDelModelo = ComputeModelCenter(*modelo, objeto.pose);
		if (TieneAnimacion(objeto))   // reloj parado en t = 0: la puerta empieza cerrada
			objeto.reproductor.duracion = modelo->animaciones[objeto.animationIndex].duracion;
		escena.objetos.push_back(std::move(objeto));
		return true;
	}

	bool SelectAnimation(AnimatedObject& objeto, const std::string& nombre, std::string& error)
	{
		if (objeto.modelo == nullptr) return false;
		int indice = 0;   // sin nombre: la primera animación
		if (!nombre.empty())
		{
			indice = -1;
			for (std::size_t i = 0; i < objeto.modelo->animaciones.size(); ++i)
				if (objeto.modelo->animaciones[i].nombre == nombre) { indice = static_cast<int>(i); break; }
			if (indice < 0)
			{
				error = "el modelo de '" + objeto.id + "' no tiene la animacion '" + nombre + "'";
				return false;
			}
		}
		objeto.animationIndex = indice;
		if (TieneAnimacion(objeto))   // reloj parado en t = 0, con la duración de la animación elegida
			objeto.reproductor.duracion = objeto.modelo->animaciones[indice].duracion;
		return true;
	}

	AnimatedObject* FindAnimatedObject(AnimatedScene& escena, const std::string& id)
	{
		for (AnimatedObject& objeto : escena.objetos)
			if (objeto.id == id) return &objeto;
		return nullptr;
	}

	glm::mat4 ObjectBaseMatrix(const AnimatedObject& objeto)
	{
		const glm::mat4 colocar = glm::translate(glm::mat4(1.0f), objeto.position);
		const glm::mat4 girar = glm::rotate(colocar, glm::radians(objeto.yawDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
		return glm::scale(girar, glm::vec3(objeto.escala));   // trasladar * girar * escalar
	}

	void BeginAnimation(AnimatedObject& objeto)
	{
		if (!TieneAnimacion(objeto)) return;
		RestoreRestPose(*objeto.modelo, objeto.pose);
		objeto.reproductor.Start(objeto.modelo->animaciones[objeto.animationIndex].duracion);
		objeto.estado = DoorState::Opening;
	}

	void ToggleDoor(AnimatedObject& objeto)
	{
		if (!TieneAnimacion(objeto)) return;
		switch (objeto.estado)
		{
		case DoorState::Closed:
		case DoorState::Closing:    // si se estaba cerrando, vuelve a abrirse desde donde esté
			objeto.estado = DoorState::Opening;
			objeto.reproductor.PlayAnimation(+1.0f);
			break;
		case DoorState::Open:
		case DoorState::Opening:    // si se estaba abriendo, empieza a cerrarse desde donde esté
			objeto.estado = DoorState::Closing;
			objeto.reproductor.PlayAnimation(-1.0f);
			break;
		}
	}

	const char* NombreDelEstado(DoorState estado)
	{
		switch (estado)
		{
		case DoorState::Closed:  return "Cerrada";
		case DoorState::Opening: return "Abriendo";
		case DoorState::Open:  return "Abierta";
		case DoorState::Closing: return "Cerrando";
		}
		return "?";
	}

	void UpdateAnimatedObjects(AnimatedScene& escena, float deltaSegundos)
	{
		for (AnimatedObject& objeto : escena.objetos)
		{
			if (!TieneAnimacion(objeto)) continue;
			objeto.reproductor.Update(deltaSegundos);
			PasarAlEstadoFinalSiHaTerminado(objeto);
			ApplyAnimation(objeto.pose, objeto.modelo->animaciones[objeto.animationIndex], objeto.reproductor.tiempo);
		}
	}

	void ClearAnimatedScene(AnimatedScene& escena)
	{
		escena.objetos.clear();   // primero las instancias, que apuntan a los modelos
		escena.modelos.clear();
	}

	glm::vec3 InteractionPoint(const AnimatedObject& objeto)
	{
		// glTF (Y arriba) -> juego (Z arriba) -> colocación de esta instancia en la habitación.
		const glm::mat4 aMundo = ObjectBaseMatrix(objeto) * YUpToZUpMatrix();
		return glm::vec3(aMundo * glm::vec4(objeto.centroDelModelo, 1.0f));
	}

	AnimatedObject* FindInteractableObject(AnimatedScene& escena, const glm::vec3& origen,
		const glm::vec3& direccion, float radio, float cosenoMinimo)
	{
		if (glm::length(direccion) < 1.0e-6f) return nullptr;
		const glm::vec3 mirada = glm::normalize(direccion);

		AnimatedObject* mejor = nullptr;
		float mejorCoseno = -2.0f;
		for (AnimatedObject& objeto : escena.objetos)
		{
			if (!TieneAnimacion(objeto)) continue;
			const glm::vec3 punto = InteractionPoint(objeto);
			if (glm::distance(origen, punto) > radio) continue;              // demasiado lejos

			const float coseno = CosenoHaciaElPunto(origen, mirada, punto);
			if (coseno < cosenoMinimo) continue;                             // no está delante
			if (coseno > mejorCoseno) { mejor = &objeto; mejorCoseno = coseno; }   // la más centrada gana
		}
		return mejor;
	}

	AnimatedObject* InteractWithObject(AnimatedScene& escena, const glm::vec3& origen, const glm::vec3& direccion)
	{
		AnimatedObject* objeto = FindInteractableObject(escena, origen, direccion);
		if (objeto != nullptr) ToggleDoor(*objeto);
		return objeto;
	}

	bool CanInteract(bool juegoPausado, bool windowFocused, bool escribiendoEnInterfaz)
	{
		return !juegoPausado && windowFocused && !escribiendoEnInterfaz;
	}
}
