// MundoFisico.cpp
// Implementación de MundoFisico con Bullet Physics. Ver MundoFisico.h.
#include "src/physics/PhysicsWorld.h"

#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletCollision/CollisionDispatch/btManifoldResult.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace fisica
{
	namespace
	{
		btVector3 ABullet(const glm::vec3& v) { return btVector3(v.x, v.y, v.z); }
		glm::vec3 AGlm(const btVector3& v) { return glm::vec3(v.x(), v.y(), v.z()); }
		constexpr int IndiceMaterialRebotador = 0x484952;

		bool AjustarContactoRebotador(btManifoldPoint& contacto,
			const btCollisionObjectWrapper* objetoA, int, int,
			const btCollisionObjectWrapper* objetoB, int, int)
		{
			if (objetoA->getCollisionObject()->getUserIndex() == IndiceMaterialRebotador ||
				objetoB->getCollisionObject()->getUserIndex() == IndiceMaterialRebotador)
			{
				contacto.m_combinedRestitution = 0.82f;
				contacto.m_combinedFriction = 0.2f;
			}
			return true;
		}
	}

	// Todo lo que Bullet necesita. El ORDEN de los miembros importa: se destruyen en
	// orden inverso, y el mundo debe morir ANTES que el solver, el broadphase, etc.
	struct PhysicsWorld::Impl
	{
		btDefaultCollisionConfiguration configuracion;                    // reglas internas de colisión
		btCollisionDispatcher despachador{ &configuracion };              // decide qué pares chocan
		btDbvtBroadphase broadphase;                                      // descarta pares lejanos rápido
		btSequentialImpulseConstraintSolver solver;                       // resuelve contactos
		btGhostPairCallback llamadaFantasma;                              // debe vivir MÁS que el mundo (ver jugador)
		btDiscreteDynamicsWorld mundo{ &despachador, &broadphase, &solver, &configuracion };

		// Los cuerpos y sus formas los poseemos nosotros (Bullet no libera nada por sí mismo).
		struct Cuerpo
		{
			BodyType tipo = BodyType::Estatico;
			glm::vec3 mitadTamano{ 0.0f };
			std::unique_ptr<btCollisionShape> forma;
			std::unique_ptr<btDefaultMotionState> estadoMovimiento;
			std::unique_ptr<btRigidBody> cuerpo;
		};
		std::vector<Cuerpo> cuerpos;
		glm::vec3 velocidadDeseadaJugador{ 0.0f };

		// Jugador. El "fantasma" (ghost) es un objeto que detecta contactos pero no recibe
		// fuerzas: el controlador de personaje lo mueve él mismo. Necesita que el broadphase
		// avise de sus solapamientos (btGhostPairCallback), si no, no vería nada.
		std::unique_ptr<btCapsuleShapeZ> formaJugador;   // "Z" = cápsula vertical en nuestro mundo
		std::unique_ptr<btPairCachingGhostObject> fantasmaJugador;
		std::unique_ptr<btKinematicCharacterController> controladorJugador;

		// Cuerpo que lleva "en la mano" el jugador (CuerpoInvalido = ninguno) y hacia dónde va.
		IdCuerpo cuerpoSujeto = CuerpoInvalido;
		glm::vec3 destinoSujecion{ 0.0f };
		std::map<IdCuerpo, glm::vec3> destinosSujecion;

		// Guarda el cuerpo en el primer hueco libre (un cuerpo eliminado deja un hueco) y
		// devuelve su id. Así los ids de los demás cuerpos nunca cambian.
		IdCuerpo GuardarCuerpo(Cuerpo&& nuevo)
		{
			for (std::size_t i = 0; i < cuerpos.size(); ++i)
				if (!cuerpos[i].cuerpo)
				{
					nuevo.cuerpo->setUserIndex2(static_cast<int>(i));
					cuerpos[i] = std::move(nuevo);
					return static_cast<IdCuerpo>(i);
				}
			nuevo.cuerpo->setUserIndex2(static_cast<int>(cuerpos.size()));
			cuerpos.push_back(std::move(nuevo));
			return static_cast<IdCuerpo>(cuerpos.size()) - 1;
		}

		const btRigidBody* Buscar(IdCuerpo id) const
		{
			if (id < 0 || id >= static_cast<int>(cuerpos.size())) return nullptr;
			return cuerpos[id].cuerpo.get();   // nullptr si ya fue eliminado
		}
	};

	PhysicsWorld::PhysicsWorld() : impl(std::make_unique<Impl>())
	{
		// Z es vertical en este motor: la gravedad tira hacia -Z (9,81 m/s^2).
		impl->mundo.setGravity(btVector3(0.0f, 0.0f, -9.81f));
		impl->broadphase.getOverlappingPairCache()->setInternalGhostPairCallback(&impl->llamadaFantasma);
		gContactAddedCallback = AjustarContactoRebotador;
	}

	PhysicsWorld::~PhysicsWorld()
	{
		EliminarJugador();
		EliminarTodosLosCuerpos();   // los cuerpos se sacan del mundo antes de destruirlo
	}

	int PhysicsWorld::Actualizar(float deltaSegundos)
	{
		// En pausa el tiempo se descarta: al reanudar no hay "salto" de golpe.
		if (pausa || deltaSegundos <= 0.0f) return 0;

		// Un fotograma enorme se recorta: preferimos que el mundo vaya "a cámara lenta"
		// un instante antes que simular cientos de pasos y bloquear el juego.
		acumulador += std::min(deltaSegundos, TiempoMaximoPorFotograma);

		int pasos = 0;
		while (acumulador >= PasoFijo && pasos < PasosMaximosPorFotograma)
		{
			// maxSubSteps = 0 y fixedTimeStep = PasoFijo => Bullet hace EXACTAMENTE un paso
			// de 1/60 s. El bucle de acumulación lo controlamos nosotros, no Bullet.
			EmpujarRebotadores();
			ImpulsarCuerpoSujeto();   // antes de cada paso: la mano tira del objeto
			impl->mundo.stepSimulation(PasoFijo, 0, PasoFijo);
			acumulador -= PasoFijo;
			++pasos;
		}
		pasosTotales += pasos;
		return pasos;
	}

	// Velocidad máxima con la que la mano arrastra un objeto (m/s). Limitarla evita que, al apretarlo
	// contra una pared, Bullet tenga que deshacer un choque violento: 6 m/s son 10 cm por paso.
	namespace { constexpr float VelocidadMaximaDeSujecion = 6.0f; }

	void PhysicsWorld::ImpulsarCuerpoSujeto()
	{
		for (const auto& sujecion : impl->destinosSujecion)
		{
			btRigidBody* cuerpo = const_cast<btRigidBody*>(impl->Buscar(sujecion.first));
			if (cuerpo == nullptr) continue;
			// Velocidad que llevaría el cuerpo hasta la mano en UN paso, recortada al máximo.
			glm::vec3 velocidad = (sujecion.second - AGlm(cuerpo->getWorldTransform().getOrigin())) / PasoFijo;
			const float rapidez = glm::length(velocidad);
			if (rapidez > VelocidadMaximaDeSujecion) velocidad *= VelocidadMaximaDeSujecion / rapidez;
			cuerpo->activate(true);
			cuerpo->setLinearVelocity(ABullet(velocidad));
			cuerpo->setAngularVelocity(btVector3(0, 0, 0));
		}
	}

	void PhysicsWorld::EmpujarRebotadores()
	{
		if (!impl->fantasmaJugador || !impl->controladorJugador) return;
		glm::vec2 movimiento(impl->velocidadDeseadaJugador.x, impl->velocidadDeseadaJugador.y);
		const float rapidez = glm::length(movimiento);
		if (rapidez < 0.1f) return;
		movimiento /= rapidez;

		const glm::vec3 centroJugador = AGlm(impl->fantasmaJugador->getWorldTransform().getOrigin());
		for (int i = 0; i < impl->fantasmaJugador->getNumOverlappingObjects(); ++i)
		{
			btRigidBody* cuerpo = btRigidBody::upcast(impl->fantasmaJugador->getOverlappingObject(i));
			if (cuerpo == nullptr || cuerpo->getInvMass() <= 0.0f) continue;
			const int id = cuerpo->getUserIndex2();
			if (id < 0 || id >= static_cast<int>(impl->cuerpos.size()) ||
				impl->cuerpos[static_cast<std::size_t>(id)].tipo != BodyType::Rebotador ||
				id == impl->cuerpoSujeto)
				continue;

			glm::vec2 haciaObjeto(cuerpo->getWorldTransform().getOrigin().x() - centroJugador.x,
				cuerpo->getWorldTransform().getOrigin().y() - centroJugador.y);
			const float distancia = glm::length(haciaObjeto);
			if (distancia < 1.0e-4f) continue;
			haciaObjeto /= distancia;
			const float avance = glm::dot(movimiento, haciaObjeto);
			if (avance < 0.2f) continue;

			const float masa = 1.0f / static_cast<float>(cuerpo->getInvMass());
			const float magnitudImpulso = masa * std::min(rapidez, 5.0f) * 0.08f * avance;
			cuerpo->activate(true);
			cuerpo->applyCentralImpulse(ABullet(glm::vec3(movimiento.x, movimiento.y, 0.0f) * magnitudImpulso));

			btVector3 velocidad = cuerpo->getLinearVelocity();
			const float rapidezHorizontal = std::sqrt(velocidad.x() * velocidad.x() + velocidad.y() * velocidad.y());
			if (rapidezHorizontal > 7.0f)
			{
				const float escala = 7.0f / rapidezHorizontal;
				velocidad.setX(velocidad.x() * escala);
				velocidad.setY(velocidad.y() * escala);
				cuerpo->setLinearVelocity(velocidad);
			}
		}
	}

	glm::vec3 PhysicsWorld::Gravedad() const { return AGlm(impl->mundo.getGravity()); }

	IdCuerpo PhysicsWorld::CrearCaja(BodyType tipo, const glm::vec3& centro, const glm::quat& rotacion,
		const glm::vec3& mitadTamano, float masaKg)
	{
		return CrearForma(tipo, centro, rotacion, mitadTamano, masaKg, BodyShape::Caja);
	}

	IdCuerpo PhysicsWorld::CrearEsfera(BodyType tipo, const glm::vec3& centro, float radio, float masaKg)
	{
		return CrearForma(tipo, centro, glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
			glm::vec3(radio), masaKg, BodyShape::Esfera);
	}

	IdCuerpo PhysicsWorld::CrearForma(BodyType tipo, const glm::vec3& centro, const glm::quat& rotacion,
		const glm::vec3& mitadTamano, float masaKg, BodyShape forma)
	{
		const bool tamanoValido = mitadTamano.x > 0.0f && mitadTamano.y > 0.0f && mitadTamano.z > 0.0f &&
			std::isfinite(mitadTamano.x + mitadTamano.y + mitadTamano.z);
		if (!tamanoValido) return CuerpoInvalido;
		if (EsCuerpoDinamico(tipo) && !(masaKg > 0.0f)) return CuerpoInvalido;
		const float masa = EsCuerpoDinamico(tipo) ? masaKg : 0.0f;

		Impl::Cuerpo nuevo;
		nuevo.tipo = tipo;
		nuevo.mitadTamano = mitadTamano;
		if (forma == BodyShape::Esfera)
			nuevo.forma = std::make_unique<btSphereShape>(mitadTamano.x);
		else
			nuevo.forma = std::make_unique<btBoxShape>(ABullet(mitadTamano));

		btTransform transformacion;
		transformacion.setIdentity();
		transformacion.setOrigin(ABullet(centro));
		transformacion.setRotation(btQuaternion(rotacion.x, rotacion.y, rotacion.z, rotacion.w));
		nuevo.estadoMovimiento = std::make_unique<btDefaultMotionState>(transformacion);

		btVector3 inercia(0.0f, 0.0f, 0.0f);
		if (masa > 0.0f) nuevo.forma->calculateLocalInertia(masa, inercia);

		btRigidBody::btRigidBodyConstructionInfo info(masa, nuevo.estadoMovimiento.get(),
			nuevo.forma.get(), inercia);
		info.m_friction = tipo == BodyType::Rebotador ? 0.2f : 0.8f;
		info.m_restitution = tipo == BodyType::Rebotador ? 0.82f : 0.1f;
		nuevo.cuerpo = std::make_unique<btRigidBody>(info);
		if (tipo == BodyType::Rebotador)
		{
			nuevo.cuerpo->setUserIndex(IndiceMaterialRebotador);
			nuevo.cuerpo->setCollisionFlags(nuevo.cuerpo->getCollisionFlags() |
				btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
			nuevo.cuerpo->setRollingFriction(0.02f);
			nuevo.cuerpo->setSpinningFriction(0.02f);
		}
		if (tipo == BodyType::Cinematico)
		{
			// Bullet no lo simula: lee su pose del "motion state" que le damos nosotros.
			nuevo.cuerpo->setCollisionFlags(nuevo.cuerpo->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
			nuevo.cuerpo->setActivationState(DISABLE_DEACTIVATION);   // que nunca se "duerma"
		}

		impl->mundo.addRigidBody(nuevo.cuerpo.get());
		return impl->GuardarCuerpo(std::move(nuevo));
	}

	bool PhysicsWorld::AplicarImpulso(IdCuerpo id, const glm::vec3& impulso)
	{
		btRigidBody* cuerpo = const_cast<btRigidBody*>(impl->Buscar(id));
		if (cuerpo == nullptr || !EsCuerpoDinamico(impl->cuerpos[static_cast<std::size_t>(id)].tipo)) return false;
		cuerpo->activate(true);
		cuerpo->applyCentralImpulse(ABullet(impulso));
		return true;
	}

	bool PhysicsWorld::AplicarAceleracionHorizontal(IdCuerpo id, const glm::vec3& aceleracion, float deltaSegundos)
	{
		btRigidBody* cuerpo = const_cast<btRigidBody*>(impl->Buscar(id));
		if (cuerpo == nullptr || !EsCuerpoDinamico(impl->cuerpos[static_cast<std::size_t>(id)].tipo) ||
			!std::isfinite(aceleracion.x) || !std::isfinite(aceleracion.y) ||
			!std::isfinite(deltaSegundos) || deltaSegundos <= 0.0f)
			return false;

		const btScalar masaInversa = cuerpo->getInvMass();
		if (masaInversa <= btScalar(0.0)) return false;
		const btScalar masa = btScalar(1.0) / masaInversa;
		cuerpo->activate(true);
		cuerpo->applyCentralImpulse(btVector3(aceleracion.x, aceleracion.y, 0.0f) * masa * deltaSegundos);
		return true;
	}

	bool PhysicsWorld::TeletransportarCuerpoDinamico(IdCuerpo id, const glm::vec3& centro)
	{
		btRigidBody* cuerpo = const_cast<btRigidBody*>(impl->Buscar(id));
		if (cuerpo == nullptr || !EsCuerpoDinamico(impl->cuerpos[static_cast<std::size_t>(id)].tipo) ||
			!std::isfinite(centro.x) || !std::isfinite(centro.y) || !std::isfinite(centro.z))
			return false;

		btTransform transformacion = cuerpo->getWorldTransform();
		transformacion.setOrigin(ABullet(centro));
		cuerpo->setWorldTransform(transformacion);
		cuerpo->setInterpolationWorldTransform(transformacion);
		cuerpo->getMotionState()->setWorldTransform(transformacion);
		cuerpo->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));
		cuerpo->setAngularVelocity(btVector3(0.0f, 0.0f, 0.0f));
		cuerpo->clearForces();
		cuerpo->activate(true);
		if (cuerpo->getBroadphaseHandle() != nullptr)
		{
			impl->mundo.getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(
				cuerpo->getBroadphaseHandle(), &impl->despachador);
			impl->mundo.updateSingleAabb(cuerpo);
		}
		return true;
	}

	bool PhysicsWorld::PuedeColocarCuerpoDinamico(IdCuerpo id, const glm::vec3& centro) const
	{
		const btRigidBody* cuerpo = impl->Buscar(id);
		if (cuerpo == nullptr || !EsCuerpoDinamico(impl->cuerpos[static_cast<std::size_t>(id)].tipo) ||
			!std::isfinite(centro.x) || !std::isfinite(centro.y) || !std::isfinite(centro.z))
			return false;

		btCollisionObject consulta;
		consulta.setCollisionShape(const_cast<btCollisionShape*>(cuerpo->getCollisionShape()));
		btTransform transformacion = cuerpo->getWorldTransform();
		transformacion.setOrigin(ABullet(centro));
		consulta.setWorldTransform(transformacion);

		struct ResultadoSolapamiento : btCollisionWorld::ContactResultCallback
		{
			const btCollisionObject* ignorado;
			bool bloqueado = false;

			explicit ResultadoSolapamiento(const btCollisionObject* objetoIgnorado) : ignorado(objetoIgnorado) {}

			bool needsCollision(btBroadphaseProxy* proxy) const override
			{
				return proxy->m_clientObject != ignorado &&
					btCollisionWorld::ContactResultCallback::needsCollision(proxy);
			}

			btScalar addSingleResult(btManifoldPoint& contacto,
				const btCollisionObjectWrapper*, int, int,
				const btCollisionObjectWrapper*, int, int) override
			{
				if (contacto.getDistance() < btScalar(-0.02)) bloqueado = true;
				return btScalar(0.0);
			}
		} resultado(cuerpo);
		impl->mundo.contactTest(&consulta, resultado);
		return !resultado.bloqueado;
	}

	void PhysicsWorld::MoverCinematico(IdCuerpo id, const glm::vec3& centro, const glm::quat& rotacion)
	{
		btRigidBody* cuerpo = const_cast<btRigidBody*>(impl->Buscar(id));
		if (cuerpo == nullptr || impl->cuerpos[id].tipo != BodyType::Cinematico) return;
		btTransform t;
		t.setIdentity();
		t.setOrigin(ABullet(centro));
		t.setRotation(btQuaternion(rotacion.x, rotacion.y, rotacion.z, rotacion.w));
		cuerpo->getMotionState()->setWorldTransform(t);   // lo que Bullet lee en el próximo paso
		cuerpo->setWorldTransform(t);                     // y lo que devuelve PoseDe() ya
	}

	void PhysicsWorld::EliminarCuerpo(IdCuerpo id)
	{
		if (impl->Buscar(id) == nullptr) return;
		if (id == impl->cuerpoSujeto) impl->cuerpoSujeto = CuerpoInvalido;   // no dejar un agarre colgando
		impl->destinosSujecion.erase(id);
		Impl::Cuerpo& c = impl->cuerpos[id];
		impl->mundo.removeRigidBody(c.cuerpo.get());   // primero se saca del mundo...
		c.cuerpo.reset();                              // ...y luego se libera (cuerpo, estado, forma)
		c.estadoMovimiento.reset();
		c.forma.reset();
	}

	IdCuerpo PhysicsWorld::CrearCajaEstatica(const glm::vec3& centro, const glm::vec3& mitadTamano)
	{
		return CrearCaja(BodyType::Estatico, centro, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), mitadTamano, 0.0f);
	}

	IdCuerpo PhysicsWorld::CrearCajaDinamica(const glm::vec3& centro, const glm::vec3& mitadTamano, float masaKg)
	{
		return CrearCaja(BodyType::Dinamico, centro, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), mitadTamano, masaKg);
	}

	BodyPose PhysicsWorld::PoseDe(IdCuerpo id) const
	{
		BodyPose pose;
		const btRigidBody* cuerpo = impl->Buscar(id);
		if (cuerpo == nullptr) return pose;

		const btTransform& t = cuerpo->getWorldTransform();
		pose.posicion = AGlm(t.getOrigin());
		const btQuaternion q = t.getRotation();
		pose.rotacion = glm::quat(q.w(), q.x(), q.y(), q.z());
		return pose;
	}

	glm::vec3 PhysicsWorld::VelocidadDe(IdCuerpo id) const
	{
		const btRigidBody* cuerpo = impl->Buscar(id);
		return cuerpo ? AGlm(cuerpo->getLinearVelocity()) : glm::vec3(0.0f);
	}

	bool PhysicsWorld::HayContacto(IdCuerpo id) const
	{
		const btRigidBody* cuerpo = impl->Buscar(id);
		if (cuerpo == nullptr) return false;

		for (int i = 0; i < impl->despachador.getNumManifolds(); ++i)
		{
			const btPersistentManifold* manifold = impl->despachador.getManifoldByIndexInternal(i);
			if (manifold->getBody0() != cuerpo && manifold->getBody1() != cuerpo) continue;
			for (int j = 0; j < manifold->getNumContacts(); ++j)
				if (manifold->getContactPoint(j).getDistance() <= btScalar(0.0)) return true;
		}
		return false;
	}
	glm::vec3 PhysicsWorld::VelocidadAngularDe(IdCuerpo id) const
	{
		const btRigidBody* cuerpo = impl->Buscar(id);
		return cuerpo ? AGlm(cuerpo->getAngularVelocity()) : glm::vec3(0.0f);
	}

	bool PhysicsWorld::EstaDormido(IdCuerpo id) const
	{
		const btRigidBody* cuerpo = impl->Buscar(id);
		return cuerpo != nullptr && cuerpo->getActivationState() == ISLAND_SLEEPING;
	}

	bool PhysicsWorld::EstaActivo(IdCuerpo id) const
	{
		const btRigidBody* cuerpo = impl->Buscar(id);
		return cuerpo != nullptr && cuerpo->isActive();
	}

	bool PhysicsWorld::FijarPoseCinematica(IdCuerpo id, const glm::vec3& posicion, const glm::quat& rotacion)
	{
		btRigidBody* cuerpo = const_cast<btRigidBody*>(impl->Buscar(id));
		if (!cuerpo || impl->cuerpos[id].tipo != BodyType::Cinematico ||
			!std::isfinite(posicion.x + posicion.y + posicion.z) || !std::isfinite(rotacion.x + rotacion.y + rotacion.z + rotacion.w)) return false;
		btTransform transformacion; transformacion.setIdentity();
		transformacion.setOrigin(ABullet(posicion));
		const glm::quat q = glm::normalize(rotacion);
		transformacion.setRotation(btQuaternion(q.x, q.y, q.z, q.w));
		cuerpo->setWorldTransform(transformacion);
		cuerpo->setInterpolationWorldTransform(transformacion);
		if (cuerpo->getMotionState()) cuerpo->getMotionState()->setWorldTransform(transformacion);
		impl->mundo.updateSingleAabb(cuerpo);
		cuerpo->activate(true);
		return true;
	}

	void PhysicsWorld::EliminarTodosLosCuerpos()
	{
		for (std::size_t i = 0; i < impl->cuerpos.size(); ++i)
			EliminarCuerpo(static_cast<IdCuerpo>(i));
		impl->cuerpos.clear();
		acumulador = 0.0f;
	}

	int PhysicsWorld::NumeroDeCuerpos() const
	{
		return static_cast<int>(std::count_if(impl->cuerpos.begin(), impl->cuerpos.end(),
			[](const Impl::Cuerpo& c) { return c.cuerpo != nullptr; }));
	}

	std::vector<CollisionBox> PhysicsWorld::CajasDeColision() const
	{
		std::vector<CollisionBox> cajas;
		for (std::size_t i = 0; i < impl->cuerpos.size(); ++i)
		{
			const Impl::Cuerpo& c = impl->cuerpos[i];
			if (!c.cuerpo) continue;
			CollisionBox caja;
			caja.tipo = c.tipo;
			caja.pose = PoseDe(static_cast<IdCuerpo>(i));
			caja.mitadTamano = c.mitadTamano;
			caja.dormido = EstaDormido(static_cast<IdCuerpo>(i));
			cajas.push_back(caja);
		}
		return cajas;
	}

	// ---- Jugador -------------------------------------------------------------

	namespace
	{
		// La cápsula mide 'Altura' en total: cilindro + una semiesfera arriba y otra abajo.
		constexpr float AlturaCilindroDePie = PlayerDimensions::Altura - 2.0f * PlayerDimensions::Radio;
		float SuavizadoHermite(float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		float EscalaVerticalJugador(float altura)
		{
			return (altura - 2.0f * PlayerDimensions::Radio) / AlturaCilindroDePie;
		}

		btVector3 CentroDesdePies(const glm::vec3& pies, float altura)
		{
			return ABullet(pies + glm::vec3(0.0f, 0.0f, altura * 0.5f));
		}

		class ComprobarEspacioParaLevantarse : public btCollisionWorld::ContactResultCallback
		{
		public:
			ComprobarEspacioParaLevantarse(const btCollisionObject& jugador, float alturaMinimaContacto)
				: objetoJugador(jugador), alturaMinimaContacto(alturaMinimaContacto)
			{
				m_collisionFilterGroup = btBroadphaseProxy::CharacterFilter;
				m_collisionFilterMask = btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter;
			}

			btScalar addSingleResult(btManifoldPoint& contacto,
				const btCollisionObjectWrapper* objetoA, int, int,
				const btCollisionObjectWrapper* objetoB, int, int) override
			{
				if (contacto.getDistance() >= 0.0f) return 0.0f;
				const btVector3 punto = objetoA->getCollisionObject() == &objetoJugador
					? contacto.getPositionWorldOnA()
					: contacto.getPositionWorldOnB();
				if (punto.z() > alturaMinimaContacto) hayObstaculo = true;
				return 0.0f;
			}

			bool HayObstaculo() const { return hayObstaculo; }

		private:
			const btCollisionObject& objetoJugador;
			float alturaMinimaContacto;
			bool hayObstaculo = false;
		};

		class ComprobarSolapeSpawn : public btCollisionWorld::ContactResultCallback
		{
		public:
			ComprobarSolapeSpawn()
			{
				m_collisionFilterGroup = btBroadphaseProxy::CharacterFilter;
				m_collisionFilterMask = btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter;
			}
			btScalar addSingleResult(btManifoldPoint& contacto,
				const btCollisionObjectWrapper*, int, int, const btCollisionObjectWrapper*, int, int) override
			{
				if (contacto.getDistance() < btScalar(-0.025)) haySolape = true;
				return btScalar(0.0);
			}
			bool HaySolape() const { return haySolape; }
		private:
			bool haySolape = false;
		};
	}

	bool PhysicsWorld::ValidarSpawnJugador(const glm::vec3& posicionPies, glm::vec3& piesAjustados,
		glm::vec3& normalSuelo, std::string& error) const
	{
		if (!std::isfinite(posicionPies.x) || !std::isfinite(posicionPies.y) || !std::isfinite(posicionPies.z))
		{
			error = "La posición inicial del jugador contiene coordenadas no finitas.";
			return false;
		}

		const btVector3 desde = ABullet(posicionPies + glm::vec3(0.0f, 0.0f, 0.25f));
		const btVector3 hasta = ABullet(posicionPies - glm::vec3(0.0f, 0.0f, 0.45f));
		btCollisionWorld::ClosestRayResultCallback suelo(desde, hasta);
		suelo.m_collisionFilterGroup = btBroadphaseProxy::CharacterFilter;
		suelo.m_collisionFilterMask = btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter;
		impl->mundo.rayTest(desde, hasta, suelo);
		if (!suelo.hasHit() || suelo.m_hitNormalWorld.z() < btScalar(0.70))
		{
			error = "No hay una superficie de suelo física horizontal bajo el spawn (radio de comprobación 45 cm).";
			return false;
		}

		const float zSuelo = static_cast<float>(suelo.m_hitPointWorld.z());
		if (std::abs(posicionPies.z - zSuelo) > 0.20f)
		{
			error = "La altura de pies del spawn difiere más de 20 cm del suelo físico.";
			return false;
		}
		piesAjustados = glm::vec3(posicionPies.x, posicionPies.y, zSuelo);
		normalSuelo = glm::normalize(AGlm(suelo.m_hitNormalWorld));

		btCapsuleShapeZ capsula(PlayerDimensions::Radio, AlturaCilindroDePie);
		btCollisionObject prueba;
		prueba.setCollisionShape(&capsula);
		btTransform transformacion;
		transformacion.setIdentity();
		transformacion.setOrigin(CentroDesdePies(piesAjustados + glm::vec3(0.0f, 0.0f, 0.035f), PlayerDimensions::Altura));
		prueba.setWorldTransform(transformacion);
		ComprobarSolapeSpawn contacto;
		impl->mundo.contactTest(&prueba, contacto);
		if (contacto.HaySolape())
		{
			error = "El spawn tiene la cápsula del jugador solapada con una pared, mueble u otro collider.";
			return false;
		}
		error.clear();
		return true;
	}

	bool PhysicsWorld::CrearJugador(const glm::vec3& posicionPies)
	{
		if (HayJugador()) return false;
		impl->velocidadDeseadaJugador = glm::vec3(0.0f);
		alturaJugador = PlayerDimensions::Altura;
		impl->formaJugador = std::make_unique<btCapsuleShapeZ>(PlayerDimensions::Radio, AlturaCilindroDePie);

		impl->fantasmaJugador = std::make_unique<btPairCachingGhostObject>();
		btTransform t;
		t.setIdentity();
		t.setOrigin(CentroDesdePies(posicionPies, PlayerDimensions::Altura));
		impl->fantasmaJugador->setWorldTransform(t);
		impl->fantasmaJugador->setCollisionShape(impl->formaJugador.get());
		impl->fantasmaJugador->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);

		impl->controladorJugador = std::make_unique<btKinematicCharacterController>(
			impl->fantasmaJugador.get(), impl->formaJugador.get(), PlayerDimensions::AlturaEscalon, btVector3(0.0f, 0.0f, 1.0f));
		// Por defecto Bullet supone que "arriba" es Y; aquí es Z. Sin esto la gravedad iría de lado.
		impl->controladorJugador->setUp(btVector3(0.0f, 0.0f, 1.0f));
		impl->controladorJugador->setGravity(impl->mundo.getGravity());
		impl->controladorJugador->setMaxSlope(btRadians(50.0f));   // rampas más empinadas no se suben
		// Bullet solo corrige solapamientos MAYORES que este valor (por defecto 20 cm: una puerta
		// que se cierra atravesaría al jugador). 2 cm: la corrección es gradual (20 % por iteración),
		// así que la puerta lo EMPUJA con suavidad hasta sacarlo, sin saltos.
		impl->controladorJugador->setMaxPenetrationDepth(0.02f);

		// El fantasma colisiona con estáticos/cinemáticos (suelo, paredes, puertas) y dinámicos.
		impl->mundo.addCollisionObject(impl->fantasmaJugador.get(), btBroadphaseProxy::CharacterFilter,
			btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter);
		// "Acción": Bullet llama a controlador->updateAction() en cada paso fijo de la simulación.
		impl->mundo.addAction(impl->controladorJugador.get());
		return true;
	}

	void PhysicsWorld::EliminarJugador()
	{
		if (!HayJugador()) return;
		impl->velocidadDeseadaJugador = glm::vec3(0.0f);
		impl->mundo.removeAction(impl->controladorJugador.get());
		impl->mundo.removeCollisionObject(impl->fantasmaJugador.get());
		impl->controladorJugador.reset();   // el controlador apunta al fantasma: muere primero
		impl->fantasmaJugador.reset();
		impl->formaJugador.reset();
		alturaJugador = PlayerDimensions::Altura;
		progresoAgachado = 0.0f;
	}

	bool PhysicsWorld::HayJugador() const { return impl->controladorJugador != nullptr; }

	void PhysicsWorld::FijarVelocidadJugador(const glm::vec3& metrosPorSegundo)
	{
		impl->velocidadDeseadaJugador = glm::vec3(metrosPorSegundo.x, metrosPorSegundo.y, 0.0f);
		if (!HayJugador()) return;
		// Bullet no quiere una velocidad sino el DESPLAZAMIENTO de cada paso. Como todos los
		// pasos duran lo mismo (PasoFijo), basta multiplicar: independiente de los FPS.
		const glm::vec3 porPaso(metrosPorSegundo.x * PasoFijo, metrosPorSegundo.y * PasoFijo, 0.0f);
		impl->controladorJugador->setWalkDirection(ABullet(porPaso));
	}

	bool PhysicsWorld::ActualizarJugadorAgachado(bool agachado, float deltaSegundos)
	{
		if (!HayJugador()) return false;

		// El tiempo avanza de forma lineal (progreso 0..1) y la altura se obtiene con una curva suave
		// (arranca y llega con velocidad 0): el cambio se nota progresivo, sin tirones al empezar/terminar.
		const float objetivo = agachado ? 1.0f : 0.0f;
		if (progresoAgachado == objetivo) return true;

		const float duracion = (PlayerDimensions::Altura - PlayerDimensions::AlturaAgachado) / PlayerDimensions::VelocidadCambioAltura;
		const float avance = std::max(deltaSegundos, 0.0f) / duracion;
		const float progresoNuevo = progresoAgachado + std::copysign(
			std::min(std::abs(objetivo - progresoAgachado), avance), objetivo - progresoAgachado);
		const float alturaNueva = PlayerDimensions::Altura -
			(PlayerDimensions::Altura - PlayerDimensions::AlturaAgachado) * SuavizadoHermite(progresoNuevo);
		if (progresoNuevo == progresoAgachado) return true;   // delta 0 (pausa): no se toca Bullet
		const glm::vec3 pies = PosicionPiesJugador();

		if (alturaNueva > alturaJugador)
		{
			btCapsuleShapeZ formaCandidata(PlayerDimensions::Radio,
				alturaNueva - 2.0f * PlayerDimensions::Radio);
			btCollisionObject prueba;
			btTransform transformacion;
			transformacion.setIdentity();
			transformacion.setOrigin(CentroDesdePies(pies, alturaNueva));
			prueba.setWorldTransform(transformacion);
			prueba.setCollisionShape(&formaCandidata);

			ComprobarEspacioParaLevantarse comprobar(prueba, pies.z + alturaJugador - 0.01f);
			impl->mundo.contactTest(&prueba, comprobar);
			if (comprobar.HayObstaculo()) return false;   // el techo manda: el progreso no avanza
		}

		// Solo se llega aquí cuando la altura cambia de verdad (no en todos los fotogramas). 'warp' recoloca el centro
		// para que los pies no se muevan al cambiar la altura; no toca la velocidad vertical del controlador.
		impl->formaJugador->setLocalScaling(btVector3(1.0f, 1.0f, EscalaVerticalJugador(alturaNueva)));
		impl->controladorJugador->warp(CentroDesdePies(pies, alturaNueva));
		impl->mundo.updateSingleAabb(impl->fantasmaJugador.get());
		alturaJugador = alturaNueva;
		progresoAgachado = progresoNuevo;
		return true;
	}

	void PhysicsWorld::TeletransportarJugador(const glm::vec3& posicionPies)
	{
		if (!HayJugador()) return;
		impl->velocidadDeseadaJugador = glm::vec3(0.0f);
		impl->controladorJugador->warp(CentroDesdePies(posicionPies, alturaJugador));
	}

	glm::vec3 PhysicsWorld::PosicionPiesJugador() const
	{
		if (!HayJugador()) return glm::vec3(0.0f);
		return AGlm(impl->fantasmaJugador->getWorldTransform().getOrigin())
			- glm::vec3(0.0f, 0.0f, alturaJugador * 0.5f);
	}

	float PhysicsWorld::AlturaOjosJugador() const
	{
		return alturaJugador - (PlayerDimensions::Altura - PlayerDimensions::AlturaOjos);
	}

	bool PhysicsWorld::JugadorEnSuelo() const { return HayJugador() && impl->controladorJugador->onGround(); }

	// ---- Agarre --------------------------------------------------------------

	IdCuerpo PhysicsWorld::CuerpoAlFinalDelRayo(const glm::vec3& origen, const glm::vec3& direccion, float alcance,
		float* distanciaImpacto) const
	{
		if (distanciaImpacto != nullptr) *distanciaImpacto = std::numeric_limits<float>::infinity();
		if (glm::length(direccion) < 1.0e-6f) return CuerpoInvalido;
		const glm::vec3 fin = origen + glm::normalize(direccion) * alcance;
		btCollisionWorld::ClosestRayResultCallback resultado(ABullet(origen), ABullet(fin));
		// El jugador (grupo "Character") no cuenta: el ojo está dentro de su propia cápsula.
		resultado.m_collisionFilterMask = btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter;
		impl->mundo.rayTest(ABullet(origen), ABullet(fin), resultado);
		if (!resultado.hasHit()) return CuerpoInvalido;
		if (distanciaImpacto != nullptr)
			*distanciaImpacto = alcance * static_cast<float>(resultado.m_closestHitFraction);

		for (std::size_t i = 0; i < impl->cuerpos.size(); ++i)
			if (impl->cuerpos[i].cuerpo.get() == resultado.m_collisionObject) return static_cast<IdCuerpo>(i);
		return CuerpoInvalido;
	}

	float PhysicsWorld::DistanciaObstaculoEnRayo(const glm::vec3& origen, const glm::vec3& direccion,
		float alcance, IdCuerpo ignorar) const
	{
		if (!std::isfinite(alcance) || alcance <= 0.0f || glm::length(direccion) < 1.0e-6f)
			return 0.0f;
		const glm::vec3 fin = origen + glm::normalize(direccion) * alcance;
		struct ResultadoRayoSinCuerpo : btCollisionWorld::ClosestRayResultCallback
		{
			const btCollisionObject* ignorado;

			ResultadoRayoSinCuerpo(const btVector3& desde, const btVector3& hasta,
				const btCollisionObject* objetoIgnorado)
				: ClosestRayResultCallback(desde, hasta), ignorado(objetoIgnorado)
			{
				m_collisionFilterMask = btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter;
			}

			bool needsCollision(btBroadphaseProxy* proxy) const override
			{
				return proxy->m_clientObject != ignorado &&
					btCollisionWorld::ClosestRayResultCallback::needsCollision(proxy);
			}
		};

		const btCollisionObject* objetoIgnorado = impl->Buscar(ignorar);
		ResultadoRayoSinCuerpo resultado(ABullet(origen), ABullet(fin), objetoIgnorado);
		impl->mundo.rayTest(ABullet(origen), ABullet(fin), resultado);
		return resultado.hasHit()
			? alcance * static_cast<float>(resultado.m_closestHitFraction)
			: alcance;
	}

	bool PhysicsWorld::EsDinamico(IdCuerpo id) const
	{
		return impl->Buscar(id) != nullptr && EsCuerpoDinamico(impl->cuerpos[id].tipo);
	}

	bool PhysicsWorld::SujetarCuerpo(IdCuerpo id)
	{
		if (!EsDinamico(id)) return false;
		SoltarCuerpoSujeto();
		if (!SujetarCuerpoAutoritativo(id)) return false;
		impl->cuerpoSujeto = id;
		impl->destinoSujecion = AGlm(impl->Buscar(id)->getWorldTransform().getOrigin());
		return true;
	}

	void PhysicsWorld::SoltarCuerpoSujeto()
	{
		LanzarCuerpoSujeto(glm::vec3(0.0f));
	}

	bool PhysicsWorld::LanzarCuerpoSujeto(const glm::vec3& velocidad)
	{
		const IdCuerpo id = impl->cuerpoSujeto;
		impl->cuerpoSujeto = CuerpoInvalido;
		return SoltarCuerpoAutoritativo(id, velocidad);
	}

	bool PhysicsWorld::SujetarCuerpoAutoritativo(IdCuerpo id)
	{
		if (!EsDinamico(id) || EstaSujeto(id)) return false;
		btRigidBody* cuerpo = const_cast<btRigidBody*>(impl->Buscar(id));
		cuerpo->setFlags(cuerpo->getFlags() | BT_DISABLE_WORLD_GRAVITY);
		cuerpo->setGravity(btVector3(0, 0, 0));
		cuerpo->activate(true);
		impl->destinosSujecion[id] = AGlm(cuerpo->getWorldTransform().getOrigin());
		return true;
	}

	void PhysicsWorld::FijarDestinoDeSujecion(IdCuerpo id, const glm::vec3& destino)
	{
		auto it = impl->destinosSujecion.find(id);
		if (it != impl->destinosSujecion.end() && std::isfinite(destino.x + destino.y + destino.z)) it->second = destino;
	}

	bool PhysicsWorld::SoltarCuerpoAutoritativo(IdCuerpo id, const glm::vec3& velocidad)
	{
		btRigidBody* cuerpo = const_cast<btRigidBody*>(impl->Buscar(id));
		if (!cuerpo || impl->destinosSujecion.erase(id) == 0) return false;
		cuerpo->setFlags(cuerpo->getFlags() & ~BT_DISABLE_WORLD_GRAVITY);
		cuerpo->setGravity(impl->mundo.getGravity());
		cuerpo->setLinearVelocity(ABullet(velocidad));
		cuerpo->activate(true);
		return true;
	}

	bool PhysicsWorld::EstaSujeto(IdCuerpo id) const { return impl->destinosSujecion.find(id) != impl->destinosSujecion.end(); }

	IdCuerpo PhysicsWorld::CuerpoSujeto() const { return impl->cuerpoSujeto; }
	void PhysicsWorld::FijarDestinoDeSujecion(const glm::vec3& destino)
	{
		impl->destinoSujecion = destino;
		if (impl->cuerpoSujeto != CuerpoInvalido) FijarDestinoDeSujecion(impl->cuerpoSujeto, destino);
	}
	glm::vec3 PhysicsWorld::DestinoDeSujecion() const { return impl->destinoSujecion; }

	std::vector<glm::vec3> AristasDeCaja(const CollisionBox& caja)
	{
		// Las 8 esquinas: cada bit del índice elige el signo (-/+) en X, Y y Z.
		glm::vec3 esquinas[8];
		for (int i = 0; i < 8; ++i)
		{
			const glm::vec3 signo((i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : -1.0f);
			esquinas[i] = caja.pose.posicion + caja.pose.rotacion * (signo * caja.mitadTamano);
		}
		// Dos esquinas forman una arista si difieren en exactamente un bit.
		std::vector<glm::vec3> puntos;
		for (int i = 0; i < 8; ++i)
			for (int bit = 1; bit <= 4; bit <<= 1)
				if ((i & bit) == 0)
				{
					puntos.push_back(esquinas[i]);
					puntos.push_back(esquinas[i | bit]);
				}
		return puntos;
	}

	// ---- DemoCuboCaida -------------------------------------------------------

	void FallingCubeDemo::Iniciar()
	{
		Detener();   // quita SOLO los cuerpos de la demo anterior (los de la habitación no se tocan)

		// Suelo estático: una caja grande y fina (masa 0). Su cara superior queda en
		// AlturaSuperiorSuelo, igual que el suelo visible de la habitación.
		const glm::vec3 mitadSuelo(8.0f, 5.0f, 0.15f);
		const glm::vec3 centroSuelo(5.0f, 0.0f, AlturaSuperiorSuelo - mitadSuelo.z);
		suelo = mundo.CrearCajaEstatica(centroSuelo, mitadSuelo);

		// Cubo dinámico: empieza en el aire, en una zona libre de la sala.
		const glm::vec3 centroCubo(6.0f, -1.0f, AlturaInicialCentro);
		cubo = mundo.CrearCajaDinamica(centroCubo, glm::vec3(MitadCubo), MasaCuboKg);
		activa = true;
	}

	void FallingCubeDemo::Detener()
	{
		mundo.EliminarCuerpo(suelo);
		mundo.EliminarCuerpo(cubo);
		suelo = cubo = CuerpoInvalido;
		activa = false;
	}
}
