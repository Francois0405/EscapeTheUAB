// MundoFisico.cpp
// Implementación de MundoFisico con Bullet Physics. Ver MundoFisico.h.
#include "MundoFisico.h"

#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>

#include <algorithm>
#include <cmath>

namespace fisica
{
	namespace
	{
		btVector3 ABullet(const glm::vec3& v) { return btVector3(v.x, v.y, v.z); }
		glm::vec3 AGlm(const btVector3& v) { return glm::vec3(v.x(), v.y(), v.z()); }
	}

	// Todo lo que Bullet necesita. El ORDEN de los miembros importa: se destruyen en
	// orden inverso, y el mundo debe morir ANTES que el solver, el broadphase, etc.
	struct MundoFisico::Impl
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
			TipoCuerpo tipo = TipoCuerpo::Estatico;
			std::unique_ptr<btBoxShape> forma;
			std::unique_ptr<btDefaultMotionState> estadoMovimiento;
			std::unique_ptr<btRigidBody> cuerpo;
		};
		std::vector<Cuerpo> cuerpos;

		// Jugador. El "fantasma" (ghost) es un objeto que detecta contactos pero no recibe
		// fuerzas: el controlador de personaje lo mueve él mismo. Necesita que el broadphase
		// avise de sus solapamientos (btGhostPairCallback), si no, no vería nada.
		std::unique_ptr<btCapsuleShapeZ> formaJugador;   // "Z" = cápsula vertical en nuestro mundo
		std::unique_ptr<btPairCachingGhostObject> fantasmaJugador;
		std::unique_ptr<btKinematicCharacterController> controladorJugador;

		// Cuerpo que lleva "en la mano" el jugador (CuerpoInvalido = ninguno) y hacia dónde va.
		IdCuerpo cuerpoSujeto = CuerpoInvalido;
		glm::vec3 destinoSujecion{ 0.0f };

		// Guarda el cuerpo en el primer hueco libre (un cuerpo eliminado deja un hueco) y
		// devuelve su id. Así los ids de los demás cuerpos nunca cambian.
		IdCuerpo GuardarCuerpo(Cuerpo&& nuevo)
		{
			for (std::size_t i = 0; i < cuerpos.size(); ++i)
				if (!cuerpos[i].cuerpo) { cuerpos[i] = std::move(nuevo); return static_cast<IdCuerpo>(i); }
			cuerpos.push_back(std::move(nuevo));
			return static_cast<IdCuerpo>(cuerpos.size()) - 1;
		}

		const btRigidBody* Buscar(IdCuerpo id) const
		{
			if (id < 0 || id >= static_cast<int>(cuerpos.size())) return nullptr;
			return cuerpos[id].cuerpo.get();   // nullptr si ya fue eliminado
		}
	};

	MundoFisico::MundoFisico() : impl(std::make_unique<Impl>())
	{
		// Z es vertical en este motor: la gravedad tira hacia -Z (9,81 m/s^2).
		impl->mundo.setGravity(btVector3(0.0f, 0.0f, -9.81f));
		impl->broadphase.getOverlappingPairCache()->setInternalGhostPairCallback(&impl->llamadaFantasma);
	}

	MundoFisico::~MundoFisico()
	{
		EliminarJugador();
		EliminarTodosLosCuerpos();   // los cuerpos se sacan del mundo antes de destruirlo
	}

	int MundoFisico::Actualizar(float deltaSegundos)
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

	void MundoFisico::ImpulsarCuerpoSujeto()
	{
		btRigidBody* cuerpo = const_cast<btRigidBody*>(impl->Buscar(impl->cuerpoSujeto));
		if (cuerpo == nullptr) return;
		// Velocidad que llevaría el cuerpo hasta la mano en UN paso, recortada al máximo.
		glm::vec3 velocidad = (impl->destinoSujecion - AGlm(cuerpo->getWorldTransform().getOrigin())) / PasoFijo;
		const float rapidez = glm::length(velocidad);
		if (rapidez > VelocidadMaximaDeSujecion) velocidad *= VelocidadMaximaDeSujecion / rapidez;
		cuerpo->activate(true);                              // un cuerpo "dormido" ignoraría la velocidad
		cuerpo->setLinearVelocity(ABullet(velocidad));
		cuerpo->setAngularVelocity(btVector3(0, 0, 0));      // no gira solo mientras se sujeta
	}

	glm::vec3 MundoFisico::Gravedad() const { return AGlm(impl->mundo.getGravity()); }

	IdCuerpo MundoFisico::CrearCaja(TipoCuerpo tipo, const glm::vec3& centro, const glm::quat& rotacion,
		const glm::vec3& mitadTamano, float masaKg)
	{
		// Una caja de lado 0 (o negativo / no finito) no tiene forma: la rechazamos.
		const bool tamanoValido = mitadTamano.x > 0.0f && mitadTamano.y > 0.0f && mitadTamano.z > 0.0f &&
			std::isfinite(mitadTamano.x + mitadTamano.y + mitadTamano.z);
		if (!tamanoValido) return CuerpoInvalido;
		if (tipo == TipoCuerpo::Dinamico && !(masaKg > 0.0f)) return CuerpoInvalido;
		const float masa = (tipo == TipoCuerpo::Dinamico) ? masaKg : 0.0f;   // estático/cinemático: masa 0

		Impl::Cuerpo nuevo;
		nuevo.tipo = tipo;
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
		info.m_friction = 0.8f;
		info.m_restitution = 0.1f;   // casi sin rebote
		nuevo.cuerpo = std::make_unique<btRigidBody>(info);
		if (tipo == TipoCuerpo::Cinematico)
		{
			// Bullet no lo simula: lee su pose del "motion state" que le damos nosotros.
			nuevo.cuerpo->setCollisionFlags(nuevo.cuerpo->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
			nuevo.cuerpo->setActivationState(DISABLE_DEACTIVATION);   // que nunca se "duerma"
		}

		impl->mundo.addRigidBody(nuevo.cuerpo.get());
		return impl->GuardarCuerpo(std::move(nuevo));
	}

	void MundoFisico::MoverCinematico(IdCuerpo id, const glm::vec3& centro, const glm::quat& rotacion)
	{
		btRigidBody* cuerpo = const_cast<btRigidBody*>(impl->Buscar(id));
		if (cuerpo == nullptr || impl->cuerpos[id].tipo != TipoCuerpo::Cinematico) return;
		btTransform t;
		t.setIdentity();
		t.setOrigin(ABullet(centro));
		t.setRotation(btQuaternion(rotacion.x, rotacion.y, rotacion.z, rotacion.w));
		cuerpo->getMotionState()->setWorldTransform(t);   // lo que Bullet lee en el próximo paso
		cuerpo->setWorldTransform(t);                     // y lo que devuelve PoseDe() ya
	}

	void MundoFisico::EliminarCuerpo(IdCuerpo id)
	{
		if (impl->Buscar(id) == nullptr) return;
		if (id == impl->cuerpoSujeto) impl->cuerpoSujeto = CuerpoInvalido;   // no dejar un agarre colgando
		Impl::Cuerpo& c = impl->cuerpos[id];
		impl->mundo.removeRigidBody(c.cuerpo.get());   // primero se saca del mundo...
		c.cuerpo.reset();                              // ...y luego se libera (cuerpo, estado, forma)
		c.estadoMovimiento.reset();
		c.forma.reset();
	}

	IdCuerpo MundoFisico::CrearCajaEstatica(const glm::vec3& centro, const glm::vec3& mitadTamano)
	{
		return CrearCaja(TipoCuerpo::Estatico, centro, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), mitadTamano, 0.0f);
	}

	IdCuerpo MundoFisico::CrearCajaDinamica(const glm::vec3& centro, const glm::vec3& mitadTamano, float masaKg)
	{
		return CrearCaja(TipoCuerpo::Dinamico, centro, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), mitadTamano, masaKg);
	}

	PoseCuerpo MundoFisico::PoseDe(IdCuerpo id) const
	{
		PoseCuerpo pose;
		const btRigidBody* cuerpo = impl->Buscar(id);
		if (cuerpo == nullptr) return pose;

		const btTransform& t = cuerpo->getWorldTransform();
		pose.posicion = AGlm(t.getOrigin());
		const btQuaternion q = t.getRotation();
		pose.rotacion = glm::quat(q.w(), q.x(), q.y(), q.z());
		return pose;
	}

	glm::vec3 MundoFisico::VelocidadDe(IdCuerpo id) const
	{
		const btRigidBody* cuerpo = impl->Buscar(id);
		return cuerpo ? AGlm(cuerpo->getLinearVelocity()) : glm::vec3(0.0f);
	}

	bool MundoFisico::EstaDormido(IdCuerpo id) const
	{
		const btRigidBody* cuerpo = impl->Buscar(id);
		return cuerpo != nullptr && cuerpo->getActivationState() == ISLAND_SLEEPING;
	}

	void MundoFisico::EliminarTodosLosCuerpos()
	{
		for (std::size_t i = 0; i < impl->cuerpos.size(); ++i)
			EliminarCuerpo(static_cast<IdCuerpo>(i));
		impl->cuerpos.clear();
		acumulador = 0.0f;
	}

	int MundoFisico::NumeroDeCuerpos() const
	{
		return static_cast<int>(std::count_if(impl->cuerpos.begin(), impl->cuerpos.end(),
			[](const Impl::Cuerpo& c) { return c.cuerpo != nullptr; }));
	}

	std::vector<CajaColision> MundoFisico::CajasDeColision() const
	{
		std::vector<CajaColision> cajas;
		for (std::size_t i = 0; i < impl->cuerpos.size(); ++i)
		{
			const Impl::Cuerpo& c = impl->cuerpos[i];
			if (!c.cuerpo) continue;
			CajaColision caja;
			caja.tipo = c.tipo;
			caja.pose = PoseDe(static_cast<IdCuerpo>(i));
			caja.mitadTamano = AGlm(c.forma->getHalfExtentsWithMargin());
			caja.dormido = EstaDormido(static_cast<IdCuerpo>(i));
			cajas.push_back(caja);
		}
		return cajas;
	}

	// ---- Jugador -------------------------------------------------------------

	namespace
	{
		// La cápsula mide 'Altura' en total: cilindro + una semiesfera arriba y otra abajo.
		constexpr float AlturaCilindroDePie = MedidasJugador::Altura - 2.0f * MedidasJugador::Radio;
		float SuavizadoHermite(float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		float EscalaVerticalJugador(float altura)
		{
			return (altura - 2.0f * MedidasJugador::Radio) / AlturaCilindroDePie;
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
	}

	bool MundoFisico::CrearJugador(const glm::vec3& posicionPies)
	{
		if (HayJugador()) return false;
		alturaJugador = MedidasJugador::Altura;
		impl->formaJugador = std::make_unique<btCapsuleShapeZ>(MedidasJugador::Radio, AlturaCilindroDePie);

		impl->fantasmaJugador = std::make_unique<btPairCachingGhostObject>();
		btTransform t;
		t.setIdentity();
		t.setOrigin(CentroDesdePies(posicionPies, MedidasJugador::Altura));
		impl->fantasmaJugador->setWorldTransform(t);
		impl->fantasmaJugador->setCollisionShape(impl->formaJugador.get());
		impl->fantasmaJugador->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);

		impl->controladorJugador = std::make_unique<btKinematicCharacterController>(
			impl->fantasmaJugador.get(), impl->formaJugador.get(), MedidasJugador::AlturaEscalon, btVector3(0.0f, 0.0f, 1.0f));
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

	void MundoFisico::EliminarJugador()
	{
		if (!HayJugador()) return;
		impl->mundo.removeAction(impl->controladorJugador.get());
		impl->mundo.removeCollisionObject(impl->fantasmaJugador.get());
		impl->controladorJugador.reset();   // el controlador apunta al fantasma: muere primero
		impl->fantasmaJugador.reset();
		impl->formaJugador.reset();
		alturaJugador = MedidasJugador::Altura;
		progresoAgachado = 0.0f;
	}

	bool MundoFisico::HayJugador() const { return impl->controladorJugador != nullptr; }

	void MundoFisico::FijarVelocidadJugador(const glm::vec3& metrosPorSegundo)
	{
		if (!HayJugador()) return;
		// Bullet no quiere una velocidad sino el DESPLAZAMIENTO de cada paso. Como todos los
		// pasos duran lo mismo (PasoFijo), basta multiplicar: independiente de los FPS.
		const glm::vec3 porPaso(metrosPorSegundo.x * PasoFijo, metrosPorSegundo.y * PasoFijo, 0.0f);
		impl->controladorJugador->setWalkDirection(ABullet(porPaso));
	}

	bool MundoFisico::ActualizarJugadorAgachado(bool agachado, float deltaSegundos)
	{
		if (!HayJugador()) return false;

		// El tiempo avanza de forma lineal (progreso 0..1) y la altura se obtiene con una curva suave
		// (arranca y llega con velocidad 0): el cambio se nota progresivo, sin tirones al empezar/terminar.
		const float objetivo = agachado ? 1.0f : 0.0f;
		if (progresoAgachado == objetivo) return true;

		const float duracion = (MedidasJugador::Altura - MedidasJugador::AlturaAgachado) / MedidasJugador::VelocidadCambioAltura;
		const float avance = std::max(deltaSegundos, 0.0f) / duracion;
		const float progresoNuevo = progresoAgachado + std::copysign(
			std::min(std::abs(objetivo - progresoAgachado), avance), objetivo - progresoAgachado);
		const float alturaNueva = MedidasJugador::Altura -
			(MedidasJugador::Altura - MedidasJugador::AlturaAgachado) * SuavizadoHermite(progresoNuevo);
		if (progresoNuevo == progresoAgachado) return true;   // delta 0 (pausa): no se toca Bullet
		const glm::vec3 pies = PosicionPiesJugador();

		if (alturaNueva > alturaJugador)
		{
			btCapsuleShapeZ formaCandidata(MedidasJugador::Radio,
				alturaNueva - 2.0f * MedidasJugador::Radio);
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

	void MundoFisico::TeletransportarJugador(const glm::vec3& posicionPies)
	{
		if (!HayJugador()) return;
		impl->controladorJugador->warp(CentroDesdePies(posicionPies, alturaJugador));
	}

	glm::vec3 MundoFisico::PosicionPiesJugador() const
	{
		if (!HayJugador()) return glm::vec3(0.0f);
		return AGlm(impl->fantasmaJugador->getWorldTransform().getOrigin())
			- glm::vec3(0.0f, 0.0f, alturaJugador * 0.5f);
	}

	float MundoFisico::AlturaOjosJugador() const
	{
		return alturaJugador - (MedidasJugador::Altura - MedidasJugador::AlturaOjos);
	}

	bool MundoFisico::JugadorEnSuelo() const { return HayJugador() && impl->controladorJugador->onGround(); }

	// ---- Agarre --------------------------------------------------------------

	IdCuerpo MundoFisico::CuerpoAlFinalDelRayo(const glm::vec3& origen, const glm::vec3& direccion, float alcance) const
	{
		if (glm::length(direccion) < 1.0e-6f) return CuerpoInvalido;
		const glm::vec3 fin = origen + glm::normalize(direccion) * alcance;
		btCollisionWorld::ClosestRayResultCallback resultado(ABullet(origen), ABullet(fin));
		// El jugador (grupo "Character") no cuenta: el ojo está dentro de su propia cápsula.
		resultado.m_collisionFilterMask = btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter;
		impl->mundo.rayTest(ABullet(origen), ABullet(fin), resultado);
		if (!resultado.hasHit()) return CuerpoInvalido;

		for (std::size_t i = 0; i < impl->cuerpos.size(); ++i)
			if (impl->cuerpos[i].cuerpo.get() == resultado.m_collisionObject) return static_cast<IdCuerpo>(i);
		return CuerpoInvalido;
	}

	bool MundoFisico::EsDinamico(IdCuerpo id) const
	{
		return impl->Buscar(id) != nullptr && impl->cuerpos[id].tipo == TipoCuerpo::Dinamico;
	}

	bool MundoFisico::SujetarCuerpo(IdCuerpo id)
	{
		if (!EsDinamico(id)) return false;
		SoltarCuerpoSujeto();
		btRigidBody* cuerpo = const_cast<btRigidBody*>(impl->Buscar(id));
		// La gravedad del mundo se desactiva SOLO para este cuerpo: el indicador evita que
		// btDiscreteDynamicsWorld::setGravity se la vuelva a poner.
		cuerpo->setFlags(cuerpo->getFlags() | BT_DISABLE_WORLD_GRAVITY);
		cuerpo->setGravity(btVector3(0, 0, 0));
		cuerpo->activate(true);
		impl->cuerpoSujeto = id;
		impl->destinoSujecion = AGlm(cuerpo->getWorldTransform().getOrigin());   // empieza quieto
		return true;
	}

	void MundoFisico::SoltarCuerpoSujeto()
	{
		LanzarCuerpoSujeto(glm::vec3(0.0f));
	}

	bool MundoFisico::LanzarCuerpoSujeto(const glm::vec3& velocidad)
	{
		btRigidBody* cuerpo = const_cast<btRigidBody*>(impl->Buscar(impl->cuerpoSujeto));
		impl->cuerpoSujeto = CuerpoInvalido;
		if (cuerpo == nullptr) return false;
		cuerpo->setFlags(cuerpo->getFlags() & ~BT_DISABLE_WORLD_GRAVITY);
		cuerpo->setGravity(impl->mundo.getGravity());
		cuerpo->setLinearVelocity(ABullet(velocidad));
		cuerpo->activate(true);
		return true;
	}

	IdCuerpo MundoFisico::CuerpoSujeto() const { return impl->cuerpoSujeto; }
	void MundoFisico::FijarDestinoDeSujecion(const glm::vec3& destino) { impl->destinoSujecion = destino; }
	glm::vec3 MundoFisico::DestinoDeSujecion() const { return impl->destinoSujecion; }

	std::vector<glm::vec3> AristasDeCaja(const CajaColision& caja)
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

	void DemoCuboCaida::Iniciar()
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

	void DemoCuboCaida::Detener()
	{
		mundo.EliminarCuerpo(suelo);
		mundo.EliminarCuerpo(cubo);
		suelo = cubo = CuerpoInvalido;
		activa = false;
	}
}
