// EscenaFisica.cpp
#include "src/physics/PhysicsScene.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

namespace fisica
{
	namespace
	{
		// Rotación (sin escala) del objeto, con la misma convención que el dibujo.
		glm::mat4 RotacionDeObjeto(const SceneObject& objeto)
		{
			glm::mat4 r(1.0f);
			r = glm::rotate(r, glm::radians(objeto.rotacion.z), glm::vec3(0.0f, 0.0f, 1.0f));
			r = glm::rotate(r, glm::radians(objeto.rotacion.y), glm::vec3(0.0f, 1.0f, 0.0f));
			r = glm::rotate(r, glm::radians(objeto.rotacion.x), glm::vec3(1.0f, 0.0f, 0.0f));
			return r;
		}

		// Quita la escala de una matriz de 3x3 quedándose solo con el giro.
		glm::quat SoloGiro(const glm::mat4& m)
		{
			glm::mat3 base(m);
			for (int columna = 0; columna < 3; ++columna) base[columna] = glm::normalize(base[columna]);
			return glm::quat_cast(base);
		}
	}

	glm::mat4 ObjectMatrix(const SceneObject& objeto)
	{
		const glm::mat4 colocar = glm::translate(glm::mat4(1.0f), objeto.posicion);
		return glm::scale(colocar * RotacionDeObjeto(objeto), objeto.escala);
	}

	bool LoadSceneAnimatedObjects(const std::vector<SceneAnimatedObject>& animados,
		const std::filesystem::path& carpetaDelJson, glb::AnimatedScene& destino, std::string& error)
	{
		for (const SceneAnimatedObject& datos : animados)
		{
			const std::string ruta = (carpetaDelJson / datos.modelo).lexically_normal().string();
			if (!glb::AddAnimatedObject(destino, datos.id, ruta, datos.posicion, datos.giroZGrados, error))
			{
				error = "objeto animado '" + datos.id + "': " + error;
				return false;
			}
			glb::AnimatedObject& instancia = destino.objetos.back();
			instancia.escala = datos.escala;
			if (!glb::SelectAnimation(instancia, datos.animation, error)) return false;
		}
		return true;
	}

	void PhysicsScene::Empty()
	{
		for (IdCuerpo id : todos) mundo.RemoveBody(id);
		todos.clear();
		dinamicos.clear();
		colisionesNodo.clear();
	}

	// Calcula dónde está AHORA la caja de un nodo animado, en coordenadas del mundo.
	//   W = colocación de la instancia * (glTF Y-arriba -> Z-arriba) * matriz de mundo del nodo
	// W puede llevar escala; se separa el giro y la escala se aplica una sola vez a halfSize.
	void PhysicsScene::PlaceBox(const Colision& caja, const glb::AnimatedObject& objeto, glm::vec3& centro,
		glm::quat& rotacion, glm::vec3& mitad) const
	{
		const glm::mat4 mundoNodo = glb::ObjectBaseMatrix(objeto) * glb::YUpToZUpMatrix() *
			glb::NodeWorldMatrix(*objeto.modelo, objeto.pose, caja.nodo);
		centro = glm::vec3(mundoNodo * glm::vec4(caja.desplazamiento, 1.0f));
		rotacion = SoloGiro(mundoNodo);
		mitad = caja.mitadTamano * glm::vec3(glm::length(glm::vec3(mundoNodo[0])),
			glm::length(glm::vec3(mundoNodo[1])), glm::length(glm::vec3(mundoNodo[2])));
	}

	bool PhysicsScene::Create(const std::vector<SceneObject>& objetos, const glb::AnimatedScene& animados,
		const std::vector<SceneAnimatedObject>& datosAnimados, std::string& error, bool autoridadFisica)
	{
		Empty();   // esta EscenaFisica solo gestiona SU escena
		this->autoridadFisica = autoridadFisica;
		auto fallar = [this, &error](const std::string& mensaje)
		{
			error = mensaje;
			Empty();   // deshace lo creado hasta ahora: nada queda a medias
			return false;
		};

		// 1) Objetos OBJ con "physics".
		for (int i = 0; i < static_cast<int>(objetos.size()); ++i)
		{
			const SceneObject& objeto = objetos[i];
			if (!objeto.fisica.presente) continue;   // sin físicas: se queda como antes

			const glm::mat4 giro = RotacionDeObjeto(objeto);
			if (objeto.pared.presente && !objeto.pared.aperturas.empty())
			{
				std::vector<SceneWallSection> secciones;
				std::string errorPared;
				if (!GenerateWallSections(glm::abs(objeto.escala) * objeto.fisica.mitadTamano,
					objeto.pared.aperturas, secciones, errorPared))
					return fallar("pared '" + objeto.id + "': " + errorPared);
				for (const auto& seccion : secciones)
				{
					const glm::vec3 centroSeccion = objeto.posicion + glm::vec3(giro * glm::vec4(seccion.centroLocal, 0.0f));
					const IdCuerpo id = mundo.CreateBox(BodyType::Estatico, centroSeccion,
						glm::quat_cast(glm::mat3(giro)), seccion.mitad, 0.0f);
					if (id == CuerpoInvalido) return fallar("no se pudo crear un segmento fisico de la pared '" + objeto.id + "'");
					todos.push_back(id);
				}
				continue; // sustituye el collider original por los paños sólidos que rodean las aperturas
			}
			const glm::vec3 desplazamiento = objeto.escala * objeto.fisica.desplazamiento;   // escala UNA vez
			const glm::vec3 centro = objeto.posicion + glm::vec3(giro * glm::vec4(desplazamiento, 0.0f));
			const glm::vec3 mitad = glm::abs(objeto.escala) * objeto.fisica.mitadTamano;

			const BodyType tipoFisico = IsDynamicBody(objeto.fisica.tipo) && !autoridadFisica
				? BodyType::Cinematico : objeto.fisica.tipo;
			const float masaFisica = IsDynamicBody(tipoFisico) ? objeto.fisica.masa : 0.0f;
			const IdCuerpo id = objeto.fisica.forma == BodyShape::Esfera
				? mundo.CreateSphere(tipoFisico, centro, std::max({ mitad.x, mitad.y, mitad.z }), masaFisica)
				: mundo.CreateBox(tipoFisico, centro, glm::quat_cast(glm::mat3(giro)), mitad, masaFisica);
			if (id == CuerpoInvalido)
				return fallar("no se pudo crear la caja fisica de '" + objeto.id + "' (tamano o masa no validos)");
			todos.push_back(id);
			if (IsDynamicBody(objeto.fisica.tipo))
				dinamicos.push_back({ i, id, desplazamiento, objeto.escala, objeto.fisica.forma,
					glm::quat_cast(glm::mat3(giro)) });
		}

		// 2) Colisionadores de los objetos animados.
		for (int i = 0; i < static_cast<int>(datosAnimados.size()); ++i)
		{
			const glb::AnimatedObject* instancia = glb::FindAnimatedObject(const_cast<glb::AnimatedScene&>(animados), datosAnimados[i].id);
			if (instancia == nullptr)
				return fallar("el objeto animado '" + datosAnimados[i].id + "' no esta cargado");

			for (const SceneCollider& datos : datosAnimados[i].colisionadores)
			{
				Colision caja;
				caja.indiceAnimado = static_cast<int>(instancia - animados.objetos.data());
				caja.nodo = glb::FindNodeByName(*instancia->modelo, datos.nodo);
				if (caja.nodo < 0)
					return fallar("'" + datosAnimados[i].id + "': el modelo no tiene un nodo llamado '" + datos.nodo + "'");
				caja.tipo = datos.tipo;
				caja.mitadTamano = datos.mitadTamano;
				caja.desplazamiento = datos.desplazamiento;

				glm::vec3 centro, mitad;
				glm::quat rotacion;
				PlaceBox(caja, *instancia, centro, rotacion, mitad);
				caja.cuerpo = mundo.CreateBox(caja.tipo, centro, rotacion, mitad, 0.0f);
				if (caja.cuerpo == CuerpoInvalido)
					return fallar("no se pudo crear el colisionador del nodo '" + datos.nodo + "' de '" + datosAnimados[i].id + "'");
				todos.push_back(caja.cuerpo);
				colisionesNodo.push_back(caja);
			}
		}
		error.clear();
		return true;
	}

	bool PhysicsScene::PushDynamicObject(int indiceObjeto, const glm::vec3& impulso)
	{
		for (const Dinamico& dinamico : dinamicos)
			if (dinamico.indiceObjeto == indiceObjeto)
				return mundo.ApplyImpulseToBody(dinamico.cuerpo, impulso);
		return false;
	}

	bool PhysicsScene::DynamicObjectPosition(int indiceObjeto, glm::vec3& posicion) const
	{
		for (const Dinamico& dinamico : dinamicos)
			if (dinamico.indiceObjeto == indiceObjeto)
			{
				posicion = mundo.PoseOf(dinamico.cuerpo).posicion;
				return true;
			}
		return false;
	}

	bool PhysicsScene::DynamicObjectVelocity(int indiceObjeto, glm::vec3& velocidad) const
	{
		for (const Dinamico& dinamico : dinamicos)
			if (dinamico.indiceObjeto == indiceObjeto)
			{
				velocidad = mundo.VelocityOf(dinamico.cuerpo);
				return true;
			}
		return false;
	}

	bool PhysicsScene::ApplyDynamicObjectHorizontalAcceleration(int indiceObjeto,
		const glm::vec3& aceleracion, float deltaSegundos)
	{
		for (const Dinamico& dinamico : dinamicos)
			if (dinamico.indiceObjeto == indiceObjeto)
				return mundo.ApplyHorizontalAcceleration(dinamico.cuerpo, aceleracion, deltaSegundos);
		return false;
	}

	bool PhysicsScene::TeleportDynamicObject(int indiceObjeto, const glm::vec3& centro)
	{
		for (const Dinamico& dinamico : dinamicos)
			if (dinamico.indiceObjeto == indiceObjeto)
				return mundo.TeleportDynamicBody(dinamico.cuerpo, centro);
		return false;
	}

	bool PhysicsScene::CanPlaceDynamicObject(int indiceObjeto, const glm::vec3& centro) const
	{
		for (const Dinamico& dinamico : dinamicos)
			if (dinamico.indiceObjeto == indiceObjeto)
				return mundo.CanPlaceDynamicBody(dinamico.cuerpo, centro);
		return false;
	}

	float PhysicsScene::DynamicObjectObstacleDistanceOnRay(int indiceObjeto,
		const glm::vec3& origen, const glm::vec3& direccion, float alcance) const
	{
		for (const Dinamico& dinamico : dinamicos)
			if (dinamico.indiceObjeto == indiceObjeto)
				return mundo.ObstacleDistanceOnRay(origen, direccion, alcance, dinamico.cuerpo);
		return 0.0f;
	}

	bool PhysicsScene::HasDynamicObjectContact(int indiceObjeto) const
	{
		for (const Dinamico& dinamico : dinamicos)
			if (dinamico.indiceObjeto == indiceObjeto) return mundo.HasContact(dinamico.cuerpo);
		return false;
	}
	bool PhysicsScene::HoldRemoteDynamic(int indiceObjeto, const glm::vec3& origen, const glm::vec3& direccion)
	{
		for (const Dinamico& d : dinamicos)
		{
			if (d.indiceObjeto != indiceObjeto || !mundo.IsDynamic(d.cuerpo) || mundo.IsHeld(d.cuerpo)) continue;
			if (mundo.BodyAtRayEnd(origen, direccion, AlcanceDeAgarre) != d.cuerpo) return false;
			return mundo.HoldBodyAuthoritative(d.cuerpo);
		}
		return false;
	}

	void PhysicsScene::UpdateRemoteGrab(int indiceObjeto, const glm::vec3& destino)
	{
		for (const Dinamico& d : dinamicos)
			if (d.indiceObjeto == indiceObjeto && mundo.IsHeld(d.cuerpo))
				mundo.SetHoldDestination(d.cuerpo, destino);
	}

	bool PhysicsScene::ReleaseRemoteDynamic(int indiceObjeto, const glm::vec3& velocidad)
	{
		for (const Dinamico& d : dinamicos)
			if (d.indiceObjeto == indiceObjeto) return mundo.ReleaseAuthoritativeBody(d.cuerpo, velocidad);
		return false;
	}

	bool PhysicsScene::HeldDynamicObject(int indiceObjeto) const
	{
		for (const Dinamico& d : dinamicos)
			if (d.indiceObjeto == indiceObjeto) return mundo.IsHeld(d.cuerpo);
		return false;
	}

	bool PhysicsScene::ApplyReplicatedState(const DynamicState& estado)
	{
		for (const Dinamico& d : dinamicos)
			if (d.indiceObjeto == estado.indiceObjeto)
				return mundo.SetKinematicPose(d.cuerpo, estado.pose.posicion, estado.pose.rotacion);
		return false;
	}

	void PhysicsScene::CaptureDynamicStates(std::vector<DynamicState>& salida) const
	{
		salida.clear(); salida.reserve(dinamicos.size());
		for (const Dinamico& d : dinamicos)
		{
			DynamicState e; e.indiceObjeto = d.indiceObjeto; e.pose = mundo.PoseOf(d.cuerpo);
			e.velocidad = mundo.VelocityOf(d.cuerpo); e.velocidadAngular = mundo.AngularVelocityOf(d.cuerpo);
			e.activo = mundo.IsBodyActive(d.cuerpo); e.agarrado = mundo.IsHeld(d.cuerpo);
			salida.push_back(e);
		}
	}

	void PhysicsScene::SyncKinematics(const glb::AnimatedScene& animados)
	{
		for (const Colision& caja : colisionesNodo)
		{
			if (caja.tipo != BodyType::Cinematico) continue;   // los estáticos no se mueven nunca
			glm::vec3 centro, mitad;
			glm::quat rotacion;
			PlaceBox(caja, animados.objetos[caja.indiceAnimado], centro, rotacion, mitad);
			mundo.MoveKinematic(caja.cuerpo, centro, rotacion);
		}
	}

	glm::mat4 PhysicsScene::VisualMatrixOf(int indice, const SceneObject& objeto) const
	{
		for (const Dinamico& d : dinamicos)
		{
			if (d.indiceObjeto != indice) continue;
			// Bullet mueve el centro del colisionador; el modelo tiene su origen en otro punto, así
			// que se deshace el desplazamiento: M = trasladar(centro) * giro * trasladar(-offset) * escalar.
			const BodyPose pose = mundo.PoseOf(d.cuerpo);
			// La rotación de una esfera no cambia su hitbox y no debe desplazar el modelo respecto al offset.
			const glm::quat rotacion = d.forma == BodyShape::Esfera ? d.rotacionInicial : pose.rotacion;
			return glm::translate(glm::mat4(1.0f), pose.posicion) * glm::mat4_cast(rotacion) *
				glm::translate(glm::mat4(1.0f), -d.desplazamientoEscalado) * glm::scale(glm::mat4(1.0f), d.escala);
		}
		return ObjectMatrix(objeto);
	}

	// ---- Agarre ---------------------------------------------------------------

	// Punto común para agarres locales y remotos. Los remotos deben usar el rayo de interacción,
	// no la mano sintética del avatar de escritorio, que está mucho más cerca de la cabeza.
	glm::vec3 PhysicsScene::GrabDestination(const glm::vec3& origen, const glm::vec3& direccion)
	{
		const float longitud = glm::length(direccion);
		if (!std::isfinite(longitud) || longitud < 1.0e-5f ||
			!std::isfinite(origen.x + origen.y + origen.z)) return origen;
		const glm::vec3 mirada = direccion / longitud;
		glm::vec3 horizontal(mirada.x, mirada.y, 0.0f);
		if (glm::length(horizontal) < 1.0e-4f) horizontal = glm::vec3(1.0f, 0.0f, 0.0f);
		horizontal = glm::normalize(horizontal);
		const float subida = std::clamp(mirada.z * DistanciaDeLaMano, -0.4f, 0.6f);
		return origen + horizontal * (DistanciaDeLaMano * 0.9f) + glm::vec3(0.0f, 0.0f, subida);
	}

	int PhysicsScene::TryGrab(const glm::vec3& origen, const glm::vec3& direccion)
	{
		const IdCuerpo mirado = mundo.BodyAtRayEnd(origen, direccion, AlcanceDeAgarre);
		const int indice = DynamicObjectOfBody(mirado);
		if (!HoldDynamicObject(indice, origen, direccion)) return -1;
		return indice;
	}

	int PhysicsScene::DynamicObjectOfBody(IdCuerpo cuerpo) const
	{
		for (const Dinamico& d : dinamicos)
			if (d.cuerpo == cuerpo && cuerpo != CuerpoInvalido) return d.indiceObjeto;
		return -1;
	}

	int PhysicsScene::AnimatedObjectOfBody(IdCuerpo cuerpo) const
	{
		for (const Colision& c : colisionesNodo)
			if (c.cuerpo == cuerpo && cuerpo != CuerpoInvalido) return c.indiceAnimado;
		return -1;
	}

	bool PhysicsScene::HoldDynamicObject(int indiceObjeto, const glm::vec3& origen, const glm::vec3& direccion)
	{
		for (const Dinamico& d : dinamicos)
		{
			if (d.indiceObjeto != indiceObjeto) continue;
			if (!mundo.HoldBody(d.cuerpo)) return false;
			mundo.SetHoldDestination(GrabDestination(origen, direccion));
			return true;
		}
		return false;
	}

	void PhysicsScene::UpdateGrab(const glm::vec3& origen, const glm::vec3& direccion)
	{
		if (!HasGrab()) return;
		mundo.SetHoldDestination(GrabDestination(origen, direccion));
		// Si algo (una pared) frena el objeto y la mano ya está lejos, se suelta: así no queda
		// "tensando" contra el obstáculo ni cruza al otro lado cuando por fin cede.
		if (glm::distance(mundo.PoseOf(mundo.HeldBody()).posicion, mundo.HoldDestination()) > SoltarSiSeAleja)
			ReleaseGrab();
	}

	void PhysicsScene::ReleaseGrab() { mundo.ReleaseHeldBody(); }
	bool PhysicsScene::ThrowGrab(const glm::vec3& velocidad) { return mundo.ThrowHeldBody(velocidad); }
	bool PhysicsScene::HasGrab() const { return GrabbedObject() >= 0; }

	int PhysicsScene::GrabbedObject() const
	{
		const IdCuerpo sujeto = mundo.HeldBody();
		for (const Dinamico& d : dinamicos)
			if (d.cuerpo == sujeto && sujeto != CuerpoInvalido) return d.indiceObjeto;
		return -1;
	}
}
