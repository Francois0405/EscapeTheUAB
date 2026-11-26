#include "EditorVisibility.h"

#include <algorithm>
#include <cmath>

namespace editor
{
	std::vector<ElementClass> ClassifyElements(const std::vector<SceneObject>& objetos)
	{
		std::vector<ElementClass> clases(objetos.size(), ElementClass::Object);

		// Altura media de la arquitectura: separa el techo (por encima) del suelo (por debajo).
		float sumaZ = 0.0f;
		int cuenta = 0;
		for (const SceneObject& objeto : objetos)
			if (objeto.categoria == "architecture") { sumaZ += objeto.position.z; ++cuenta; }
		const float mediaZ = cuenta > 0 ? sumaZ / static_cast<float>(cuenta) : 0.0f;

		for (std::size_t i = 0; i < objetos.size(); ++i)
		{
			const SceneObject& objeto = objetos[i];
			if (objeto.categoria != "architecture") continue;
			// Medias dimensiones en el mundo: escala * caja de física (si la hay) o escala * 1.
			const glm::vec3 base = objeto.fisica.presente ? objeto.fisica.halfExtents : glm::vec3(1.0f);
			const glm::vec3 d = glm::abs(objeto.escala) * base;
			const float horizontalMenor = std::min(d.x, d.y);
			if (d.z <= 0.5f * horizontalMenor)
				clases[i] = objeto.position.z >= mediaZ ? ElementClass::Ceiling : ElementClass::Floor;
			else if (horizontalMenor <= 0.5f * d.z)
				clases[i] = ElementClass::Wall;
			else
				clases[i] = ElementClass::Architecture;
		}
		return clases;
	}
}
