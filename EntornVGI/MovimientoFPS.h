// MovimientoFPS.h
// -----------------------------------------------------------------------------
// Velocidad, sprint, estamina y balanceo visual de la cámara FPS. Solo usa la biblioteca estándar:
// ni Bullet, ni OpenGL, ni GLFW, así que se prueba sin ventana (tests/MovimientoFPSTests.cpp).
//
// REPARTO DE TRABAJO
//   * Este módulo decide CUÁNTO corre el jugador (velocidad deseada) y cuánto se balancea la cámara.
//   * La velocidad se entrega a Bullet con MundoFisico::FijarVelocidadJugador: el módulo nunca mueve nada.
//   * El balanceo es SOLO visual (un desplazamiento de la matriz de vista en pantalla). No toca la cápsula
//     ni la dirección de colisiones/interacción, y NO se aplica en VR (ahí manda la cabeza del jugador).
// -----------------------------------------------------------------------------
#pragma once

#include <algorithm>
#include <cmath>

namespace fps
{
	// Un fotograma muy largo (arrastrar la ventana, un parón) no debe disparar la simulación.
	constexpr float DeltaMaximo = 0.05f;
	inline float LimitarDelta(float delta) { return std::clamp(delta, 0.0f, DeltaMaximo); }

	// Aproximación exponencial hacia un objetivo, independiente de los FPS: tras t segundos queda
	// exp(-tasa*t) de la distancia, se ejecute en un paso o en muchos.
	inline float Acercar(float actual, float objetivo, float tasaPorSegundo, float delta)
	{
		return actual + (objetivo - actual) * (1.0f - std::exp(-tasaPorSegundo * std::max(delta, 0.0f)));
	}

	// 0 -> 1 con arranque y llegada suaves (derivada 0 en los extremos).
	inline float SuavizadoHermite(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	// -------------------------------------------------------------------------
	// Velocidades, sprint y estamina
	// -------------------------------------------------------------------------
	struct ParametrosMovimiento
	{
		float velocidadCaminar = 3.0f;    // m/s
		float velocidadAgachado = 1.4f;   // m/s
		float velocidadSprint = 5.2f;     // m/s
		float suavizadoVelocidad = 7.0f;  // 1/s: cambios caminar <-> correr en ~0,4 s

		float estaminaMaxima = 100.0f;
		float consumoPorSegundo = 20.0f;      // sprint continuo: 5 s
		float recuperacionPorSegundo = 12.0f; // de vacío a lleno: ~8,3 s (tras el retardo)
		float retardoRecuperacion = 1.0f;     // s sin sprint antes de empezar a recuperar
		float minimoTrasAgotarse = 25.0f;     // tras agotarse hay que llegar a este valor para volver a correr
		float agachadoMaximoParaSprint = 0.02f;   // fracción de agachado (0 de pie, 1 agachado) tolerada para correr
	};

	// Lo que ocurre en este fotograma. Quien llama ya ha aplicado las reglas de pausa/foco/modo.
	struct EntradaMovimiento
	{
		bool activo = false;          // modo FPS jugable (no pausa, sin foco perdido, sin paneles de depuración)
		bool quiereMoverse = false;   // hay alguna tecla de dirección pulsada
		bool sprintPedido = false;    // Shift mantenido
		bool enSuelo = true;
		float fraccionAgachado = 0.0f; // 0 = de pie, 1 = totalmente agachado
	};

	class MovimientoJugador
	{
	public:
		ParametrosMovimiento parametros;

		// Avanza el estado un fotograma. Devuelve la velocidad (m/s) con la que mover al jugador si hay teclas.
		float Actualizar(float deltaSinLimitar, const EntradaMovimiento& entrada)
		{
			const float delta = LimitarDelta(deltaSinLimitar);
			if (!entrada.activo)
			{	// Pausa o entrada bloqueada: el mundo del jugador se congela (ni consume, ni recupera, ni acelera).
				sprintando = false;
				return velocidadActual;
			}
			const ParametrosMovimiento& p = parametros;
			estamina = std::clamp(estamina, 0.0f, p.estaminaMaxima);

			// ¿Se puede correr ahora?
			if (agotada && estamina >= p.minimoTrasAgotarse) agotada = false;
			sprintando = entrada.sprintPedido && entrada.quiereMoverse && entrada.enSuelo && !agotada &&
				estamina > 0.0f && entrada.fraccionAgachado <= p.agachadoMaximoParaSprint;

			if (sprintando)
			{
				estamina -= p.consumoPorSegundo * delta;
				tiempoSinSprint = 0.0f;
				if (estamina <= 0.0f) { estamina = 0.0f; agotada = true; sprintando = false; }
			}
			else
			{
				tiempoSinSprint += delta;
				if (tiempoSinSprint >= p.retardoRecuperacion)
					estamina = std::min(p.estaminaMaxima, estamina + p.recuperacionPorSegundo * delta);
				if (agotada && estamina >= p.minimoTrasAgotarse) agotada = false;
			}

			// Velocidad deseada: caminar <-> agachado según lo agachado que se esté; sprint si procede.
			const float agachado = std::clamp(entrada.fraccionAgachado, 0.0f, 1.0f);
			const float objetivo = sprintando ? p.velocidadSprint
				: p.velocidadCaminar + (p.velocidadAgachado - p.velocidadCaminar) * agachado;
			velocidadActual = Acercar(velocidadActual, objetivo, p.suavizadoVelocidad, delta);
			return velocidadActual;
		}

		float Velocidad() const { return velocidadActual; }
		bool Sprintando() const { return sprintando; }
		bool Agotada() const { return agotada; }
		float Estamina() const { return estamina; }
		float FraccionEstamina() const { return parametros.estaminaMaxima > 0.0f ? estamina / parametros.estaminaMaxima : 0.0f; }
		float TiempoSinSprint() const { return tiempoSinSprint; }

		void Reiniciar()
		{
			estamina = parametros.estaminaMaxima;
			velocidadActual = parametros.velocidadCaminar;
			tiempoSinSprint = 0.0f;
			sprintando = agotada = false;
		}

	private:
		float estamina = 100.0f;
		float velocidadActual = 3.0f;
		float tiempoSinSprint = 0.0f;
		bool sprintando = false;
		bool agotada = false;
	};

	// -------------------------------------------------------------------------
	// Balanceo visual de la cámara y del móvil
	// -------------------------------------------------------------------------
	struct ParametrosBalanceo
	{
		float multiplicador = 1.0f;          // ajuste global desde el panel F1 (0 = sin balanceo)
		float amplitudCaminar = 0.015f;      // m (vertical): 1,5 cm
		float amplitudCorrer = 0.035f;       // m: 3,5 cm
		float amplitudAgachado = 0.008f;     // m: 0,8 cm
		float lateralRelativo = 0.6f;        // el balanceo lateral es esta fracción del vertical
		float zancadaCaminar = 1.5f;         // m recorridos por ciclo completo (pie izquierdo + derecho)
		float zancadaCorrer = 2.2f;          // m
		float entrada = 8.0f;                // 1/s: rapidez con que aparece al empezar a andar
		float salida = 5.0f;                 // 1/s: rapidez con que se amortigua al detenerse
		float velocidadMinima = 0.15f;       // m/s reales por debajo de los cuales se considera parado
	};

	struct DesplazamientoVisual
	{
		float lateral = 0.0f;    // m, hacia la derecha de la cámara
		float vertical = 0.0f;   // m, hacia arriba
	};

	class BalanceoCamara
	{
	public:
		ParametrosBalanceo parametros;

		// 'distanciaHorizontal': metros REALES recorridos por la cápsula en este fotograma (no la velocidad pedida:
		// si se choca con una pared, la cámara no se balancea). 'velocidadObjetivo' = MovimientoJugador::Velocidad().
		void Actualizar(float deltaSinLimitar, float distanciaHorizontal, bool enSuelo, bool activo,
			float velocidadObjetivo, const ParametrosMovimiento& mov, float fraccionAgachado)
		{
			const float delta = LimitarDelta(deltaSinLimitar);
			if (!activo || delta <= 0.0f) return;   // en pausa se queda como está

			const ParametrosBalanceo& p = parametros;
			const float velocidadReal = distanciaHorizontal / delta;
			const bool andando = enSuelo && velocidadReal > p.velocidadMinima;

			// Cuánto de "correr" hay: 0 al caminar, 1 al correr (continuo, sigue a la velocidad suavizada).
			const float rango = std::max(mov.velocidadSprint - mov.velocidadCaminar, 1e-3f);
			const float correr = std::clamp((velocidadObjetivo - mov.velocidadCaminar) / rango, 0.0f, 1.0f);
			const float agachado = std::clamp(fraccionAgachado, 0.0f, 1.0f);
			const float zancada = p.zancadaCaminar + (p.zancadaCorrer - p.zancadaCaminar) * correr;
			if (andando) fase += 6.28318530718f * distanciaHorizontal / std::max(zancada, 0.1f);
			if (fase > 6.28318530718f * 1000.0f) fase = std::fmod(fase, 6.28318530718f);   // sin perder precisión

			peso = Acercar(peso, andando ? 1.0f : 0.0f, andando ? p.entrada : p.salida, delta);

			const float amplitudDePie = p.amplitudCaminar + (p.amplitudCorrer - p.amplitudCaminar) * correr;
			amplitud = (amplitudDePie + (p.amplitudAgachado - amplitudDePie) * agachado) * p.multiplicador;
		}

		// Dos pasos por ciclo en vertical (cada pie) y uno en lateral (de un lado a otro).
		DesplazamientoVisual Desplazamiento() const
		{
			DesplazamientoVisual d;
			d.vertical = std::sin(2.0f * fase) * amplitud * peso;
			d.lateral = std::sin(fase) * amplitud * peso * parametros.lateralRelativo;
			return d;
		}

		float Fase() const { return fase; }                 // radianes (un ciclo = 2*pi)
		float Peso() const { return peso; }                 // 0 parado ... 1 andando
		// Relación con la amplitud de caminar de pie: 1 al caminar, ~2,3 al correr, ~0,5 agachado. Para el móvil.
		float FactorDeAmplitud() const
		{
			const float base = parametros.amplitudCaminar * parametros.multiplicador;
			return base > 1e-6f ? amplitud / base : 0.0f;
		}
		void Reiniciar() { fase = 0.0f; peso = 0.0f; amplitud = 0.0f; }

	private:
		float fase = 0.0f;
		float peso = 0.0f;
		float amplitud = 0.0f;
	};

	// -------------------------------------------------------------------------
	// Barra de estamina: opacidad y valor mostrado, suavizados
	// -------------------------------------------------------------------------
	class BarraEstamina
	{
	public:
		float visible = 0.0f;   // opacidad 0..1
		float mostrada = 1.0f;  // fracción mostrada 0..1

		void Actualizar(float deltaSinLimitar, float fraccionReal, bool sprintando)
		{
			const float delta = LimitarDelta(deltaSinLimitar);
			mostrada = Acercar(mostrada, fraccionReal, 10.0f, delta);
			const bool hayQueMostrar = sprintando || fraccionReal < 0.999f;
			visible = Acercar(visible, hayQueMostrar ? 1.0f : 0.0f, hayQueMostrar ? 12.0f : 3.0f, delta);
		}
	};
}
