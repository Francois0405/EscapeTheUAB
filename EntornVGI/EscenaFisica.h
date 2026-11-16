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

#include "MundoFisico.h"
#include "ObjetosAnimadosGLB.h"
#include "SceneLoader.h"

#include <filesystem>
#include <string>
#include <vector>

namespace fisica
{
	// Matriz de colocación de un objeto OBJ: trasladar * Rz * Ry * Rx * escalar.
	// (Es la misma que usa el dibujo de main.cpp.)
	glm::mat4 MatrizDeObjeto(const SceneObject& objeto);

	// Crea las instancias GLB que pide el JSON. Los modelos se cargan una sola vez por
	// archivo, aunque haya varias puertas, y cada instancia tiene su estado propio.
	// Si algo falla devuelve false; el llamador debe descartar 'destino'.
	bool CargarObjetosAnimadosDeEscena(const std::vector<SceneObjetoAnimado>& animados,
		const std::filesystem::path& carpetaDelJson, glb::EscenaAnimada& destino, std::string& error);

	class EscenaFisica
	{
	public:
		explicit EscenaFisica(MundoFisico& mundoCompartido) : mundo(mundoCompartido) {}
		~EscenaFisica() { Vaciar(); }
		EscenaFisica(const EscenaFisica&) = delete;
		EscenaFisica& operator=(const EscenaFisica&) = delete;

		// Crea los cuerpos de todos los objetos con "physics" y de los colisionadores de los
		// animados. Es ATÓMICO: si algo falla no queda ningún cuerpo a medias y los cuerpos que
		// ya existían (escena anterior) no se tocan. Para cambiar de habitación: crear la nueva
		// en otra EscenaFisica o llamar a Vaciar() antes.
		bool Crear(const std::vector<SceneObject>& objetos, const glb::EscenaAnimada& animados,
			const std::vector<SceneObjetoAnimado>& datosAnimados, std::string& error);

		// Retira del mundo todos los cuerpos de esta escena (y solo los suyos).
		void Vaciar();

		// Coloca los cuerpos cinemáticos donde está AHORA su nodo animado.
		// Llamar cada fotograma DESPUÉS de animar y ANTES de MundoFisico::Actualizar.
		void SincronizarCinematicos(const glb::EscenaAnimada& animados);

		// Matriz para dibujar el objeto 'indice' de la lista pasada a Crear().
		// Estáticos y objetos sin físicas: la de la escena. Dinámicos: la que calcula Bullet.
		glm::mat4 MatrizVisualDe(int indice, const SceneObject& objeto) const;

		// --- Agarrar objetos (acción genérica "interactuar con lo que miro") ------------------------
		// Solo recibe un ORIGEN y una DIRECCIÓN del mundo: en PC son los ojos y la mirada; en VR serán
		// la mano o el mando. Solo se pueden agarrar los objetos "dynamic" de la habitación; paredes,
		// suelos, muebles estáticos y puertas no. Si hay algo (una pared) entre el ojo y el objeto,
		// el rayo se detiene en ello y no se agarra nada.

		static constexpr float AlcanceDeAgarre = 2.0f;       // m: hasta dónde llega el rayo
		static constexpr float DistanciaDeLaMano = 1.2f;     // m: a qué distancia delante queda el objeto
		static constexpr float SoltarSiSeAleja = 2.0f;       // m: si queda atascado más lejos de la mano, se suelta

		// Agarra el objeto dinámico que se mira. Devuelve su índice en la lista de la escena (-1 si no hay).
		int IntentarAgarrar(const glm::vec3& origen, const glm::vec3& direccion);
		// Devuelve el objeto dinámico o animado asociado a un cuerpo físico, o -1 si no corresponde.
		int ObjetoDinamicoDeCuerpo(IdCuerpo cuerpo) const;
		int ObjetoAnimadoDeCuerpo(IdCuerpo cuerpo) const;
		// Sujeta el cuerpo dinámico previamente seleccionado por el raycast.
		bool SujetarObjetoDinamico(int indiceObjeto, const glm::vec3& origen, const glm::vec3& direccion);
		// Coloca la "mano" para este fotograma y suelta el objeto si se ha quedado atascado lejos.
		void ActualizarAgarre(const glm::vec3& origen, const glm::vec3& direccion);
		void SoltarAgarre();
		bool LanzarAgarre(const glm::vec3& velocidad);
		bool HayAgarre() const;
		// Índice (en la lista de la escena) del objeto que se sujeta, o -1.
		int ObjetoAgarrado() const;

		int NumeroDeCuerpos() const { return static_cast<int>(todos.size()); }
		int NumeroDeDinamicos() const { return static_cast<int>(dinamicos.size()); }

	private:
		// Un objeto OBJ dinámico: hay que copiar su pose de Bullet al dibujo.
		struct Dinamico
		{
			int indiceObjeto = -1;
			IdCuerpo cuerpo = CuerpoInvalido;
			glm::vec3 desplazamientoEscalado{ 0.0f };   // escala * offset (en espacio del modelo)
			glm::vec3 escala{ 1.0f };
		};
		// Una caja unida a un nodo de un modelo animado.
		struct Colision
		{
			int indiceAnimado = -1;
			int nodo = -1;
			IdCuerpo cuerpo = CuerpoInvalido;
			TipoCuerpo tipo = TipoCuerpo::Estatico;
			glm::vec3 mitadTamano{ 0.5f };
			glm::vec3 desplazamiento{ 0.0f };
		};

		void ColocarCaja(const Colision& caja, const glb::ObjetoAnimado& objeto, glm::vec3& centro,
			glm::quat& rotacion, glm::vec3& mitad) const;

		MundoFisico& mundo;
		std::vector<IdCuerpo> todos;          // todo lo que hay que quitar en Vaciar()
		std::vector<Dinamico> dinamicos;
		std::vector<Colision> colisionesNodo;
	};
}
