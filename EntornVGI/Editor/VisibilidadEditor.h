// VisibilidadEditor.h
// -----------------------------------------------------------------------------
// Visibilidad TEMPORAL de la vista de edición. No modifica la escena ni el JSON, no toca los colisionadores y no se guarda:
// al volver a PLAY siempre se dibuja la habitación completa.
//
// Clasificación de la arquitectura SIN depender del nombre: se usa la colección del JSON ("architecture"), la forma
// (losa horizontal fina / pared vertical fina, calculada con la escala y la caja) y la altura relativa. Un techo es
// una losa horizontal por encima del resto de la arquitectura; un suelo, por debajo.
// -----------------------------------------------------------------------------
#pragma once

#include "../SceneLoader.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace editor
{
	enum class ClaseElemento { Objeto, Suelo, Techo, Pared, Arquitectura };

	// Una clase por objeto, en el mismo orden que 'objetos'.
	std::vector<ClaseElemento> ClasificarElementos(const std::vector<SceneObject>& objetos);

	struct VisibilidadEditor
	{
		bool ocultarTecho = false;
		bool ocultarParedes = false;
		bool autoOcultarTechoEnVistaSuperior = true;
		bool aislar = false;                         // solo se ve el elemento 'idAislado'
		std::string idAislado;
		std::unordered_set<std::string> ocultos;     // elementos ocultos uno a uno (por id)

		void MostrarTodo()
		{
			ocultarTecho = ocultarParedes = aislar = false;
			idAislado.clear();
			ocultos.clear();
		}

		bool Visible(const SceneObject& objeto, ClaseElemento clase, bool vistaSuperior) const
		{
			if (aislar) return objeto.id == idAislado;
			if (ocultos.contains(objeto.id)) return false;
			if (clase == ClaseElemento::Techo && (ocultarTecho || (autoOcultarTechoEnVistaSuperior && vistaSuperior))) return false;
			if (clase == ClaseElemento::Pared && ocultarParedes) return false;
			return true;
		}

		// Las puertas GLB solo se ocultan al aislar otro elemento.
		bool AnimadoVisible(const std::string& id) const { return !aislar || id == idAislado; }
	};
}
