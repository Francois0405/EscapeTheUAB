// MovimientoFPSTests.cpp
// Pruebas de velocidad, sprint, estamina y balanceo (MovimientoFPS.h) sin ventana ni fisica.
#include "MovimientoFPS.h"

#include <cmath>
#include <iostream>

using namespace fps;

namespace
{
	int fallos = 0;
	void comprobar(bool condicion, const char* caso)
	{
		if (!condicion) { std::cerr << "FALLO: " << caso << '\n'; ++fallos; }
	}
	bool casi(float a, float b, float eps) { return std::abs(a - b) <= eps; }

	EntradaMovimiento corriendo()
	{
		EntradaMovimiento e;
		e.activo = e.quiereMoverse = e.sprintPedido = e.enSuelo = true;
		return e;
	}

	// Avanza 'segundos' con pasos de 1/fps; devuelve el estado final.
	void Simular(MovimientoJugador& m, const EntradaMovimiento& e, float segundos, int fpsSimulados)
	{
		const int pasos = static_cast<int>(std::lround(segundos * fpsSimulados));
		for (int i = 0; i < pasos; ++i) m.Actualizar(1.0f / fpsSimulados, e);
	}
}

int main()
{
	// 1) Parametros iniciales pedidos.
	{
		MovimientoJugador m;
		comprobar(m.parametros.velocidadCaminar == 3.0f && m.parametros.velocidadAgachado == 1.4f &&
			m.parametros.velocidadSprint == 5.2f, "velocidades iniciales 3,0 / 1,4 / 5,2 m/s");
		comprobar(m.parametros.estaminaMaxima == 100.0f && m.parametros.consumoPorSegundo == 20.0f &&
			m.parametros.recuperacionPorSegundo == 12.0f && m.parametros.retardoRecuperacion == 1.0f,
			"estamina 100, consumo 20/s, recuperacion 12/s tras 1 s");
		comprobar(m.Estamina() == 100.0f, "empieza con la estamina llena");
	}

	// 2) Consumo independiente de los FPS (30, 60, 120): 2 s corriendo = 40 puntos.
	for (int f : { 30, 60, 120 })
	{
		MovimientoJugador m;
		Simular(m, corriendo(), 2.0f, f);
		comprobar(casi(m.Estamina(), 60.0f, 0.5f), "consumo de 2 s corriendo es 40 puntos a cualquier FPS");
		comprobar(m.Sprintando(), "sigue corriendo con estamina");
	}

	// 3) Recuperacion: retardo de 1 s y luego 12 puntos/s, igual a 30/60/120 FPS.
	for (int f : { 30, 60, 120 })
	{
		MovimientoJugador m;
		Simular(m, corriendo(), 2.0f, f);                 // 60
		EntradaMovimiento quieto = corriendo();
		quieto.sprintPedido = false;
		Simular(m, quieto, 0.9f, f);
		comprobar(casi(m.Estamina(), 60.0f, 0.5f), "no recupera durante el retardo de 1 s");
		Simular(m, quieto, 1.1f, f);                       // 1 s mas de recuperacion efectiva
		comprobar(casi(m.Estamina(), 72.0f, 1.0f), "recupera 12 puntos/s despues del retardo");
		Simular(m, quieto, 20.0f, f);
		comprobar(m.Estamina() == 100.0f, "nunca supera el maximo");
	}

	// 4) Agotamiento: a los 5 s se acaba y obliga a recuperar el minimo antes de volver a correr.
	{
		MovimientoJugador m;
		Simular(m, corriendo(), 4.9f, 60);
		comprobar(m.Sprintando() && !m.Agotada(), "a los 4,9 s aun corre");
		Simular(m, corriendo(), 0.3f, 60);
		comprobar(m.Agotada() && !m.Sprintando() && m.Estamina() == 0.0f, "a los 5 s se agota");
		// Mantener Shift no sirve de nada: no corre ni consume, y no recupera (la tecla sigue pulsada = sprint pedido,
		// pero eso no cuenta como descanso hasta que se cumpla el retardo; aqui se recupera pasado 1 s).
		Simular(m, corriendo(), 1.0f, 60);
		comprobar(!m.Sprintando(), "agotado: no corre aunque se mantenga Shift");
		Simular(m, corriendo(), 1.0f, 60);   // 1 s de recuperacion = 12 puntos < minimo 25
		comprobar(m.Agotada() && !m.Sprintando(), "con 12 puntos aun no puede volver a correr");
		Simular(m, corriendo(), 1.5f, 60);   // supera 25
		comprobar(!m.Agotada() && m.Sprintando(), "al superar el minimo vuelve a poder correr");
	}

	// 5) Sin consumo: pausa, sin desplazamiento, agachado, en el aire.
	{
		MovimientoJugador m;
		EntradaMovimiento e = corriendo();
		e.activo = false;
		Simular(m, e, 3.0f, 60);
		comprobar(m.Estamina() == 100.0f && !m.Sprintando(), "en pausa no consume");

		e = corriendo();
		e.quiereMoverse = false;
		Simular(m, e, 3.0f, 60);
		comprobar(m.Estamina() == 100.0f && !m.Sprintando(), "Shift sin desplazamiento no consume");

		e = corriendo();
		e.fraccionAgachado = 0.5f;
		Simular(m, e, 3.0f, 60);
		comprobar(m.Estamina() == 100.0f && !m.Sprintando(), "agachado no se puede correr ni se consume");

		e = corriendo();
		e.enSuelo = false;
		Simular(m, e, 3.0f, 60);
		comprobar(m.Estamina() == 100.0f && !m.Sprintando(), "en el aire no se corre");

		// La pausa tambien congela la recuperacion y el retardo.
		MovimientoJugador n;
		Simular(n, corriendo(), 2.0f, 60);
		const float antes = n.Estamina();
		EntradaMovimiento pausa;
		Simular(n, pausa, 10.0f, 60);
		comprobar(n.Estamina() == antes, "la pausa congela la estamina");
	}

	// 6) Velocidades y transiciones suaves.
	{
		MovimientoJugador m;
		EntradaMovimiento caminar = corriendo();
		caminar.sprintPedido = false;
		Simular(m, caminar, 2.0f, 60);
		comprobar(casi(m.Velocidad(), 3.0f, 0.01f), "caminando va a 3 m/s");

		m.Actualizar(1.0f / 60.0f, corriendo());
		comprobar(m.Velocidad() > 3.0f && m.Velocidad() < 3.5f, "al empezar a correr acelera poco a poco (no salta a 5,2)");
		Simular(m, corriendo(), 1.0f, 60);
		comprobar(casi(m.Velocidad(), 5.2f, 0.05f), "tras ~1 s corre a 5,2 m/s");

		Simular(m, caminar, 1.5f, 60);
		comprobar(casi(m.Velocidad(), 3.0f, 0.05f), "al soltar Shift vuelve a 3 m/s suavemente");

		EntradaMovimiento agachado = caminar;
		agachado.fraccionAgachado = 1.0f;
		Simular(m, agachado, 2.0f, 60);
		comprobar(casi(m.Velocidad(), 1.4f, 0.02f), "agachado va a 1,4 m/s");
		agachado.fraccionAgachado = 0.5f;
		Simular(m, agachado, 2.0f, 60);
		comprobar(casi(m.Velocidad(), 2.2f, 0.03f), "a medio agachar la velocidad esta a medio camino");

		// Independiente de los FPS: tras 0,5 s de sprint la velocidad es la misma a 30 y a 120 FPS.
		MovimientoJugador a, b;
		Simular(a, corriendo(), 0.5f, 30);
		Simular(b, corriendo(), 0.5f, 120);
		comprobar(casi(a.Velocidad(), b.Velocidad(), 0.01f), "la aceleracion no depende de los FPS");
	}

	// 7) Un fotograma gigante no dispara nada.
	{
		MovimientoJugador m;
		m.Actualizar(10.0f, corriendo());
		comprobar(m.Estamina() > 98.0f, "un delta enorme se limita a 0,05 s");
		comprobar(LimitarDelta(-1.0f) == 0.0f, "un delta negativo se ignora");
	}

	// 8) Curva de Hermite.
	{
		comprobar(SuavizadoHermite(0.0f) == 0.0f && SuavizadoHermite(1.0f) == 1.0f && casi(SuavizadoHermite(0.5f), 0.5f, 1e-6f),
			"la curva de Hermite pasa por 0, 0,5 y 1");
		comprobar(SuavizadoHermite(0.05f) < 0.05f && SuavizadoHermite(0.95f) > 0.95f, "arranca y llega despacio");
		comprobar(SuavizadoHermite(-3.0f) == 0.0f && SuavizadoHermite(7.0f) == 1.0f, "se limita al rango");
	}

	// 9) Balanceo: proporcional al desplazamiento real, amortiguado y de amplitud moderada.
	{
		ParametrosMovimiento mov;
		BalanceoCamara b;
		float maxVertical = 0.0f;
		// Camina 4 s a 3 m/s con 60 FPS.
		for (int i = 0; i < 240; ++i)
		{
			b.Actualizar(1.0f / 60.0f, 3.0f / 60.0f, true, true, 3.0f, mov, 0.0f);
			maxVertical = std::max(maxVertical, std::abs(b.Desplazamiento().vertical));
		}
		comprobar(maxVertical > 0.010f && maxVertical <= 0.016f, "caminar: balanceo vertical entre 1 y 1,6 cm");
		comprobar(b.Peso() > 0.99f, "el peso llega a 1 andando");
		// Fase: 12 m / 1,5 m por ciclo = 8 ciclos.
		comprobar(casi(b.Fase(), 8.0f * 6.28318530718f, 0.2f), "la fase depende de la distancia real recorrida");

		// Se detiene: se amortigua, no se corta de golpe.
		b.Actualizar(1.0f / 60.0f, 0.0f, true, true, 3.0f, mov, 0.0f);
		comprobar(b.Peso() > 0.9f && b.Peso() < 1.0f, "al parar el peso baja gradualmente");
		for (int i = 0; i < 120; ++i) b.Actualizar(1.0f / 60.0f, 0.0f, true, true, 3.0f, mov, 0.0f);
		comprobar(std::abs(b.Desplazamiento().vertical) < 5e-4f && std::abs(b.Desplazamiento().lateral) < 5e-4f,
			"tras detenerse unos segundos el balanceo desaparece");

		// Correr: mas amplitud (3-4 cm).
		BalanceoCamara c;
		float maxCorrer = 0.0f;
		for (int i = 0; i < 300; ++i)
		{
			c.Actualizar(1.0f / 60.0f, 5.2f / 60.0f, true, true, 5.2f, mov, 0.0f);
			maxCorrer = std::max(maxCorrer, std::abs(c.Desplazamiento().vertical));
		}
		comprobar(maxCorrer > 0.030f && maxCorrer <= 0.036f, "correr: balanceo vertical entre 3 y 3,6 cm");
		comprobar(c.FactorDeAmplitud() > 2.0f && c.FactorDeAmplitud() < 2.6f, "el movil se mueve ~2,3 veces mas al correr");

		// Agachado: menos que caminando.
		BalanceoCamara d;
		float maxAgachado = 0.0f;
		for (int i = 0; i < 300; ++i)
		{
			d.Actualizar(1.0f / 60.0f, 1.4f / 60.0f, true, true, 1.4f, mov, 1.0f);
			maxAgachado = std::max(maxAgachado, std::abs(d.Desplazamiento().vertical));
		}
		comprobar(maxAgachado < maxVertical && maxAgachado > 0.004f, "agachado: balanceo menor que caminando");

		// Sin avance real (choca con una pared) no hay balanceo aunque se pulse la tecla.
		BalanceoCamara e;
		for (int i = 0; i < 120; ++i) e.Actualizar(1.0f / 60.0f, 0.0f, true, true, 3.0f, mov, 0.0f);
		comprobar(e.Peso() < 1e-3f && e.Fase() == 0.0f, "sin desplazamiento real no hay balanceo");

		// En el aire o en pausa tampoco avanza la fase.
		BalanceoCamara g;
		g.Actualizar(1.0f / 60.0f, 0.05f, false, true, 3.0f, mov, 0.0f);
		g.Actualizar(1.0f / 60.0f, 0.05f, true, false, 3.0f, mov, 0.0f);
		comprobar(g.Fase() == 0.0f, "ni en el aire ni en pausa avanza la fase");
	}

	// 10) La misma distancia da la misma fase a 30, 60 y 120 FPS.
	{
		ParametrosMovimiento mov;
		float fases[3];
		int k = 0;
		for (int f : { 30, 60, 120 })
		{
			BalanceoCamara b;
			for (int i = 0; i < 3 * f; ++i) b.Actualizar(1.0f / f, 3.0f / f, true, true, 3.0f, mov, 0.0f);
			fases[k++] = b.Fase();
		}
		comprobar(casi(fases[0], fases[1], 0.02f) && casi(fases[1], fases[2], 0.02f), "la fase del balanceo no depende de los FPS");
	}

	// 11) Barra de estamina.
	{
		BarraEstamina barra;
		for (int i = 0; i < 120; ++i) barra.Actualizar(1.0f / 60.0f, 1.0f, false);
		comprobar(barra.visible < 0.01f, "con la estamina llena la barra esta oculta");
		for (int i = 0; i < 30; ++i) barra.Actualizar(1.0f / 60.0f, 0.5f, true);
		comprobar(barra.visible > 0.9f && barra.mostrada < 0.8f && barra.mostrada > 0.5f, "al correr aparece y baja con suavidad");
		for (int i = 0; i < 600; ++i) barra.Actualizar(1.0f / 60.0f, 1.0f, false);
		comprobar(barra.visible < 0.01f && casi(barra.mostrada, 1.0f, 1e-3f), "al llenarse se desvanece");
	}

	if (fallos == 0) std::cout << "MovimientoFPSTests: todo correcto\n";
	return fallos == 0 ? 0 : 1;
}
