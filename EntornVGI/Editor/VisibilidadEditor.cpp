#include "VisibilidadEditor.h"

#include <algorithm>
#include <cmath>

namespace editor
{
	std::vector<ClaseElemento> ClasificarElementos(const std::vector<SceneObject>& objetos)
	{
		std::vector<ClaseElemento> clases(objetos.size(), ClaseElemento::Objeto);

		// Altura media de la arquitectura: separa el techo (por encima) del suelo (por debajo).
		float sumaZ = 0.0f;
		int cuenta = 0;
		for (const SceneObject& objeto : objetos)
			if (objeto.categoria == "architecture") { sumaZ += objeto.posicion.z; ++cuenta; }
		const float mediaZ = cuenta > 0 ? sumaZ / static_cast<float>(cuenta) : 0.0f;

		for (std::size_t i = 0; i < objetos.size(); ++i)
		{
			const SceneObject& objeto = objetos[i];
			if (objeto.categoria != "architecture") continue;
			// Medias dimensiones en el mundo: escala * caja de física (si la hay) o escala * 1.
			const glm::vec3 base = objeto.fisica.presente ? objeto.fisica.mitadTamano : glm::vec3(1.0f);
			const glm::vec3 d = glm::abs(objeto.escala) * base;
			const float horizontalMenor = std::min(d.x, d.y);
			if (d.z <= 0.5f * horizontalMenor)
				clases[i] = objeto.posicion.z >= mediaZ ? ClaseElemento::Techo : ClaseElemento::Suelo;
			else if (horizontalMenor <= 0.5f * d.z)
				clases[i] = ClaseElemento::Pared;
			else
				clases[i] = ClaseElemento::Arquitectura;
		}
		return clases;
	}
}
