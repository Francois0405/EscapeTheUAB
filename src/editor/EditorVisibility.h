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

#include "src/scene/SceneLoader.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace editor
{
	enum class ElementClass { Object, Floor, Ceiling, Wall, Architecture };

	// Una clase por objeto, en el mismo orden que 'objetos'.
	std::vector<ElementClass> ClassifyElements(const std::vector<SceneObject>& objetos);

	struct EditorVisibility
	{
		bool hideCeiling = false;
		bool hideWalls = false;
		bool autoOcultarTechoEnVistaSuperior = true;
		bool isolateSelection = false;                         // solo se ve el elemento 'idAislado'
		std::string isolatedId;
		std::unordered_set<std::string> ocultos;     // elementos ocultos uno a uno (por id)

		void ShowAll()
		{
			hideCeiling = hideWalls = isolateSelection = false;
			isolatedId.clear();
			ocultos.clear();
		}

		bool Visible(const SceneObject& objeto, ElementClass clase, bool vistaSuperior) const
		{
			if (isolateSelection) return objeto.id == isolatedId;
			if (ocultos.contains(objeto.id)) return false;
			if (clase == ElementClass::Ceiling && (hideCeiling || (autoOcultarTechoEnVistaSuperior && vistaSuperior))) return false;
			if (clase == ElementClass::Wall && hideWalls) return false;
			return true;
		}

		// Las puertas GLB solo se ocultan al aislar otro elemento.
		bool AnimatedVisible(const std::string& id) const { return !isolateSelection || id == isolatedId; }
	};
}
