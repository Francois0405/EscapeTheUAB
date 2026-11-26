// LimitesModelo.h
// -----------------------------------------------------------------------------
// Cajas envolventes de modelos y prueba rayo-caja orientada. Sin OpenGL ni ImGui: se prueba sin ventana.
// Las usan el picking del editor (no depende de que el objeto tenga un colisionador Bullet) y el
// colisionador de caja inicial de los modelos nuevos.
// -----------------------------------------------------------------------------
#pragma once

#include "src/rendering/GlbModel.h"

#include <glm/glm.hpp>

#include <filesystem>

namespace editor
{
	// Caja alineada con los ejes del MODELO (antes de aplicar posición, giro y escala del objeto).
	struct LocalBox
	{
		glm::vec3 minimo{ -0.5f };
		glm::vec3 maximo{ 0.5f };
		bool valida = false;

		glm::vec3 CenterPoint() const { return (minimo + maximo) * 0.5f; }
		glm::vec3 HalfSize() const { return (maximo - minimo) * 0.5f; }
		float VolumeValue() const { const glm::vec3 t = maximo - minimo; return t.x * t.y * t.z; }
	};

	// Lee las líneas "v x y z" de un OBJ. Devuelve una caja no válida si no hay vértices o no se puede abrir.
	LocalBox ObjBounds(const std::filesystem::path& ruta);

	// Caja de un modelo GLB en su pose de reposo, ya en el espacio del juego (Z arriba, tras el cambio de ejes de glTF).
	LocalBox GlbBounds(const glb::GlbModel& modelo);

	// Garantiza una mitad mínima en cada eje (objetos muy finos, como una puerta o una lámina, siguen siendo seleccionables).
	LocalBox WithMinimumHalfExtent(LocalBox caja, float mitadMinima);

	// Caja alineada con el MUNDO que contiene la caja local transformada por 'modelo'.
	LocalBox BoxInWorld(const glm::mat4& modelo, const LocalBox& caja);

	// Rayo (origen + t * direccion, t >= 0) contra la caja local orientada por 'modelo'. 'distancia' es el valor de t.
	// Vale para proyección en perspectiva y ortográfica: solo necesita el origen y la dirección del rayo en el mundo.
	bool RayBoxIntersection(const glm::vec3& origen, const glm::vec3& direccion, const glm::mat4& modelo,
		const LocalBox& caja, float& distancia);
}
