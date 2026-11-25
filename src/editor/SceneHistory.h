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

#include "src/scene/SceneLoader.h"

#include <cstddef>
#include <vector>

namespace editor
{
	struct SceneState
	{
		std::vector<SceneObject> objetos;
		std::vector<SceneAnimatedObject> animados;
	};

	inline bool AreEqual(const ScenePhysics& a, const ScenePhysics& b)
	{
		return a.presente == b.presente && a.tipo == b.tipo && a.forma == b.forma && a.mitadTamano == b.mitadTamano &&
			a.desplazamiento == b.desplazamiento && a.masa == b.masa;
	}

	inline bool AreEqual(const SceneObject& a, const SceneObject& b)
	{
		return a.id == b.id && a.categoria == b.categoria && a.modelo == b.modelo && a.posicion == b.posicion &&
			a.rotacion == b.rotacion && a.escala == b.escala && AreEqual(a.fisica, b.fisica) &&
			a.luz.presente == b.luz.presente && a.luz.encendida == b.luz.encendida &&
			a.luz.desplazamiento == b.luz.desplazamiento && a.luz.color == b.luz.color &&
			a.luz.intensidad == b.luz.intensidad && a.luz.areaSize == b.luz.areaSize &&
			a.material.presente == b.material.presente && a.material.textura == b.material.textura &&
			a.material.tileSize == b.material.tileSize && a.material.roughness == b.material.roughness &&
			a.material.dirtStrength == b.material.dirtStrength && a.material.dirtScale == b.material.dirtScale &&
			a.material.dirtSeed == b.material.dirtSeed &&
			a.permitirPenetracionArquitectura == b.permitirPenetracionArquitectura;
	}

	inline bool AreEqual(const SceneAnimatedObject& a, const SceneAnimatedObject& b)
	{
		if (a.id != b.id || a.modelo != b.modelo || a.posicion != b.posicion || a.giroZGrados != b.giroZGrados ||
			a.escala != b.escala || a.animation != b.animation ||
			a.permitirPenetracionArquitectura != b.permitirPenetracionArquitectura ||
			a.colisionadores.size() != b.colisionadores.size()) return false;
		for (std::size_t i = 0; i < a.colisionadores.size(); ++i)
		{
			const auto& ca = a.colisionadores[i];
			const auto& cb = b.colisionadores[i];
			if (ca.nodo != cb.nodo || ca.tipo != cb.tipo || ca.mitadTamano != cb.mitadTamano || ca.desplazamiento != cb.desplazamiento) return false;
		}
		return true;
	}

	inline bool AreEqual(const SceneState& a, const SceneState& b)
	{
		if (a.objetos.size() != b.objetos.size() || a.animados.size() != b.animados.size()) return false;
		for (std::size_t i = 0; i < a.objetos.size(); ++i) if (!AreEqual(a.objetos[i], b.objetos[i])) return false;
		for (std::size_t i = 0; i < a.animados.size(); ++i) if (!AreEqual(a.animados[i], b.animados[i])) return false;
		return true;
	}

	class SceneHistory
	{
	public:
		static constexpr std::size_t LimiteOperaciones = 200;

		void ResetState()
		{
			deshacer.clear();
			rehacer.clear();
			transaccionAbierta = false;
		}

		// Operación puntual (añadir, eliminar, duplicar...): 'antes' es el estado ANTERIOR al cambio.
		// Cualquier operación nueva descarta la rama de rehacer, que ya no corresponde a la historia actual.
		void RegisterElement(SceneState antes)
		{
			deshacer.push_back(std::move(antes));
			if (deshacer.size() > LimiteOperaciones) deshacer.erase(deshacer.begin());
			rehacer.clear();
		}

		// Arrastre o edición continua: se llama UNA vez al empezar (con el estado sin modificar).
		void BeginTransaction(const SceneState& antes)
		{
			if (transaccionAbierta) return;
			inicioTransaccion = antes;
			transaccionAbierta = true;
		}

		bool IsTransactionOpen() const { return transaccionAbierta; }

		// Al terminar: registra UNA operación si el estado cambió de verdad. Devuelve si registró algo.
		bool CommitTransaction(const SceneState& despues)
		{
			if (!transaccionAbierta) return false;
			transaccionAbierta = false;
			if (AreEqual(inicioTransaccion, despues)) return false;
			RegisterElement(std::move(inicioTransaccion));
			return true;
		}

		bool CanUndo() const { return !deshacer.empty(); }
		bool CanRedo() const { return !rehacer.empty(); }
		std::size_t UndoOperations() const { return deshacer.size(); }
		std::size_t RedoOperations() const { return rehacer.size(); }

		// 'actual' es el estado visible ahora; se sustituye por el anterior y el actual pasa a la pila de rehacer.
		bool Undo(SceneState& actual)
		{
			transaccionAbierta = false;
			if (deshacer.empty()) return false;
			rehacer.push_back(std::move(actual));
			actual = std::move(deshacer.back());
			deshacer.pop_back();
			return true;
		}

		bool Redo(SceneState& actual)
		{
			transaccionAbierta = false;
			if (rehacer.empty()) return false;
			deshacer.push_back(std::move(actual));
			actual = std::move(rehacer.back());
			rehacer.pop_back();
			return true;
		}

	private:
		std::vector<SceneState> deshacer;
		std::vector<SceneState> rehacer;
		SceneState inicioTransaccion;
		bool transaccionAbierta = false;
	};
}
