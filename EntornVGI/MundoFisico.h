// MundoFisico.h
// -----------------------------------------------------------------------------
// Envoltorio PEQUEÑO de Bullet Physics. No contiene ninguna lógica de OpenGL, así
// que se puede probar sin ventana (ver tests/PhysicsTests.cpp).
//
// Convenciones del motor (comprobadas en las escenas JSON y en docs/):
//   * Eje Z = vertical (arriba).  Por eso la gravedad es (0, 0, -9.81).
//   * Unidad = 1 metro.
//   * Los tipos públicos son de GLM; los de Bullet no salen de MundoFisico.cpp.
//
// Idea general (cómo funciona Bullet):
//   1. Un "mundo" contiene "cuerpos rígidos" (btRigidBody).
//   2. Cada cuerpo tiene una FORMA (caja, esfera...) y una MASA.
//        masa = 0  -> cuerpo estático (suelo, pared): nunca se mueve.
//        masa > 0  -> cuerpo dinámico: lo mueven la gravedad y las colisiones.
//   3. Cada fotograma se llama a Actualizar(deltaTime); el mundo avanza en pasos
//      FIJOS de 1/60 s, sin importar los FPS.
//   4. Para dibujar, se pide la posición/rotación del cuerpo con PoseDe().
// -----------------------------------------------------------------------------
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <memory>
#include <vector>

namespace fisica
{
	// Identificador de un cuerpo creado en el mundo (índice interno; -1 = inválido).
	using IdCuerpo = int;
	constexpr IdCuerpo CuerpoInvalido = -1;

	// Tipo de cuerpo rígido:
	//   Estatico   : masa 0, nunca se mueve (suelos, paredes, muebles, marcos de puerta).
	//   Dinamico   : masa > 0, lo mueven la gravedad y las colisiones.
	//   Cinematico : lo movemos NOSOTROS cada fotograma (p. ej. la hoja de una puerta animada);
	//                empuja a los dinámicos pero nada le empuja a él.
	enum class TipoCuerpo { Estatico, Dinamico, Cinematico };

	// Posición y orientación de un cuerpo en coordenadas del motor.
	struct PoseCuerpo
	{
		glm::vec3 posicion{ 0.0f };
		glm::quat rotacion{ 1.0f, 0.0f, 0.0f, 0.0f };   // w, x, y, z
	};

	// Medidas del jugador (metros). Una cápsula vertical (eje Z): un cilindro con dos medias
	// esferas. Su posición de referencia son los PIES, no el centro: así es lo mismo en PC y VR.
	namespace MedidasJugador
	{
		constexpr float Radio = 0.25f;         // 50 cm de ancho: cabe por el hueco de 80 cm de una puerta (con la hoja abierta quedan ~72 cm)
		constexpr float Altura = 1.80f;        // de los pies a la coronilla
		constexpr float AlturaOjos = 1.70f;    // pies -> ojos (solo para la cámara FPS; en VR manda el visor)
		constexpr float AlturaEscalon = 0.10f; // lo máximo que sube sin saltar (no queremos trepar a cajas)
	}

	// Una caja de colisión tal y como está AHORA en el mundo (para depurar y dibujarla).
	struct CajaColision
	{
		TipoCuerpo tipo = TipoCuerpo::Estatico;
		PoseCuerpo pose;                         // centro y orientación en el mundo
		glm::vec3 mitadTamano{ 0.0f };           // medias dimensiones finales, en metros
		bool dormido = false;
	};

	class MundoFisico
	{
	public:
		// Paso fijo de la simulación: 60 pasos por segundo.
		static constexpr float PasoFijo = 1.0f / 60.0f;
		// Tiempo máximo que aceptamos simular de golpe en un fotograma. Si el juego se
		// congela 2 s (arrastrar la ventana, depurador...) NO simulamos 120 pasos de golpe.
		static constexpr float TiempoMaximoPorFotograma = 0.25f;
		// Seguridad adicional: pasos máximos por llamada (0.25 s / (1/60) = 15).
		static constexpr int PasosMaximosPorFotograma = 15;

		MundoFisico();
		~MundoFisico();
		MundoFisico(const MundoFisico&) = delete;
		MundoFisico& operator=(const MundoFisico&) = delete;

		// Avanza la simulación 'deltaSegundos' de tiempo real. Acumula el tiempo y ejecuta
		// 0, 1 o varios pasos fijos. Con pausa activa no simula ni acumula nada.
		// Devuelve cuántos pasos fijos se han ejecutado.
		int Actualizar(float deltaSegundos);

		void FijarPausa(bool pausado) { pausa = pausado; }
		bool EstaEnPausa() const { return pausa; }

		// Gravedad actual (por defecto (0, 0, -9.81)).
		glm::vec3 Gravedad() const;

		// Crea una caja de cualquier tipo. 'rotacion' es la orientación del mundo y
		// 'mitadTamano' las medias dimensiones FINALES (ya escaladas). La masa solo importa
		// en los dinámicos. Devuelve CuerpoInvalido si los datos no son válidos.
		IdCuerpo CrearCaja(TipoCuerpo tipo, const glm::vec3& centro, const glm::quat& rotacion,
			const glm::vec3& mitadTamano, float masaKg);

		// Mueve un cuerpo cinemático a una pose nueva (llamar antes de Actualizar).
		void MoverCinematico(IdCuerpo id, const glm::vec3& centro, const glm::quat& rotacion);

		// Quita del mundo un cuerpo concreto y libera su memoria (no hace nada si no existe).
		void EliminarCuerpo(IdCuerpo id);

		// Crea una caja estática (masa 0). 'mitadTamano' son las MEDIAS dimensiones
		// (como en test.obj, que mide ±1 antes de escalarse).
		IdCuerpo CrearCajaEstatica(const glm::vec3& centro, const glm::vec3& mitadTamano);

		// Crea una caja dinámica con masa en kg.
		IdCuerpo CrearCajaDinamica(const glm::vec3& centro, const glm::vec3& mitadTamano, float masaKg);

		// Pose actual de un cuerpo (identidad si el id no es válido).
		PoseCuerpo PoseDe(IdCuerpo id) const;

		// Velocidad lineal actual (m/s). Útil para saber si un cuerpo ya está en reposo.
		glm::vec3 VelocidadDe(IdCuerpo id) const;

		// True si Bullet ha "dormido" el cuerpo porque lleva un rato quieto.
		bool EstaDormido(IdCuerpo id) const;

		// Elimina todos los cuerpos pero conserva el mundo (se usa al reiniciar la demo).
		void EliminarTodosLosCuerpos();

		int NumeroDeCuerpos() const;   // cuerpos rígidos (el jugador no cuenta)

		// --- Jugador (Fase 3) --------------------------------------------------------------
		// El jugador es UN cuerpo "de personaje" de Bullet (btKinematicCharacterController):
		// no es un cuerpo rígido normal. Cada paso fijo Bullet intenta mover la cápsula la
		// distancia que le pedimos, la frena contra lo que choque, la DESLIZA a lo largo de
		// paredes (en vez de pararla en seco) y le aplica la gravedad. Solo controla la
		// POSICIÓN: nunca gira ni toca la orientación de la cabeza (importante para VR).

		// Crea la cápsula con los pies en 'posicionPies'. Falso si ya existía.
		bool CrearJugador(const glm::vec3& posicionPies);
		void EliminarJugador();
		bool HayJugador() const;

		// Velocidad HORIZONTAL deseada en m/s (el mundo, ejes X e Y). La Z no se pide: la
		// pone la gravedad. Se mantiene hasta que se vuelva a llamar; (0,0,0) = quieto.
		void FijarVelocidadJugador(const glm::vec3& metrosPorSegundo);

		// Coloca al jugador sin pasar por las paredes (cambio de habitación, depuración).
		void TeletransportarJugador(const glm::vec3& posicionPies);

		glm::vec3 PosicionPiesJugador() const;
		bool JugadorEnSuelo() const;

		// --- Agarrar un objeto (Fase 4a) --------------------------------------------------
		// "Agarrar" NO teletransporta el objeto: sigue siendo el MISMO cuerpo dinámico de Bullet, pero
		// (1) sin gravedad y (2) con una velocidad que en cada paso fijo lo lleva hacia la "mano"
		// (un punto del mundo). Así, si la mano atraviesa una pared, el objeto se queda pegado a ella
		// en vez de cruzarla, y la posición dibujada y la física son siempre la misma.
		// Solo se sujeta un objeto a la vez (una mano). Al soltar, recupera la gravedad.

		// Primer cuerpo (estático, cinemático o dinámico; el jugador no) que toca el rayo dentro de
		// 'alcance' metros, o CuerpoInvalido. Sirve para saber qué se está mirando.
		IdCuerpo CuerpoAlFinalDelRayo(const glm::vec3& origen, const glm::vec3& direccion, float alcance) const;

		bool EsDinamico(IdCuerpo id) const;
		// Empieza a sujetar un cuerpo dinámico (suelta el anterior). Falso si no se puede.
		bool SujetarCuerpo(IdCuerpo id);
		void SoltarCuerpoSujeto();
		IdCuerpo CuerpoSujeto() const;
		// Punto del mundo hacia el que se lleva el cuerpo sujeto (se puede cambiar cada fotograma).
		void FijarDestinoDeSujecion(const glm::vec3& destino);
		glm::vec3 DestinoDeSujecion() const;

		// Todas las cajas existentes, con su pose actual (para el modo "Mostrar colisionadores").
		std::vector<CajaColision> CajasDeColision() const;
		// Pasos fijos ejecutados desde que se creó el mundo (para tests y depuración).
		long long PasosTotales() const { return pasosTotales; }

	private:
		void ImpulsarCuerpoSujeto();   // un "tirón" de la mano antes de cada paso fijo

		// El "pimpl" evita incluir las cabeceras de Bullet en todo el proyecto.
		struct Impl;
		std::unique_ptr<Impl> impl;

		float acumulador = 0.0f;      // tiempo real pendiente de simular
		bool pausa = false;
		long long pasosTotales = 0;
	};

	// Las 12 aristas de una caja como 24 puntos del mundo (pares inicio-fin), para dibujarla
	// con GL_LINES. Aplica la orientación de la caja: no es un cubo alineado con los ejes.
	std::vector<glm::vec3> AristasDeCaja(const CajaColision& caja);

	// -------------------------------------------------------------------------
	// Demostración de la Fase 1: un suelo estático y un cubo dinámico que cae.
	// Vive aparte de la escena JSON: NO toca ninguna de las 12 instancias de
	// habitacion_beta.json; solo coincide con la altura del suelo de esa sala.
	//
	// Para crear OTRO objeto físico se repite el patrón de Iniciar():
	//   id = mundo.CrearCajaDinamica(centro, mitadTamano, masaKg);
	// y cada fotograma se dibuja con mundo.PoseDe(id).
	// -------------------------------------------------------------------------
	class DemoCuboCaida
	{
	public:
		// La demo NO tiene mundo propio: usa el mundo único del juego y solo es dueña de
		// su suelo y su cubo (los quita con Detener(), sin tocar los cuerpos de la habitación).
		explicit DemoCuboCaida(MundoFisico& mundoCompartido) : mundo(mundoCompartido) {}
		~DemoCuboCaida() { Detener(); }
		DemoCuboCaida(const DemoCuboCaida&) = delete;
		DemoCuboCaida& operator=(const DemoCuboCaida&) = delete;

		// Cara superior del suelo de habitacion_beta (centro z=-1.85, semialtura 0.15).
		static constexpr float AlturaSuperiorSuelo = -1.70f;
		static constexpr float MitadCubo = 0.25f;                  // cubo de 0,5 m de lado
		static constexpr float MasaCuboKg = 2.0f;
		static constexpr float AlturaInicialCentro = 1.20f;        // desde donde se suelta

		// (Re)crea suelo y cubo en su posición inicial y vuelve a empezar.
		void Iniciar();
		// Quita suelo y cubo (la demo deja de existir y de dibujarse).
		void Detener();
		bool EstaActiva() const { return activa; }

		PoseCuerpo PoseCubo() const { return mundo.PoseDe(cubo); }
		glm::vec3 VelocidadCubo() const { return mundo.VelocidadDe(cubo); }
		bool CuboEnReposo() const { return mundo.EstaDormido(cubo); }
		glm::vec3 MitadTamanoCubo() const { return glm::vec3(MitadCubo); }

		MundoFisico& Mundo() { return mundo; }

	private:
		MundoFisico& mundo;
		IdCuerpo suelo = CuerpoInvalido;
		IdCuerpo cubo = CuerpoInvalido;
		bool activa = false;
	};
}
