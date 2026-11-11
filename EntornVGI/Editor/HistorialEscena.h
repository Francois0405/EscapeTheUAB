// HistorialEscena.h
// -----------------------------------------------------------------------------
// Deshacer / rehacer del editor. Cada operación guarda el ESTADO EDITABLE completo de la escena antes de cambiarla
// (instantánea): es el mecanismo equivalente al Command Pattern para una escena pequeña, y cubre con el mismo código
// mover, rotar, escalar, añadir, eliminar, duplicar y los cambios del Inspector.
//
// Un arrastre del gizmo (o la edición de un campo) es UNA sola operación: IniciarTransaccion() al empezar y
// CerrarTransaccion() al terminar; si no cambió nada, no se registra.
// Sin ImGui, OpenGL ni Bullet: se prueba sin ventana.
// -----------------------------------------------------------------------------
#pragma once

#include "../SceneLoader.h"

#include <cstddef>
#include <vector>

namespace editor
{
	struct EstadoEscena
	{
		std::vector<SceneObject> objetos;
		std::vector<SceneObjetoAnimado> animados;
	};

	inline bool Iguales(const SceneFisica& a, const SceneFisica& b)
	{
		return a.presente == b.presente && a.tipo == b.tipo && a.mitadTamano == b.mitadTamano &&
			a.desplazamiento == b.desplazamiento && a.masa == b.masa;
	}

	inline bool Iguales(const SceneObject& a, const SceneObject& b)
	{
		return a.id == b.id && a.categoria == b.categoria && a.modelo == b.modelo && a.posicion == b.posicion &&
			a.rotacion == b.rotacion && a.escala == b.escala && Iguales(a.fisica, b.fisica);
	}

	inline bool Iguales(const SceneObjetoAnimado& a, const SceneObjetoAnimado& b)
	{
		return a.id == b.id && a.modelo == b.modelo && a.posicion == b.posicion && a.giroZGrados == b.giroZGrados &&
			a.escala == b.escala && a.animacion == b.animacion && a.colisionadores.size() == b.colisionadores.size();
	}

	inline bool Iguales(const EstadoEscena& a, const EstadoEscena& b)
	{
		if (a.objetos.size() != b.objetos.size() || a.animados.size() != b.animados.size()) return false;
		for (std::size_t i = 0; i < a.objetos.size(); ++i) if (!Iguales(a.objetos[i], b.objetos[i])) return false;
		for (std::size_t i = 0; i < a.animados.size(); ++i) if (!Iguales(a.animados[i], b.animados[i])) return false;
		return true;
	}

	class HistorialEscena
	{
	public:
		static constexpr std::size_t LimiteOperaciones = 200;

		void Reiniciar()
		{
			deshacer.clear();
			rehacer.clear();
			transaccionAbierta = false;
		}

		// Operación puntual (añadir, eliminar, duplicar...): 'antes' es el estado ANTERIOR al cambio.
		// Cualquier operación nueva descarta la rama de rehacer, que ya no corresponde a la historia actual.
		void Registrar(EstadoEscena antes)
		{
			deshacer.push_back(std::move(antes));
			if (deshacer.size() > LimiteOperaciones) deshacer.erase(deshacer.begin());
			rehacer.clear();
		}

		// Arrastre o edición continua: se llama UNA vez al empezar (con el estado sin modificar).
		void IniciarTransaccion(const EstadoEscena& antes)
		{
			if (transaccionAbierta) return;
			inicioTransaccion = antes;
			transaccionAbierta = true;
		}

		bool TransaccionAbierta() const { return transaccionAbierta; }

		// Al terminar: registra UNA operación si el estado cambió de verdad. Devuelve si registró algo.
		bool CerrarTransaccion(const EstadoEscena& despues)
		{
			if (!transaccionAbierta) return false;
			transaccionAbierta = false;
			if (Iguales(inicioTransaccion, despues)) return false;
			Registrar(std::move(inicioTransaccion));
			return true;
		}

		bool PuedeDeshacer() const { return !deshacer.empty(); }
		bool PuedeRehacer() const { return !rehacer.empty(); }
		std::size_t OperacionesParaDeshacer() const { return deshacer.size(); }
		std::size_t OperacionesParaRehacer() const { return rehacer.size(); }

		// 'actual' es el estado visible ahora; se sustituye por el anterior y el actual pasa a la pila de rehacer.
		bool Deshacer(EstadoEscena& actual)
		{
			transaccionAbierta = false;
			if (deshacer.empty()) return false;
			rehacer.push_back(std::move(actual));
			actual = std::move(deshacer.back());
			deshacer.pop_back();
			return true;
		}

		bool Rehacer(EstadoEscena& actual)
		{
			transaccionAbierta = false;
			if (rehacer.empty()) return false;
			deshacer.push_back(std::move(actual));
			actual = std::move(rehacer.back());
			rehacer.pop_back();
			return true;
		}

	private:
		std::vector<EstadoEscena> deshacer;
		std::vector<EstadoEscena> rehacer;
		EstadoEscena inicioTransaccion;
		bool transaccionAbierta = false;
	};
}
