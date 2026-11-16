// LimitesModelo.h
// -----------------------------------------------------------------------------
// Cajas envolventes de modelos y prueba rayo-caja orientada. Sin OpenGL ni ImGui: se prueba sin ventana.
// Las usan el picking del editor (no depende de que el objeto tenga un colisionador Bullet) y el
// colisionador de caja inicial de los modelos nuevos.
// -----------------------------------------------------------------------------
#pragma once

#include "../ModeloGLB.h"

#include <glm/glm.hpp>

#include <filesystem>

namespace editor
{
	// Caja alineada con los ejes del MODELO (antes de aplicar posición, giro y escala del objeto).
	struct CajaLocal
	{
		glm::vec3 minimo{ -0.5f };
		glm::vec3 maximo{ 0.5f };
		bool valida = false;

		glm::vec3 Centro() const { return (minimo + maximo) * 0.5f; }
		glm::vec3 MitadTamano() const { return (maximo - minimo) * 0.5f; }
		float Volumen() const { const glm::vec3 t = maximo - minimo; return t.x * t.y * t.z; }
	};

	// Lee las líneas "v x y z" de un OBJ. Devuelve una caja no válida si no hay vértices o no se puede abrir.
	CajaLocal LimitesDeOBJ(const std::filesystem::path& ruta);

	// Caja de un modelo GLB en su pose de reposo, ya en el espacio del juego (Z arriba, tras el cambio de ejes de glTF).
	CajaLocal LimitesDeGLB(const glb::ModeloGLB& modelo);

	// Garantiza una mitad mínima en cada eje (objetos muy finos, como una puerta o una lámina, siguen siendo seleccionables).
	CajaLocal ConMitadMinima(CajaLocal caja, float mitadMinima);

	// Caja alineada con el MUNDO que contiene la caja local transformada por 'modelo'.
	CajaLocal CajaEnMundo(const glm::mat4& modelo, const CajaLocal& caja);

	// Rayo (origen + t * direccion, t >= 0) contra la caja local orientada por 'modelo'. 'distancia' es el valor de t.
	// Vale para proyección en perspectiva y ortográfica: solo necesita el origen y la dirección del rayo en el mundo.
	bool InterseccionRayoCaja(const glm::vec3& origen, const glm::vec3& direccion, const glm::mat4& modelo,
		const CajaLocal& caja, float& distancia);
}
