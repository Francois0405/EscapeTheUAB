#include "EditorEscenas.h"

#include "SelectorObjetos.h"
#include "imgui.h"
#include "../third_party/ImGuizmo.h"
#include "../EscenaFisica.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace editor
{
	namespace
	{
		constexpr float MinimoEscala = 0.001f;
		constexpr float Pi = 3.14159265358979323846f;
		constexpr float AlturaBarra = 46.0f;
		constexpr float DimensionMinimaParaColisionador = 0.30f;   // por debajo es un objeto decorativo pequeño

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

		glm::mat4 MatrizDeAnimado(const SceneObjetoAnimado& a)
		{
			glm::mat4 m = glm::translate(glm::mat4(1.0f), a.posicion);
			m = glm::rotate(m, glm::radians(a.giroZGrados), glm::vec3(0.0f, 0.0f, 1.0f));
			return glm::scale(m, glm::vec3(a.escala));
		}

		const char* NombreClase(ClaseElemento clase)
		{
			switch (clase)
			{
			case ClaseElemento::Suelo: return "floor";
			case ClaseElemento::Techo: return "ceiling";
			case ClaseElemento::Pared: return "wall";
			case ClaseElemento::Arquitectura: return "architecture";
			default: return "object";
			}
		}
	}

	// ---- Peticiones hacia main.cpp ------------------------------------------------------------

	bool EditorEscenas::ConsumeEntradaPlay() { const bool v = salirAPlay; salirAPlay = false; return v; }
	bool EditorEscenas::ConsumeGuardarRequest() { const bool v = guardarSolicitado; guardarSolicitado = false; return v; }
	bool EditorEscenas::ConsumeCerrarConfirmado() { const bool v = cerrarConfirmado; cerrarConfirmado = false; return v; }
	bool EditorEscenas::ConsumeCerrarTrasGuardar() { const bool v = cerrarTrasGuardar; cerrarTrasGuardar = false; return v; }

	void EditorEscenas::Activar(bool activar)
	{
		if (activar == activo) return;
		activo = activar;
		if (!activo) { salirAPlay = true; TerminarCaptura(); modoCamara = ModoCamara::Ninguno; }   // el historial se conserva
	}

	void EditorEscenas::FijarRaizModelos(const std::filesystem::path& raiz)
	{
		raizModelos = raiz;
		inventarioCargado = false;
	}

	void EditorEscenas::NuevaEscena()
	{
		historial.Reiniciar();
		glbEliminados.clear();
		reconstruirFisicaSolicitada = false;
		hayRespaldo = false;
		seleccionado = -1;
		bloqueoGizmoTrasSeleccion.Reiniciar();
		modificada = false;
		cajasObj.clear();
		cajasGlb.clear();
		visibilidad.MostrarTodo();
		mensajeEstado.clear();
	}

	// ---- Entrada --------------------------------------------------------------------------------

	void EditorEscenas::Atajo(int tecla, int accion, int modificadores, bool escribiendoTexto)
	{
		const bool control = (modificadores & GLFW_MOD_CONTROL) != 0;
		const bool mayus = (modificadores & GLFW_MOD_SHIFT) != 0;
		if (accion == GLFW_PRESS &&
			tecla == GLFW_KEY_F6 &&
			!escribiendoTexto)
		{
			Activar(!activo);
			return;
		}
		if (!activo) return;

		// Flechas / RePag / AvPag: mover el objeto seleccionado (o la cámara si no hay selección). Admiten repetición del teclado.
		Flecha flecha = Flecha::Arriba;
		bool esFlecha = true;
		switch (tecla)
		{
		case GLFW_KEY_UP: flecha = Flecha::Arriba; break;
		case GLFW_KEY_DOWN: flecha = Flecha::Abajo; break;
		case GLFW_KEY_LEFT: flecha = Flecha::Izquierda; break;
		case GLFW_KEY_RIGHT: flecha = Flecha::Derecha; break;
		case GLFW_KEY_PAGE_UP: flecha = Flecha::Subir; break;
		case GLFW_KEY_PAGE_DOWN: flecha = Flecha::Bajar; break;
		default: esFlecha = false; break;
		}
		if (esFlecha)
		{
			const unsigned bit = 1u << static_cast<unsigned>(flecha);
			if (accion == GLFW_RELEASE) { teclasFlechaPulsadas &= ~bit; return; }
			// Con un campo de texto o un widget en uso, las flechas son de ImGui y no tocan la escena.
			if (escribiendoTexto || entrada.imguiActivo) return;
			teclasFlechaPulsadas |= bit;
			flechasPendientes.push_back({ flecha, control, mayus });
			return;
		}

		if (accion != GLFW_PRESS) return;
		// Con un campo de texto/numérico en edición, los atajos del editor no deben actuar (ImGui gestiona ahí sus teclas).
		if (escribiendoTexto) return;
		if (control)
		{
			if (tecla == GLFW_KEY_S) guardarSolicitado = true;
			else if (tecla == GLFW_KEY_Z) { if (mayus) rehacerSolicitado = true; else deshacerSolicitado = true; }
			else if (tecla == GLFW_KEY_Y) rehacerSolicitado = true;
			else if (tecla == GLFW_KEY_D) duplicarSolicitado = true;
			return;
		}
		switch (tecla)
		{
		case GLFW_KEY_1: herramienta = Herramienta::Mover; break;
		case GLFW_KEY_2: herramienta = Herramienta::Rotar; break;
		case GLFW_KEY_3: herramienta = Herramienta::Escalar; break;
		case GLFW_KEY_F: enfocarSolicitado = true; break;
		case GLFW_KEY_DELETE: eliminarSolicitado = true; break;
		default: break;
		}
	}

	void EditorEscenas::TerminarCaptura()
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
	void EditorEscenas::Actualizar(GLFWwindow* ventana, float delta)
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
		const bool ortografica = camara.Ortografica();

		if (modoCamara == ModoCamara::Ninguno)
		{
			if (entrada.viewportLibre)
			{
				if (derecho) modoCamara = ortografica ? ModoCamara::Pan : ModoCamara::Mirar;
				else if (medio) modoCamara = ModoCamara::Pan;
				else if (izquierdo && alt) modoCamara = ortografica ? ModoCamara::Pan : ModoCamara::Orbitar;
			}
			if (modoCamara != ModoCamara::Ninguno)
			{
				pivoteOrbita = camara.Pivote();
				reiniciarDelta = true;
				dx = dy = 0.0f;
				if (modoCamara == ModoCamara::Mirar || modoCamara == ModoCamara::Orbitar)
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
			const bool sigue = (modoCamara == ModoCamara::Mirar) ? derecho
				: (modoCamara == ModoCamara::Orbitar) ? izquierdo
				: (derecho || medio || (izquierdo && alt));
			if (!sigue) { TerminarCaptura(); modoCamara = ModoCamara::Ninguno; return; }
		}

		switch (modoCamara)
		{
		case ModoCamara::Mirar:
		{
			camara.Mirar(dx, dy, opciones);
			if (!io.WantTextInput)
			{
				const float mult = MultiplicadorVelocidad(ctrl, mayus, opciones);   // Ctrl prevalece sobre Shift
				const auto tecla = [ventana](int k) { return glfwGetKey(ventana, k) == GLFW_PRESS ? 1.0f : 0.0f; };
				camara.Mover(tecla(GLFW_KEY_W) - tecla(GLFW_KEY_S), tecla(GLFW_KEY_D) - tecla(GLFW_KEY_A),
					tecla(GLFW_KEY_E) - tecla(GLFW_KEY_Q), delta, mult, opciones);
			}
			break;
		}
		case ModoCamara::Orbitar: camara.Orbitar(pivoteOrbita, dx, dy, opciones); break;
		case ModoCamara::Pan: camara.Pan(dx, dy, io.DisplaySize.y, opciones); break;
		default: break;
		}
	}

	void EditorEscenas::RuedaRaton(float cantidad)
	{
		if (!activo) return;
		// La rueda es del panel ImGui si el cursor está encima (Hierarchy, Inspector, Asset Browser...) y no de la cámara.
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || ImGuizmo::IsUsing() || modoCamara != ModoCamara::Ninguno) return;
		camara.Zoom(cantidad, opciones);
	}

	void EditorEscenas::Matrices(float aspecto, glm::mat4& vista, glm::mat4& proyeccion) const
	{
		camara.Matrices(aspecto, vista, proyeccion);
	}

	// ---- Estado, historial -------------------------------------------------------------------

	EstadoEscena EditorEscenas::Capturar(const ContextoEscena& contexto) const
	{
		return EstadoEscena{ contexto.objetos, contexto.animados };
	}

	void EditorEscenas::SincronizarGlb(const ContextoEscena& contexto) const
	{
		for (const SceneObjetoAnimado& animado : contexto.animados)
		{
			if (glb::ObjetoAnimado* instancia = glb::BuscarObjeto(contexto.escenaGlb, animado.id))
			{
				instancia->posicion = animado.posicion;
				instancia->giroZGrados = animado.giroZGrados;
				instancia->escala = animado.escala;
			}
			if (!animado.paredId.empty() && !animado.aperturaId.empty())
			{
				auto pared = std::find_if(contexto.objetos.begin(), contexto.objetos.end(),
					[&](const SceneObject& item) { return item.id == animado.paredId && item.pared.presente; });
				if (pared == contexto.objetos.end()) continue;
				const float yaw = glm::radians(pared->rotacion.z);
				const glm::vec3 ejeU(std::cos(yaw), std::sin(yaw), 0.0f);
				auto apertura = std::find_if(pared->pared.aperturas.begin(), pared->pared.aperturas.end(),
					[&](const SceneAperturaPared& item) { return item.id == animado.aperturaId; });
				if (apertura == pared->pared.aperturas.end()) continue;
				apertura->centerU = glm::dot(animado.posicion - pared->posicion, ejeU);
				const glm::vec3 mitad = glm::abs(pared->escala) * (pared->fisica.presente ? pared->fisica.mitadTamano : glm::vec3(1.0f));
				apertura->base = std::max(0.0f, animado.posicion.z - (pared->posicion.z - mitad.z));
			}
		}
	}

	void EditorEscenas::Restaurar(ContextoEscena contexto, EstadoEscena estado)
	{
		contexto.objetos = std::move(estado.objetos);
		contexto.animados = std::move(estado.animados);
		// Reconcile GLB instances by stable id. Models stay cached in EscenaAnimada::modelos;
		// undo only restores an instance and never reloads or removes the shared asset.
		for (auto it = contexto.escenaGlb.objetos.begin(); it != contexto.escenaGlb.objetos.end();)
		{
			const bool remains = std::any_of(contexto.animados.begin(), contexto.animados.end(),
				[&](const SceneObjetoAnimado& item) { return item.id == it->id; });
			if (!remains) it = contexto.escenaGlb.objetos.erase(it); else ++it;
		}
		for (const SceneObjetoAnimado& data : contexto.animados)
		{
			if (glb::BuscarObjeto(contexto.escenaGlb, data.id)) continue;
			auto cached = glbEliminados.find(data.id);
			if (cached != glbEliminados.end()) contexto.escenaGlb.objetos.push_back(cached->second);
		}
		SincronizarGlb(contexto);   // vista y cajas se rehacen solas: en EDIT todo se dibuja desde estas listas
		reconstruirFisicaSolicitada = true;
		const int total = static_cast<int>(contexto.objetos.size() + contexto.animados.size());
		if (seleccionado >= total) FijarSeleccion(total - 1);
		hayRespaldo = false;
		MarcarModificada();
	}

	bool EditorEscenas::Deshacer(ContextoEscena contexto)
	{
		EstadoEscena actual = Capturar(contexto);
		if (!historial.Deshacer(actual)) { InformarEstado("Nothing to undo"); return false; }
		Restaurar(contexto, std::move(actual));
		InformarEstado("Undo");
		return true;
	}

	bool EditorEscenas::Rehacer(ContextoEscena contexto)
	{
		EstadoEscena actual = Capturar(contexto);
		if (!historial.Rehacer(actual)) { InformarEstado("Nothing to redo"); return false; }
		Restaurar(contexto, std::move(actual));
		InformarEstado("Redo");
		return true;
	}

	void EditorEscenas::AbrirTransaccionSiHaceFalta()
	{
		if (!historial.TransaccionAbierta() && hayRespaldo) historial.IniciarTransaccion(respaldo);
	}

	void EditorEscenas::CerrarTransaccionSiTermino(const ContextoEscena& contexto)
	{
		if (historial.TransaccionAbierta() && !ImGuizmo::IsUsing() && !ImGui::IsAnyItemActive() && teclasFlechaPulsadas == 0)
			historial.CerrarTransaccion(Capturar(contexto));
	}

	bool EditorEscenas::IndiceValido(const ContextoEscena& contexto, int indice) const
	{
		return indice >= 0 && indice < static_cast<int>(contexto.objetos.size() + contexto.animados.size());
	}

	std::string EditorEscenas::IdDe(const ContextoEscena& contexto, int indice) const
	{
		if (!IndiceValido(contexto, indice)) return {};
		if (!EsAnimado(contexto, indice)) return contexto.objetos[static_cast<std::size_t>(indice)].id;
		return contexto.animados[static_cast<std::size_t>(indice) - contexto.objetos.size()].id;
	}

	std::string EditorEscenas::IdUnico(const ContextoEscena& contexto, const std::string& base) const
	{
		auto existe = [&contexto](const std::string& id) {
			return std::any_of(contexto.objetos.begin(), contexto.objetos.end(), [&id](const SceneObject& o) { return o.id == id; }) ||
				std::any_of(contexto.animados.begin(), contexto.animados.end(), [&id](const SceneObjetoAnimado& a) { return a.id == id; });
		};
		std::string id = base;
		for (unsigned int sufijo = 1; existe(id); ++sufijo) id = base + "_" + std::to_string(sufijo);
		return id;
	}

	// ---- Cajas y selección ------------------------------------------------------------------

	CajaLocal EditorEscenas::CajaDeObjeto(const SceneObject& objeto)
	{
		auto it = cajasObj.find(objeto.modelo);
		if (it == cajasObj.end())
		{
			CajaLocal caja = LimitesDeOBJ(objeto.modelo);
			it = cajasObj.emplace(objeto.modelo, caja).first;
		}
		CajaLocal caja = it->second;
		if (!caja.valida)
		{	// OBJ ilegible: usa la caja de física, o un cubo unitario, para que siga siendo seleccionable
			caja.valida = true;
			if (objeto.fisica.presente)
			{
				caja.minimo = objeto.fisica.desplazamiento - objeto.fisica.mitadTamano;
				caja.maximo = objeto.fisica.desplazamiento + objeto.fisica.mitadTamano;
			}
		}
		return ConMitadMinima(caja, 0.02f);
	}

	CajaLocal EditorEscenas::CajaDeAnimado(const ContextoEscena& contexto, const SceneObjetoAnimado& animado)
	{
		auto it = cajasGlb.find(animado.modelo);
		if (it == cajasGlb.end())
		{
			CajaLocal caja;
			if (glb::ObjetoAnimado* instancia = glb::BuscarObjeto(contexto.escenaGlb, animado.id))
				if (instancia->modelo != nullptr) caja = LimitesDeGLB(*instancia->modelo);
			it = cajasGlb.emplace(animado.modelo, caja).first;
		}
		CajaLocal caja = it->second;
		if (!caja.valida) { caja.valida = true; caja.minimo = glm::vec3(-0.5f, -0.1f, 0.0f); caja.maximo = glm::vec3(0.5f, 0.1f, 2.1f); }
		return ConMitadMinima(caja, 0.05f);   // una puerta es fina: se garantiza un grosor seleccionable
	}

	EditorEscenas::ElementoSeleccionable EditorEscenas::Elemento(const ContextoEscena& contexto, int indice)
	{
		ElementoSeleccionable e;
		e.indice = indice;
		if (!IndiceValido(contexto, indice)) return e;
		if (!EsAnimado(contexto, indice))
		{
			const SceneObject& objeto = contexto.objetos[static_cast<std::size_t>(indice)];
			e.modelo = fisica::MatrizDeObjeto(objeto);
			e.caja = CajaDeObjeto(objeto);
		}
		else
		{
			const SceneObjetoAnimado& animado = contexto.animados[static_cast<std::size_t>(indice) - contexto.objetos.size()];
			e.modelo = MatrizDeAnimado(animado);
			e.caja = CajaDeAnimado(contexto, animado);
		}
		return e;
	}

	std::vector<CajaOrientada> EditorEscenas::CajasDeObjeto(const SceneObject& objeto, const glm::mat4& matriz)
	{
		if (objeto.pared.presente && !objeto.pared.aperturas.empty())
		{
			const glm::vec3 escala(glm::length(glm::vec3(matriz[0])), glm::length(glm::vec3(matriz[1])), glm::length(glm::vec3(matriz[2])));
			glm::mat4 pose = glm::translate(glm::mat4(1.0f), glm::vec3(matriz[3]));
			glm::mat3 rotacion;
			for (int eje = 0; eje < 3; ++eje) rotacion[eje] = glm::normalize(glm::vec3(matriz[eje]));
			pose *= glm::mat4(rotacion);
			const glm::vec3 mitadPared = glm::abs(escala) * (objeto.fisica.presente ? objeto.fisica.mitadTamano : CajaDeObjeto(objeto).MitadTamano());
			std::vector<SceneSeccionPared> secciones;
			std::string error;
			if (GenerarSeccionesPared(mitadPared, objeto.pared.aperturas, secciones, error))
			{
				std::vector<CajaOrientada> cajas;
				for (const auto& seccion : secciones)
					cajas.push_back(TransformarCaja(pose, seccion.centroLocal, seccion.mitad));
				return cajas;
			}
		}
		if (objeto.fisica.presente)
		{
			const glm::mat4 collider = matriz * glm::translate(glm::mat4(1.0f), objeto.fisica.desplazamiento);
			return { TransformarCaja(collider, glm::vec3(0.0f), objeto.fisica.mitadTamano) };
		}
		const CajaLocal limites = CajaDeObjeto(objeto);
		return { TransformarCaja(matriz, limites.Centro(), limites.MitadTamano()) };
	}

	std::vector<CajaOrientada> EditorEscenas::CajasDeAnimado(const ContextoEscena& contexto, const SceneObjetoAnimado& animado)
	{
		const glb::ObjetoAnimado* instancia = glb::BuscarObjeto(contexto.escenaGlb, animado.id);
		if (instancia != nullptr && instancia->modelo != nullptr && !animado.colisionadores.empty())
		{
			glm::mat4 base = glm::translate(glm::mat4(1.0f), animado.posicion);
			base = glm::rotate(base, glm::radians(animado.giroZGrados), glm::vec3(0, 0, 1));
			base = glm::scale(base, glm::vec3(animado.escala));
			std::vector<CajaOrientada> cajas;
			for (const SceneColisionador& datos : animado.colisionadores)
			{
				const int nodo = glb::BuscarNodoPorNombre(*instancia->modelo, datos.nodo);
				if (nodo < 0) continue;
				const glm::mat4 mundoNodo = base * glb::MatrizYArribaAZArriba() *
					glb::MatrizMundoDelNodo(*instancia->modelo, instancia->pose, nodo);
				cajas.push_back(TransformarCaja(mundoNodo, datos.desplazamiento, datos.mitadTamano));
			}
			if (!cajas.empty()) return cajas;
		}
		return { TransformarCaja(MatrizDeAnimado(animado), CajaDeAnimado(contexto, animado).Centro(),
			CajaDeAnimado(contexto, animado).MitadTamano()) };
	}

	ResultadoColocacion EditorEscenas::ValidarObjeto(const ContextoEscena& contexto, const SceneObject& candidato, int indiceIgnorado,
		const glm::mat4* matrizPropuesta)
	{
		const SceneObject& actual = indiceIgnorado >= 0 && indiceIgnorado < static_cast<int>(contexto.objetos.size())
			? contexto.objetos[static_cast<std::size_t>(indiceIgnorado)] : candidato;
		const auto propuesta = CajasDeObjeto(candidato, matrizPropuesta != nullptr ? *matrizPropuesta : fisica::MatrizDeObjeto(candidato));
		const auto anterior = indiceIgnorado >= 0 && indiceIgnorado < static_cast<int>(contexto.objetos.size())
			? CajasDeObjeto(actual, fisica::MatrizDeObjeto(actual)) : std::vector<CajaOrientada>{};
		std::vector<ObstaculoColocacion> obstaculos;
		constexpr float toleranciaContacto = 0.01f;
		for (std::size_t i = 0; i < contexto.objetos.size(); ++i)
		{
			if (static_cast<int>(i) == indiceIgnorado || contexto.objetos[i].id == candidato.id) continue;
			const SceneObject& objeto = contexto.objetos[i];
			const auto cajas = CajasDeObjeto(objeto, fisica::MatrizDeObjeto(objeto));
			const glm::vec3 dimensiones = objeto.fisica.presente
				? glm::abs(objeto.escala) * objeto.fisica.mitadTamano
				: glm::abs(objeto.escala) * CajaDeObjeto(objeto).MitadTamano();
			const float minimo = std::min({ dimensiones.x, dimensiones.y, dimensiones.z });
			const float medio = dimensiones.x + dimensiones.y + dimensiones.z - minimo - std::max({ dimensiones.x, dimensiones.y, dimensiones.z });
			const float maximo = std::max({ dimensiones.x, dimensiones.y, dimensiones.z });
			const bool paredPorForma = objeto.fisica.presente && objeto.fisica.tipo == fisica::TipoCuerpo::Estatico &&
				minimo < 0.30f && medio > 1.0f && maximo > 4.0f;
			const bool arquitectura = objeto.categoria == "architecture" || paredPorForma;
			const std::string tipo = paredPorForma && objeto.categoria != "architecture" ? "wall" :
				objeto.categoria == "architecture" ? "architecture" : "object";
			for (const CajaOrientada& caja : cajas)
				obstaculos.push_back({ caja, objeto.id, tipo, arquitectura });
		}
		for (const SceneObjetoAnimado& animado : contexto.animados)
		{
			if (animado.id == candidato.id) continue;
			for (const CajaOrientada& caja : CajasDeAnimado(contexto, animado))
				obstaculos.push_back({ caja, animado.id, "animated object", false });
		}
		return editor::ValidarColocacion(propuesta, anterior, obstaculos,
			candidato.permitirPenetracionArquitectura, toleranciaContacto);
	}

	ResultadoColocacion EditorEscenas::ValidarAnimado(const ContextoEscena& contexto, const SceneObjetoAnimado& candidato, int indiceIgnorado)
	{
		const SceneObjetoAnimado& actual = indiceIgnorado >= static_cast<int>(contexto.objetos.size()) &&
			indiceIgnorado < static_cast<int>(contexto.objetos.size() + contexto.animados.size())
			? contexto.animados[static_cast<std::size_t>(indiceIgnorado) - contexto.objetos.size()] : candidato;
		const auto propuesta = CajasDeAnimado(contexto, candidato);
		const auto anterior = indiceIgnorado >= static_cast<int>(contexto.objetos.size()) &&
			indiceIgnorado < static_cast<int>(contexto.objetos.size() + contexto.animados.size())
			? CajasDeAnimado(contexto, actual) : std::vector<CajaOrientada>{};
		std::vector<ObstaculoColocacion> obstaculos;
		for (std::size_t i = 0; i < contexto.objetos.size(); ++i)
		{
			SceneObject objeto = contexto.objetos[i];
			if (objeto.id == candidato.paredId && !candidato.aperturaId.empty())
			{
				auto apertura = std::find_if(objeto.pared.aperturas.begin(), objeto.pared.aperturas.end(),
					[&](const SceneAperturaPared& a) { return a.id == candidato.aperturaId; });
				if (apertura != objeto.pared.aperturas.end())
				{
					const float yaw = glm::radians(objeto.rotacion.z);
					const glm::vec3 ejeU(std::cos(yaw), std::sin(yaw), 0.0f);
					apertura->centerU = glm::dot(candidato.posicion - objeto.posicion, ejeU);
					const glm::vec3 mitad = glm::abs(objeto.escala) * (objeto.fisica.presente ? objeto.fisica.mitadTamano : glm::vec3(1.0f));
					apertura->base = candidato.posicion.z - (objeto.posicion.z - mitad.z);
					std::vector<SceneSeccionPared> secciones;
					std::string error;
					if (!GenerarSeccionesPared(mitad, objeto.pared.aperturas, secciones, error))
						return { false, "wall opening: " + error };
					const float normalOffset = glm::length(candidato.posicion - objeto.posicion - ejeU * apertura->centerU - glm::vec3(0, 0, candidato.posicion.z - objeto.posicion.z));
					if (normalOffset > mitad.y + 0.15f || apertura->base < -0.01f)
						return { false, "door must remain aligned with its associated wall" };
				}
			}
			const auto cajas = CajasDeObjeto(objeto, fisica::MatrizDeObjeto(objeto));
			const glm::vec3 dimensiones = objeto.fisica.presente
				? glm::abs(objeto.escala) * objeto.fisica.mitadTamano
				: glm::abs(objeto.escala) * CajaDeObjeto(objeto).MitadTamano();
			const float minimo = std::min({ dimensiones.x, dimensiones.y, dimensiones.z });
			const float medio = dimensiones.x + dimensiones.y + dimensiones.z - minimo - std::max({ dimensiones.x, dimensiones.y, dimensiones.z });
			const float maximo = std::max({ dimensiones.x, dimensiones.y, dimensiones.z });
			const bool arquitectura = objeto.categoria == "architecture" ||
				(objeto.fisica.presente && objeto.fisica.tipo == fisica::TipoCuerpo::Estatico && minimo < 0.30f && medio > 1.0f && maximo > 4.0f);
			for (const CajaOrientada& caja : cajas) obstaculos.push_back({ caja, objeto.id, arquitectura ? "architecture" : "object", arquitectura });
		}
		for (std::size_t i = 0; i < contexto.animados.size(); ++i)
		{
			const SceneObjetoAnimado& otro = contexto.animados[i];
			if (static_cast<int>(contexto.objetos.size() + i) == indiceIgnorado || otro.id == candidato.id) continue;
			for (const CajaOrientada& caja : CajasDeAnimado(contexto, otro))
				obstaculos.push_back({ caja, otro.id, "animated object", false });
		}
		return editor::ValidarColocacion(propuesta, anterior, obstaculos,
			candidato.permitirPenetracionArquitectura, 0.01f);
	}

	void EditorEscenas::SeleccionarPorRaton(ContextoEscena contexto, int ancho, int alto)
	{
		const ImGuiIO& io = ImGui::GetIO();
		// Prioridad 4: solo un clic simple en el viewport (no sobre un panel, ni arrastrando el gizmo, ni navegando con Alt/botones).
		if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) || !entrada.viewportLibre || entrada.altPulsado ||
			modoCamara != ModoCamara::Ninguno || ImGuizmo::IsOver()) return;
		if (ancho <= 0 || alto <= 0) return;

		glm::mat4 vista, proyeccion;
		Matrices(static_cast<float>(ancho) / static_cast<float>(alto), vista, proyeccion);
		const Rayo rayo = RayoDesdePantalla(vista, proyeccion, glm::vec2(io.MousePos.x, io.MousePos.y),
			glm::vec4(0.0f, 0.0f, static_cast<float>(ancho), static_cast<float>(alto)));

		std::vector<CandidatoSeleccion> candidatos;
		const int total = static_cast<int>(contexto.objetos.size() + contexto.animados.size());
		for (int i = 0; i < total; ++i)
		{
			if (!EsAnimado(contexto, i))
			{
				if (!ObjetoVisible(static_cast<std::size_t>(i))) continue;   // lo oculto nunca bloquea la selección
			}
			else if (!visibilidad.AnimadoVisible(IdDe(contexto, i))) continue;
			const ElementoSeleccionable e = Elemento(contexto, i);
			candidatos.push_back({ e.indice, e.modelo, e.caja });
		}
		FijarSeleccion(ElegirMasCercano(rayo, candidatos));
	}

	void EditorEscenas::ActualizarVisibilidad(const ContextoEscena& contexto)
	{
		clases = ClasificarElementos(contexto.objetos);
		visibles.assign(contexto.objetos.size(), 1);
		const bool superior = camara.vista == Vista::Superior;
		for (std::size_t i = 0; i < contexto.objetos.size(); ++i)
			visibles[i] = visibilidad.Visible(contexto.objetos[i], clases[i], superior) ? 1 : 0;
		aisladoEsAnimado = visibilidad.aislar &&
			std::any_of(contexto.animados.begin(), contexto.animados.end(),
				[this](const SceneObjetoAnimado& a) { return a.id == visibilidad.idAislado; });
	}

	bool EditorEscenas::ObjetoVisible(std::size_t indice) const
	{
		return !activo || indice >= visibles.size() || visibles[indice] != 0;
	}

	bool EditorEscenas::MostrarAnimados() const
	{
		return !activo || !visibilidad.aislar || aisladoEsAnimado;
	}

	void EditorEscenas::EnfocarSeleccion(const ContextoEscena& contexto)
	{
		if (!IndiceValido(contexto, seleccionado)) return;
		const ElementoSeleccionable e = Elemento(contexto, seleccionado);
		const CajaLocal mundo = CajaEnMundo(e.modelo, e.caja);
		if (!mundo.valida) return;
		camara.Enfocar(mundo.Centro(), glm::length(mundo.MitadTamano()));
	}

	float EditorEscenas::AlturaDelSuelo(const ContextoEscena& contexto) const
	{
		const std::vector<ClaseElemento> c = ClasificarElementos(contexto.objetos);
		float techoDelSuelo = std::numeric_limits<float>::lowest();
		for (std::size_t i = 0; i < contexto.objetos.size(); ++i)
		{
			if (c[i] != ClaseElemento::Suelo) continue;
			const SceneObject& o = contexto.objetos[i];
			const float mitad = std::abs(o.escala.z) * (o.fisica.presente ? o.fisica.mitadTamano.z : 1.0f);
			techoDelSuelo = std::max(techoDelSuelo, o.posicion.z + mitad);
		}
		return techoDelSuelo;
	}

	// ---- Operaciones ---------------------------------------------------------------------------

	std::string EditorEscenas::AgregarModelo(ContextoEscena contexto, const std::string& clave, bool yArribaAZArriba,
		bool colisionador, bool dinamico)
	{
		if (clave.empty()) return {};

		SceneObject objeto;   // todo explícito: una escala sin inicializar (0) deja el modelo invisible y el JSON inválido
		objeto.id = IdUnico(contexto, "obj_" + std::filesystem::path(clave).stem().string());
		objeto.categoria = "objects";
		objeto.modelo = clave;
		objeto.posicion = glm::vec3(0.0f);
		objeto.rotacion = yArribaAZArriba ? glm::vec3(90.0f, 0.0f, 0.0f) : glm::vec3(0.0f);
		objeto.escala = glm::vec3(1.0f);

		CajaLocal caja = CajaDeObjeto(objeto);
		const CajaLocal mundo = CajaEnMundo(fisica::MatrizDeObjeto(objeto), caja);
		const float suelo = AlturaDelSuelo(contexto);
		const glm::vec3 centro = mundo.Centro();
		const glm::vec3 objetivo = camara.Pivote();
		objeto.posicion.x = objetivo.x - centro.x;
		objeto.posicion.y = objetivo.y - centro.y;
		// Apoyado sobre el suelo si la sala lo tiene; si no, centrado en el punto que mira la cámara.
		objeto.posicion.z = suelo > std::numeric_limits<float>::lowest() / 2.0f ? suelo - mundo.minimo.z : objetivo.z - centro.z;

		std::string aviso;
		if (colisionador)
		{
			const glm::vec3 tamano = mundo.maximo - mundo.minimo;
			if (std::max({ tamano.x, tamano.y, tamano.z }) < DimensionMinimaParaColisionador)
				aviso = " (small decorative object: no collider created; add one from Inspector > Physics)";
			else
			{
				objeto.fisica.presente = true;
				objeto.fisica.tipo = dinamico ? fisica::TipoCuerpo::Dinamico : fisica::TipoCuerpo::Estatico;
				objeto.fisica.mitadTamano = caja.MitadTamano();
				objeto.fisica.desplazamiento = caja.Centro();
				objeto.fisica.masa = dinamico ? 5.0f : 0.0f;
			}
		}
		const ResultadoColocacion resultado = ValidarObjeto(contexto, objeto, -1);
		if (!resultado.valida)
		{
			mensajeColocacion = "Placement blocked by " + resultado.bloqueador;
			mensajeEsError = true;
			InformarEstado(mensajeColocacion, true);
			return {};
		}
		mensajeColocacion.clear();
		historial.Registrar(Capturar(contexto));
		const std::string id = objeto.id;
		contexto.objetos.push_back(std::move(objeto));
		FijarSeleccion(static_cast<int>(contexto.objetos.size()) - 1);
		hayRespaldo = false;
		MarcarModificada();
		InformarEstado("Added " + id + aviso);
		return id;
	}

	bool EditorEscenas::DuplicarSeleccion(ContextoEscena contexto)
	{
		if (!IndiceValido(contexto, seleccionado)) return false;
		if (EsAnimado(contexto, seleccionado))
		{
			SceneObjetoAnimado copia = contexto.animados[static_cast<std::size_t>(seleccionado) - contexto.objetos.size()];
			if (copia.paredId.empty() || copia.aperturaId.empty())
			{ InformarEstado("Install this door in a parametric wall before duplicating it.", true); return false; }
			const int paredIndex = static_cast<int>(std::distance(contexto.objetos.begin(), std::find_if(contexto.objetos.begin(), contexto.objetos.end(),
				[&](const SceneObject& item) { return item.id == copia.paredId && item.pared.presente; })));
			if (paredIndex < 0 || paredIndex >= static_cast<int>(contexto.objetos.size()))
			{ InformarEstado("The associated wall no longer exists.", true); return false; }
			SceneObject& pared = contexto.objetos[static_cast<std::size_t>(paredIndex)];
			auto apertura = std::find_if(pared.pared.aperturas.begin(), pared.pared.aperturas.end(),
				[&](const SceneAperturaPared& a) { return a.id == copia.aperturaId; });
			if (apertura == pared.pared.aperturas.end())
			{ InformarEstado("The door opening association is missing.", true); return false; }
			copia.id = IdUnico(contexto, copia.id + "_copy");
			copia.aperturaId = copia.id;
			SceneAperturaPared nuevoVano = *apertura;
			nuevoVano.id = copia.id;
			nuevoVano.idPuerta = copia.id;
			const float desplazamientoU = nuevoVano.centerU + 1.25f + nuevoVano.ancho * 0.5f <= std::abs(pared.escala.x) - 0.1f ? 1.25f : -1.25f;
			nuevoVano.centerU += desplazamientoU;
			std::vector<SceneSeccionPared> secciones;
			std::vector<SceneAperturaPared> vanos = pared.pared.aperturas;
			vanos.push_back(nuevoVano);
			std::string errorVano;
			if (!GenerarSeccionesPared(glm::abs(pared.escala) * pared.fisica.mitadTamano, vanos, secciones, errorVano))
			{ InformarEstado("Door duplicate blocked: " + errorVano, true); return false; }
			const float angulo = glm::radians(pared.rotacion.z);
			const glm::vec3 ejeU(std::cos(angulo), std::sin(angulo), 0.0f);
			copia.posicion += ejeU * desplazamientoU;
			copia.paredId = pared.id;
			const EstadoEscena estadoAnterior = Capturar(contexto);
			pared.pared.aperturas.push_back(nuevoVano);
			contexto.animados.push_back(copia);
			if (const glb::ObjetoAnimado* original = glb::BuscarObjeto(contexto.escenaGlb,
				contexto.animados[static_cast<std::size_t>(seleccionado) - contexto.objetos.size()].id))
			{
				glb::ObjetoAnimado clon = *original;
				clon.id = copia.id;
				clon.posicion = copia.posicion;
				clon.giroZGrados = copia.giroZGrados;
				clon.escala = copia.escala;
				clon.pose = glb::CrearPoseDeReposo(*clon.modelo);
				clon.estado = glb::EstadoPuerta::Cerrada;
				clon.reproductor = {};
				glbEliminados[copia.id] = clon; // permite que Redo reconstruya la instancia sin recargar el modelo compartido
				contexto.escenaGlb.objetos.push_back(std::move(clon));
			}
			else
			{
				contexto.animados.pop_back();
				pared.pared.aperturas.pop_back();
				InformarEstado("The shared GLB model is not loaded.", true); return false;
			}
			const int nuevoIndice = static_cast<int>(contexto.objetos.size() + contexto.animados.size() - 1);
			const auto validacion = ValidarAnimado(contexto, contexto.animados.back(), nuevoIndice);
			if (!validacion.valida)
			{
				contexto.animados.pop_back();
				contexto.escenaGlb.objetos.pop_back();
				pared.pared.aperturas.pop_back();
				InformarEstado("Door duplicate blocked by " + validacion.bloqueador, true); return false;
			}
			historial.Registrar(estadoAnterior);
			reconstruirFisicaSolicitada = true;
			FijarSeleccion(nuevoIndice); hayRespaldo = false; MarcarModificada();
			InformarEstado("Door duplicated with a separate wall opening.");
			return true;
		}
		SceneObject copia = contexto.objetos[static_cast<std::size_t>(seleccionado)];
		copia.id = IdUnico(contexto, copia.id + "_copy");
		copia.categoria = "objects";
		copia.posicion.x += 0.5f;
		const ResultadoColocacion resultado = ValidarObjeto(contexto, copia, -1);
		if (!resultado.valida)
		{
			mensajeColocacion = "Duplicate blocked by " + resultado.bloqueador;
			InformarEstado(mensajeColocacion, true);
			return false;
		}
		mensajeColocacion.clear();
		historial.Registrar(Capturar(contexto));
		contexto.objetos.push_back(std::move(copia));
		FijarSeleccion(static_cast<int>(contexto.objetos.size()) - 1);
		hayRespaldo = false;
		MarcarModificada();
		InformarEstado("Duplicated");
		return true;
	}

	bool EditorEscenas::EliminarSeleccion(ContextoEscena contexto, bool cerrarHueco)
	{
		if (!IndiceValido(contexto, seleccionado)) return false;
		historial.Registrar(Capturar(contexto));
		std::string id;
		if (EsAnimado(contexto, seleccionado))
		{
			const std::size_t index = static_cast<std::size_t>(seleccionado) - contexto.objetos.size();
			const SceneObjetoAnimado& datos = contexto.animados[index];
			id = datos.id;
			if (!datos.paredId.empty() && !datos.aperturaId.empty())
			{
				auto pared = std::find_if(contexto.objetos.begin(), contexto.objetos.end(),
					[&](const SceneObject& item) { return item.id == datos.paredId && item.pared.presente; });
				if (pared != contexto.objetos.end())
				{
					auto apertura = std::find_if(pared->pared.aperturas.begin(), pared->pared.aperturas.end(),
						[&](const SceneAperturaPared& a) { return a.id == datos.aperturaId && a.idPuerta == datos.id; });
					if (apertura != pared->pared.aperturas.end())
					{
						if (cerrarHueco) pared->pared.aperturas.erase(apertura);
						else apertura->idPuerta.clear();
					}
				}
			}
			if (glb::ObjetoAnimado* instancia = glb::BuscarObjeto(contexto.escenaGlb, id))
			{
				glbEliminados[id] = *instancia;
				auto& instancias = contexto.escenaGlb.objetos;
				instancias.erase(std::remove_if(instancias.begin(), instancias.end(),
					[&](const glb::ObjetoAnimado& item) { return item.id == id; }), instancias.end());
			}
			contexto.animados.erase(contexto.animados.begin() + static_cast<std::ptrdiff_t>(index));
			reconstruirFisicaSolicitada = true;
		}
		else
		{
			id = contexto.objetos[static_cast<std::size_t>(seleccionado)].id;
			contexto.objetos.erase(contexto.objetos.begin() + seleccionado);
		}
		const int total = static_cast<int>(contexto.objetos.size() + contexto.animados.size());
		FijarSeleccion(total == 0 ? -1 : std::min(seleccionado, total - 1));
		hayRespaldo = false;
		MarcarModificada();
		InformarEstado(std::string("Deleted ") + id + (cerrarHueco ? " and closed its wall opening" : "; wall opening kept") + " (Ctrl+Z to restore)");
		return true;
	}

	bool EditorEscenas::InstalarPuertaEnPared(ContextoEscena contexto, int indicePuerta, int indicePared)
	{
		if (indicePuerta < 0 || indicePuerta >= static_cast<int>(contexto.animados.size()) ||
			indicePared < 0 || indicePared >= static_cast<int>(contexto.objetos.size())) return false;
		SceneObjetoAnimado& puerta = contexto.animados[static_cast<std::size_t>(indicePuerta)];
		SceneObject& paredNueva = contexto.objetos[static_cast<std::size_t>(indicePared)];
		if (!paredNueva.pared.presente || (paredNueva.fisica.presente && paredNueva.fisica.tipo != fisica::TipoCuerpo::Estatico))
		{ InformarEstado("Select a parametric wall with static collision first.", true); return false; }
		float mitadAncho = 0.0f, alturaMarco = 0.0f;
		for (const SceneColisionador& c : puerta.colisionadores)
			if (c.nodo == "DoorFrame")
			{
				mitadAncho = std::max(mitadAncho, std::abs(c.desplazamiento.x) + c.mitadTamano.x);
				alturaMarco = std::max(alturaMarco, c.desplazamiento.y + c.mitadTamano.y);
			}
		if (!(mitadAncho > 0.0f && alturaMarco > 0.0f))
		{ InformarEstado("DoorFrame colliders are required to derive the opening dimensions.", true); return false; }
		const float a = glm::radians(paredNueva.rotacion.z);
		const glm::vec3 ejeU(std::cos(a), std::sin(a), 0.0f);
		const glm::vec3 delta = puerta.posicion - paredNueva.posicion;
		SceneAperturaPared nueva;
		nueva.id = puerta.id + "_opening";
		for (unsigned suffix = 2; std::any_of(paredNueva.pared.aperturas.begin(), paredNueva.pared.aperturas.end(),
			[&](const SceneAperturaPared& x) { return x.id == nueva.id; }); ++suffix)
			nueva.id = puerta.id + "_opening_" + std::to_string(suffix);
		nueva.idPuerta = puerta.id;
		nueva.centerU = glm::dot(delta, ejeU);
		nueva.ancho = mitadAncho * 2.0f + 0.04f; // margen de montaje pequeño alrededor del marco
		nueva.alto = alturaMarco;
		nueva.base = 0.0f;
		const glm::vec3 halfWall = glm::abs(paredNueva.escala) *
			(paredNueva.fisica.presente ? paredNueva.fisica.mitadTamano : glm::vec3(1.0f));
		std::vector<SceneSeccionPared> secciones;
		std::vector<SceneAperturaPared> propuestaAperturas;
		for (const auto& ap : paredNueva.pared.aperturas)
			if (ap.idPuerta != puerta.id) propuestaAperturas.push_back(ap);
		propuestaAperturas.push_back(nueva);
		std::string error;
		if (!GenerarSeccionesPared(halfWall, propuestaAperturas, secciones, error))
		{ InformarEstado("Door opening rejected: " + error, true); return false; }

		const EstadoEscena anterior = Capturar(contexto);
		const SceneObjetoAnimado puertaAnterior = puerta;
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
				[&](const SceneAperturaPared& ap) { return ap.id == puerta.aperturaId; }), old.pared.aperturas.end());
		}
		paredNueva.pared.aperturas = std::move(propuestaAperturas);
		puerta.paredId = paredNueva.id;
		puerta.aperturaId = nueva.id;
		puerta.posicion = paredNueva.posicion + ejeU * nueva.centerU + glm::vec3(0.0f, 0.0f,
			paredNueva.posicion.z - halfWall.z - puerta.posicion.z);
		puerta.giroZGrados = paredNueva.rotacion.z;
		const auto resultado = ValidarAnimado(contexto, puerta, static_cast<int>(contexto.objetos.size()) + indicePuerta);
		if (!resultado.valida)
		{
			paredNueva = paredNuevaAnterior;
			if (paredAnteriorIndex >= 0 && paredAnteriorIndex < static_cast<int>(contexto.objetos.size()) && paredAnteriorIndex != indicePared)
				contexto.objetos[static_cast<std::size_t>(paredAnteriorIndex)] = paredAnterior;
			puerta = puertaAnterior;
			InformarEstado("Door installation blocked by " + resultado.bloqueador, true);
			return false;
		}
		historial.Registrar(anterior);
		SincronizarGlb(contexto);
		reconstruirFisicaSolicitada = true;
		MarcarModificada(); mensajeColocacion.clear();
		InformarEstado("Door installed; wall opening and static colliders updated.");
		return true;
	}

	void EditorEscenas::ProcesarSolicitudes(ContextoEscena contexto)
	{
		// Flechas: mueven el objeto seleccionado (herramienta Mover) o, sin selección, la cámara. Una pulsación mantenida
		// (con la repetición del teclado) forma UNA operación de historial: se cierra al soltar todas las flechas.
		for (const PulsacionFlecha& pulsacion : flechasPendientes)
		{
			if (!IndiceValido(contexto, seleccionado))
			{
				bool snap = false;
				const float paso = LongitudPasoFlecha(pulsacion.ctrl, pulsacion.shift, opciones, snap) * 5.0f;
				camara.posicion += DireccionFlecha(pulsacion.flecha, camara) * paso;
				continue;
			}
			if (herramienta != Herramienta::Mover) continue;
			if (!EsAnimado(contexto, seleccionado))
			{
				SceneObject& objeto = contexto.objetos[static_cast<std::size_t>(seleccionado)];
				SceneObject candidato = objeto;
				candidato.posicion = AplicarFlecha(objeto.posicion, pulsacion.flecha, camara, pulsacion.ctrl, pulsacion.shift, opciones);
				const ResultadoColocacion resultado = ValidarObjeto(contexto, candidato, seleccionado);
				if (!resultado.valida)
				{
					mensajeColocacion = "Movement blocked by " + resultado.bloqueador;
					InformarEstado(mensajeColocacion, true);
					continue;
				}
				if (candidato.posicion == objeto.posicion) continue;
				AbrirTransaccionSiHaceFalta();
				objeto.posicion = candidato.posicion;
			}
			else
			{
				SceneObjetoAnimado& puerta = contexto.animados[static_cast<std::size_t>(seleccionado) - contexto.objetos.size()];
				SceneObjetoAnimado candidato = puerta;
				candidato.posicion = AplicarFlecha(puerta.posicion, pulsacion.flecha, camara, pulsacion.ctrl, pulsacion.shift, opciones);
				const ResultadoColocacion resultado = ValidarAnimado(contexto, candidato, seleccionado);
				if (!resultado.valida)
				{
					mensajeColocacion = "Movement blocked by " + resultado.bloqueador;
					InformarEstado(mensajeColocacion, true);
					continue;
				}
				if (candidato.posicion == puerta.posicion) continue;
				AbrirTransaccionSiHaceFalta();
				puerta.posicion = candidato.posicion;
				SincronizarGlb(contexto);
			}
			mensajeColocacion.clear();
			MarcarModificada();
		}
		flechasPendientes.clear();
		if (deshacerSolicitado) { deshacerSolicitado = false; Deshacer(contexto); }
		if (rehacerSolicitado) { rehacerSolicitado = false; Rehacer(contexto); }
		if (duplicarSolicitado) { duplicarSolicitado = false; DuplicarSeleccion(contexto); }
		if (eliminarSolicitado) { eliminarSolicitado = false; EliminarSeleccion(contexto, true); }
		if (eliminarConservarHuecoSolicitado) { eliminarConservarHuecoSolicitado = false; EliminarSeleccion(contexto, false); }
		if (anadirSolicitado) { anadirSolicitado = false; AnadirDesdeAssetBrowser(contexto); }
		if (enfocarSolicitado) { enfocarSolicitado = false; EnfocarSeleccion(contexto); }
	}

	void EditorEscenas::AnadirDesdeAssetBrowser(ContextoEscena contexto)
	{
		if (assetEsPuerta)
		{
			if (puertaSeleccionada.empty() || !cargadorPuertas)
			{ InformarEstado("No GLB door loader is registered.", true); return; }
			std::error_code ec;
			const auto absoluta = std::filesystem::absolute(raizAnimaciones / puertaSeleccionada, ec).lexically_normal();
			if (ec) { InformarEstado("Could not resolve the selected GLB path.", true); return; }
			SceneObjetoAnimado nuevo;
			nuevo.id = IdUnico(contexto, absoluta.stem().string());
			nuevo.posicion = camara.Pivote();
			const auto relativa = absoluta.lexically_relative(raizEscena);
			if (relativa.empty() || relativa.is_absolute()) { InformarEstado("Could not create a portable GLB path relative to the scene.", true); return; }
			nuevo.modelo = relativa.generic_string();
			for (const SceneObjetoAnimado& existente : contexto.animados)
			{
				std::error_code rutaError;
				const auto existenteAbs = std::filesystem::absolute(raizEscena / existente.modelo, rutaError).lexically_normal();
				if (!rutaError && existenteAbs == absoluta) { nuevo.colisionadores = existente.colisionadores; nuevo.escala = existente.escala; break; }
			}
			if (nuevo.colisionadores.empty()) { InformarEstado("This GLB has no door collider template in the loaded scene.", true); return; }
			const EstadoEscena anterior = Capturar(contexto);
			std::string error;
			if (!cargadorPuertas(absoluta, nuevo, contexto, error))
			{ InformarEstado("Door asset error: " + error, true); return; }
			historial.Registrar(anterior);
			contexto.animados.push_back(nuevo);
			reconstruirFisicaSolicitada = true;
			FijarSeleccion(static_cast<int>(contexto.objetos.size() + contexto.animados.size() - 1));
			hayRespaldo = false; MarcarModificada();
			InformarEstado("Door instance added. Select a parametric wall and install the opening in Inspector.");
			return;
		}
		if (assetSeleccionado.empty()) { InformarEstado("Select a model in the Asset Browser first", true); return; }
		if (!cargadorModelos) { InformarEstado("Asset error: no model loader is registered", true); return; }
		std::error_code ec;
		const std::filesystem::path absoluta = std::filesystem::absolute(raizModelos / assetSeleccionado, ec).lexically_normal();
		if (ec) { InformarEstado("Asset error: could not resolve " + assetSeleccionado, true); return; }
		std::string clave, error;
		if (!cargadorModelos(absoluta, clave, error))
		{
			InformarEstado("Asset error: " + (error.empty() ? std::string("could not load model") : error), true);
			return;
		}
		AgregarModelo(contexto, clave, assetYArriba, assetColisionador, assetDinamico);
	}

	void EditorEscenas::CambiarTransformacion(SceneObject& objeto, const glm::mat4& matriz)
	{
		if (!MatrizGizmoValida(matriz))
		{
			InformarEstado("Gizmo transform rejected: non-finite or non-affine matrix", true);
			return;
		}
		const glm::vec3 posicion(matriz[3]);
		glm::vec3 escala(glm::length(glm::vec3(matriz[0])), glm::length(glm::vec3(matriz[1])), glm::length(glm::vec3(matriz[2])));
		for (int eje = 0; eje < 3; ++eje) escala[eje] *= objeto.escala[eje] < 0.0f ? -1.0f : 1.0f;
		if (std::abs(glm::determinant(glm::mat3(matriz))) < 1.0e-9f || !VectorFinito(posicion) || !VectorFinito(escala)) return;

		glm::mat3 rotacion(matriz);
		for (int eje = 0; eje < 3; ++eje)
			rotacion[eje] = glm::normalize(rotacion[eje]) * (objeto.escala[eje] < 0.0f ? -1.0f : 1.0f);
		// Euler XYZ que reconstruye la matriz del motor Rz * Ry * Rx.
		const float y = std::asin(std::clamp(-rotacion[0][2], -1.0f, 1.0f));
		const float cy = std::cos(y);
		float x = 0.0f, z = 0.0f;
		if (std::abs(cy) > 1.0e-5f)
		{
			x = std::atan2(rotacion[1][2], rotacion[2][2]);
			z = std::atan2(rotacion[0][1], rotacion[0][0]);
		}
		else
		{
			x = std::atan2(-rotacion[2][1], rotacion[1][1]);
		}
		glm::vec3 nuevaRotacion(glm::degrees(x), glm::degrees(y), glm::degrees(z));
		for (int eje = 0; eje < 3; ++eje)
			nuevaRotacion[eje] += 360.0f * std::round((objeto.rotacion[eje] - nuevaRotacion[eje]) / 360.0f);
		if (!VectorFinito(nuevaRotacion)) return;
		const glm::vec3 nuevaEscala = glm::max(glm::abs(escala), glm::vec3(MinimoEscala)) * glm::sign(escala);
		if (posicion == objeto.posicion && nuevaRotacion == objeto.rotacion && nuevaEscala == objeto.escala) return;
		objeto.posicion = posicion;
		objeto.rotacion = nuevaRotacion;
		objeto.escala = nuevaEscala;
		MarcarModificada();
	}

	// ---- Interfaz ------------------------------------------------------------------------------------

	void EditorEscenas::DibujarBarra(ContextoEscena contexto)
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (!activo)
    		return;

		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, AlturaBarra), ImGuiCond_Always);
		ImGui::Begin("Editor Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_HorizontalScrollbar);
		if (ImGui::Button(modificada ? "Save*" : "Save")) guardarSolicitado = true;
		ImGui::SameLine();
		ImGui::BeginDisabled(!historial.PuedeDeshacer());
		if (ImGui::Button("Undo")) deshacerSolicitado = true;
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!historial.PuedeRehacer());
		if (ImGui::Button("Redo")) rehacerSolicitado = true;
		ImGui::EndDisabled();
		ImGui::SameLine();
		auto herramientaBoton = [this](const char* texto, Herramienta h) {
			const bool actual = herramienta == h;
			if (actual) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.75f, 1.0f));
			if (ImGui::Button(texto)) herramienta = h;
			if (actual) ImGui::PopStyleColor();
		};
		herramientaBoton("Move [1]", Herramienta::Mover); ImGui::SameLine();
		herramientaBoton("Rotate [2]", Herramienta::Rotar); ImGui::SameLine();
		herramientaBoton("Scale [3]", Herramienta::Escalar);
		ImGui::SameLine(); ImGui::Checkbox("Local", &local);
		ImGui::SameLine(); ImGui::Checkbox("Snap", &snap);
		ImGui::SameLine(); ImGui::Checkbox("Boxes", &mostrarColisionadores);
		ImGui::SameLine();
		if (ImGui::Button(camara.vista == Vista::Perspectiva ? "Top view" : "Perspective"))
			camara.CambiarVista(camara.vista == Vista::Perspectiva ? Vista::Superior : Vista::Perspectiva);
		ImGui::SameLine();
		if (ImGui::Button("Front")) camara.CambiarVista(Vista::Frontal);
		ImGui::SameLine();
		if (ImGui::Button("Right")) camara.CambiarVista(Vista::Derecha);
		ImGui::SameLine();
		ImGui::Checkbox("Camera", &mostrarOpcionesCamara);
		ImGui::SameLine();
		ImGui::Checkbox("Debug input", &depurarEntrada);
		ImGui::SameLine();
		ImGui::TextDisabled("Arrows: 10cm | Ctrl 1cm | Shift 50cm | Ctrl+Shift grid %.0fcm | PgUp/PgDn Z", opciones.pasoCuadricula * 100.0f);
		if (modificada) { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.95f, 0.72f, 0.15f, 1.0f), "Unsaved changes"); }
		if (!mensajeEstado.empty())
		{
			ImGui::SameLine();
			if (mensajeEsError) ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.3f, 1.0f), "%s", mensajeEstado.c_str());
			else ImGui::TextUnformatted(mensajeEstado.c_str());
		}
		ImGui::End();
		(void)contexto;
	}

	void EditorEscenas::DibujarJerarquia(ContextoEscena contexto)
	{
		const ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(0.0f, AlturaBarra + 4.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(270.0f, std::max(260.0f, io.DisplaySize.y * 0.45f)), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(ImVec2(180.0f, 140.0f), ImVec2(io.DisplaySize.x, io.DisplaySize.y));
		if (ImGui::Begin("Hierarchy"))
		{
			if (ImGui::CollapsingHeader("Editor visibility (not saved)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("Hide ceiling", &visibilidad.ocultarTecho);
				ImGui::Checkbox("Hide walls", &visibilidad.ocultarParedes);
				ImGui::Checkbox("Auto-hide ceiling in top view", &visibilidad.autoOcultarTechoEnVistaSuperior);
				if (ImGui::Button("Show all")) visibilidad.MostrarTodo();
				ImGui::SameLine();
				if (!visibilidad.aislar)
				{
					ImGui::BeginDisabled(!IndiceValido(contexto, seleccionado));
					if (ImGui::Button("Isolate selected"))
					{ visibilidad.aislar = true; visibilidad.idAislado = IdDe(contexto, seleccionado); }
					ImGui::EndDisabled();
				}
				else if (ImGui::Button("End isolation")) { visibilidad.aislar = false; visibilidad.idAislado.clear(); }
				ImGui::TextDisabled("Hidden items stay in the scene, the JSON and the physics.");
			}
			ImGui::Separator();
			if (ImGui::BeginChild("jerarquia-lista", ImVec2(0.0f, 0.0f), false))
			{
				const int total = static_cast<int>(contexto.objetos.size() + contexto.animados.size());
				for (int i = 0; i < total; ++i)
				{
					const bool animado = EsAnimado(contexto, i);
					const std::string id = IdDe(contexto, i);
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
					else if (static_cast<std::size_t>(i) < clases.size() && clases[static_cast<std::size_t>(i)] != ClaseElemento::Objeto)
						etiqueta += std::string("  [") + NombreClase(clases[static_cast<std::size_t>(i)]) + "]";
					if (ImGui::Selectable(etiqueta.c_str(), seleccionado == i, ImGuiSelectableFlags_AllowDoubleClick))
					{
						FijarSeleccion(i);
						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) enfocarSolicitado = true;
					}
					if (seleccionado == i && seleccionCambiada) { ImGui::SetScrollHereY(0.5f); }
					ImGui::PopID();
				}
				seleccionCambiada = false;
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}

	void EditorEscenas::DibujarInspector(ContextoEscena contexto)
	{
		const ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(std::max(280.0f, io.DisplaySize.x - 320.0f), AlturaBarra + 4.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(310.0f, std::max(300.0f, io.DisplaySize.y * 0.6f)), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(ImVec2(220.0f, 160.0f), ImVec2(io.DisplaySize.x, io.DisplaySize.y));
		if (ImGui::Begin("Inspector"))
		{
			if (!mensajeColocacion.empty())
				ImGui::TextColored(ImVec4(1.0f, 0.28f, 0.22f, 1.0f), "%s", mensajeColocacion.c_str());
			if (!IndiceValido(contexto, seleccionado))
				ImGui::TextUnformatted("Select an object in Hierarchy or click it in the viewport");
			else if (!EsAnimado(contexto, seleccionado))
			{
				SceneObject& objeto = contexto.objetos[static_cast<std::size_t>(seleccionado)];
				ImGui::Text("ID: %s", objeto.id.c_str());
				ImGui::Text("Collection: %s", objeto.categoria.c_str());
				ImGui::TextWrapped("Model: %s", objeto.modelo.c_str());
				ImGui::Separator();
				ImGui::TextUnformatted("Transform");
				glm::vec3 solicitada = objeto.posicion;
				if (ImGui::InputFloat3("Position", &solicitada.x, "%.3f"))
				{
					SceneObject candidato = objeto;
					candidato.posicion = solicitada;
					const ResultadoColocacion resultado = VectorFinito(solicitada) ? ValidarObjeto(contexto, candidato, seleccionado) :
						ResultadoColocacion{ false, "non-finite position" };
					if (resultado.valida)
					{
						if (solicitada != objeto.posicion) { AbrirTransaccionSiHaceFalta(); objeto.posicion = solicitada; MarcarModificada(); }
						mensajeColocacion.clear();
					}
					else { mensajeColocacion = "Position blocked by " + resultado.bloqueador; InformarEstado(mensajeColocacion, true); }
				}
			glm::vec3 rotacionSolicitada = objeto.rotacion;
			if (ImGui::InputFloat3("Rotation", &rotacionSolicitada.x, "%.2f"))
			{
				SceneObject candidato = objeto;
				candidato.rotacion = rotacionSolicitada;
				const ResultadoColocacion resultado = VectorFinito(rotacionSolicitada) ? ValidarObjeto(contexto, candidato, seleccionado) :
					ResultadoColocacion{ false, "non-finite rotation" };
				if (resultado.valida)
				{
					if (rotacionSolicitada != objeto.rotacion) { AbrirTransaccionSiHaceFalta(); objeto.rotacion = rotacionSolicitada; MarcarModificada(); }
					mensajeColocacion.clear();
				}
				else { mensajeColocacion = "Rotation blocked by " + resultado.bloqueador; InformarEstado(mensajeColocacion, true); }
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
				const ResultadoColocacion resultado = ValidarObjeto(contexto, candidato, seleccionado);
				if (resultado.valida)
				{
					if (escalaSolicitada != objeto.escala) { AbrirTransaccionSiHaceFalta(); objeto.escala = escalaSolicitada; MarcarModificada(); }
					mensajeColocacion.clear();
				}
				else { mensajeColocacion = "Scale blocked by " + resultado.bloqueador; InformarEstado(mensajeColocacion, true); }
			}
			bool permitirArquitectura = objeto.permitirPenetracionArquitectura;
			if (ImGui::Checkbox("Allow architecture penetration (unsafe)", &permitirArquitectura))
			{
				AbrirTransaccionSiHaceFalta();
				objeto.permitirPenetracionArquitectura = permitirArquitectura;
				MarcarModificada();
			}
			ImGui::TextDisabled("Opt-in exception: ignores static architecture only.");
				ImGui::Separator();
				ImGui::TextUnformatted("Instance material");
				bool usaMaterial = objeto.material.presente;
				if (ImGui::Checkbox("Override material", &usaMaterial))
				{
					AbrirTransaccionSiHaceFalta();
					objeto.material.presente = usaMaterial;
					MarcarModificada();
				}
				if (objeto.material.presente)
				{
					char texturaMaterial[512]{};
					std::snprintf(texturaMaterial, sizeof(texturaMaterial), "%s", objeto.material.textura.c_str());
					if (ImGui::InputText("Texture (relative to scene)", texturaMaterial, sizeof(texturaMaterial)))
					{
						objeto.material.textura = texturaMaterial;
						AbrirTransaccionSiHaceFalta(); MarcarModificada();
					}
					if (ImGui::Button("Elegir textura")) ImGui::OpenPopup("Selector de textura");
					ImGui::SameLine();
					if (ImGui::Button("Restaurar material original"))
					{
						AbrirTransaccionSiHaceFalta(); objeto.material = SceneMaterial{}; MarcarModificada();
					}
					if (ImGui::BeginPopup("Selector de textura"))
					{
						ImGui::InputTextWithHint("##texture-filter", "Filtrar por nombre...", filtroTexturas, sizeof(filtroTexturas));
						const char* categorias[] = { "Todas", "Suelo", "Pared", "Techo", "Otros" };
						ImGui::Combo("Categoría", &categoriaTexturas, categorias, 5);
						ImGui::BeginChild("##texture-list", ImVec2(330.0f, 260.0f), true);
						std::error_code scanError;
						if (!raizModelos.empty() && std::filesystem::exists(raizModelos, scanError))
						for (std::filesystem::recursive_directory_iterator it(raizModelos, scanError), end; !scanError && it != end; it.increment(scanError))
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
							if (categoriaTexturas != 0 && categoriaTexturas != cat) continue;
							if (filtroTexturas[0] && low.find(filtroTexturas) == std::string::npos) continue;
							const auto absoluta = std::filesystem::absolute(it->path(), scanError).lexically_normal();
							if (scanError) { scanError.clear(); continue; }
							const auto relativa = absoluta.lexically_relative(raizEscena);
							if (relativa.empty() || relativa.is_absolute()) continue;
							const std::string ruta = relativa.generic_string();
							const std::string label = std::string(categorias[cat]) + " / " + it->path().filename().string();
							if (ImGui::Selectable(label.c_str(), objeto.material.textura == ruta))
							{
								AbrirTransaccionSiHaceFalta(); objeto.material.textura = ruta; MarcarModificada();
								ImGui::CloseCurrentPopup();
							}
							if (objeto.material.textura == ruta && cargadorTexturas)
							{
								const unsigned int tex = cargadorTexturas(absoluta);
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
						AbrirTransaccionSiHaceFalta(); MarcarModificada();
					}
					if (ImGui::SliderFloat("Roughness", &objeto.material.roughness, 0.0f, 1.0f, "%.2f"))
					{ AbrirTransaccionSiHaceFalta(); MarcarModificada(); }
					if (ImGui::SliderFloat("World dirt", &objeto.material.dirtStrength, 0.0f, 0.5f, "%.2f"))
					{ AbrirTransaccionSiHaceFalta(); MarcarModificada(); }
					if (objeto.material.dirtStrength > 0.0f)
					{
						if (ImGui::SliderFloat("Dirt scale (cells/m)", &objeto.material.dirtScale, 0.05f, 4.0f, "%.2f"))
						{ AbrirTransaccionSiHaceFalta(); MarcarModificada(); }
						if (ImGui::InputFloat("Dirt seed", &objeto.material.dirtSeed, 1.0f, 10.0f, "%.0f"))
						{ AbrirTransaccionSiHaceFalta(); MarcarModificada(); }
					}
				}
				ImGui::Separator();
				ImGui::TextUnformatted("Physics");
				int tipo = !objeto.fisica.presente ? 0 :
					(objeto.fisica.tipo == fisica::TipoCuerpo::Rebotador ? 3 :
						(objeto.fisica.tipo == fisica::TipoCuerpo::Dinamico ? 2 : 1));
				const int tipoAnterior = tipo;
				const char* tipos[] = { "None", "Static box", "Dynamic box", "Bouncy sphere" };
				if (ImGui::Combo("Collider", &tipo, tipos, 4) && tipo != tipoAnterior)
				{
					historial.Registrar(Capturar(contexto));
					hayRespaldo = false;
					objeto.fisica.presente = tipo != 0;
					if (tipo != 0)
					{
						objeto.fisica.forma = tipo == 3 ? fisica::FormaCuerpo::Esfera : fisica::FormaCuerpo::Caja;
						if (tipoAnterior == 0)
						{	// medidas iniciales a partir de los límites del modelo
							const CajaLocal caja = CajaDeObjeto(objeto);
							objeto.fisica.mitadTamano = caja.MitadTamano();
							objeto.fisica.desplazamiento = caja.Centro();
						}
						objeto.fisica.tipo = tipo == 3 ? fisica::TipoCuerpo::Rebotador :
							(tipo == 2 ? fisica::TipoCuerpo::Dinamico : fisica::TipoCuerpo::Estatico);
						if ((tipo == 2 || tipo == 3) && objeto.fisica.masa <= 0.0f)
							objeto.fisica.masa = 0.25f;
					}
					MarcarModificada();
				}
				if (objeto.fisica.presente)
				{
					if (ImGui::InputFloat3("Half size", &objeto.fisica.mitadTamano.x, "%.3f"))
					{
						for (int eje = 0; eje < 3; ++eje)
							objeto.fisica.mitadTamano[eje] = std::max(0.005f, std::isfinite(objeto.fisica.mitadTamano[eje]) ? objeto.fisica.mitadTamano[eje] : 0.5f);
						AbrirTransaccionSiHaceFalta(); MarcarModificada();
					}
					if (ImGui::InputFloat3("Offset", &objeto.fisica.desplazamiento.x, "%.3f"))
					{ AbrirTransaccionSiHaceFalta(); MarcarModificada(); }
					if (fisica::EsCuerpoDinamico(objeto.fisica.tipo) &&
						ImGui::InputFloat("Mass (kg)", &objeto.fisica.masa, 0.5f, 5.0f, "%.2f"))
					{
						if (!(objeto.fisica.masa > 0.0f)) objeto.fisica.masa = 0.1f;
						AbrirTransaccionSiHaceFalta(); MarcarModificada();
					}
					ImGui::TextDisabled("Colliders are rebuilt when you press Play.");
				}
			}
			else
			{
				SceneObjetoAnimado& animado = contexto.animados[static_cast<std::size_t>(seleccionado) - contexto.objetos.size()];
				ImGui::Text("ID: %s", animado.id.c_str());
				ImGui::TextUnformatted("Type: GLB animated instance");
				ImGui::TextWrapped("Model: %s", animado.modelo.c_str());
				ImGui::TextWrapped("Animation: %s", animado.animacion.empty() ? "(first in file)" : animado.animacion.c_str());
				ImGui::Text("Colliders: %d", static_cast<int>(animado.colisionadores.size()));
				for (const SceneColisionador& c : animado.colisionadores)
					ImGui::BulletText("%s (%s)", c.nodo.c_str(), c.tipo == fisica::TipoCuerpo::Cinematico ? "kinematic" : "static");
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
					InstalarPuertaEnPared(contexto, seleccionado - static_cast<int>(contexto.objetos.size()), paredDestinoPuerta);
				ImGui::TextDisabled("Opening dimensions are derived from DoorFrame collider bounds.");
				ImGui::Separator();
				ImGui::TextUnformatted("Transform (instance only)");
				glm::vec3 posicionSolicitada = animado.posicion;
				if (ImGui::InputFloat3("Position", &posicionSolicitada.x, "%.3f"))
				{
					SceneObjetoAnimado candidato = animado;
					candidato.posicion = posicionSolicitada;
					const ResultadoColocacion resultado = VectorFinito(posicionSolicitada) ? ValidarAnimado(contexto, candidato, seleccionado) :
						ResultadoColocacion{ false, "non-finite position" };
					if (resultado.valida)
					{
						if (posicionSolicitada != animado.posicion) { AbrirTransaccionSiHaceFalta(); animado.posicion = posicionSolicitada; MarcarModificada(); SincronizarGlb(contexto); }
						mensajeColocacion.clear();
					}
					else { mensajeColocacion = "Door position blocked by " + resultado.bloqueador; InformarEstado(mensajeColocacion, true); }
				}
				float giro = animado.giroZGrados;
				if (ImGui::InputFloat("Rotation Z", &giro, 1.0f, 15.0f, "%.2f") && std::isfinite(giro))
				{
					SceneObjetoAnimado candidato = animado;
					candidato.giroZGrados = giro;
					const ResultadoColocacion resultado = ValidarAnimado(contexto, candidato, seleccionado);
					if (resultado.valida)
					{
						if (giro != animado.giroZGrados) { AbrirTransaccionSiHaceFalta(); animado.giroZGrados = giro; MarcarModificada(); SincronizarGlb(contexto); }
						mensajeColocacion.clear();
					}
					else { mensajeColocacion = "Door rotation blocked by " + resultado.bloqueador; InformarEstado(mensajeColocacion, true); }
				}
				bool permitirArquitectura = animado.permitirPenetracionArquitectura;
				if (ImGui::Checkbox("Allow architecture penetration (unsafe)", &permitirArquitectura))
				{
					AbrirTransaccionSiHaceFalta();
					animado.permitirPenetracionArquitectura = permitirArquitectura;
					MarcarModificada();
				}
				ImGui::TextDisabled("Opt-in exception: ignores static architecture only.");
				ImGui::Text("Scale: %.3f (uniform, not editable here)", animado.escala);
				ImGui::TextWrapped("The animation, nodes and colliders are preserved; colliders follow the new placement when you press Play.");
			}
			if (IndiceValido(contexto, seleccionado))
			{
				ImGui::Separator();
				if (ImGui::Button("Focus")) enfocarSolicitado = true;
				ImGui::SameLine();
				ImGui::BeginDisabled(EsAnimado(contexto, seleccionado));
				if (ImGui::Button("Duplicate")) duplicarSolicitado = true;
				ImGui::EndDisabled();
				ImGui::SameLine();
				if (EsAnimado(contexto, seleccionado))
				{
					const SceneObjetoAnimado& puerta = contexto.animados[static_cast<std::size_t>(seleccionado) - contexto.objetos.size()];
					if (!puerta.aperturaId.empty())
					{
						if (ImGui::Button("Delete + close opening")) eliminarSolicitado = true;
						if (ImGui::Button("Delete only; keep opening")) eliminarConservarHuecoSolicitado = true;
					}
					else if (ImGui::Button("Delete")) eliminarSolicitado = true;
				}
				else if (ImGui::Button("Delete")) eliminarSolicitado = true;
			}
		}
		ImGui::End();
	}

	void EditorEscenas::DibujarAssetBrowser(ContextoEscena contexto)
	{
		const ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(0.0f, AlturaBarra + 4.0f + std::max(260.0f, io.DisplaySize.y * 0.45f) + 6.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(270.0f, std::max(220.0f, io.DisplaySize.y * 0.35f)), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(ImVec2(200.0f, 160.0f), ImVec2(io.DisplaySize.x, io.DisplaySize.y));
		if (ImGui::Begin("Asset Browser"))
		{
			if (ImGui::Button("Refresh")) { inventarioCargado = false; inventarioPuertasCargado = false; }
			ImGui::SameLine();
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputTextWithHint("##filtro", "filter...", filtroAssets, sizeof(filtroAssets));
			if (!inventarioCargado)
			{
				std::error_code ec;
				modelosDisponibles.clear();
				if (std::filesystem::exists(raizModelos, ec))
					for (std::filesystem::recursive_directory_iterator it(raizModelos, ec), fin; !ec && it != fin; it.increment(ec))
						if (it->is_regular_file(ec) && it->path().extension() == ".obj")
							modelosDisponibles.push_back(it->path().lexically_relative(raizModelos));
				std::sort(modelosDisponibles.begin(), modelosDisponibles.end());
				inventarioCargado = true;
			}
			if (!inventarioPuertasCargado)
			{
				std::error_code ec;
				puertasDisponibles.clear();
				if (std::filesystem::exists(raizAnimaciones, ec))
					for (std::filesystem::recursive_directory_iterator it(raizAnimaciones, ec), fin; !ec && it != fin; it.increment(ec))
						if (it->is_regular_file(ec) && it->path().extension() == ".glb")
							puertasDisponibles.push_back(it->path().lexically_relative(raizAnimaciones));
				std::sort(puertasDisponibles.begin(), puertasDisponibles.end());
				inventarioPuertasCargado = true;
			}
			const float alturaOpciones = ImGui::GetFrameHeightWithSpacing() * 4.0f + 6.0f;
			if (ImGui::BeginChild("model-list", ImVec2(0.0f, -alturaOpciones), true))
			{
				if (modelosDisponibles.empty())
					ImGui::TextWrapped("No .obj found in %s", raizModelos.generic_string().c_str());
				for (const auto& modelo : modelosDisponibles)
				{
					const std::string relativa = modelo.generic_string();
					if (filtroAssets[0] != '\0')
					{
						std::string a = relativa, b = filtroAssets;
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
						assetSeleccionado == relativa,
						ImGuiSelectableFlags_AllowDoubleClick))
					{
						assetSeleccionado = relativa;
						assetEsPuerta = false;

						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
							anadirSolicitado = true;
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
				if (puertasDisponibles.empty()) ImGui::TextDisabled("No .glb files in %s", raizAnimaciones.generic_string().c_str());
				for (const auto& ruta : puertasDisponibles)
				{
					const std::string relativa = ruta.generic_string();
					std::string low = relativa, filtro = filtroAssets;
					std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
					std::transform(filtro.begin(), filtro.end(), filtro.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
					if (!filtro.empty() && low.find(filtro) == std::string::npos) continue;
					if (ImGui::Selectable((ruta.stem().string() + "##door").c_str(), assetEsPuerta && puertaSeleccionada == ruta))
					{ puertaSeleccionada = ruta; assetEsPuerta = true; }
				}
			}
			ImGui::EndChild();
			ImGui::Checkbox("Model is Y-up (rotate 90 deg on X)", &assetYArriba);
			ImGui::Checkbox("Add box collider", &assetColisionador);
			if (assetColisionador)
			{
				ImGui::SameLine();
				ImGui::Checkbox("dynamic", &assetDinamico);
			}
			ImGui::BeginDisabled(assetEsPuerta ? puertaSeleccionada.empty() : assetSeleccionado.empty());
			if (ImGui::Button("Add to scene")) anadirSolicitado = true;
			ImGui::EndDisabled();
			if (assetEsPuerta && !puertaSeleccionada.empty()) { ImGui::SameLine(); ImGui::TextDisabled("%s", puertaSeleccionada.generic_string().c_str()); }
			else if (!assetSeleccionado.empty()) { ImGui::SameLine(); ImGui::TextDisabled("%s", assetSeleccionado.c_str()); }
			if (mensajeEsError && !mensajeEstado.empty()) ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.3f, 1.0f), "%s", mensajeEstado.c_str());
		}
		ImGui::End();
		(void)contexto;
	}

	void EditorEscenas::DibujarGizmo(ContextoEscena contexto, int ancho, int alto)
	{
		if (!IndiceValido(contexto, seleccionado) || ancho <= 0 || alto <= 0) return;
		const ImGuiIO& io = ImGui::GetIO();
		if (!bloqueoGizmoTrasSeleccion.PermitirManipular(io.MouseDown[ImGuiMouseButton_Left]))
		{
			// El clic completo que selecciona un elemento no puede iniciar el gizmo.
			gizmoEnUso = false;
			matrizCambiada = false;
			return;
		}
		ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());   // por encima de los paneles: el gizmo no se esconde detrás
		ImGuizmo::SetRect(0.0f, 0.0f, static_cast<float>(ancho), static_cast<float>(alto));
		ImGuizmo::SetOrthographic(camara.Ortografica());
		glm::mat4 vista, proyeccion;
		Matrices(static_cast<float>(ancho) / static_cast<float>(alto), vista, proyeccion);

		const bool animado = EsAnimado(contexto, seleccionado);
		const std::string idSeleccionado = IdDe(contexto, seleccionado);
		const bool reutilizarPrevisualizacion = previsualizacionGizmoActiva &&
			idPrevisualizacionGizmo == idSeleccionado && io.MouseDown[ImGuiMouseButton_Left];
		glm::mat4 matriz = reutilizarPrevisualizacion ? matrizPrevisualizacionGizmo : Elemento(contexto, seleccionado).modelo;
		ImGuizmo::OPERATION operacion = ImGuizmo::TRANSLATE;
		if (herramienta == Herramienta::Rotar) operacion = animado ? ImGuizmo::ROTATE_Z : ImGuizmo::ROTATE;
		if (herramienta == Herramienta::Escalar)
		{
			operacion = ImGuizmo::SCALE;
		}
		// ImGuizmo se desactiva SOLO si algo mas tiene la entrada: navegacion de camara, Alt pulsado, o la escala de una puerta
		// (no se toca para no alterar sus colisionadores). NO se usa io.WantCaptureMouse: ImGuizmo lo activa el mismo
		// (SetNextFrameWantCaptureMouse) al pasar sobre un manipulador, y desactivarlo por eso impedia empezar el arrastre.
		// Si el cursor esta sobre un panel, ImGuizmo ya ignora el clic (IsHoveringWindow).
		const bool escalaDePuerta = animado && herramienta == Herramienta::Escalar;
		ImGuizmo::PushID(idSeleccionado.c_str());
		ImGuizmo::Enable(GizmoHabilitado(ImGuizmo::IsUsing(), modoCamara != ModoCamara::Ninguno, entrada.altPulsado, escalaDePuerta));
		float paso[3] = { 0.5f, 0.5f, 0.5f };
		if (herramienta == Herramienta::Rotar) paso[0] = paso[1] = paso[2] = 15.0f;
		if (herramienta == Herramienta::Escalar) paso[0] = paso[1] = paso[2] = 0.1f;
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

		const glm::mat4 matrizAceptada = Elemento(contexto, seleccionado).modelo;
		ConservarAlturaEnTop(matriz, matrizAceptada, camara.vista == Vista::Superior, herramienta == Herramienta::Mover);
		ResultadoColocacion resultado;
		if (!MatrizGizmoValida(matriz)) resultado = { false, "invalid transform matrix" };
		else if (!animado)
			resultado = ValidarObjeto(contexto, contexto.objetos[static_cast<std::size_t>(seleccionado)], seleccionado, &matriz);
		else
		{
			SceneObjetoAnimado candidato = contexto.animados[static_cast<std::size_t>(seleccionado) - contexto.objetos.size()];
			candidato.posicion = glm::vec3(matriz[3]);
			float giro = glm::degrees(std::atan2(matriz[0][1], matriz[0][0]));
			giro += 360.0f * std::round((candidato.giroZGrados - giro) / 360.0f);
			candidato.giroZGrados = giro;
			resultado = VectorFinito(candidato.posicion) && std::isfinite(giro)
				? ValidarAnimado(contexto, candidato, seleccionado) : ResultadoColocacion{ false, "invalid door transform" };
		}

		previsualizacionGizmoActiva = gizmoEnUso;
		previsualizacionGizmoInvalida = !resultado.valida;
		matrizPrevisualizacionGizmo = matriz;
		idPrevisualizacionGizmo = idSeleccionado;
		if (!resultado.valida)
		{
			mensajeColocacion = "Placement blocked by " + resultado.bloqueador;
			InformarEstado(mensajeColocacion, true);
			if (depurarEntrada)
				std::fprintf(stderr, "[EditorGizmo] rejected id=%s view=%d using=%d blocker=%s before=(%.3f,%.3f,%.3f) candidate=(%.3f,%.3f,%.3f)\n",
					idSeleccionado.c_str(), static_cast<int>(camara.vista), gizmoEnUso ? 1 : 0, resultado.bloqueador.c_str(),
					matrizAceptada[3].x, matrizAceptada[3].y, matrizAceptada[3].z, matriz[3].x, matriz[3].y, matriz[3].z);
			return;
		}
		mensajeColocacion.clear();
		if (depurarEntrada)
			std::fprintf(stderr, "[EditorGizmo] accepted id=%s view=%d using=%d before=(%.3f,%.3f,%.3f) after=(%.3f,%.3f,%.3f)\n",
				idSeleccionado.c_str(), static_cast<int>(camara.vista), gizmoEnUso ? 1 : 0,
				matrizAceptada[3].x, matrizAceptada[3].y, matrizAceptada[3].z, matriz[3].x, matriz[3].y, matriz[3].z);
		AbrirTransaccionSiHaceFalta();
		if (!animado)
			CambiarTransformacion(contexto.objetos[static_cast<std::size_t>(seleccionado)], matriz);
		else
		{
			SceneObjetoAnimado& puerta = contexto.animados[static_cast<std::size_t>(seleccionado) - contexto.objetos.size()];
			const glm::vec3 posicion(matriz[3]);
			float giro = glm::degrees(std::atan2(matriz[0][1], matriz[0][0]));
			giro += 360.0f * std::round((puerta.giroZGrados - giro) / 360.0f);
			if (VectorFinito(posicion) && std::isfinite(giro))
			{
				puerta.posicion = posicion;
				puerta.giroZGrados = giro;
				SincronizarGlb(contexto);
				MarcarModificada();
			}
		}
		if (!gizmoEnUso)
		{
			previsualizacionGizmoActiva = false;
			idPrevisualizacionGizmo.clear();
		}
	}

	void EditorEscenas::DibujarContornoSeleccion(ContextoEscena contexto, int ancho, int alto)
	{
		if (!IndiceValido(contexto, seleccionado) || ancho <= 0 || alto <= 0) return;
		glm::mat4 vista, proyeccion;
		Matrices(static_cast<float>(ancho) / static_cast<float>(alto), vista, proyeccion);
		const ElementoSeleccionable e = Elemento(contexto, seleccionado);
		const bool dibujarCandidato = previsualizacionGizmoActiva && previsualizacionGizmoInvalida &&
			idPrevisualizacionGizmo == IdDe(contexto, seleccionado);
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

	void EditorEscenas::DibujarOpcionesCamara()
	{
		if (!mostrarOpcionesCamara) return;
		if (ImGui::Begin("Editor Camera", &mostrarOpcionesCamara, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::SliderFloat("Mouse sensitivity (deg/px)", &opciones.sensibilidad, 0.02f, 0.6f, "%.3f");
			ImGui::Checkbox("Invert Y axis", &opciones.invertirY);
			ImGui::SliderFloat("Camera speed (m/s)", &opciones.velocidad, 0.5f, 40.0f, "%.1f");
			ImGui::SliderFloat("Pan speed", &opciones.velocidadPan, 0.1f, 4.0f, "%.2f");
			ImGui::SliderFloat("Zoom speed", &opciones.velocidadZoom, 0.1f, 4.0f, "%.2f");
			ImGui::SliderFloat("Shift multiplier", &opciones.multiplicadorShift, 1.0f, 10.0f, "%.2f");
			ImGui::SliderFloat("Ctrl multiplier", &opciones.multiplicadorCtrl, 0.05f, 1.0f, "%.2f");
			ImGui::SliderFloat("Snap grid (m)", &opciones.pasoCuadricula, 0.01f, 2.0f, "%.2f");
			ImGui::TextDisabled("Ctrl + Shift together: Ctrl wins (precision).");
			ImGui::Separator();
			ImGui::TextUnformatted("RMB look | RMB+WASD/QE fly | MMB pan | Alt+LMB orbit | wheel zoom | F focus");
			ImGui::Text("yaw %.1f  pitch %.1f  pos %.2f %.2f %.2f", camara.yaw, camara.pitch,
				camara.posicion.x, camara.posicion.y, camara.posicion.z);
		}
		ImGui::End();
	}

	// Instrumentación temporal del gizmo: ventana opcional y, con ESCAPETHEUAB_EDITOR_DEBUG=1, una línea por cambio en consola.
	void EditorEscenas::DibujarDepuracion()
	{
		if (!depurarEntrada) return;
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
		if (depuracionPorConsola)
		{
			static std::string anterior;
			const std::string actual = linea;
			if (actual != anterior) { std::fprintf(stderr, "[editor] %s\n", linea); anterior = actual; }
		}
		ImGui::SetNextWindowBgAlpha(0.6f);
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - 260.0f, io.DisplaySize.y - 70.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Editor input debug", &depurarEntrada, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("SelectedObject: %d   Operation: %s", seleccionado, nombresOperacion);
			ImGui::Text("IsOver: %d  IsUsing: %d  MouseDown: %d", gizmoSobre ? 1 : 0, usando ? 1 : 0, io.MouseDown[0] ? 1 : 0);
			ImGui::Text("ViewportHovered: %d  MatrixChanged: %d  WantCaptureMouse: %d", entrada.sobrePanel ? 0 : 1, matrizCambiada ? 1 : 0, io.WantCaptureMouse ? 1 : 0);
		}
		ImGui::End();
	}

	void EditorEscenas::DibujarInterfaz(ContextoEscena contexto, int ancho, int alto)
	{
		ImGuizmo::BeginFrame();
		if (!depuracionIniciada)
		{
			depuracionIniciada = true;
			const char* variable = std::getenv("ESCAPETHEUAB_EDITOR_DEBUG");
			depuracionPorConsola = variable != nullptr && variable[0] != '\0' && variable[0] != '0';
			depurarEntrada = depurarEntrada || depuracionPorConsola;
		}
		if (activo)
		{
			// Arbitraje de la entrada de este fotograma (ver Actualizar).
			entrada.sobrePanel = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
			entrada.imguiActivo = ImGui::IsAnyItemActive();
			entrada.gizmoUsando = ImGuizmo::IsUsing();
			entrada.popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
			entrada.altPulsado = ImGui::GetIO().KeyAlt;
			entrada.viewportLibre = ViewportLibre(entrada.sobrePanel, entrada.imguiActivo, entrada.gizmoUsando, entrada.popup);
			haySeleccionValida = IndiceValido(contexto, seleccionado);
			// Estado de reposo: si no hay arrastre ni campo en edición, así estaba la escena antes de cualquier cambio.
			if (!historial.TransaccionAbierta() && !ImGuizmo::IsUsing() && !ImGui::IsAnyItemActive())
			{
				respaldo = Capturar(contexto);
				hayRespaldo = true;
			}
			ActualizarVisibilidad(contexto);
		}
		DibujarBarra(contexto);

		if (confirmacionCierre) ImGui::OpenPopup("Unsaved scene changes");
		if (ImGui::BeginPopupModal("Unsaved scene changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("The scene has unsaved changes.");
			if (ImGui::Button("Save and close")) { guardarSolicitado = true; cerrarTrasGuardar = true; confirmacionCierre = false; ImGui::CloseCurrentPopup(); }
			ImGui::SameLine();
			if (ImGui::Button("Discard and close")) { cerrarConfirmado = true; confirmacionCierre = false; ImGui::CloseCurrentPopup(); }
			ImGui::SameLine();
			if (ImGui::Button("Continue editing")) { confirmacionCierre = false; ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}
		if (!activo) return;

		DibujarJerarquia(contexto);
		DibujarInspector(contexto);
		DibujarAssetBrowser(contexto);
		DibujarOpcionesCamara();
		SeleccionarPorRaton(contexto, ancho, alto);
		DibujarGizmo(contexto, ancho, alto);
		DibujarContornoSeleccion(contexto, ancho, alto);
		DibujarDepuracion();
		CerrarTransaccionSiTermino(contexto);
		ProcesarSolicitudes(contexto);
	}
}
