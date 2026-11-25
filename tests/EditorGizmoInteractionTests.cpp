#include "src/editor/CamaraEditor.h"
#include "imgui.h"
#include "ImGuizmo.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <iostream>

namespace
{
	int fallos = 0;
	void comprobar(bool condicion, const char* mensaje)
	{
		if (!condicion) { std::cerr << "FALLO: " << mensaje << '\n'; ++fallos; }
	}

	struct VentanaImGuiSinVentana
	{
		VentanaImGuiSinVentana()
		{
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			io.DisplaySize = ImVec2(800.0f, 600.0f);
			io.DeltaTime = 1.0f / 60.0f;
			unsigned char* pixels = nullptr;
			int ancho = 0, alto = 0;
			io.Fonts->GetTexDataAsRGBA32(&pixels, &ancho, &alto);
			ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
		}
		~VentanaImGuiSinVentana() { ImGui::DestroyContext(); }

		void Iniciar(ImVec2 cursor, bool botonIzquierdo)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.MousePos = cursor;
			io.MouseDown[ImGuiMouseButton_Left] = botonIzquierdo;
			ImGui::NewFrame();
			ImGuizmo::BeginFrame();
			ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
			ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);
			ImGuizmo::SetOrthographic(true);
		}
		void Terminar() { ImGui::EndFrame(); }
	};

	bool DibujarGizmoSiPermitido(editor::BloqueoGizmoTrasSeleccion& bloqueo, glm::mat4& matriz,
		const glm::mat4& vista, const glm::mat4& proyeccion)
	{
		if (!bloqueo.PermitirManipular(ImGui::GetIO().MouseDown[ImGuiMouseButton_Left])) return false;
		return ImGuizmo::Manipulate(&vista[0][0], &proyeccion[0][0], ImGuizmo::TRANSLATE,
			ImGuizmo::WORLD, &matriz[0][0], nullptr, nullptr);
	}
}

int main()
{
	using namespace editor;
	VentanaImGuiSinVentana imgui;
	const glm::vec3 centro(0.0f, 0.0f, -3.2f);
	const glm::mat4 vista = glm::lookAt(centro + glm::vec3(0, 0, 100), centro, glm::vec3(0, 1, 0));
	const glm::mat4 proyeccion = glm::ortho(-10.0f, 10.0f, -7.5f, 7.5f, 0.05f, 400.0f);
	glm::mat4 objeto = glm::translate(glm::mat4(1.0f), centro);
	const glm::mat4 original = objeto;
	BloqueoGizmoTrasSeleccion bloqueo;
	// El objeto recién seleccionado queda bajo el puntero. Se simula el clic y su
	// continuación con un movimiento grande que, sin arbitraje, podría iniciar el gizmo.
	bloqueo.Bloquear();
	imgui.Iniciar(ImVec2(400.0f, 300.0f), true);
	const bool cambioAlSeleccionar = DibujarGizmoSiPermitido(bloqueo, objeto, vista, proyeccion);
	imgui.Terminar();
	imgui.Iniciar(ImVec2(610.0f, 70.0f), true);
	const bool cambioAlArrastrarElClicDeSeleccion = DibujarGizmoSiPermitido(bloqueo, objeto, vista, proyeccion);
	imgui.Terminar();
	comprobar(!cambioAlSeleccionar && !cambioAlArrastrarElClicDeSeleccion && objeto == original,
		"ImGuizmo no recibe ni modifica la matriz durante el clic de seleccion");

	imgui.Iniciar(ImVec2(610.0f, 70.0f), false);
	comprobar(!DibujarGizmoSiPermitido(bloqueo, objeto, vista, proyeccion), "el fotograma que suelta el clic sigue bloqueado");
	imgui.Terminar();
	comprobar(!bloqueo.Bloqueado(), "al soltar se rearma la siguiente interaccion");

	if (fallos == 0) std::cout << "EditorGizmoInteractionTests: todo correcto\n";
	return fallos == 0 ? 0 : 1;
}
