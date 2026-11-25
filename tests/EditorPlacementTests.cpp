#include "src/editor/PlacementValidator.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <iostream>
#include <vector>

namespace
{
	int fallos = 0;
	void comprobar(bool condicion, const char* caso)
	{
		if (!condicion) { std::cerr << "FALLO: " << caso << '\n'; ++fallos; }
	}

	editor::OrientedBox caja(const glm::vec3& centro, const glm::vec3& mitad, float giro = 0.0f)
	{
		const glm::mat4 m = glm::translate(glm::mat4(1.0f), centro) *
			glm::rotate(glm::mat4(1.0f), glm::radians(giro), glm::vec3(0, 0, 1));
		return editor::TransformBox(m, glm::vec3(0), mitad);
	}
}

int main()
{
	using namespace editor;
	const PlacementObstacle pared{ caja({ 0, 0, 1.5f }, { 8.0f, 0.15f, 1.5f }), "pared_sur", "pared grande y fina", true };
	comprobar(ObbPenetration(caja({ 0, 0, 1.5f }, { 0.4f, 0.4f, 0.4f }), pared.caja) > 0.0f,
		"una caja atraviesa una pared grande y fina orientada");

	const auto rotadaA = caja({ 0, 0, 0 }, { 2.0f, 0.10f, 0.10f }, 45.0f);
	const auto rotadaB = caja({ 0, 2.8f, 0 }, { 2.0f, 0.10f, 0.10f }, 45.0f);
	comprobar(ObbPenetration(rotadaA, rotadaB) == 0.0f,
		"dos OBB rotadas separadas no colisionan por solape de sus AABB mundiales");

	const auto suelo = caja({ 0, 0, -0.1f }, { 5, 5, 0.1f });
	const auto muebleApoyado = caja({ 0, 0, 0.5f }, { 0.5f, 0.5f, 0.5f });
	comprobar(ObbPenetration(suelo, muebleApoyado) == 0.0f, "un mueble en contacto con el suelo se acepta");
	comprobar(ObbPenetration(caja({ 0, 0, 1.5f }, { 0.5f, 0.3f, 0.5f }),
		caja({ 0, 0, 0.5f }, { 0.6f, 0.5f, 0.5f })) == 0.0f, "un monitor apoyado sobre una mesa se acepta");

	const std::vector<PlacementObstacle> vano = {
		{ caja({ -1.0f, 0, 1.0f }, { 0.9f, 0.15f, 1.0f }), "muro_izquierdo", "marco izquierdo", true },
		{ caja({ 1.0f, 0, 1.0f }, { 0.9f, 0.15f, 1.0f }), "muro_derecho", "marco derecho", true }
	};
	comprobar(ValidatePlacement({ caja({ 0, 0, 1 }, { 0.08f, 0.12f, 0.95f }) }, {}, vano, false).valida,
		"la hoja y sus cajas caben en un vano con hueco real");
	const PlacementObstacle muroMacizo{ caja({ 0, 0, 1 }, { 2, 0.15f, 1 }), "muro", "pared maciza", true };
	const auto puertaSobreMuro = ValidatePlacement({ caja({ 0, 0, 1 }, { 0.08f, 0.12f, 0.95f }) }, {}, { muroMacizo }, false);
	comprobar(!puertaSobreMuro.valida && puertaSobreMuro.bloqueador.find("pared maciza") != std::string::npos,
		"una puerta no se coloca sobre muro macizo y el resultado identifica al bloqueador");
	comprobar(ValidatePlacement({ caja({ 0, 0, 1 }, { 0.08f, 0.12f, 0.95f }) }, {}, { muroMacizo }, true).valida,
		"la penetracion de arquitectura solo se permite con la excepcion opt-in");
	const PlacementObstacle mesa{ caja({ 0, 0, 0.5f }, { 1, 1, 0.5f }), "mesa", "mesa", false };
	comprobar(!ValidatePlacement({ caja({ 0, 0, 1 }, { 0.1f, 0.1f, 0.2f }) }, {}, { muroMacizo, mesa }, true).valida,
		"la excepcion arquitectonica no omite muebles");

	const auto legacyAnterior = caja({ 0, 0, 1 }, { 0.5f, 0.5f, 0.5f });
	comprobar(ValidatePlacement({ caja({ 0, 0, 1.1f }, { 0.5f, 0.5f, 0.5f }) }, { legacyAnterior }, { mesa }, false).valida,
		"una escena antigua puede editar una penetracion preexistente sin profundizarla");
	comprobar(!ValidatePlacement({ caja({ 0, 0, 0.8f }, { 0.5f, 0.5f, 0.5f }) }, { legacyAnterior }, { mesa }, false).valida,
		"una penetracion antigua no puede empeorarse");

	if (fallos == 0) std::cout << "EditorPlacementTests: todas las comprobaciones OBB pasaron.\n";
	return fallos == 0 ? 0 : 1;
}
