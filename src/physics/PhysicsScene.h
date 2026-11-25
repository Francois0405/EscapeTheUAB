// EscenaFisica.h
// -----------------------------------------------------------------------------
// Puente entre la ESCENA (objetos del JSON) y el MUNDO FÍSICO (Bullet).
//
//   SceneObject / SceneObjetoAnimado  --Crear()-->  cuerpos en MundoFisico
//   MundoFisico (Bullet)              --MatrizVisualDe()-->  matriz para dibujar
//   animación GLB (la hoja de la puerta) --SincronizarCinematicos()--> cuerpo cinemático
//
// No usa OpenGL, GLFW ni la cámara: solo coordenadas del MUNDO. Así lo pueden consultar
// igual el jugador de PC y, más adelante, el de VR.
//
// ESPACIO DE LAS CAJAS ("physics.halfSize" y "physics.offset" del JSON)
//   Se miden en el espacio del MODELO, es decir, con las coordenadas de los vértices del
//   archivo OBJ y ANTES de la escala del objeto. La escala se aplica una sola vez, aquí:
//       R       = rotación del objeto  (Rz * Ry * Rx, la misma que usa el dibujo)
//       centro  = posicion + R * (escala * offset)
//       mitad   = |escala| * halfSize
//       giro    = R
//   Con tools/medir_obj.py se obtienen halfSize y offset de un OBJ.
// -----------------------------------------------------------------------------
#pragma once

#include "src/physics/PhysicsWorld.h"
#include "src/animation/GlbAnimatedObjects.h"
#include "src/scene/SceneLoader.h"

#include <filesystem>
#include <string>
#include <vector>

namespace fisica
{
	// Matriz de colocación de un objeto OBJ: trasladar * Rz * Ry * Rx * escalar.
	// (Es la misma que usa el dibujo de main.cpp.)
	glm::mat4 ObjectMatrix(const SceneObject& objeto);

	// Crea las instancias GLB que pide el JSON. Los modelos se cargan una sola vez por
	// archivo, aunque haya varias puertas, y cada instancia tiene su estado propio.
	// Si algo falla devuelve false; el llamador debe descartar 'destino'.
	bool LoadSceneAnimatedObjects(const std::vector<SceneAnimatedObject>& animados,
		const std::filesystem::path& carpetaDelJson, glb::AnimatedScene& destino, std::string& error);

	class PhysicsScene
	{
	public:
		struct DynamicState
		{
			int objectIndex = -1;
			BodyPose pose;
			glm::vec3 speed{ 0.0f }, angularSpeed{ 0.0f };
			bool activo = false, agarrado = false;
		};
		explicit PhysicsScene(PhysicsWorld& mundoCompartido) : mundo(mundoCompartido) {}
		~PhysicsScene() { ClearBodies(); }
		PhysicsScene(const PhysicsScene&) = delete;
		PhysicsScene& operator=(const PhysicsScene&) = delete;

		// Crea los cuerpos de todos los objetos con "physics" y de los colisionadores de los
		// animados. Es ATÓMICO: si algo falla no queda ningún cuerpo a medias y los cuerpos que
		// ya existían (escena anterior) no se tocan. Para cambiar de habitación: crear la nueva
		// en otra EscenaFisica o llamar a Vaciar() antes.
		bool Create(const std::vector<SceneObject>& objetos, const glb::AnimatedScene& animados,
			const std::vector<SceneAnimatedObject>& datosAnimados, std::string& error, bool physicsAuthority = true);
		bool PushDynamicObject(int objectIndex, const glm::vec3& impulso);
		bool DynamicObjectPosition(int objectIndex, glm::vec3& position) const;
		bool DynamicObjectVelocity(int objectIndex, glm::vec3& speed) const;
		bool ApplyDynamicObjectHorizontalAcceleration(int objectIndex, const glm::vec3& aceleracion,
			float deltaSegundos);
		bool TeleportDynamicObject(int objectIndex, const glm::vec3& centro);
		bool CanPlaceDynamicObject(int objectIndex, const glm::vec3& centro) const;
		float DynamicObjectObstacleDistanceOnRay(int objectIndex, const glm::vec3& origen,
			const glm::vec3& direccion, float alcance) const;
		bool HasDynamicObjectContact(int objectIndex) const;

		// Retira del mundo todos los cuerpos de esta escena (y solo los suyos).
		void ClearBodies();

		// Coloca los cuerpos cinemáticos donde está AHORA su nodo animado.
		// Llamar cada fotograma DESPUÉS de animar y ANTES de MundoFisico::Actualizar.
		void SyncKinematics(const glb::AnimatedScene& animados);

		// Matriz para dibujar el objeto 'indice' de la lista pasada a Crear().
		// Estáticos y objetos sin físicas: la de la escena. Dinámicos: la que calcula Bullet.
		glm::mat4 VisualMatrixOf(int indice, const SceneObject& objeto) const;

		// --- Agarrar objetos (acción genérica "interactuar con lo que miro") ------------------------
		// Solo recibe un ORIGEN y una DIRECCIÓN del mundo: en PC son los ojos y la mirada; en VR serán
		// la mano o el mando. Solo se pueden agarrar los objetos "dynamic" de la habitación; paredes,
		// suelos, muebles estáticos y puertas no. Si hay algo (una pared) entre el ojo y el objeto,
		// el rayo se detiene en ello y no se agarra nada.

		static constexpr float AlcanceDeAgarre = 2.0f;       // m: hasta dónde llega el rayo
		static constexpr float DistanciaDeLaMano = 1.2f;     // m: a qué distancia delante queda el objeto
		static constexpr float SoltarSiSeAleja = 2.0f;       // m: si queda atascado más lejos de la mano, se suelta
		static glm::vec3 GrabDestination(const glm::vec3& origen, const glm::vec3& direccion);

		// Agarra el objeto dinámico que se mira. Devuelve su índice en la lista de la escena (-1 si no hay).
		int TryGrab(const glm::vec3& origen, const glm::vec3& direccion);
		// Devuelve el objeto dinámico o animado asociado a un cuerpo físico, o -1 si no corresponde.
		int DynamicObjectOfBody(IdCuerpo cuerpo) const;
		int AnimatedObjectOfBody(IdCuerpo cuerpo) const;
		// Sujeta el cuerpo dinámico previamente seleccionado por el raycast.
		bool HoldDynamicObject(int objectIndex, const glm::vec3& origen, const glm::vec3& direccion);
		// Coloca la "mano" para este fotograma y suelta el objeto si se ha quedado atascado lejos.
		void UpdateGrab(const glm::vec3& origen, const glm::vec3& direccion);
		void ReleaseGrab();
		bool ThrowGrab(const glm::vec3& speed);
		bool HasGrab() const;
		// Índice (en la lista de la escena) del objeto que se sujeta, o -1.
		int GrabbedObject() const;
		bool HoldRemoteDynamic(int objectIndex, const glm::vec3& origen, const glm::vec3& direccion);
		void UpdateRemoteGrab(int objectIndex, const glm::vec3& destino);
		bool ReleaseRemoteDynamic(int objectIndex, const glm::vec3& speed);
		bool HeldDynamicObject(int objectIndex) const;
		bool ApplyReplicatedState(const DynamicState& estado);
		void CaptureDynamicStates(std::vector<DynamicState>& salida) const;

		int BodyCount() const { return static_cast<int>(todos.size()); }
		int DynamicCount() const { return static_cast<int>(dinamicos.size()); }

	private:
		// Un objeto OBJ dinámico: hay que copiar su pose de Bullet al dibujo.
		struct Dinamico
		{
			int objectIndex = -1;
			IdCuerpo cuerpo = CuerpoInvalido;
			glm::vec3 desplazamientoEscalado{ 0.0f };   // escala * offset (en espacio del modelo)
			glm::vec3 escala{ 1.0f };
			BodyShape forma = BodyShape::Box;
			glm::quat rotacionInicial{ 1.0f, 0.0f, 0.0f, 0.0f };
		};
		// Una caja unida a un nodo de un modelo animado.
		struct Colision
		{
			int indiceAnimado = -1;
			int nodo = -1;
			IdCuerpo cuerpo = CuerpoInvalido;
			BodyType tipo = BodyType::Static;
			glm::vec3 halfExtents{ 0.5f };
			glm::vec3 desplazamiento{ 0.0f };
		};
		
		void PlaceBox(const Colision& caja, const glb::AnimatedObject& objeto, glm::vec3& centro,
			glm::quat& eulerRotation, glm::vec3& mitad) const;

		PhysicsWorld& mundo;
		std::vector<IdCuerpo> todos;          // todo lo que hay que quitar en Vaciar()
		std::vector<Dinamico> dinamicos;
		std::vector<Colision> colisionesNodo;
		bool physicsAuthority = true;
	};
}
