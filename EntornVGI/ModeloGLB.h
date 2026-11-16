// ModeloGLB.h
// -----------------------------------------------------------------------------
// Modelo cargado desde un archivo .glb (glTF 2.0 binario) exportado de Blender.
//
// Este archivo y ModeloGLB.cpp SOLO manejan datos (no usan OpenGL):
//     cargar el .glb  ->  poner la pose de una animación  ->  calcular matrices
// Dibujarlo es trabajo de DibujoGLB.h.
//
// Cómo está organizado un .glb (lo mismo que en Blender):
//   - NODOS: son los objetos de Blender. Cada nodo tiene posición, rotación y escala
//     LOCALES, es decir, relativas a su nodo PADRE. Un nodo puede tener una malla.
//   - JERARQUÍA: la puerta es hija del marco y los pomos son hijos de la puerta.
//     Cuando gira la puerta, los pomos la siguen porque su matriz de mundo es:
//          mundo(hijo) = mundo(padre) * local(hijo)
//   - ANIMACIONES: listas de keyframes que cambian la posición, rotación o escala
//     local de algunos nodos (ver AnimacionGLB.h).
//
// El código NO supone cómo es el árbol: sirve para una puerta, un cajón o lo que
// se exporte, con cualquier número de nodos, en cualquier orden.
// -----------------------------------------------------------------------------
#ifndef MODELO_GLB_H
#define MODELO_GLB_H

#include <filesystem>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "AnimacionGLB.h"

namespace glb
{
	// Un trozo de malla con un único material. Las posiciones están en el espacio
	// LOCAL del nodo, así que el pivote es el origen del objeto en Blender.
	struct PrimitivaGLB
	{
		std::vector<glm::vec3> posiciones;
		std::vector<glm::vec3> normales;
		std::vector<unsigned int> indices;   // de tres en tres: un triángulo
		glm::vec4 colorBase = glm::vec4(1.0f);

		// Identificadores de OpenGL. Los rellena DibujoGLB; 0 significa "aún no subido".
		unsigned int vao = 0;
		unsigned int vbo = 0;
		unsigned int ebo = 0;
	};

	// Una malla de Blender puede tener varias primitivas (una por material).
	struct MallaGLB
	{
		std::string nombre;
		std::vector<PrimitivaGLB> primitivas;
	};

	// Posición, rotación y escala de un nodo respecto a su padre.
	struct TransformacionLocal
	{
		glm::vec3 posicion = glm::vec3(0.0f);
		glm::quat rotacion = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);   // w, x, y, z: sin giro
		glm::vec3 escala = glm::vec3(1.0f);
	};

	struct NodoGLB
	{
		std::string nombre;
		int padre = -1;                 // índice del nodo padre; -1 si es raíz
		int indiceMalla = -1;           // índice en ModeloGLB::mallas; -1 si no tiene malla
		TransformacionLocal reposo;     // pose guardada en el archivo (no cambia nunca)
	};

	// Los datos de ModeloGLB se comparten: se cargan UNA vez y sirven para todas las puertas
	// que usen ese .glb. Lo que cambia en cada puerta es su POSE: la transformación actual
	// de cada nodo (la de la puerta 1 puede estar abierta y la de la puerta 2 cerrada).
	// 'nodos[i]' de la pose corresponde al nodo i del modelo.
	struct PoseGLB
	{
		std::vector<TransformacionLocal> nodos;
	};

	struct ModeloGLB
	{
		std::vector<NodoGLB> nodos;
		std::vector<MallaGLB> mallas;
		std::vector<AnimacionGLB> animaciones;
		std::vector<std::string> avisos;   // cosas del archivo que se ignoraron (texturas, huesos...)
	};

	// Carga un .glb. Si hay cualquier error devuelve false y deja 'modelo' como estaba.
	bool CargarModeloGLB(const std::filesystem::path& ruta, ModeloGLB& modelo, std::string& error);

	// --- Jerarquía y matrices ---------------------------------------------------

	// Matriz local de un nodo: primero escala, luego rotación y por último posición.
	glm::mat4 MatrizLocal(const TransformacionLocal& transformacion);

	// Matriz de mundo de un nodo con una pose dada: sube por los padres multiplicando
	// sus matrices locales.
	glm::mat4 MatrizMundoDelNodo(const ModeloGLB& modelo, const PoseGLB& pose, int indiceNodo);

	// Devuelve el índice del nodo con ese nombre, o -1 si no existe.
	int BuscarNodoPorNombre(const ModeloGLB& modelo, const std::string& nombre);

	// Centro de la caja que envuelve todas las mallas con una pose dada (en el espacio del
	// glTF, Y arriba). Sirve como "punto de la puerta" para saber si el jugador la mira.
	glm::vec3 CalcularCentroDelModelo(const ModeloGLB& modelo, const PoseGLB& pose);

	// glTF usa Y hacia arriba; nuestro juego usa Z hacia arriba (igual que Blender).
	// Esta matriz (un giro de +90 grados en X) convierte (X, Y, Z) en (X, -Z, Y).
	// Se aplica UNA vez al dibujar, no a cada nodo.
	glm::mat4 MatrizYArribaAZArriba();

	// --- Animación --------------------------------------------------------------

	// Crea una pose nueva con los nodos tal y como están guardados en el archivo.
	PoseGLB CrearPoseDeReposo(const ModeloGLB& modelo);

	// Devuelve todos los nodos de la pose a lo que dice el archivo.
	void RestaurarPoseDeReposo(const ModeloGLB& modelo, PoseGLB& pose);

	// Coloca la pose en el instante 'tiempo' de la animación. Solo cambia las propiedades
	// que la animación controla; el resto queda como estaba. No toca el modelo.
	void AplicarAnimacion(PoseGLB& pose, const AnimacionGLB& animacion, float tiempo);
}

#endif
