#include "SceneEditor.h"

#include "ObjectPicker.h"
#include "imgui.h"
#include <ImGuizmo.h>
#include "src/physics/PhysicsScene.h"
#include "src/ui/UiScale.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>

namespace editor
{
	namespace
	{
		constexpr float MinimoEscala = 0.001f;
		constexpr float Pi = 3.14159265358979323846f;
		constexpr float AlturaBarra = 46.0f;
		constexpr float DimensionMinimaParaColisionador = 0.30f;   // por debajo es un objeto decorativo pequeño

		void AjustarVentanaAlCambiarEscala(const char* nombre, float escala, const ImVec2& displaySize)
		{
			struct EstadoVentana
			{
				float escala = 0.0f;
			};
			static std::unordered_map<std::string, EstadoVentana> estados;

			EstadoVentana& estado = estados[nombre];
			const ImVec2 tamanoActual = ImGui::GetWindowSize();
			const ImVec2 posicionActual = ImGui::GetWindowPos();
			if (estado.escala > 0.0f && std::abs(escala - estado.escala) > 0.001f)
			{
				const float factor = escala / estado.escala;
				const uiscale::UiWindowRect ajustada = uiscale::ScaleUiRect(
					{ posicionActual.x, posicionActual.y, tamanoActual.x, tamanoActual.y },
					factor, displaySize.x, displaySize.y);
				ImGui::SetWindowSize(ImVec2(ajustada.ancho, ajustada.alto), ImGuiCond_Always);
				ImGui::SetWindowPos(ImVec2(ajustada.x, ajustada.y), ImGuiCond_Always);
			}
			estado.escala = escala;
		}

		bool VectorFinito(const glm::vec3& v)
		{
			return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
		}

		bool MatrizGizmoValida(const glm::mat4& matriz)
		{
			for (int columna = 0; columna < 4; ++columna)
				for (int fila = 0; fila < 4; ++fila)
					if (!std::isfinite(matriz[columna][fila])) return false;
			return std::abs(matriz[0][3]) < 1.0e-4f && std::abs(matriz[1][3]) < 1.0e-4f &&
				std::abs(matriz[2][3]) < 1.0e-4f && std::abs(matriz[3][3] - 1.0f) < 1.0e-4f &&
				std::abs(glm::determinant(glm::mat3(matriz))) >= 1.0e-9f;
		}

		glm::mat4 MatrizDeAnimado(const SceneAnimatedObject& a)
		{
			glm::mat4 m = glm::translate(glm::mat4(1.0f), a.position);
			m = glm::rotate(m, glm::radians(a.yawDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
			return glm::scale(m, glm::vec3(a.escala));
		}

		const char* NombreClase(ElementClass clase)
		{
			switch (clase)
			{
			case ElementClass::Floor: return "floor";
			case ElementClass::Ceiling: return "ceiling";
			case ElementClass::Wall: return "wall";
			case ElementClass::Architecture: return "architecture";
			default: return "object";
			}
		}
	}

	// ---- Peticiones hacia src/app ------------------------------------------------------------

	bool SceneEditor::ConsumePlayEntry() { const bool v = exitToPlay; exitToPlay = false; return v; }
	bool SceneEditor::ConsumeSaveRequest() { const bool v = saveRequested; saveRequested = false; return v; }
	bool SceneEditor::ConsumeCloseConfirmed() { const bool v = closeConfirmed; closeConfirmed = false; return v; }
	bool SceneEditor::ConsumeCloseAfterSave() { const bool v = closeAfterSave; closeAfterSave = false; return v; }

	void SceneEditor::SetActive(bool activar)
	{
		if (activar == activo) return;
		activo = activar;
		if (!activo) { exitToPlay = true; EndCapture(); modoCamara = CameraMode::None; }   // el historial se conserva
	}

	void SceneEditor::SetModelsRoot(const std::filesystem::path& raiz)
	{
		modelsRoot = raiz;
		inventoryLoaded = false;
	}

	void SceneEditor::NewScene()
	{
		historial.Reset();
		glbEliminados.clear();
		rebuildPhysicsRequested = false;
		hayRespaldo = false;
		seleccionado = -1;
		bloqueoGizmoTrasSeleccion.Reset();
		modificada = false;
		cajasObj.clear();
		cajasGlb.clear();
		visibilidad.ShowAll();
		statusMessage.clear();
	}

	// ---- Entrada --------------------------------------------------------------------------------

	void SceneEditor::Shortcut(int tecla, int accion, int modificadores, bool escribiendoTexto)
	{
		const bool control = (modificadores & GLFW_MOD_CONTROL) != 0;
		const bool mayus = (modificadores & GLFW_MOD_SHIFT) != 0;
		if (accion == GLFW_PRESS &&
			tecla == GLFW_KEY_F6 &&
			!escribiendoTexto)
		{
			SetActive(!activo);
			return;
		}
		if (!activo) return;

		// Flechas / RePag / AvPag: mover el objeto seleccionado (o la cámara si no hay selección). Admiten repetición del teclado.
		ArrowKey flecha = ArrowKey::Up;
		bool esFlecha = true;
		switch (tecla)
		{
		case GLFW_KEY_UP: flecha = ArrowKey::Up; break;
		case GLFW_KEY_DOWN: flecha = ArrowKey::Down; break;
		case GLFW_KEY_LEFT: flecha = ArrowKey::Left; break;
		case GLFW_KEY_RIGHT: flecha = ArrowKey::Right; break;
		case GLFW_KEY_PAGE_UP: flecha = ArrowKey::Raise; break;
		case GLFW_KEY_PAGE_DOWN: flecha = ArrowKey::Lower; break;
		default: esFlecha = false; break;
		}
		if (esFlecha)
		{
			const unsigned bit = 1u << static_cast<unsigned>(flecha);
			if (accion == GLFW_RELEASE) { teclasFlechaPulsadas &= ~bit; return; }
			// Con un campo de texto o un widget en uso, las flechas son de ImGui y no tocan la escena.
			if (escribiendoTexto || entrada.imguiActivo) return;
			teclasFlechaPulsadas |= bit;
			pendingArrows.push_back({ flecha, control, mayus });
			return;
		}

		if (accion != GLFW_PRESS) return;
		// Con un campo de texto/numérico en edición, los atajos del editor no deben actuar (ImGui gestiona ahí sus teclas).
		if (escribiendoTexto) return;
		if (control)
		{
			if (tecla == GLFW_KEY_S) saveRequested = true;
			else if (tecla == GLFW_KEY_Z) { if (mayus) redoRequested = true; else undoRequested = true; }
			else if (tecla == GLFW_KEY_Y) redoRequested = true;
			else if (tecla == GLFW_KEY_D) duplicateRequested = true;
			return;
		}
		switch (tecla)
		{
		case GLFW_KEY_1: herramienta = EditorTool::Move; break;
		case GLFW_KEY_2: herramienta = EditorTool::Rotate; break;
		case GLFW_KEY_3: herramienta = EditorTool::Scale; break;
		case GLFW_KEY_F: focusRequested = true; break;
		case GLFW_KEY_DELETE: deleteRequested = true; break;
		default: break;
		}
	}

	void SceneEditor::EndCapture()
	{
		if (ventanaCaptura == nullptr) return;
		glfwSetInputMode(ventanaCaptura, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		glfwSetCursorPos(ventanaCaptura, cursorGuardadoX, cursorGuardadoY);   // el cursor reaparece donde estaba: sin saltos
		ventanaCaptura = nullptr;
		reiniciarDelta = true;
	}

	// Navegación de cámara. Prioridad de la entrada (de mayor a menor):
	//   1. widget de ImGui en uso  2. arrastre de ImGuizmo  3. navegación explícita (botón derecho/central, Alt+izquierdo)
	//   4. selección por clic  5. atajos de teclado.
	// La navegación solo puede EMPEZAR con el cursor en el viewport y sin 1 ni 2; una vez empezada sigue hasta soltar el botón.
	void SceneEditor::Update(GLFWwindow* ventana, float delta)
	{
		if (!activo || ventana == nullptr) return;
		const ImGuiIO& io = ImGui::GetIO();
		double x = 0.0, y = 0.0;
		glfwGetCursorPos(ventana, &x, &y);
		float dx = static_cast<float>(x - anteriorX);
		float dy = static_cast<float>(y - anteriorY);
		anteriorX = x;
		anteriorY = y;
		if (reiniciarDelta) { dx = dy = 0.0f; reiniciarDelta = false; }

		const bool derecho = glfwGetMouseButton(ventana, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
		const bool medio = glfwGetMouseButton(ventana, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
		const bool izquierdo = glfwGetMouseButton(ventana, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
		const bool alt = glfwGetKey(ventana, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(ventana, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
		const bool ctrl = glfwGetKey(ventana, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(ventana, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
		const bool mayus = glfwGetKey(ventana, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(ventana, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
		const bool ortografica = camara.IsOrthographic();

		if (modoCamara == CameraMode::None)
		{
			if (entrada.viewportLibre)
			{
				if (derecho) modoCamara = ortografica ? CameraMode::Pan : CameraMode::Look;
				else if (medio) modoCamara = CameraMode::Pan;
				else if (izquierdo && alt) modoCamara = ortografica ? CameraMode::Pan : CameraMode::Orbit;
			}
			if (modoCamara != CameraMode::None)
			{
				orbitPivot = camara.PivotPoint();
				reiniciarDelta = true;
				dx = dy = 0.0f;
				if (modoCamara == CameraMode::Look || modoCamara == CameraMode::Orbit)
				{	// cursor capturado: deltas ilimitados y sin salirse de la ventana
					cursorGuardadoX = x;
					cursorGuardadoY = y;
					ventanaCaptura = ventana;
					glfwSetInputMode(ventana, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				}
			}
		}
		else
		{
			const bool sigue = (modoCamara == CameraMode::Look) ? derecho
				: (modoCamara == CameraMode::Orbit) ? izquierdo
				: (derecho || medio || (izquierdo && alt));
			if (!sigue) { EndCapture(); modoCamara = CameraMode::None; return; }
		}

		switch (modoCamara)
		{
		case CameraMode::Look:
		{
			camara.Look(dx, dy, opciones);
			if (!io.WantTextInput)
			{
				const float mult = SpeedMultiplier(ctrl, mayus, opciones);   // Ctrl prevalece sobre Shift
				const auto tecla = [ventana](int k) { return glfwGetKey(ventana, k) == GLFW_PRESS ? 1.0f : 0.0f; };
				camara.Move(tecla(GLFW_KEY_W) - tecla(GLFW_KEY_S), tecla(GLFW_KEY_D) - tecla(GLFW_KEY_A),
					tecla(GLFW_KEY_E) - tecla(GLFW_KEY_Q), delta, mult, opciones);
			}
			break;
		}
		case CameraMode::Orbit: camara.Orbit(orbitPivot, dx, dy, opciones); break;
		case CameraMode::Pan: camara.Pan(dx, dy, io.DisplaySize.y, opciones); break;
		default: break;
		}
	}

	void SceneEditor::MouseWheelInput(float cantidad)
	{
		if (!activo) return;
		// La rueda es del panel ImGui si el cursor está encima (Hierarchy, Inspector, Asset Browser...) y no de la cámara.
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || ImGuizmo::IsUsing() || modoCamara != CameraMode::None) return;
		camara.Zoom(cantidad, opciones);
	}

	void SceneEditor::Matrices(float aspecto, glm::mat4& vista, glm::mat4& proyeccion) const
	{
		camara.Matrices(aspecto, vista, proyeccion);
	}

	// ---- Estado, historial -------------------------------------------------------------------

	SceneState SceneEditor::CaptureHistory(const EditorSceneContext& contexto) const
	{
		return SceneState{ contexto.objetos, contexto.animados };
	}

	void SceneEditor::SyncGlb(const EditorSceneContext& contexto) const
	{
		for (const SceneAnimatedObject& animado : contexto.animados)
		{
			if (glb::AnimatedObject* instancia = glb::FindAnimatedObject(contexto.escenaGlb, animado.id))
			{
				instancia->position = animado.position;
				instancia->yawDegrees = animado.yawDegrees;
				instancia->escala = animado.escala;
			}
			if (!animado.paredId.empty() && !animado.openingId.empty())
			{
				auto pared = std::find_if(contexto.objetos.begin(), contexto.objetos.end(),
					[&](const SceneObject& item) { return item.id == animado.paredId && item.pared.presente; });
				if (pared == contexto.objetos.end()) continue;
				const float yaw = glm::radians(pared->eulerRotation.z);
				const glm::vec3 ejeU(std::cos(yaw), std::sin(yaw), 0.0f);
				auto apertura = std::find_if(pared->pared.aperturas.begin(), pared->pared.aperturas.end(),
					[&](const SceneWallOpening& item) { return item.id == animado.openingId; });
				if (apertura == pared->pared.aperturas.end()) continue;
				apertura->centerU = glm::dot(animado.position - pared->position, ejeU);
				const glm::vec3 mitad = glm::abs(pared->escala) * (pared->fisica.presente ? pared->fisica.halfExtents : glm::vec3(1.0f));
				apertura->base = std::max(0.0f, animado.position.z - (pared->position.z - mitad.z));
			}
		}
	}

	void SceneEditor::RestorePose(EditorSceneContext contexto, SceneState estado)
	{
		contexto.objetos = std::move(estado.objetos);
		contexto.animados = std::move(estado.animados);
		// Reconcile GLB instances by stable id. Models stay cached in EscenaAnimada::modelos;
		// undo only restores an instance and never reloads or removes the shared asset.
		for (auto it = contexto.escenaGlb.objetos.begin(); it != contexto.escenaGlb.objetos.end();)
		{
			const bool remains = std::any_of(contexto.animados.begin(), contexto.animados.end(),
				[&](const SceneAnimatedObject& item) { return item.id == it->id; });
			if (!remains) it = contexto.escenaGlb.objetos.erase(it); else ++it;
		}
		for (const SceneAnimatedObject& data : contexto.animados)
		{
			if (glb::FindAnimatedObject(contexto.escenaGlb, data.id)) continue;
			auto cached = glbEliminados.find(data.id);
			if (cached != glbEliminados.end()) contexto.escenaGlb.objetos.push_back(cached->second);
		}
		SyncGlb(contexto);   // vista y cajas se rehacen solas: en EDIT todo se dibuja desde estas listas
		rebuildPhysicsRequested = true;
		const int total = static_cast<int>(contexto.objetos.size() + contexto.animados.size());
		if (seleccionado >= total) SetSelection(total - 1);
		hayRespaldo = false;
		MarkModified();
	}

	bool SceneEditor::Undo(EditorSceneContext contexto)
	{
		SceneState actual = CaptureHistory(contexto);
		if (!historial.Undo(actual)) { ReportStatus("Nothing to undo"); return false; }
		RestorePose(contexto, std::move(actual));
		ReportStatus("Undo");
		return true;
	}

	bool SceneEditor::Redo(EditorSceneContext contexto)
	{
		SceneState actual = CaptureHistory(contexto);
		if (!historial.Redo(actual)) { ReportStatus("Nothing to redo"); return false; }
		RestorePose(contexto, std::move(actual));
		ReportStatus("Redo");
		return true;
	}

	void SceneEditor::OpenTransactionIfNeeded()
	{
		if (!historial.IsTransactionOpen() && hayRespaldo) historial.BeginTransaction(respaldo);
	}

	void SceneEditor::CommitTransactionIfFinished(const EditorSceneContext& contexto)
	{
		if (historial.IsTransactionOpen() && !ImGuizmo::IsUsing() && !ImGui::IsAnyItemActive() && teclasFlechaPulsadas == 0)
			historial.CommitTransaction(CaptureHistory(contexto));
	}

	bool SceneEditor::IsValidIndex(const EditorSceneContext& contexto, int indice) const
	{
		return indice >= 0 && indice < static_cast<int>(contexto.objetos.size() + contexto.animados.size());
	}

	std::string SceneEditor::IdOf(const EditorSceneContext& contexto, int indice) const
	{
		if (!IsValidIndex(contexto, indice)) return {};
		if (!IsAnimated(contexto, indice)) return contexto.objetos[static_cast<std::size_t>(indice)].id;
		return contexto.animados[static_cast<std::size_t>(indice) - contexto.objetos.size()].id;
	}

	std::string SceneEditor::UniqueId(const EditorSceneContext& contexto, const std::string& base) const
	{
		auto existe = [&contexto](const std::string& id) {
			return std::any_of(contexto.objetos.begin(), contexto.objetos.end(), [&id](const SceneObject& o) { return o.id == id; }) ||
				std::any_of(contexto.animados.begin(), contexto.animados.end(), [&id](const SceneAnimatedObject& a) { return a.id == id; });
		};
		std::string id = base;
		for (unsigned int sufijo = 1; existe(id); ++sufijo) id = base + "_" + std::to_string(sufijo);
		return id;
	}

	// ---- Cajas y selección ------------------------------------------------------------------

	LocalBox SceneEditor::BoxOfObject(const SceneObject& objeto)
	{
		auto it = cajasObj.find(objeto.modelo);
		if (it == cajasObj.end())
		{
			LocalBox caja = ObjBounds(objeto.modelo);
			it = cajasObj.emplace(objeto.modelo, caja).first;
		}
		LocalBox caja = it->second;
		if (!caja.valida)
		{	// OBJ ilegible: usa la caja de física, o un cubo unitario, para que siga siendo seleccionable
			caja.valida = true;
			if (objeto.fisica.presente)
			{
				caja.minimo = objeto.fisica.desplazamiento - objeto.fisica.halfExtents;
				caja.maximo = objeto.fisica.desplazamiento + objeto.fisica.halfExtents;
			}
		}
		return WithMinimumHalfExtent(caja, 0.02f);
	}

	LocalBox SceneEditor::BoxOfAnimated(const EditorSceneContext& contexto, const SceneAnimatedObject& animado)
	{
		auto it = cajasGlb.find(animado.modelo);
		if (it == cajasGlb.end())
		{
			LocalBox caja;
			if (glb::AnimatedObject* instancia = glb::FindAnimatedObject(contexto.escenaGlb, animado.id))
				if (instancia->modelo != nullptr) caja = GlbBounds(*instancia->modelo);
			it = cajasGlb.emplace(animado.modelo, caja).first;
		}
		LocalBox caja = it->second;
		if (!caja.valida) { caja.valida = true; caja.minimo = glm::vec3(-0.5f, -0.1f, 0.0f); caja.maximo = glm::vec3(0.5f, 0.1f, 2.1f); }
		return WithMinimumHalfExtent(caja, 0.05f);   // una puerta es fina: se garantiza un grosor seleccionable
	}

	SceneEditor::SelectableElement SceneEditor::ElementAt(const EditorSceneContext& contexto, int indice)
	{
		SelectableElement e;
		e.indice = indice;
		if (!IsValidIndex(contexto, indice)) return e;
		if (!IsAnimated(contexto, indice))
		{
			const SceneObject& objeto = contexto.objetos[static_cast<std::size_t>(indice)];
			e.modelo = physics::ObjectMatrix(objeto);
			e.caja = BoxOfObject(objeto);
		}
		else
		{
			const SceneAnimatedObject& animado = contexto.animados[static_cast<std::size_t>(indice) - contexto.objetos.size()];
			e.modelo = MatrizDeAnimado(animado);
			e.caja = BoxOfAnimated(contexto, animado);
		}
		return e;
	}

	std::vector<OrientedBox> SceneEditor::BoxesOfObject(const SceneObject& objeto, const glm::mat4& matriz)
	{
		if (objeto.pared.presente && !objeto.pared.aperturas.empty())
		{
			const glm::vec3 escala(glm::length(glm::vec3(matriz[0])), glm::length(glm::vec3(matriz[1])), glm::length(glm::vec3(matriz[2])));
			glm::mat4 pose = glm::translate(glm::mat4(1.0f), glm::vec3(matriz[3]));
			glm::mat3 eulerRotation;
			for (int eje = 0; eje < 3; ++eje) eulerRotation[eje] = glm::normalize(glm::vec3(matriz[eje]));
			pose *= glm::mat4(eulerRotation);
			const glm::vec3 mitadPared = glm::abs(escala) * (objeto.fisica.presente ? objeto.fisica.halfExtents : BoxOfObject(objeto).HalfSize());
			std::vector<SceneWallSection> secciones;
			std::string error;
			if (GenerateWallSections(mitadPared, objeto.pared.aperturas, secciones, error))
			{
				std::vector<OrientedBox> cajas;
				for (const auto& seccion : secciones)
					cajas.push_back(TransformBox(pose, seccion.centroLocal, seccion.mitad));
				return cajas;
			}
		}
		if (objeto.fisica.presente)
		{
			const glm::mat4 collider = matriz * glm::translate(glm::mat4(1.0f), objeto.fisica.desplazamiento);
			return { TransformBox(collider, glm::vec3(0.0f), objeto.fisica.halfExtents) };
		}
		const LocalBox limites = BoxOfObject(objeto);
		return { TransformBox(matriz, limites.CenterPoint(), limites.HalfSize()) };
	}

	std::vector<OrientedBox> SceneEditor::BoxesOfAnimated(const EditorSceneContext& contexto, const SceneAnimatedObject& animado)
	{
		const glb::AnimatedObject* instancia = glb::FindAnimatedObject(contexto.escenaGlb, animado.id);
		if (instancia != nullptr && instancia->modelo != nullptr && !animado.colisionadores.empty())
		{
			glm::mat4 base = glm::translate(glm::mat4(1.0f), animado.position);
			base = glm::rotate(base, glm::radians(animado.yawDegrees), glm::vec3(0, 0, 1));
			base = glm::scale(base, glm::vec3(animado.escala));
			std::vector<OrientedBox> cajas;
			for (const SceneCollider& datos : animado.colisionadores)
			{
				const int nodo = glb::FindNodeByName(*instancia->modelo, datos.nodo);
				if (nodo < 0) continue;
				const glm::mat4 mundoNodo = base * glb::YUpToZUpMatrix() *
					glb::NodeWorldMatrix(*instancia->modelo, instancia->pose, nodo);
				cajas.push_back(TransformBox(mundoNodo, datos.desplazamiento, datos.halfExtents));
			}
			if (!cajas.empty()) return cajas;
		}
		return { TransformBox(MatrizDeAnimado(animado), BoxOfAnimated(contexto, animado).CenterPoint(),
			BoxOfAnimated(contexto, animado).HalfSize()) };
	}

	PlacementResult SceneEditor::ValidateObject(const EditorSceneContext& contexto, const SceneObject& candidato, int indiceIgnorado,
		const glm::mat4* matrizPropuesta)
	{
		const SceneObject& actual = indiceIgnorado >= 0 && indiceIgnorado < static_cast<int>(contexto.objetos.size())
			? contexto.objetos[static_cast<std::size_t>(indiceIgnorado)] : candidato;
		const auto propuesta = BoxesOfObject(candidato, matrizPropuesta != nullptr ? *matrizPropuesta : physics::ObjectMatrix(candidato));
		const auto anterior = indiceIgnorado >= 0 && indiceIgnorado < static_cast<int>(contexto.objetos.size())
			? BoxesOfObject(actual, physics::ObjectMatrix(actual)) : std::vector<OrientedBox>{};
		std::vector<PlacementObstacle> obstaculos;
		constexpr float toleranciaContacto = 0.01f;
		for (std::size_t i = 0; i < contexto.objetos.size(); ++i)
		{
			if (static_cast<int>(i) == indiceIgnorado || contexto.objetos[i].id == candidato.id) continue;
			const SceneObject& objeto = contexto.objetos[i];
			const auto cajas = BoxesOfObject(objeto, physics::ObjectMatrix(objeto));
			const glm::vec3 dimensiones = objeto.fisica.presente
				? glm::abs(objeto.escala) * objeto.fisica.halfExtents
				: glm::abs(objeto.escala) * BoxOfObject(objeto).HalfSize();
			const float minimo = std::min({ dimensiones.x, dimensiones.y, dimensiones.z });
			const float medio = dimensiones.x + dimensiones.y + dimensiones.z - minimo - std::max({ dimensiones.x, dimensiones.y, dimensiones.z });
			const float maximo = std::max({ dimensiones.x, dimensiones.y, dimensiones.z });
			const bool paredPorForma = objeto.fisica.presente && objeto.fisica.tipo == physics::BodyType::Static &&
				minimo < 0.30f && medio > 1.0f && maximo > 4.0f;
			const bool arquitectura = objeto.categoria == "architecture" || paredPorForma;
			const std::string tipo = paredPorForma && objeto.categoria != "architecture" ? "wall" :
				objeto.categoria == "architecture" ? "architecture" : "object";
			for (const OrientedBox& caja : cajas)
				obstaculos.push_back({ caja, objeto.id, tipo, arquitectura });
		}
		for (const SceneAnimatedObject& animado : contexto.animados)
		{
			if (animado.id == candidato.id) continue;
			for (const OrientedBox& caja : BoxesOfAnimated(contexto, animado))
				obstaculos.push_back({ caja, animado.id, "animated object", false });
		}
		return editor::ValidatePlacement(propuesta, anterior, obstaculos,
			candidato.permitirPenetracionArquitectura, toleranciaContacto);
	}

	PlacementResult SceneEditor::ValidateAnimated(const EditorSceneContext& contexto, const SceneAnimatedObject& candidato, int indiceIgnorado)
	{
		const SceneAnimatedObject& actual = indiceIgnorado >= static_cast<int>(contexto.objetos.size()) &&
			indiceIgnorado < static_cast<int>(contexto.objetos.size() + contexto.animados.size())
			? contexto.animados[static_cast<std::size_t>(indiceIgnorado) - contexto.objetos.size()] : candidato;
		const auto propuesta = BoxesOfAnimated(contexto, candidato);
		const auto anterior = indiceIgnorado >= static_cast<int>(contexto.objetos.size()) &&
			indiceIgnorado < static_cast<int>(contexto.objetos.size() + contexto.animados.size())
			? BoxesOfAnimated(contexto, actual) : std::vector<OrientedBox>{};
		std::vector<PlacementObstacle> obstaculos;
		for (std::size_t i = 0; i < contexto.objetos.size(); ++i)
		{
			SceneObject objeto = contexto.objetos[i];
			if (objeto.id == candidato.paredId && !candidato.openingId.empty())
			{
				auto apertura = std::find_if(objeto.pared.aperturas.begin(), objeto.pared.aperturas.end(),
					[&](const SceneWallOpening& a) { return a.id == candidato.openingId; });
				if (apertura != objeto.pared.aperturas.end())
				{
					const float yaw = glm::radians(objeto.eulerRotation.z);
					const glm::vec3 ejeU(std::cos(yaw), std::sin(yaw), 0.0f);
					apertura->centerU = glm::dot(candidato.position - objeto.position, ejeU);
					const glm::vec3 mitad = glm::abs(objeto.escala) * (objeto.fisica.presente ? objeto.fisica.halfExtents : glm::vec3(1.0f));
					apertura->base = candidato.position.z - (objeto.position.z - mitad.z);
					std::vector<SceneWallSection> secciones;
					std::string error;
					if (!GenerateWallSections(mitad, objeto.pared.aperturas, secciones, error))
						return { false, "wall opening: " + error };
					const float normalOffset = glm::length(candidato.position - objeto.position - ejeU * apertura->centerU - glm::vec3(0, 0, candidato.position.z - objeto.position.z));
					if (normalOffset > mitad.y + 0.15f || apertura->base < -0.01f)
						return { false, "door must remain aligned with its associated wall" };
				}
			}
			const auto cajas = BoxesOfObject(objeto, physics::ObjectMatrix(objeto));
			const glm::vec3 dimensiones = objeto.fisica.presente
				? glm::abs(objeto.escala) * objeto.fisica.halfExtents
				: glm::abs(objeto.escala) * BoxOfObject(objeto).HalfSize();
			const float minimo = std::min({ dimensiones.x, dimensiones.y, dimensiones.z });
			const float medio = dimensiones.x + dimensiones.y + dimensiones.z - minimo - std::max({ dimensiones.x, dimensiones.y, dimensiones.z });
			const float maximo = std::max({ dimensiones.x, dimensiones.y, dimensiones.z });
			const bool arquitectura = objeto.categoria == "architecture" ||
				(objeto.fisica.presente && objeto.fisica.tipo == physics::BodyType::Static && minimo < 0.30f && medio > 1.0f && maximo > 4.0f);
			for (const OrientedBox& caja : cajas) obstaculos.push_back({ caja, objeto.id, arquitectura ? "architecture" : "object", arquitectura });
		}
		for (std::size_t i = 0; i < contexto.animados.size(); ++i)
		{
			const SceneAnimatedObject& otro = contexto.animados[i];
			if (static_cast<int>(contexto.objetos.size() + i) == indiceIgnorado || otro.id == candidato.id) continue;
			for (const OrientedBox& caja : BoxesOfAnimated(contexto, otro))
				obstaculos.push_back({ caja, otro.id, "animated object", false });
		}
		return editor::ValidatePlacement(propuesta, anterior, obstaculos,
			candidato.permitirPenetracionArquitectura, 0.01f);
	}

	void SceneEditor::SelectByMouse(EditorSceneContext contexto, int ancho, int alto)
	{
		const ImGuiIO& io = ImGui::GetIO();
		// Prioridad 4: solo un clic simple en el viewport (no sobre un panel, ni arrastrando el gizmo, ni navegando con Alt/botones).
		if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) || !entrada.viewportLibre || entrada.altPressed ||
			modoCamara != CameraMode::None || ImGuizmo::IsOver()) return;
		if (ancho <= 0 || alto <= 0) return;

		glm::mat4 vista, proyeccion;
		Matrices(static_cast<float>(ancho) / static_cast<float>(alto), vista, proyeccion);
		const PickRay rayo = RayFromScreen(vista, proyeccion, glm::vec2(io.MousePos.x, io.MousePos.y),
			glm::vec4(0.0f, 0.0f, static_cast<float>(ancho), static_cast<float>(alto)));

		std::vector<SelectionCandidate> candidatos;
		const int total = static_cast<int>(contexto.objetos.size() + contexto.animados.size());
		for (int i = 0; i < total; ++i)
		{
			if (!IsAnimated(contexto, i))
			{
				if (!IsObjectVisible(static_cast<std::size_t>(i))) continue;   // lo oculto nunca bloquea la selección
			}
			else if (!visibilidad.AnimatedVisible(IdOf(contexto, i))) continue;
			const SelectableElement e = ElementAt(contexto, i);
			candidatos.push_back({ e.indice, e.modelo, e.caja });
		}
		SetSelection(PickNearest(rayo, candidatos));
	}

	void SceneEditor::UpdateVisibility(const EditorSceneContext& contexto)
	{
		clases = ClassifyElements(contexto.objetos);
		visibles.assign(contexto.objetos.size(), 1);
		const bool superior = camara.vista == ViewKind::Top;
		for (std::size_t i = 0; i < contexto.objetos.size(); ++i)
			visibles[i] = visibilidad.Visible(contexto.objetos[i], clases[i], superior) ? 1 : 0;
		aisladoEsAnimado = visibilidad.isolateSelection &&
			std::any_of(contexto.animados.begin(), contexto.animados.end(),
				[this](const SceneAnimatedObject& a) { return a.id == visibilidad.isolatedId; });
	}

	bool SceneEditor::IsObjectVisible(std::size_t indice) const
	{
		return !activo || indice >= visibles.size() || visibles[indice] != 0;
	}

	bool SceneEditor::ShowAnimated() const
	{
		return !activo || !visibilidad.isolateSelection || aisladoEsAnimado;
	}

	void SceneEditor::FocusSelection(const EditorSceneContext& contexto)
	{
		if (!IsValidIndex(contexto, seleccionado)) return;
		const SelectableElement e = ElementAt(contexto, seleccionado);
		const LocalBox mundo = BoxInWorld(e.modelo, e.caja);
		if (!mundo.valida) return;
		camara.FocusOn(mundo.CenterPoint(), glm::length(mundo.HalfSize()));
	}

	float SceneEditor::FloorHeight(const EditorSceneContext& contexto) const
	{
		const std::vector<ElementClass> c = ClassifyElements(contexto.objetos);
		float techoDelSuelo = std::numeric_limits<float>::lowest();
		for (std::size_t i = 0; i < contexto.objetos.size(); ++i)
		{
			if (c[i] != ElementClass::Floor) continue;
			const SceneObject& o = contexto.objetos[i];
			const float mitad = std::abs(o.escala.z) * (o.fisica.presente ? o.fisica.halfExtents.z : 1.0f);
			techoDelSuelo = std::max(techoDelSuelo, o.position.z + mitad);
		}
		return techoDelSuelo;
	}

	// ---- Operaciones ---------------------------------------------------------------------------

	std::string SceneEditor::AppendModel(EditorSceneContext contexto, const std::string& clave, bool yArribaAZArriba,
		bool colisionador, bool dinamico)
	{
		if (clave.empty()) return {};

		SceneObject objeto;   // todo explícito: una escala sin inicializar (0) deja el modelo invisible y el JSON inválido
		objeto.id = UniqueId(contexto, "obj_" + std::filesystem::path(clave).stem().string());
		objeto.categoria = "objects";
		objeto.modelo = clave;
		objeto.position = glm::vec3(0.0f);
		objeto.eulerRotation = yArribaAZArriba ? glm::vec3(90.0f, 0.0f, 0.0f) : glm::vec3(0.0f);
		objeto.escala = glm::vec3(1.0f);

		LocalBox caja = BoxOfObject(objeto);
		const LocalBox mundo = BoxInWorld(physics::ObjectMatrix(objeto), caja);
		const float suelo = FloorHeight(contexto);
		const glm::vec3 centro = mundo.CenterPoint();
		const glm::vec3 objetivo = camara.PivotPoint();
		objeto.position.x = objetivo.x - centro.x;
		objeto.position.y = objetivo.y - centro.y;
		// Apoyado sobre el suelo si la sala lo tiene; si no, centrado en el punto que mira la cámara.
		objeto.position.z = suelo > std::numeric_limits<float>::lowest() / 2.0f ? suelo - mundo.minimo.z : objetivo.z - centro.z;

		std::string aviso;
		if (colisionador)
		{
			const glm::vec3 tamano = mundo.maximo - mundo.minimo;
			if (std::max({ tamano.x, tamano.y, tamano.z }) < DimensionMinimaParaColisionador)
				aviso = " (small decorative object: no collider created; add one from Inspector > Physics)";
			else
			{
				objeto.fisica.presente = true;
				objeto.fisica.tipo = dinamico ? physics::BodyType::Dynamic : physics::BodyType::Static;
				objeto.fisica.halfExtents = caja.HalfSize();
				objeto.fisica.desplazamiento = caja.CenterPoint();
				objeto.fisica.masa = dinamico ? 5.0f : 0.0f;
			}
		}
		PlacementResult resultado = ValidateObject(contexto, objeto, -1);
		// Si no cabe junto al pivote, se deja en una zona de preparacion a la derecha de la escena.
		bool enZonaPreparacion = false;
		if (!resultado.valida)
		{
			std::vector<OrientedBox> cajasObstaculo;
			for (const SceneObject& existente : contexto.objetos)
			{
				const auto cajas = BoxesOfObject(existente, physics::ObjectMatrix(existente));
				cajasObstaculo.insert(cajasObstaculo.end(), cajas.begin(), cajas.end());
			}
			for (const SceneAnimatedObject& existente : contexto.animados)
			{
				const auto cajas = BoxesOfAnimated(contexto, existente);
				cajasObstaculo.insert(cajasObstaculo.end(), cajas.begin(), cajas.end());
			}

			const auto cajasPropuestas = BoxesOfObject(objeto, physics::ObjectMatrix(objeto));
			float desplazamientoX = 0.0f;
			if (CalculateStagingOffsetX(cajasPropuestas, cajasObstaculo, 1.0f, desplazamientoX) &&
				std::isfinite(objeto.position.x + desplazamientoX))
			{
				objeto.position.x += desplazamientoX;
				resultado = ValidateObject(contexto, objeto, -1);
				enZonaPreparacion = resultado.valida;
			}
		}
		if (!resultado.valida)
		{
			placementMessage = "Placement blocked by " + resultado.bloqueador;
			messageIsError = true;
			ReportStatus(placementMessage, true);
			return {};
		}
		placementMessage.clear();
		historial.RegisterElement(CaptureHistory(contexto));
		const std::string id = objeto.id;
		contexto.objetos.push_back(std::move(objeto));
		SetSelection(static_cast<int>(contexto.objetos.size()) - 1);
		if (enZonaPreparacion) FocusSelection(contexto);
		hayRespaldo = false;
		MarkModified();
		ReportStatus(enZonaPreparacion
			? "Added " + id + " in the staging area; scale it down, then move it into the scene." + aviso
			: "Added " + id + aviso);
		return id;
	}

	bool SceneEditor::DuplicateSelection(EditorSceneContext contexto)
	{
		if (!IsValidIndex(contexto, seleccionado)) return false;
		if (IsAnimated(contexto, seleccionado))
		{
			SceneAnimatedObject copia = contexto.animados[static_cast<std::size_t>(seleccionado) - contexto.objetos.size()];
			if (copia.paredId.empty() || copia.openingId.empty())
			{ ReportStatus("Install this door in a parametric wall before duplicating it.", true); return false; }
			const int paredIndex = static_cast<int>(std::distance(contexto.objetos.begin(), std::find_if(contexto.objetos.begin(), contexto.objetos.end(),
				[&](const SceneObject& item) { return item.id == copia.paredId && item.pared.presente; })));
			if (paredIndex < 0 || paredIndex >= static_cast<int>(contexto.objetos.size()))
			{ ReportStatus("The associated wall no longer exists.", true); return false; }
			SceneObject& pared = contexto.objetos[static_cast<std::size_t>(paredIndex)];
			auto apertura = std::find_if(pared.pared.aperturas.begin(), pared.pared.aperturas.end(),
				[&](const SceneWallOpening& a) { return a.id == copia.openingId; });
			if (apertura == pared.pared.aperturas.end())
			{ ReportStatus("The door opening association is missing.", true); return false; }
			copia.id = UniqueId(contexto, copia.id + "_copy");
			copia.openingId = copia.id;
			SceneWallOpening nuevoVano = *apertura;
			nuevoVano.id = copia.id;
			nuevoVano.idPuerta = copia.id;
			const float desplazamientoU = nuevoVano.centerU + 1.25f + nuevoVano.ancho * 0.5f <= std::abs(pared.escala.x) - 0.1f ? 1.25f : -1.25f;
			nuevoVano.centerU += desplazamientoU;
			std::vector<SceneWallSection> secciones;
			std::vector<SceneWallOpening> vanos = pared.pared.aperturas;
			vanos.push_back(nuevoVano);
			std::string errorVano;
			if (!GenerateWallSections(glm::abs(pared.escala) * pared.fisica.halfExtents, vanos, secciones, errorVano))
			{ ReportStatus("Door duplicate blocked: " + errorVano, true); return false; }
			const float angulo = glm::radians(pared.eulerRotation.z);
			const glm::vec3 ejeU(std::cos(angulo), std::sin(angulo), 0.0f);
			copia.position += ejeU * desplazamientoU;
			copia.paredId = pared.id;
			const SceneState estadoAnterior = CaptureHistory(contexto);
			pared.pared.aperturas.push_back(nuevoVano);
			contexto.animados.push_back(copia);
			if (const glb::AnimatedObject* original = glb::FindAnimatedObject(contexto.escenaGlb,
				contexto.animados[static_cast<std::size_t>(seleccionado) - contexto.objetos.size()].id))
			{
				glb::AnimatedObject clon = *original;
				clon.id = copia.id;
				clon.position = copia.position;
				clon.yawDegrees = copia.yawDegrees;
				clon.escala = copia.escala;
				clon.pose = glb::CreateRestPose(*clon.modelo);
				clon.estado = glb::DoorState::Closed;
				clon.reproductor = {};
				glbEliminados[copia.id] = clon; // permite que Redo reconstruya la instancia sin recargar el modelo compartido
				contexto.escenaGlb.objetos.push_back(std::move(clon));
			}
			else
			{
				contexto.animados.pop_back();
				pared.pared.aperturas.pop_back();
				ReportStatus("The shared GLB model is not loaded.", true); return false;
			}
			const int nuevoIndice = static_cast<int>(contexto.objetos.size() + contexto.animados.size() - 1);
			const auto validacion = ValidateAnimated(contexto, contexto.animados.back(), nuevoIndice);
			if (!validacion.valida)
			{
				contexto.animados.pop_back();
				contexto.escenaGlb.objetos.pop_back();
				pared.pared.aperturas.pop_back();
				ReportStatus("Door duplicate blocked by " + validacion.bloqueador, true); return false;
			}
			historial.RegisterElement(estadoAnterior);
			rebuildPhysicsRequested = true;
			SetSelection(nuevoIndice); hayRespaldo = false; MarkModified();
			ReportStatus("Door duplicated with a separate wall opening.");
			return true;
		}
		SceneObject copia = contexto.objetos[static_cast<std::size_t>(seleccionado)];
		copia.id = UniqueId(contexto, copia.id + "_copy");
		copia.categoria = "objects";
		copia.position.x += 0.5f;
		const PlacementResult resultado = ValidateObject(contexto, copia, -1);
		if (!resultado.valida)
		{
			placementMessage = "Duplicate blocked by " + resultado.bloqueador;
			ReportStatus(placementMessage, true);
			return false;
		}
		placementMessage.clear();
		historial.RegisterElement(CaptureHistory(contexto));
		contexto.objetos.push_back(std::move(copia));
		SetSelection(static_cast<int>(contexto.objetos.size()) - 1);
		hayRespaldo = false;
		MarkModified();
		ReportStatus("Duplicated");
		return true;
	}

	bool SceneEditor::DeleteSelection(EditorSceneContext contexto, bool cerrarHueco)
	{
		if (!IsValidIndex(contexto, seleccionado)) return false;
		historial.RegisterElement(CaptureHistory(contexto));
		std::string id;
		if (IsAnimated(contexto, seleccionado))
		{
			const std::size_t index = static_cast<std::size_t>(seleccionado) - contexto.objetos.size();
			const SceneAnimatedObject& datos = contexto.animados[index];
			id = datos.id;
			if (!datos.paredId.empty() && !datos.openingId.empty())
			{
				auto pared = std::find_if(contexto.objetos.begin(), contexto.objetos.end(),
					[&](const SceneObject& item) { return item.id == datos.paredId && item.pared.presente; });
				if (pared != contexto.objetos.end())
				{
					auto apertura = std::find_if(pared->pared.aperturas.begin(), pared->pared.aperturas.end(),
						[&](const SceneWallOpening& a) { return a.id == datos.openingId && a.idPuerta == datos.id; });
					if (apertura != pared->pared.aperturas.end())
					{
						if (cerrarHueco) pared->pared.aperturas.erase(apertura);
						else apertura->idPuerta.clear();
					}
				}
			}
			if (glb::AnimatedObject* instancia = glb::FindAnimatedObject(contexto.escenaGlb, id))
			{
				glbEliminados[id] = *instancia;
				auto& instancias = contexto.escenaGlb.objetos;
				instancias.erase(std::remove_if(instancias.begin(), instancias.end(),
					[&](const glb::AnimatedObject& item) { return item.id == id; }), instancias.end());
			}
			contexto.animados.erase(contexto.animados.begin() + static_cast<std::ptrdiff_t>(index));
			rebuildPhysicsRequested = true;
		}
		else
		{
			id = contexto.objetos[static_cast<std::size_t>(seleccionado)].id;
			contexto.objetos.erase(contexto.objetos.begin() + seleccionado);
		}
		const int total = static_cast<int>(contexto.objetos.size() + contexto.animados.size());
		SetSelection(total == 0 ? -1 : std::min(seleccionado, total - 1));
		hayRespaldo = false;
		MarkModified();
		ReportStatus(std::string("Deleted ") + id + (cerrarHueco ? " and closed its wall opening" : "; wall opening kept") + " (Ctrl+Z to restore)");
		return true;
	}

	bool SceneEditor::InstallDoorInWall(EditorSceneContext contexto, int indicePuerta, int indicePared)
	{
		if (indicePuerta < 0 || indicePuerta >= static_cast<int>(contexto.animados.size()) ||
			indicePared < 0 || indicePared >= static_cast<int>(contexto.objetos.size())) return false;
		SceneAnimatedObject& puerta = contexto.animados[static_cast<std::size_t>(indicePuerta)];
		SceneObject& paredNueva = contexto.objetos[static_cast<std::size_t>(indicePared)];
		if (!paredNueva.pared.presente || (paredNueva.fisica.presente && paredNueva.fisica.tipo != physics::BodyType::Static))
		{ ReportStatus("Select a parametric wall with static collision first.", true); return false; }
		float mitadAncho = 0.0f, alturaMarco = 0.0f;
		for (const SceneCollider& c : puerta.colisionadores)
			if (c.nodo == "DoorFrame")
			{
				mitadAncho = std::max(mitadAncho, std::abs(c.desplazamiento.x) + c.halfExtents.x);
				alturaMarco = std::max(alturaMarco, c.desplazamiento.y + c.halfExtents.y);
			}
		if (!(mitadAncho > 0.0f && alturaMarco > 0.0f))
		{ ReportStatus("DoorFrame colliders are required to derive the opening dimensions.", true); return false; }
		const float a = glm::radians(paredNueva.eulerRotation.z);
		const glm::vec3 ejeU(std::cos(a), std::sin(a), 0.0f);
		const glm::vec3 delta = puerta.position - paredNueva.position;
		SceneWallOpening nueva;
		nueva.id = puerta.id + "_opening";
		for (unsigned suffix = 2; std::any_of(paredNueva.pared.aperturas.begin(), paredNueva.pared.aperturas.end(),
			[&](const SceneWallOpening& x) { return x.id == nueva.id; }); ++suffix)
			nueva.id = puerta.id + "_opening_" + std::to_string(suffix);
		nueva.idPuerta = puerta.id;
		nueva.centerU = glm::dot(delta, ejeU);
		nueva.ancho = mitadAncho * 2.0f + 0.04f; // margen de montaje pequeño alrededor del marco
		nueva.alto = alturaMarco;
		nueva.base = 0.0f;
		const glm::vec3 halfWall = glm::abs(paredNueva.escala) *
			(paredNueva.fisica.presente ? paredNueva.fisica.halfExtents : glm::vec3(1.0f));
		std::vector<SceneWallSection> secciones;
		std::vector<SceneWallOpening> propuestaAperturas;
		for (const auto& ap : paredNueva.pared.aperturas)
			if (ap.idPuerta != puerta.id) propuestaAperturas.push_back(ap);
		propuestaAperturas.push_back(nueva);
		std::string error;
		if (!GenerateWallSections(halfWall, propuestaAperturas, secciones, error))
		{ ReportStatus("Door opening rejected: " + error, true); return false; }

		const SceneState anterior = CaptureHistory(contexto);
		const SceneAnimatedObject puertaAnterior = puerta;
		const int paredAnteriorIndex = puerta.paredId.empty() ? -1 : static_cast<int>(std::distance(contexto.objetos.begin(),
			std::find_if(contexto.objetos.begin(), contexto.objetos.end(), [&](const SceneObject& p) { return p.id == puerta.paredId; })));
		const SceneObject paredNuevaAnterior = paredNueva;
		SceneObject paredAnterior;
		if (paredAnteriorIndex >= 0 && paredAnteriorIndex < static_cast<int>(contexto.objetos.size()) && paredAnteriorIndex != indicePared)
			paredAnterior = contexto.objetos[static_cast<std::size_t>(paredAnteriorIndex)];
		if (paredAnteriorIndex >= 0 && paredAnteriorIndex < static_cast<int>(contexto.objetos.size()))
		{
			auto& old = contexto.objetos[static_cast<std::size_t>(paredAnteriorIndex)];
			old.pared.aperturas.erase(std::remove_if(old.pared.aperturas.begin(), old.pared.aperturas.end(),
				[&](const SceneWallOpening& ap) { return ap.id == puerta.openingId; }), old.pared.aperturas.end());
		}
		paredNueva.pared.aperturas = std::move(propuestaAperturas);
		puerta.paredId = paredNueva.id;
		puerta.openingId = nueva.id;
		puerta.position = paredNueva.position + ejeU * nueva.centerU + glm::vec3(0.0f, 0.0f,
			paredNueva.position.z - halfWall.z - puerta.position.z);
		puerta.yawDegrees = paredNueva.eulerRotation.z;
		const auto resultado = ValidateAnimated(contexto, puerta, static_cast<int>(contexto.objetos.size()) + indicePuerta);
		if (!resultado.valida)
		{
			paredNueva = paredNuevaAnterior;
			if (paredAnteriorIndex >= 0 && paredAnteriorIndex < static_cast<int>(contexto.objetos.size()) && paredAnteriorIndex != indicePared)
				contexto.objetos[static_cast<std::size_t>(paredAnteriorIndex)] = paredAnterior;
			puerta = puertaAnterior;
			ReportStatus("Door installation blocked by " + resultado.bloqueador, true);
			return false;
		}
		historial.RegisterElement(anterior);
		SyncGlb(contexto);
		rebuildPhysicsRequested = true;
		MarkModified(); placementMessage.clear();
		ReportStatus("Door installed; wall opening and static colliders updated.");
		return true;
	}

	void SceneEditor::ProcessPendingRequests(EditorSceneContext contexto)
	{
		// Flechas: mueven el objeto seleccionado (herramienta Mover) o, sin selección, la cámara. Una pulsación mantenida
		// (con la repetición del teclado) forma UNA operación de historial: se cierra al soltar todas las flechas.
		for (const ArrowPress& pulsacion : pendingArrows)
		{
			if (!IsValidIndex(contexto, seleccionado))
			{
				bool snap = false;
				const float paso = ArrowStepLength(pulsacion.ctrl, pulsacion.shift, opciones, snap) * 5.0f;
				camara.position += ArrowDirection(pulsacion.flecha, camara) * paso;
				continue;
			}
			if (herramienta != EditorTool::Move) continue;
			if (!IsAnimated(contexto, seleccionado))
			{
				SceneObject& objeto = contexto.objetos[static_cast<std::size_t>(seleccionado)];
				SceneObject candidato = objeto;
				candidato.position = ApplyArrowKey(objeto.position, pulsacion.flecha, camara, pulsacion.ctrl, pulsacion.shift, opciones);
				const PlacementResult resultado = ValidateObject(contexto, candidato, seleccionado);
				if (!resultado.valida)
				{
					placementMessage = "Movement blocked by " + resultado.bloqueador;
					ReportStatus(placementMessage, true);
					continue;
				}
				if (candidato.position == objeto.position) continue;
				OpenTransactionIfNeeded();
				objeto.position = candidato.position;
			}
			else
			{
				SceneAnimatedObject& puerta = contexto.animados[static_cast<std::size_t>(seleccionado) - contexto.objetos.size()];
				SceneAnimatedObject candidato = puerta;
				candidato.position = ApplyArrowKey(puerta.position, pulsacion.flecha, camara, pulsacion.ctrl, pulsacion.shift, opciones);
				const PlacementResult resultado = ValidateAnimated(contexto, candidato, seleccionado);
				if (!resultado.valida)
				{
					placementMessage = "Movement blocked by " + resultado.bloqueador;
					ReportStatus(placementMessage, true);
					continue;
				}
				if (candidato.position == puerta.position) continue;
				OpenTransactionIfNeeded();
				puerta.position = candidato.position;
				SyncGlb(contexto);
			}
			placementMessage.clear();
			MarkModified();
		}
		pendingArrows.clear();
		if (undoRequested) { undoRequested = false; Undo(contexto); }
		if (redoRequested) { redoRequested = false; Redo(contexto); }
		if (duplicateRequested) { duplicateRequested = false; DuplicateSelection(contexto); }
		if (deleteRequested) { deleteRequested = false; DeleteSelection(contexto, true); }
		if (eliminarConservarHuecoSolicitado) { eliminarConservarHuecoSolicitado = false; DeleteSelection(contexto, false); }
		if (addRequested) { addRequested = false; AddFromAssetBrowser(contexto); }
		if (focusRequested) { focusRequested = false; FocusSelection(contexto); }
	}

	void SceneEditor::AddFromAssetBrowser(EditorSceneContext contexto)
	{
		if (assetIsDoor)
		{
			if (selectedDoor.empty() || !doorLoader)
			{ ReportStatus("No GLB door loader is registered.", true); return; }
			std::error_code ec;
			const auto absoluta = std::filesystem::absolute(animationsRoot / selectedDoor, ec).lexically_normal();
			if (ec) { ReportStatus("Could not resolve the selected GLB path.", true); return; }
			SceneAnimatedObject nuevo;
			nuevo.id = UniqueId(contexto, absoluta.stem().string());
			nuevo.position = camara.PivotPoint();
			const auto relativa = absoluta.lexically_relative(sceneRoot);
			if (relativa.empty() || relativa.is_absolute()) { ReportStatus("Could not create a portable GLB path relative to the scene.", true); return; }
			nuevo.modelo = relativa.generic_string();
			for (const SceneAnimatedObject& existente : contexto.animados)
			{
				std::error_code rutaError;
				const auto existenteAbs = std::filesystem::absolute(sceneRoot / existente.modelo, rutaError).lexically_normal();
				if (!rutaError && existenteAbs == absoluta) { nuevo.colisionadores = existente.colisionadores; nuevo.escala = existente.escala; break; }
			}
			if (nuevo.colisionadores.empty()) { ReportStatus("This GLB has no door collider template in the loaded scene.", true); return; }
			const SceneState anterior = CaptureHistory(contexto);
			std::string error;
			if (!doorLoader(absoluta, nuevo, contexto, error))
			{ ReportStatus("Door asset error: " + error, true); return; }
			historial.RegisterElement(anterior);
			contexto.animados.push_back(nuevo);
			rebuildPhysicsRequested = true;
			SetSelection(static_cast<int>(contexto.objetos.size() + contexto.animados.size() - 1));
			hayRespaldo = false; MarkModified();
			ReportStatus("Door instance added. Select a parametric wall and install the opening in Inspector.");
			return;
		}
		if (selectedAsset.empty()) { ReportStatus("Select a model in the Asset Browser first", true); return; }
		if (!modelLoader) { ReportStatus("Asset error: no model loader is registered", true); return; }
		std::error_code ec;
		const std::filesystem::path absoluta = std::filesystem::absolute(modelsRoot / selectedAsset, ec).lexically_normal();
		if (ec) { ReportStatus("Asset error: could not resolve " + selectedAsset, true); return; }
		std::string clave, error;
		if (!modelLoader(absoluta, clave, error))
		{
			ReportStatus("Asset error: " + (error.empty() ? std::string("could not load model") : error), true);
			return;
		}
		AppendModel(contexto, clave, assetYArriba, assetCollider, assetDynamic);
	}

	void SceneEditor::ChangeTransform(SceneObject& objeto, const glm::mat4& matriz)
	{
		if (!MatrizGizmoValida(matriz))
		{
			ReportStatus("Gizmo transform rejected: non-finite or non-affine matrix", true);
			return;
		}
		const glm::vec3 position(matriz[3]);
		glm::vec3 escala(glm::length(glm::vec3(matriz[0])), glm::length(glm::vec3(matriz[1])), glm::length(glm::vec3(matriz[2])));
		for (int eje = 0; eje < 3; ++eje) escala[eje] *= objeto.escala[eje] < 0.0f ? -1.0f : 1.0f;
		if (std::abs(glm::determinant(glm::mat3(matriz))) < 1.0e-9f || !VectorFinito(position) || !VectorFinito(escala)) return;

		glm::mat3 eulerRotation(matriz);
		for (int eje = 0; eje < 3; ++eje)
			eulerRotation[eje] = glm::normalize(eulerRotation[eje]) * (objeto.escala[eje] < 0.0f ? -1.0f : 1.0f);
		// Euler XYZ que reconstruye la matriz del motor Rz * Ry * Rx.
		const float y = std::asin(std::clamp(-eulerRotation[0][2], -1.0f, 1.0f));
		const float cy = std::cos(y);
		float x = 0.0f, z = 0.0f;
		if (std::abs(cy) > 1.0e-5f)
		{
			x = std::atan2(eulerRotation[1][2], eulerRotation[2][2]);
			z = std::atan2(eulerRotation[0][1], eulerRotation[0][0]);
		}
		else
		{
			x = std::atan2(-eulerRotation[2][1], eulerRotation[1][1]);
		}
		glm::vec3 nuevaRotacion(glm::degrees(x), glm::degrees(y), glm::degrees(z));
		for (int eje = 0; eje < 3; ++eje)
			nuevaRotacion[eje] += 360.0f * std::round((objeto.eulerRotation[eje] - nuevaRotacion[eje]) / 360.0f);
		if (!VectorFinito(nuevaRotacion)) return;
		const glm::vec3 nuevaEscala = glm::max(glm::abs(escala), glm::vec3(MinimoEscala)) * glm::sign(escala);
		if (position == objeto.position && nuevaRotacion == objeto.eulerRotation && nuevaEscala == objeto.escala) return;
		objeto.position = position;
		objeto.eulerRotation = nuevaRotacion;
		objeto.escala = nuevaEscala;
		MarkModified();
	}

	// ---- Interfaz ------------------------------------------------------------------------------------

	void SceneEditor::DrawEditorToolbar(EditorSceneContext contexto)
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (!activo)
    		return;

		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, AlturaBarra * io.FontGlobalScale), ImGuiCond_Always);
		ImGui::Begin("Editor Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_HorizontalScrollbar);
		if (ImGui::Button(modificada ? "Save*" : "Save")) saveRequested = true;
		ImGui::SameLine();
		ImGui::BeginDisabled(!historial.CanUndo());
		if (ImGui::Button("Undo")) undoRequested = true;
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!historial.CanRedo());
		if (ImGui::Button("Redo")) redoRequested = true;
		ImGui::EndDisabled();
		ImGui::SameLine();
		auto herramientaBoton = [this](const char* texto, EditorTool h) {
			const bool actual = herramienta == h;
			if (actual) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.75f, 1.0f));
			if (ImGui::Button(texto)) herramienta = h;
			if (actual) ImGui::PopStyleColor();
		};
		herramientaBoton("Move [1]", EditorTool::Move); ImGui::SameLine();
		herramientaBoton("Rotate [2]", EditorTool::Rotate); ImGui::SameLine();
		herramientaBoton("Scale [3]", EditorTool::Scale);
		ImGui::SameLine(); ImGui::Checkbox("Local", &local);
		ImGui::SameLine(); ImGui::Checkbox("Snap", &snap);
		ImGui::SameLine(); ImGui::Checkbox("Boxes", &showColliders);
		ImGui::SameLine();
		if (ImGui::Button(camara.vista == ViewKind::Perspective ? "Top view" : "Perspective"))
			camara.ChangeView(camara.vista == ViewKind::Perspective ? ViewKind::Top : ViewKind::Perspective);
		ImGui::SameLine();
		if (ImGui::Button("Front")) camara.ChangeView(ViewKind::Front);
		ImGui::SameLine();
		if (ImGui::Button("Right")) camara.ChangeView(ViewKind::Right);
		ImGui::SameLine();
		ImGui::Checkbox("Camera", &showCameraOptions);
		ImGui::SameLine();
		ImGui::Checkbox("Debug input", &debugInput);
		ImGui::SameLine();
		ImGui::TextDisabled("Arrows: 10cm | Ctrl 1cm | Shift 50cm | Ctrl+Shift grid %.0fcm | PgUp/PgDn Z", opciones.gridStep * 100.0f);
		if (modificada) { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.95f, 0.72f, 0.15f, 1.0f), "Unsaved changes"); }
		if (!statusMessage.empty())
		{
			ImGui::SameLine();
			if (messageIsError) ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.3f, 1.0f), "%s", statusMessage.c_str());
			else ImGui::TextUnformatted(statusMessage.c_str());
		}
		ImGui::End();
		(void)contexto;
	}

	void SceneEditor::DrawHierarchy(EditorSceneContext contexto)
	{
		const ImGuiIO& io = ImGui::GetIO();
		const float escalaUI = io.FontGlobalScale;
		ImGui::SetNextWindowPos(ImVec2(0.0f, (AlturaBarra + 4.0f) * escalaUI), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(270.0f * escalaUI,
			std::max(260.0f * escalaUI, io.DisplaySize.y * 0.45f)), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(ImVec2(180.0f * escalaUI, 140.0f * escalaUI),
			ImVec2(io.DisplaySize.x, io.DisplaySize.y));
		const bool visible = ImGui::Begin("Hierarchy");
		AjustarVentanaAlCambiarEscala("Hierarchy", escalaUI, io.DisplaySize);
		if (visible)
		{
			if (ImGui::CollapsingHeader("Editor visibility (not saved)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("Hide ceiling", &visibilidad.hideCeiling);
				ImGui::Checkbox("Hide walls", &visibilidad.hideWalls);
				ImGui::Checkbox("Auto-hide ceiling in top view", &visibilidad.autoOcultarTechoEnVistaSuperior);
				if (ImGui::Button("Show all")) visibilidad.ShowAll();
				ImGui::SameLine();
				if (!visibilidad.isolateSelection)
				{
					ImGui::BeginDisabled(!IsValidIndex(contexto, seleccionado));
					if (ImGui::Button("Isolate selected"))
					{ visibilidad.isolateSelection = true; visibilidad.isolatedId = IdOf(contexto, seleccionado); }
					ImGui::EndDisabled();
				}
				else if (ImGui::Button("End isolation")) { visibilidad.isolateSelection = false; visibilidad.isolatedId.clear(); }
				ImGui::TextDisabled("Hidden items stay in the scene, the JSON and the physics.");
			}
			ImGui::Separator();
			if (ImGui::BeginChild("jerarquia-lista", ImVec2(0.0f, 0.0f), false))
			{
				const int total = static_cast<int>(contexto.objetos.size() + contexto.animados.size());
				for (int i = 0; i < total; ++i)
				{
					const bool animado = IsAnimated(contexto, i);
					const std::string id = IdOf(contexto, i);
					ImGui::PushID(i);
					if (!animado)
					{	// casilla de visibilidad por elemento
						bool visibleManual = !visibilidad.ocultos.contains(id);
						if (ImGui::Checkbox("##vis", &visibleManual))
						{
							if (visibleManual) visibilidad.ocultos.erase(id);
							else visibilidad.ocultos.insert(id);
						}
						ImGui::SameLine();
					}
					else { ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), 1.0f)); ImGui::SameLine(); }
					std::string etiqueta = id;
					if (animado) etiqueta += "  [GLB]";
					else if (static_cast<std::size_t>(i) < clases.size() && clases[static_cast<std::size_t>(i)] != ElementClass::Object)
						etiqueta += std::string("  [") + NombreClase(clases[static_cast<std::size_t>(i)]) + "]";
					if (ImGui::Selectable(etiqueta.c_str(), seleccionado == i, ImGuiSelectableFlags_AllowDoubleClick))
					{
						SetSelection(i);
						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) focusRequested = true;
					}
					if (seleccionado == i && selectionChanged) { ImGui::SetScrollHereY(0.5f); }
					ImGui::PopID();
				}
				selectionChanged = false;
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}

	void SceneEditor::DrawInspector(EditorSceneContext contexto)
	{
		const ImGuiIO& io = ImGui::GetIO();
		const float escalaUI = io.FontGlobalScale;
		ImGui::SetNextWindowPos(ImVec2(std::max(280.0f * escalaUI, io.DisplaySize.x - 320.0f * escalaUI),
			(AlturaBarra + 4.0f) * escalaUI), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(310.0f * escalaUI,
			std::max(300.0f * escalaUI, io.DisplaySize.y * 0.6f)), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(ImVec2(220.0f * escalaUI, 160.0f * escalaUI),
			ImVec2(io.DisplaySize.x, io.DisplaySize.y));
		const bool visible = ImGui::Begin("Inspector");
		AjustarVentanaAlCambiarEscala("Inspector", escalaUI, io.DisplaySize);
		if (visible)
		{
			if (!placementMessage.empty())
				ImGui::TextColored(ImVec4(1.0f, 0.28f, 0.22f, 1.0f), "%s", placementMessage.c_str());
			if (!IsValidIndex(contexto, seleccionado))
				ImGui::TextUnformatted("Select an object in Hierarchy or click it in the viewport");
			else if (!IsAnimated(contexto, seleccionado))
			{
				SceneObject& objeto = contexto.objetos[static_cast<std::size_t>(seleccionado)];
				ImGui::Text("ID: %s", objeto.id.c_str());
				ImGui::Text("Collection: %s", objeto.categoria.c_str());
				ImGui::TextWrapped("Model: %s", objeto.modelo.c_str());
				ImGui::Separator();
				ImGui::TextUnformatted("Transform");
				glm::vec3 solicitada = objeto.position;
				if (ImGui::InputFloat3("Position", &solicitada.x, "%.3f"))
				{
					SceneObject candidato = objeto;
					candidato.position = solicitada;
					const PlacementResult resultado = VectorFinito(solicitada) ? ValidateObject(contexto, candidato, seleccionado) :
						PlacementResult{ false, "non-finite position" };
					if (resultado.valida)
					{
						if (solicitada != objeto.position) { OpenTransactionIfNeeded(); objeto.position = solicitada; MarkModified(); }
						placementMessage.clear();
					}
					else { placementMessage = "Position blocked by " + resultado.bloqueador; ReportStatus(placementMessage, true); }
				}
			glm::vec3 rotacionSolicitada = objeto.eulerRotation;
			if (ImGui::InputFloat3("Rotation", &rotacionSolicitada.x, "%.2f"))
			{
				SceneObject candidato = objeto;
				candidato.eulerRotation = rotacionSolicitada;
				const PlacementResult resultado = VectorFinito(rotacionSolicitada) ? ValidateObject(contexto, candidato, seleccionado) :
					PlacementResult{ false, "non-finite rotation" };
				if (resultado.valida)
				{
					if (rotacionSolicitada != objeto.eulerRotation) { OpenTransactionIfNeeded(); objeto.eulerRotation = rotacionSolicitada; MarkModified(); }
					placementMessage.clear();
				}
				else { placementMessage = "Rotation blocked by " + resultado.bloqueador; ReportStatus(placementMessage, true); }
			}
			glm::vec3 escalaSolicitada = objeto.escala;
			if (ImGui::InputFloat3("Scale", &escalaSolicitada.x, "%.3f"))
			{
				if (!VectorFinito(escalaSolicitada)) escalaSolicitada = objeto.escala;
				for (int eje = 0; eje < 3; ++eje)
					if (std::abs(escalaSolicitada[eje]) < MinimoEscala)
						escalaSolicitada[eje] = std::copysign(MinimoEscala, escalaSolicitada[eje] == 0.0f ? 1.0f : escalaSolicitada[eje]);
				SceneObject candidato = objeto;
				candidato.escala = escalaSolicitada;
				const PlacementResult resultado = ValidateObject(contexto, candidato, seleccionado);
				if (resultado.valida)
				{
					if (escalaSolicitada != objeto.escala) { OpenTransactionIfNeeded(); objeto.escala = escalaSolicitada; MarkModified(); }
					placementMessage.clear();
				}
				else { placementMessage = "Scale blocked by " + resultado.bloqueador; ReportStatus(placementMessage, true); }
			}
			bool permitirArquitectura = objeto.permitirPenetracionArquitectura;
			if (ImGui::Checkbox("Allow architecture penetration (unsafe)", &permitirArquitectura))
			{
				OpenTransactionIfNeeded();
				objeto.permitirPenetracionArquitectura = permitirArquitectura;
				MarkModified();
			}
			ImGui::TextDisabled("Opt-in exception: ignores static architecture only.");
				ImGui::Separator();
				ImGui::TextUnformatted("Instance material");
				bool usaMaterial = objeto.material.presente;
				if (ImGui::Checkbox("Override material", &usaMaterial))
				{
					OpenTransactionIfNeeded();
					objeto.material.presente = usaMaterial;
					MarkModified();
				}
				if (objeto.material.presente)
				{
					char texturaMaterial[512]{};
					std::snprintf(texturaMaterial, sizeof(texturaMaterial), "%s", objeto.material.textura.c_str());
					if (ImGui::InputText("Texture (relative to scene)", texturaMaterial, sizeof(texturaMaterial)))
					{
						objeto.material.textura = texturaMaterial;
						OpenTransactionIfNeeded(); MarkModified();
					}
					if (ImGui::Button("Elegir textura")) ImGui::OpenPopup("Selector de textura");
					ImGui::SameLine();
					if (ImGui::Button("Restaurar material original"))
					{
						OpenTransactionIfNeeded(); objeto.material = SceneMaterial{}; MarkModified();
					}
					if (ImGui::BeginPopup("Selector de textura"))
					{
						ImGui::InputTextWithHint("##texture-filter", "Filtrar por nombre...", textureFilter, sizeof(textureFilter));
						const char* categorias[] = { "Todas", "Suelo", "Pared", "Techo", "Otros" };
						ImGui::Combo("Categoría", &textureCategory, categorias, 5);
						ImGui::BeginChild("##texture-list", ImVec2(330.0f, 260.0f), true);
						std::error_code scanError;
						if (!modelsRoot.empty() && std::filesystem::exists(modelsRoot, scanError))
						for (std::filesystem::recursive_directory_iterator it(modelsRoot, scanError), end; !scanError && it != end; it.increment(scanError))
						{
							if (!it->is_regular_file(scanError)) continue;
							std::string ext = it->path().extension().string();
							std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
							if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".tga") continue;
							std::string low = it->path().generic_string();
							std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
							int cat = 4;
							if (low.find("floor") != std::string::npos || low.find("suelo") != std::string::npos) cat = 1;
							else if (low.find("wall") != std::string::npos || low.find("pared") != std::string::npos) cat = 2;
							else if (low.find("ceiling") != std::string::npos || low.find("techo") != std::string::npos) cat = 3;
							if (textureCategory != 0 && textureCategory != cat) continue;
							if (textureFilter[0] && low.find(textureFilter) == std::string::npos) continue;
							const auto absoluta = std::filesystem::absolute(it->path(), scanError).lexically_normal();
							if (scanError) { scanError.clear(); continue; }
							const auto relativa = absoluta.lexically_relative(sceneRoot);
							if (relativa.empty() || relativa.is_absolute()) continue;
							const std::string ruta = relativa.generic_string();
							const std::string label = std::string(categorias[cat]) + " / " + it->path().filename().string();
							if (ImGui::Selectable(label.c_str(), objeto.material.textura == ruta))
							{
								OpenTransactionIfNeeded(); objeto.material.textura = ruta; MarkModified();
								ImGui::CloseCurrentPopup();
							}
							if (objeto.material.textura == ruta && textureLoader)
							{
								const unsigned int tex = textureLoader(absoluta);
								if (tex) { ImGui::SameLine(); ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(tex)), ImVec2(44, 30)); }
							}
						}
						ImGui::EndChild();
						ImGui::EndPopup();
					}
					if (ImGui::InputFloat2("Tile size (m)", &objeto.material.tileSize.x, "%.2f"))
					{
						objeto.material.tileSize.x = std::clamp(objeto.material.tileSize.x, 0.1f, 20.0f);
						objeto.material.tileSize.y = std::clamp(objeto.material.tileSize.y, 0.1f, 20.0f);
						OpenTransactionIfNeeded(); MarkModified();
					}
					if (ImGui::SliderFloat("Roughness", &objeto.material.roughness, 0.0f, 1.0f, "%.2f"))
					{ OpenTransactionIfNeeded(); MarkModified(); }
					if (ImGui::SliderFloat("World dirt", &objeto.material.dirtStrength, 0.0f, 0.5f, "%.2f"))
					{ OpenTransactionIfNeeded(); MarkModified(); }
					if (objeto.material.dirtStrength > 0.0f)
					{
						if (ImGui::SliderFloat("Dirt scale (cells/m)", &objeto.material.dirtScale, 0.05f, 4.0f, "%.2f"))
						{ OpenTransactionIfNeeded(); MarkModified(); }
						if (ImGui::InputFloat("Dirt seed", &objeto.material.dirtSeed, 1.0f, 10.0f, "%.0f"))
						{ OpenTransactionIfNeeded(); MarkModified(); }
					}
				}
				ImGui::Separator();
				ImGui::TextUnformatted("Physics");
				int tipo = !objeto.fisica.presente ? 0 :
					(objeto.fisica.tipo == physics::BodyType::Bouncer ? 3 :
						(objeto.fisica.tipo == physics::BodyType::Dynamic ? 2 : 1));
				const int tipoAnterior = tipo;
				const char* tipos[] = { "None", "Static box", "Dynamic box", "Bouncy sphere" };
				if (ImGui::Combo("Collider", &tipo, tipos, 4) && tipo != tipoAnterior)
				{
					historial.RegisterElement(CaptureHistory(contexto));
					hayRespaldo = false;
					objeto.fisica.presente = tipo != 0;
					if (tipo != 0)
					{
						objeto.fisica.forma = tipo == 3 ? physics::BodyShape::Sphere : physics::BodyShape::Box;
						if (tipoAnterior == 0)
						{	// medidas iniciales a partir de los límites del modelo
							const LocalBox caja = BoxOfObject(objeto);
							objeto.fisica.halfExtents = caja.HalfSize();
							objeto.fisica.desplazamiento = caja.CenterPoint();
						}
						objeto.fisica.tipo = tipo == 3 ? physics::BodyType::Bouncer :
							(tipo == 2 ? physics::BodyType::Dynamic : physics::BodyType::Static);
						if ((tipo == 2 || tipo == 3) && objeto.fisica.masa <= 0.0f)
							objeto.fisica.masa = 0.25f;
					}
					MarkModified();
				}
				if (objeto.fisica.presente)
				{
					if (ImGui::InputFloat3("Half size", &objeto.fisica.halfExtents.x, "%.3f"))
					{
						for (int eje = 0; eje < 3; ++eje)
							objeto.fisica.halfExtents[eje] = std::max(0.005f, std::isfinite(objeto.fisica.halfExtents[eje]) ? objeto.fisica.halfExtents[eje] : 0.5f);
						OpenTransactionIfNeeded(); MarkModified();
					}
					if (ImGui::InputFloat3("Offset", &objeto.fisica.desplazamiento.x, "%.3f"))
					{ OpenTransactionIfNeeded(); MarkModified(); }
					if (physics::IsDynamicBody(objeto.fisica.tipo) &&
						ImGui::InputFloat("Mass (kg)", &objeto.fisica.masa, 0.5f, 5.0f, "%.2f"))
					{
						if (!(objeto.fisica.masa > 0.0f)) objeto.fisica.masa = 0.1f;
						OpenTransactionIfNeeded(); MarkModified();
					}
					ImGui::TextDisabled("Colliders are rebuilt when you press Play.");
				}
				ImGui::Separator();
				ImGui::TextUnformatted("Puzzle");
				int TipoPuzzle = objeto.puzzle.tipo == fisica::TipoPuzzle::SlidingPuzzle ? 1 : 0;

				const char* tiposPuzzle[] = {"None", "Sliding Puzzle"};

				if (ImGui::Combo("Interactable type", &TipoPuzzle, tiposPuzzle, 
					IM_ARRAYSIZE(tiposPuzzle))) {
					objeto.puzzle.tipo = TipoPuzzle == 1 ? fisica::TipoPuzzle::SlidingPuzzle : 
					fisica::TipoPuzzle::Ninguno;
					MarcarModificada();
				}
			}
			else
			{
				SceneAnimatedObject& animado = contexto.animados[static_cast<std::size_t>(seleccionado) - contexto.objetos.size()];
				ImGui::Text("ID: %s", animado.id.c_str());
				ImGui::TextUnformatted("Type: GLB animated instance");
				ImGui::TextWrapped("Model: %s", animado.modelo.c_str());
				ImGui::TextWrapped("Animation: %s", animado.animation.empty() ? "(first in file)" : animado.animation.c_str());
				ImGui::Text("Colliders: %d", static_cast<int>(animado.colisionadores.size()));
				for (const SceneCollider& c : animado.colisionadores)
					ImGui::BulletText("%s (%s)", c.nodo.c_str(), c.tipo == physics::BodyType::Kinematic ? "kinematic" : "static");
				if (!animado.paredId.empty())
					for (std::size_t i = 0; i < contexto.objetos.size(); ++i)
						if (contexto.objetos[i].id == animado.paredId) paredDestinoPuerta = static_cast<int>(i);
				const char* nombrePared = paredDestinoPuerta >= 0 && paredDestinoPuerta < static_cast<int>(contexto.objetos.size())
					? contexto.objetos[static_cast<std::size_t>(paredDestinoPuerta)].id.c_str() : "Seleccionar pared paramétrica";
				if (ImGui::BeginCombo("Wall opening", nombrePared))
				{
					for (std::size_t i = 0; i < contexto.objetos.size(); ++i)
					{
						const SceneObject& candidata = contexto.objetos[i];
						if (!candidata.pared.presente) continue;
						const bool selected = paredDestinoPuerta == static_cast<int>(i);
						if (ImGui::Selectable(candidata.id.c_str(), selected)) paredDestinoPuerta = static_cast<int>(i);
						if (selected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				if (ImGui::Button(animado.paredId.empty() ? "Install on wall" : "Move opening to wall"))
					InstallDoorInWall(contexto, seleccionado - static_cast<int>(contexto.objetos.size()), paredDestinoPuerta);
				ImGui::TextDisabled("Opening dimensions are derived from DoorFrame collider bounds.");
				ImGui::Separator();
				ImGui::TextUnformatted("Transform (instance only)");
				glm::vec3 posicionSolicitada = animado.position;
				if (ImGui::InputFloat3("Position", &posicionSolicitada.x, "%.3f"))
				{
					SceneAnimatedObject candidato = animado;
					candidato.position = posicionSolicitada;
					const PlacementResult resultado = VectorFinito(posicionSolicitada) ? ValidateAnimated(contexto, candidato, seleccionado) :
						PlacementResult{ false, "non-finite position" };
					if (resultado.valida)
					{
						if (posicionSolicitada != animado.position) { OpenTransactionIfNeeded(); animado.position = posicionSolicitada; MarkModified(); SyncGlb(contexto); }
						placementMessage.clear();
					}
					else { placementMessage = "Door position blocked by " + resultado.bloqueador; ReportStatus(placementMessage, true); }
				}
				float giro = animado.yawDegrees;
				if (ImGui::InputFloat("Rotation Z", &giro, 1.0f, 15.0f, "%.2f") && std::isfinite(giro))
				{
					SceneAnimatedObject candidato = animado;
					candidato.yawDegrees = giro;
					const PlacementResult resultado = ValidateAnimated(contexto, candidato, seleccionado);
					if (resultado.valida)
					{
						if (giro != animado.yawDegrees) { OpenTransactionIfNeeded(); animado.yawDegrees = giro; MarkModified(); SyncGlb(contexto); }
						placementMessage.clear();
					}
					else { placementMessage = "Door rotation blocked by " + resultado.bloqueador; ReportStatus(placementMessage, true); }
				}
				bool permitirArquitectura = animado.permitirPenetracionArquitectura;
				if (ImGui::Checkbox("Allow architecture penetration (unsafe)", &permitirArquitectura))
				{
					OpenTransactionIfNeeded();
					animado.permitirPenetracionArquitectura = permitirArquitectura;
					MarkModified();
				}
				ImGui::TextDisabled("Opt-in exception: ignores static architecture only.");
				ImGui::Text("Scale: %.3f (uniform, not editable here)", animado.escala);
				ImGui::TextWrapped("The animation, nodes and colliders are preserved; colliders follow the new placement when you press Play.");
			}
			if (IsValidIndex(contexto, seleccionado))
			{
				ImGui::Separator();
				if (ImGui::Button("Focus")) focusRequested = true;
				ImGui::SameLine();
				ImGui::BeginDisabled(IsAnimated(contexto, seleccionado));
				if (ImGui::Button("Duplicate")) duplicateRequested = true;
				ImGui::EndDisabled();
				ImGui::SameLine();
				if (IsAnimated(contexto, seleccionado))
				{
					const SceneAnimatedObject& puerta = contexto.animados[static_cast<std::size_t>(seleccionado) - contexto.objetos.size()];
					if (!puerta.openingId.empty())
					{
						if (ImGui::Button("Delete + close opening")) deleteRequested = true;
						if (ImGui::Button("Delete only; keep opening")) eliminarConservarHuecoSolicitado = true;
					}
					else if (ImGui::Button("Delete")) deleteRequested = true;
				}
				else if (ImGui::Button("Delete")) deleteRequested = true;
			}
		}
		ImGui::End();
	}

	void SceneEditor::DrawAssetBrowser(EditorSceneContext contexto)
	{
		const ImGuiIO& io = ImGui::GetIO();
		const float escalaUI = io.FontGlobalScale;
		ImGui::SetNextWindowPos(ImVec2(0.0f, (AlturaBarra + 4.0f) * escalaUI +
			std::max(260.0f * escalaUI, io.DisplaySize.y * 0.45f) + 6.0f * escalaUI), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(270.0f * escalaUI,
			std::max(220.0f * escalaUI, io.DisplaySize.y * 0.35f)), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(ImVec2(200.0f * escalaUI, 160.0f * escalaUI),
			ImVec2(io.DisplaySize.x, io.DisplaySize.y));
		const bool visible = ImGui::Begin("Asset Browser");
		AjustarVentanaAlCambiarEscala("Asset Browser", escalaUI, io.DisplaySize);
		if (visible)
		{
			if (ImGui::Button("Refresh")) { inventoryLoaded = false; inventarioPuertasCargado = false; }
			ImGui::SameLine();
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputTextWithHint("##filtro", "filter...", assetFilter, sizeof(assetFilter));
			if (!inventoryLoaded)
			{
				std::error_code ec;
				availableModels.clear();
				if (std::filesystem::exists(modelsRoot, ec))
					for (std::filesystem::recursive_directory_iterator it(modelsRoot, ec), fin; !ec && it != fin; it.increment(ec))
						if (it->is_regular_file(ec) && it->path().extension() == ".obj")
							availableModels.push_back(it->path().lexically_relative(modelsRoot));
				std::sort(availableModels.begin(), availableModels.end());
				inventoryLoaded = true;
			}
			if (!inventarioPuertasCargado)
			{
				std::error_code ec;
				availableDoors.clear();
				if (std::filesystem::exists(animationsRoot, ec))
					for (std::filesystem::recursive_directory_iterator it(animationsRoot, ec), fin; !ec && it != fin; it.increment(ec))
						if (it->is_regular_file(ec) && it->path().extension() == ".glb")
							availableDoors.push_back(it->path().lexically_relative(animationsRoot));
				std::sort(availableDoors.begin(), availableDoors.end());
				inventarioPuertasCargado = true;
			}
			const float alturaOpciones = ImGui::GetFrameHeightWithSpacing() * 4.0f + 6.0f;
			if (ImGui::BeginChild("model-list", ImVec2(0.0f, -alturaOpciones), true))
			{
				if (availableModels.empty())
					ImGui::TextWrapped("No .obj found in %s", modelsRoot.generic_string().c_str());
				for (const auto& modelo : availableModels)
				{
					const std::string relativa = modelo.generic_string();
					if (assetFilter[0] != '\0')
					{
						std::string a = relativa, b = assetFilter;
						std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
						std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
						if (a.find(b) == std::string::npos) continue;
					}
					// Nombre visible, sin las carpetas ni la extensión.
					std::string nombreVisible = modelo.stem().string();

					// Mantener un identificador único aunque haya nombres repetidos.
					ImGui::PushID(relativa.c_str());

					if (ImGui::Selectable(
						nombreVisible.c_str(),
						selectedAsset == relativa,
						ImGuiSelectableFlags_AllowDoubleClick))
					{
						selectedAsset = relativa;
						assetIsDoor = false;

						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
							addRequested = true;
					}

					// Mostrar la ruta original al pasar el ratón.
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("%s", relativa.c_str());
					}

					ImGui::PopID();
				}
			}
			ImGui::EndChild();
			ImGui::SeparatorText("Animated GLB doors");
			if (ImGui::BeginChild("door-list", ImVec2(0.0f, 96.0f), true))
			{
				if (availableDoors.empty()) ImGui::TextDisabled("No .glb files in %s", animationsRoot.generic_string().c_str());
				for (const auto& ruta : availableDoors)
				{
					const std::string relativa = ruta.generic_string();
					std::string low = relativa, filtro = assetFilter;
					std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
					std::transform(filtro.begin(), filtro.end(), filtro.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
					if (!filtro.empty() && low.find(filtro) == std::string::npos) continue;
					if (ImGui::Selectable((ruta.stem().string() + "##door").c_str(), assetIsDoor && selectedDoor == ruta))
					{ selectedDoor = ruta; assetIsDoor = true; }
				}
			}
			ImGui::EndChild();
			ImGui::Checkbox("Model is Y-up (rotate 90 deg on X)", &assetYArriba);
			ImGui::Checkbox("Add box collider", &assetCollider);
			if (assetCollider)
			{
				ImGui::SameLine();
				ImGui::Checkbox("dynamic", &assetDynamic);
			}
			ImGui::BeginDisabled(assetIsDoor ? selectedDoor.empty() : selectedAsset.empty());
			if (ImGui::Button("Add to scene")) addRequested = true;
			ImGui::EndDisabled();
			if (assetIsDoor && !selectedDoor.empty()) { ImGui::SameLine(); ImGui::TextDisabled("%s", selectedDoor.generic_string().c_str()); }
			else if (!selectedAsset.empty()) { ImGui::SameLine(); ImGui::TextDisabled("%s", selectedAsset.c_str()); }
			if (messageIsError && !statusMessage.empty()) ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.3f, 1.0f), "%s", statusMessage.c_str());
		}
		ImGui::End();
		(void)contexto;
	}

	void SceneEditor::DrawGizmo(EditorSceneContext contexto, int ancho, int alto)
	{
		if (!IsValidIndex(contexto, seleccionado) || ancho <= 0 || alto <= 0) return;
		const ImGuiIO& io = ImGui::GetIO();
		if (!bloqueoGizmoTrasSeleccion.AllowManipulation(io.MouseDown[ImGuiMouseButton_Left]))
		{
			// El clic completo que selecciona un elemento no puede iniciar el gizmo.
			gizmoEnUso = false;
			matrizCambiada = false;
			return;
		}
		ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());   // por encima de los paneles: el gizmo no se esconde detrás
		ImGuizmo::SetRect(0.0f, 0.0f, static_cast<float>(ancho), static_cast<float>(alto));
		ImGuizmo::SetOrthographic(camara.IsOrthographic());
		glm::mat4 vista, proyeccion;
		Matrices(static_cast<float>(ancho) / static_cast<float>(alto), vista, proyeccion);

		const bool animado = IsAnimated(contexto, seleccionado);
		const std::string idSeleccionado = IdOf(contexto, seleccionado);
		const bool reutilizarPrevisualizacion = previsualizacionGizmoActiva &&
			idPrevisualizacionGizmo == idSeleccionado && io.MouseDown[ImGuiMouseButton_Left];
		glm::mat4 matriz = reutilizarPrevisualizacion ? matrizPrevisualizacionGizmo : ElementAt(contexto, seleccionado).modelo;
		ImGuizmo::OPERATION operacion = ImGuizmo::TRANSLATE;
		if (herramienta == EditorTool::Rotate) operacion = animado ? ImGuizmo::ROTATE_Z : ImGuizmo::ROTATE;
		if (herramienta == EditorTool::Scale)
		{
			operacion = ImGuizmo::SCALE;
		}
		// ImGuizmo se desactiva SOLO si algo mas tiene la entrada: navegacion de camara, Alt pulsado, o la escala de una puerta
		// (no se toca para no alterar sus colisionadores). NO se usa io.WantCaptureMouse: ImGuizmo lo activa el mismo
		// (SetNextFrameWantCaptureMouse) al pasar sobre un manipulador, y desactivarlo por eso impedia empezar el arrastre.
		// Si el cursor esta sobre un panel, ImGuizmo ya ignora el clic (IsHoveringWindow).
		const bool escalaDePuerta = animado && herramienta == EditorTool::Scale;
		ImGuizmo::PushID(idSeleccionado.c_str());
		ImGuizmo::Enable(IsGizmoEnabled(ImGuizmo::IsUsing(), modoCamara != CameraMode::None, entrada.altPressed, escalaDePuerta));
		float paso[3] = { 0.5f, 0.5f, 0.5f };
		if (herramienta == EditorTool::Rotate) paso[0] = paso[1] = paso[2] = 15.0f;
		if (herramienta == EditorTool::Scale) paso[0] = paso[1] = paso[2] = 0.1f;
		const glm::mat4 matrizAntes = matriz;
		const bool cambio = ImGuizmo::Manipulate(&vista[0][0], &proyeccion[0][0], operacion,
			local ? ImGuizmo::LOCAL : ImGuizmo::WORLD, &matriz[0][0], nullptr, snap ? paso : nullptr);
		gizmoEnUso = ImGuizmo::IsUsing();
		gizmoSobre = ImGuizmo::IsOver();
		matrizCambiada = cambio;
		operacionDepuracion = static_cast<int>(operacion);
		ImGuizmo::PopID();
		if (!cambio)
		{
			if (!gizmoEnUso && previsualizacionGizmoActiva)
			{
				previsualizacionGizmoActiva = false;
				previsualizacionGizmoInvalida = false;
				idPrevisualizacionGizmo.clear();
			}
			return;
		}

		const glm::mat4 matrizAceptada = ElementAt(contexto, seleccionado).modelo;
		KeepHeightInTopView(matriz, matrizAceptada, camara.vista == ViewKind::Top, herramienta == EditorTool::Move);
		PlacementResult resultado;
		if (!MatrizGizmoValida(matriz)) resultado = { false, "invalid transform matrix" };
		else if (!animado)
			resultado = ValidateObject(contexto, contexto.objetos[static_cast<std::size_t>(seleccionado)], seleccionado, &matriz);
		else
		{
			SceneAnimatedObject candidato = contexto.animados[static_cast<std::size_t>(seleccionado) - contexto.objetos.size()];
			candidato.position = glm::vec3(matriz[3]);
			float giro = glm::degrees(std::atan2(matriz[0][1], matriz[0][0]));
			giro += 360.0f * std::round((candidato.yawDegrees - giro) / 360.0f);
			candidato.yawDegrees = giro;
			resultado = VectorFinito(candidato.position) && std::isfinite(giro)
				? ValidateAnimated(contexto, candidato, seleccionado) : PlacementResult{ false, "invalid door transform" };
		}

		previsualizacionGizmoActiva = gizmoEnUso;
		previsualizacionGizmoInvalida = !resultado.valida;
		matrizPrevisualizacionGizmo = matriz;
		idPrevisualizacionGizmo = idSeleccionado;
		if (!resultado.valida)
		{
			placementMessage = "Placement blocked by " + resultado.bloqueador;
			ReportStatus(placementMessage, true);
			if (debugInput)
				std::fprintf(stderr, "[EditorGizmo] rejected id=%s view=%d using=%d blocker=%s before=(%.3f,%.3f,%.3f) candidate=(%.3f,%.3f,%.3f)\n",
					idSeleccionado.c_str(), static_cast<int>(camara.vista), gizmoEnUso ? 1 : 0, resultado.bloqueador.c_str(),
					matrizAceptada[3].x, matrizAceptada[3].y, matrizAceptada[3].z, matriz[3].x, matriz[3].y, matriz[3].z);
			return;
		}
		placementMessage.clear();
		if (debugInput)
			std::fprintf(stderr, "[EditorGizmo] accepted id=%s view=%d using=%d before=(%.3f,%.3f,%.3f) after=(%.3f,%.3f,%.3f)\n",
				idSeleccionado.c_str(), static_cast<int>(camara.vista), gizmoEnUso ? 1 : 0,
				matrizAceptada[3].x, matrizAceptada[3].y, matrizAceptada[3].z, matriz[3].x, matriz[3].y, matriz[3].z);
		OpenTransactionIfNeeded();
		if (!animado)
			ChangeTransform(contexto.objetos[static_cast<std::size_t>(seleccionado)], matriz);
		else
		{
			SceneAnimatedObject& puerta = contexto.animados[static_cast<std::size_t>(seleccionado) - contexto.objetos.size()];
			const glm::vec3 position(matriz[3]);
			float giro = glm::degrees(std::atan2(matriz[0][1], matriz[0][0]));
			giro += 360.0f * std::round((puerta.yawDegrees - giro) / 360.0f);
			if (VectorFinito(position) && std::isfinite(giro))
			{
				puerta.position = position;
				puerta.yawDegrees = giro;
				SyncGlb(contexto);
				MarkModified();
			}
		}
		if (!gizmoEnUso)
		{
			previsualizacionGizmoActiva = false;
			idPrevisualizacionGizmo.clear();
		}
	}

	void SceneEditor::DrawSelectionOutline(EditorSceneContext contexto, int ancho, int alto)
	{
		if (!IsValidIndex(contexto, seleccionado) || ancho <= 0 || alto <= 0) return;
		glm::mat4 vista, proyeccion;
		Matrices(static_cast<float>(ancho) / static_cast<float>(alto), vista, proyeccion);
		const SelectableElement e = ElementAt(contexto, seleccionado);
		const bool dibujarCandidato = previsualizacionGizmoActiva && previsualizacionGizmoInvalida &&
			idPrevisualizacionGizmo == IdOf(contexto, seleccionado);
		const glm::mat4 transformacion = proyeccion * vista * (dibujarCandidato ? matrizPrevisualizacionGizmo : e.modelo);
		glm::vec2 pantalla[8];
		bool delante[8];
		for (int i = 0; i < 8; ++i)
		{
			const glm::vec3 p((i & 1) ? e.caja.maximo.x : e.caja.minimo.x, (i & 2) ? e.caja.maximo.y : e.caja.minimo.y,
				(i & 4) ? e.caja.maximo.z : e.caja.minimo.z);
			const glm::vec4 c = transformacion * glm::vec4(p, 1.0f);
			delante[i] = c.w > 1.0e-4f;
			if (delante[i])
				pantalla[i] = glm::vec2((c.x / c.w * 0.5f + 0.5f) * static_cast<float>(ancho), (1.0f - (c.y / c.w * 0.5f + 0.5f)) * static_cast<float>(alto));
		}
		ImDrawList* lista = ImGui::GetForegroundDrawList();
		const ImU32 color = dibujarCandidato ? IM_COL32(255, 45, 35, 255) : IM_COL32(255, 205, 40, 255);
		for (int a = 0; a < 8; ++a)
			for (int bit = 0; bit < 3; ++bit)
			{
				const int b = a ^ (1 << bit);
				if (b < a || !delante[a] || !delante[b]) continue;
				lista->AddLine(ImVec2(pantalla[a].x, pantalla[a].y), ImVec2(pantalla[b].x, pantalla[b].y), color, 2.0f);
			}
		for (int i = 0; i < 8; ++i)
			if (delante[i]) lista->AddCircleFilled(ImVec2(pantalla[i].x, pantalla[i].y), 3.0f, color);
	}

	void SceneEditor::DrawCameraOptions()
	{
		if (!showCameraOptions) return;
		if (ImGui::Begin("Editor Camera", &showCameraOptions, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::SliderFloat("Mouse sensitivity (deg/px)", &opciones.sensibilidad, 0.02f, 0.6f, "%.3f");
			ImGui::Checkbox("Invert Y axis", &opciones.invertirY);
			ImGui::SliderFloat("Camera speed (m/s)", &opciones.speed, 0.5f, 40.0f, "%.1f");
			ImGui::SliderFloat("Pan speed", &opciones.panSpeed, 0.1f, 4.0f, "%.2f");
			ImGui::SliderFloat("Zoom speed", &opciones.zoomSpeed, 0.1f, 4.0f, "%.2f");
			ImGui::SliderFloat("Shift multiplier", &opciones.shiftMultiplier, 1.0f, 10.0f, "%.2f");
			ImGui::SliderFloat("Ctrl multiplier", &opciones.ctrlMultiplier, 0.05f, 1.0f, "%.2f");
			ImGui::SliderFloat("Snap grid (m)", &opciones.gridStep, 0.01f, 2.0f, "%.2f");
			ImGui::TextDisabled("Ctrl + Shift together: Ctrl wins (precision).");
			ImGui::Separator();
			ImGui::TextUnformatted("RMB look | RMB+WASD/QE fly | MMB pan | Alt+LMB orbit | wheel zoom | F focus");
			ImGui::Text("yaw %.1f  pitch %.1f  pos %.2f %.2f %.2f", camara.yaw, camara.pitch,
				camara.position.x, camara.position.y, camara.position.z);
		}
		ImGui::End();
	}

	// Instrumentación temporal del gizmo: ventana opcional y, con ESCAPETHEUAB_EDITOR_DEBUG=1, una línea por cambio en consola.
	void SceneEditor::DrawDebugShapes()
	{
		if (!debugInput) return;
		const ImGuiIO& io = ImGui::GetIO();
		const char* nombresOperacion = "?";
		if (operacionDepuracion & ImGuizmo::TRANSLATE) nombresOperacion = "TRANSLATE";
		else if (operacionDepuracion & ImGuizmo::ROTATE) nombresOperacion = "ROTATE";
		else if (operacionDepuracion & ImGuizmo::SCALE) nombresOperacion = "SCALE";
		const bool usando = ImGuizmo::IsUsing();
		char linea[320];
		std::snprintf(linea, sizeof(linea),
			"SelectedObject=%d Operation=%s IsOver=%d IsUsing=%d MouseDown=%d ViewportHovered=%d MatrixChanged=%d WantCaptureMouse=%d CameraMode=%d",
			seleccionado, nombresOperacion, gizmoSobre ? 1 : 0, usando ? 1 : 0, io.MouseDown[0] ? 1 : 0,
			entrada.sobrePanel ? 0 : 1, matrizCambiada ? 1 : 0, io.WantCaptureMouse ? 1 : 0, static_cast<int>(modoCamara));
		if (consoleDebugging)
		{
			static std::string anterior;
			const std::string actual = linea;
			if (actual != anterior) { std::fprintf(stderr, "[editor] %s\n", linea); anterior = actual; }
		}
		ImGui::SetNextWindowBgAlpha(0.6f);
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - 260.0f, io.DisplaySize.y - 70.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Editor input debug", &debugInput, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("SelectedObject: %d   Operation: %s", seleccionado, nombresOperacion);
			ImGui::Text("IsOver: %d  IsUsing: %d  MouseDown: %d", gizmoSobre ? 1 : 0, usando ? 1 : 0, io.MouseDown[0] ? 1 : 0);
			ImGui::Text("ViewportHovered: %d  MatrixChanged: %d  WantCaptureMouse: %d", entrada.sobrePanel ? 0 : 1, matrizCambiada ? 1 : 0, io.WantCaptureMouse ? 1 : 0);
		}
		ImGui::End();
	}

	void SceneEditor::DrawEditorInterface(EditorSceneContext contexto, int ancho, int alto)
	{
		ImGuizmo::BeginFrame();
		if (!depuracionIniciada)
		{
			depuracionIniciada = true;
			const char* variable = std::getenv("ESCAPETHEUAB_EDITOR_DEBUG");
			consoleDebugging = variable != nullptr && variable[0] != '\0' && variable[0] != '0';
			debugInput = debugInput || consoleDebugging;
		}
		if (activo)
		{
			// Arbitraje de la entrada de este fotograma (ver Actualizar).
			entrada.sobrePanel = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
			entrada.imguiActivo = ImGui::IsAnyItemActive();
			entrada.gizmoUsando = ImGuizmo::IsUsing();
			entrada.popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
			entrada.altPressed = ImGui::GetIO().KeyAlt;
			entrada.viewportLibre = FreeViewport(entrada.sobrePanel, entrada.imguiActivo, entrada.gizmoUsando, entrada.popup);
			haySeleccionValida = IsValidIndex(contexto, seleccionado);
			// Estado de reposo: si no hay arrastre ni campo en edición, así estaba la escena antes de cualquier cambio.
			if (!historial.IsTransactionOpen() && !ImGuizmo::IsUsing() && !ImGui::IsAnyItemActive())
			{
				respaldo = CaptureHistory(contexto);
				hayRespaldo = true;
			}
			UpdateVisibility(contexto);
		}
		DrawEditorToolbar(contexto);

		if (closeConfirmation) ImGui::OpenPopup("Unsaved scene changes");
		if (ImGui::BeginPopupModal("Unsaved scene changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("The scene has unsaved changes.");
			if (ImGui::Button("Save and close")) { saveRequested = true; closeAfterSave = true; closeConfirmation = false; ImGui::CloseCurrentPopup(); }
			ImGui::SameLine();
			if (ImGui::Button("Discard and close")) { closeConfirmed = true; closeConfirmation = false; ImGui::CloseCurrentPopup(); }
			ImGui::SameLine();
			if (ImGui::Button("Continue editing")) { closeConfirmation = false; ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}
		if (!activo) return;

		DrawHierarchy(contexto);
		DrawInspector(contexto);
		DrawAssetBrowser(contexto);
		DrawCameraOptions();
		SelectByMouse(contexto, ancho, alto);
		DrawGizmo(contexto, ancho, alto);
		DrawSelectionOutline(contexto, ancho, alto);
		DrawDebugShapes();
		CommitTransactionIfFinished(contexto);
		ProcessPendingRequests(contexto);
	}
}
