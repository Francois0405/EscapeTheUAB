// ObjetosAnimadosGLB.h
// -----------------------------------------------------------------------------
// Lista sencilla de "objetos animados": puertas, cajones... colocados en la habitación.
//
// Idea clave: MODELO e INSTANCIA son cosas distintas.
//   - ModeloGLB   : lo que trae el archivo .glb (mallas, nodos, animaciones). Se carga UNA vez
//                   por archivo, aunque haya diez puertas.
//   - Instancia   : una puerta concreta. Guarda dónde está, su propia pose y su propio
//                   reproductor. Así la puerta 1 puede estar abierta y la 2 cerrada.
//
// Para añadir otra puerta NO hay que escribir código nuevo: basta con una línea
// AnadirObjeto(...) con el .glb, la posición y el giro.
//
// Este archivo no usa OpenGL ni la cámara: solo posiciones en el mundo. Así sirve igual
// para jugar con teclado y ratón que con un casco VR.
// -----------------------------------------------------------------------------
#ifndef OBJETOS_ANIMADOS_GLB_H
#define OBJETOS_ANIMADOS_GLB_H

#include <map>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "ModeloGLB.h"

namespace glb
{
	// Estado de una puerta (o cajón...). Convención: el PRIMER fotograma de la animación del
	// archivo es la puerta CERRADA y el ÚLTIMO es la puerta ABIERTA.
	//   Cerrada  --E-->  Abriendo  --(llega al final)-->    Abierta
	//   Abierta  --E-->  Cerrando  --(llega al principio)--> Cerrada
	// Si se pulsa E mientras se mueve, el sentido se invierte desde donde esté:
	//   Abriendo --E--> Cerrando      Cerrando --E--> Abriendo
	enum class EstadoPuerta { Cerrada, Abriendo, Abierta, Cerrando };

	// Una puerta (o cajón...) concreta dentro de la habitación.
	struct ObjetoAnimado
	{
		std::string id;                       // nombre único, por ejemplo "puerta_entrada"
		const ModeloGLB* modelo = nullptr;    // compartido con las demás instancias del mismo .glb
		glm::vec3 posicion = glm::vec3(0.0f); // dónde está el origen del modelo en el mundo del juego
		float giroZGrados = 0.0f;             // orientación alrededor del eje vertical (Z)
		float escala = 1.0f;                  // escala uniforme de la instancia (1 = tamaño del archivo)
		PoseGLB pose;                         // pose propia: la de ESTA puerta
		ReproductorAnimacion reproductor;     // reloj propio: el de ESTA puerta
		int indiceAnimacion = 0;              // qué animación del archivo reproduce
		EstadoPuerta estado = EstadoPuerta::Cerrada;
		glm::vec3 centroDelModelo = glm::vec3(0.0f);   // punto "de la puerta" (espacio glTF) para saber si se mira
	};

	struct EscenaAnimada
	{
		// std::map no mueve sus elementos al insertar: los punteros 'modelo' de las
		// instancias siguen siendo válidos. La clave es la ruta del archivo.
		std::map<std::string, ModeloGLB> modelos;
		std::vector<ObjetoAnimado> objetos;
	};

	// Añade una instancia. Carga el .glb solo si todavía no estaba cargado.
	// Devuelve false (y no cambia nada) si el archivo falla o el id ya existe.
	bool AnadirObjeto(EscenaAnimada& escena, const std::string& id, const std::string& rutaGlb,
		const glm::vec3& posicion, float giroZGrados, std::string& error);

	// Elige qué animación del archivo reproduce la instancia, por su nombre ("" = la primera).
	// Devuelve false (y no cambia nada) si el archivo no tiene una animación con ese nombre.
	bool SeleccionarAnimacion(ObjetoAnimado& objeto, const std::string& nombre, std::string& error);

	// Busca una instancia por su id. Devuelve nullptr si no existe.
	ObjetoAnimado* BuscarObjeto(EscenaAnimada& escena, const std::string& id);

	// Dónde está colocado el modelo en el mundo (ya en Z arriba). El cambio de ejes de glTF
	// lo añade el dibujo.
	glm::mat4 MatrizBaseDelObjeto(const ObjetoAnimado& objeto);

	// Herramienta de depuración: vuelve a empezar la apertura desde la puerta cerrada.
	void IniciarAnimacion(ObjetoAnimado& objeto);

	// Lo que hace la tecla E sobre UNA puerta: abre si está cerrada, cierra si está abierta
	// e invierte el sentido si se está moviendo (sin saltos: continúa desde el instante actual).
	void AlternarPuerta(ObjetoAnimado& objeto);

	// Nombre del estado para mostrarlo en la interfaz de depuración.
	const char* NombreDelEstado(EstadoPuerta estado);

	// --- Interacción -------------------------------------------------------------
	// Estas funciones reciben una POSICIÓN y una DIRECCIÓN del mundo, no una cámara.
	// En el juego con teclado y ratón son la posición y dirección de la vista; con VR serán
	// las de la mano o el mando. No saben nada de FPS.

	// Radio máximo para poder abrir una puerta (metros) y ángulo máximo respecto a la
	// dirección de mirada. 0.7 es el coseno de unos 45 grados.
	constexpr float RADIO_INTERACCION = 2.0f;
	constexpr float COSENO_MINIMO_INTERACCION = 0.7f;

	// Punto de la puerta en el mundo del juego (Z arriba): su centro, que no cambia al abrirse.
	glm::vec3 PuntoDeInteraccion(const ObjetoAnimado& objeto);

	// Devuelve la puerta con la que se puede interactuar, o nullptr si no hay ninguna.
	// Candidatas: a menos de 'radio' metros y delante (dentro del cono de 'cosenoMinimo').
	// Si hay varias, gana la que está más centrada en la dirección de mirada.
	ObjetoAnimado* BuscarObjetoInteractuable(EscenaAnimada& escena, const glm::vec3& origen,
		const glm::vec3& direccion, float radio = RADIO_INTERACCION, float cosenoMinimo = COSENO_MINIMO_INTERACCION);

	// Busca la puerta y, si hay una, la alterna (abrir/cerrar). Devuelve la puerta usada o nullptr.
	ObjetoAnimado* InteractuarConObjeto(EscenaAnimada& escena, const glm::vec3& origen, const glm::vec3& direccion);

	// ¿Debe tenerse en cuenta la tecla E ahora? No si el juego está en pausa, la ventana no
	// tiene el foco o se está escribiendo en un cuadro de texto de ImGui.
	bool PuedeInteractuar(bool juegoPausado, bool ventanaConFoco, bool escribiendoEnInterfaz);

	// Avanza el reloj de todas las instancias y coloca cada una en su pose.
	// 'delta' es el tiempo transcurrido; pasa 0 para pausar.
	void ActualizarObjetos(EscenaAnimada& escena, float deltaSegundos);

	// Elimina todo (los modelos y las instancias).
	void VaciarEscenaAnimada(EscenaAnimada& escena);
}

#endif
