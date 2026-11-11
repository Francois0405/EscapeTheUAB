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
		hayRespaldo = false;
		seleccionado = -1;
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
		if (accion == GLFW_PRESS && tecla == GLFW_KEY_E && control && (modificadores & GLFW_MOD_ALT)) { Activar(!activo); return; }
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
			if (glb::ObjetoAnimado* instancia = glb::BuscarObjeto(contexto.escenaGlb, animado.id))
			{
				instancia->posicion = animado.posicion;
				instancia->giroZGrados = animado.giroZGrados;
				instancia->escala = animado.escala;
			}
	}

	void EditorEscenas::Restaurar(ContextoEscena contexto, EstadoEscena estado)
	{
		contexto.objetos = std::move(estado.objetos);
		contexto.animados = std::move(estado.animados);
		SincronizarGlb(contexto);   // vista y cajas se rehacen solas: en EDIT todo se dibuja desde estas listas
		const int total = static_cast<int>(contexto.objetos.size() + contexto.animados.size());
		if (seleccionado >= total) seleccionado = total - 1;
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
		seleccionado = ElegirMasCercano(rayo, candidatos);
		seleccionCambiada = true;
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
		historial.Registrar(Capturar(contexto));

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
		const std::string id = objeto.id;
		contexto.objetos.push_back(std::move(objeto));
		seleccionado = static_cast<int>(contexto.objetos.size()) - 1;
		seleccionCambiada = true;
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
			InformarEstado("GLB animated objects cannot be duplicated from the editor", true);
			return false;
		}
		historial.Registrar(Capturar(contexto));
		SceneObject copia = contexto.objetos[static_cast<std::size_t>(seleccionado)];
		copia.id = IdUnico(contexto, copia.id + "_copy");
		copia.categoria = "objects";
		copia.posicion.x += 0.5f;
		contexto.objetos.push_back(std::move(copia));
		seleccionado = static_cast<int>(contexto.objetos.size()) - 1;
		seleccionCambiada = true;
		hayRespaldo = false;
		MarcarModificada();
		InformarEstado("Duplicated");
		return true;
	}

	bool EditorEscenas::EliminarSeleccion(ContextoEscena contexto)
	{
		if (!IndiceValido(contexto, seleccionado)) return false;
		if (EsAnimado(contexto, seleccionado))
		{
			InformarEstado("GLB animated objects cannot be deleted from the editor", true);
			return false;
		}
		historial.Registrar(Capturar(contexto));
		const std::string id = contexto.objetos[static_cast<std::size_t>(seleccionado)].id;
		contexto.objetos.erase(contexto.objetos.begin() + seleccionado);
		const int total = static_cast<int>(contexto.objetos.size() + contexto.animados.size());
		seleccionado = total == 0 ? -1 : std::min(seleccionado, total - 1);
		seleccionCambiada = true;
		hayRespaldo = false;
		MarcarModificada();
		InformarEstado("Deleted " + id + " (Ctrl+Z to restore)");
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
			AbrirTransaccionSiHaceFalta();
			if (!EsAnimado(contexto, seleccionado))
			{
				SceneObject& objeto = contexto.objetos[static_cast<std::size_t>(seleccionado)];
				objeto.posicion = AplicarFlecha(objeto.posicion, pulsacion.flecha, camara, pulsacion.ctrl, pulsacion.shift, opciones);
			}
			else
			{
				SceneObjetoAnimado& puerta = contexto.animados[static_cast<std::size_t>(seleccionado) - contexto.objetos.size()];
				puerta.posicion = AplicarFlecha(puerta.posicion, pulsacion.flecha, camara, pulsacion.ctrl, pulsacion.shift, opciones);
				SincronizarGlb(contexto);
			}
			MarcarModificada();
		}
		flechasPendientes.clear();
		if (deshacerSolicitado) { deshacerSolicitado = false; Deshacer(contexto); }
		if (rehacerSolicitado) { rehacerSolicitado = false; Rehacer(contexto); }
		if (duplicarSolicitado) { duplicarSolicitado = false; DuplicarSeleccion(contexto); }
		if (eliminarSolicitado) { eliminarSolicitado = false; EliminarSeleccion(contexto); }
		if (anadirSolicitado) { anadirSolicitado = false; AnadirDesdeAssetBrowser(contexto); }
		if (enfocarSolicitado) { enfocarSolicitado = false; EnfocarSeleccion(contexto); }
	}

	void EditorEscenas::AnadirDesdeAssetBrowser(ContextoEscena contexto)
	{
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
		objeto.posicion = posicion;
		objeto.rotacion = nuevaRotacion;
		objeto.escala = glm::max(glm::abs(escala), glm::vec3(MinimoEscala)) * glm::sign(escala);
		MarcarModificada();
	}

	// ---- Interfaz ------------------------------------------------------------------------------------

	void EditorEscenas::DibujarBarra(ContextoEscena contexto)
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (!activo)
		{	// En PLAY: solo un pequeño indicador y el botón para volver al editor, sin tapar el juego.
			ImGui::SetNextWindowPos(ImVec2(6.0f, 6.0f), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.35f);
			ImGui::Begin("Editor Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
			ImGui::TextColored(ImVec4(0.45f, 0.9f, 0.45f, 1.0f), "PLAY");
			ImGui::SameLine();
			if (ImGui::SmallButton("EDIT [Ctrl+Alt+E]")) Activar(true);
			ImGui::End();
			return;
		}

		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, AlturaBarra), ImGuiCond_Always);
		ImGui::Begin("Editor Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_HorizontalScrollbar);
		ImGui::TextColored(ImVec4(0.98f, 0.62f, 0.12f, 1.0f), "EDIT");
		ImGui::SameLine();
		if (ImGui::Button("Play")) Activar(false);
		ImGui::SameLine();
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
						seleccionado = i;
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
				glm::vec3 anterior = objeto.posicion;
				if (ImGui::InputFloat3("Position", &objeto.posicion.x, "%.3f"))
				{
					if (VectorFinito(objeto.posicion)) { AbrirTransaccionSiHaceFalta(); MarcarModificada(); }
					else objeto.posicion = anterior;
				}
				anterior = objeto.rotacion;
				if (ImGui::InputFloat3("Rotation", &objeto.rotacion.x, "%.2f"))
				{
					if (VectorFinito(objeto.rotacion)) { AbrirTransaccionSiHaceFalta(); MarcarModificada(); }
					else objeto.rotacion = anterior;
				}
				anterior = objeto.escala;
				if (ImGui::InputFloat3("Scale", &objeto.escala.x, "%.3f"))
				{
					if (!VectorFinito(objeto.escala)) objeto.escala = anterior;
					for (int eje = 0; eje < 3; ++eje)
						if (std::abs(objeto.escala[eje]) < MinimoEscala)
							objeto.escala[eje] = std::copysign(MinimoEscala, objeto.escala[eje] == 0.0f ? 1.0f : objeto.escala[eje]);
					AbrirTransaccionSiHaceFalta();
					MarcarModificada();
				}
				ImGui::Separator();
				ImGui::TextUnformatted("Physics");
				int tipo = !objeto.fisica.presente ? 0 : (objeto.fisica.tipo == fisica::TipoCuerpo::Dinamico ? 2 : 1);
				const int tipoAnterior = tipo;
				const char* tipos[] = { "None", "Static box", "Dynamic box" };
				if (ImGui::Combo("Collider", &tipo, tipos, 3) && tipo != tipoAnterior)
				{
					historial.Registrar(Capturar(contexto));
					hayRespaldo = false;
					objeto.fisica.presente = tipo != 0;
					if (tipo != 0)
					{
						if (tipoAnterior == 0)
						{	// medidas iniciales a partir de los límites del modelo
							const CajaLocal caja = CajaDeObjeto(objeto);
							objeto.fisica.mitadTamano = caja.MitadTamano();
							objeto.fisica.desplazamiento = caja.Centro();
						}
						objeto.fisica.tipo = tipo == 2 ? fisica::TipoCuerpo::Dinamico : fisica::TipoCuerpo::Estatico;
						if (tipo == 2 && objeto.fisica.masa <= 0.0f) objeto.fisica.masa = 5.0f;
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
					if (objeto.fisica.tipo == fisica::TipoCuerpo::Dinamico &&
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
				ImGui::Separator();
				ImGui::TextUnformatted("Transform (instance only)");
				glm::vec3 anterior = animado.posicion;
				if (ImGui::InputFloat3("Position", &animado.posicion.x, "%.3f"))
				{
					if (VectorFinito(animado.posicion)) { AbrirTransaccionSiHaceFalta(); MarcarModificada(); SincronizarGlb(contexto); }
					else animado.posicion = anterior;
				}
				float giro = animado.giroZGrados;
				if (ImGui::InputFloat("Rotation Z", &giro, 1.0f, 15.0f, "%.2f") && std::isfinite(giro))
				{
					animado.giroZGrados = giro;
					AbrirTransaccionSiHaceFalta(); MarcarModificada(); SincronizarGlb(contexto);
				}
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
				ImGui::SameLine();
				if (ImGui::Button("Delete")) eliminarSolicitado = true;
				ImGui::EndDisabled();
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
			if (ImGui::Button("Refresh")) inventarioCargado = false;
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
					if (ImGui::Selectable(relativa.c_str(), assetSeleccionado == relativa, ImGuiSelectableFlags_AllowDoubleClick))
					{
						assetSeleccionado = relativa;
						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) anadirSolicitado = true;
					}
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
			ImGui::BeginDisabled(assetSeleccionado.empty());
			if (ImGui::Button("Add to scene")) anadirSolicitado = true;
			ImGui::EndDisabled();
			if (!assetSeleccionado.empty()) { ImGui::SameLine(); ImGui::TextDisabled("%s", assetSeleccionado.c_str()); }
			if (mensajeEsError && !mensajeEstado.empty()) ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.3f, 1.0f), "%s", mensajeEstado.c_str());
		}
		ImGui::End();
		(void)contexto;
	}

	void EditorEscenas::DibujarGizmo(ContextoEscena contexto, int ancho, int alto)
	{
		if (!IndiceValido(contexto, seleccionado) || ancho <= 0 || alto <= 0) return;
		ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());   // por encima de los paneles: el gizmo no se esconde detrás
		ImGuizmo::SetRect(0.0f, 0.0f, static_cast<float>(ancho), static_cast<float>(alto));
		ImGuizmo::SetOrthographic(camara.Ortografica());
		glm::mat4 vista, proyeccion;
		Matrices(static_cast<float>(ancho) / static_cast<float>(alto), vista, proyeccion);

		const bool animado = EsAnimado(contexto, seleccionado);
		glm::mat4 matriz = Elemento(contexto, seleccionado).modelo;
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
		ImGuizmo::Enable(GizmoHabilitado(ImGuizmo::IsUsing(), modoCamara != ModoCamara::Ninguno, entrada.altPulsado, escalaDePuerta));
		float paso[3] = { 0.5f, 0.5f, 0.5f };
		if (herramienta == Herramienta::Rotar) paso[0] = paso[1] = paso[2] = 15.0f;
		if (herramienta == Herramienta::Escalar) paso[0] = paso[1] = paso[2] = 0.1f;
		const bool cambio = ImGuizmo::Manipulate(&vista[0][0], &proyeccion[0][0], operacion,
			local ? ImGuizmo::LOCAL : ImGuizmo::WORLD, &matriz[0][0], nullptr, snap ? paso : nullptr);
		gizmoEnUso = ImGuizmo::IsUsing();
		gizmoSobre = ImGuizmo::IsOver();
		matrizCambiada = cambio;
		operacionDepuracion = static_cast<int>(operacion);
		if (!cambio) return;

		AbrirTransaccionSiHaceFalta();   // el respaldo es el estado ANTERIOR al primer cambio del arrastre
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
	}

	void EditorEscenas::DibujarContornoSeleccion(ContextoEscena contexto, int ancho, int alto)
	{
		if (!IndiceValido(contexto, seleccionado) || ancho <= 0 || alto <= 0) return;
		glm::mat4 vista, proyeccion;
		Matrices(static_cast<float>(ancho) / static_cast<float>(alto), vista, proyeccion);
		const ElementoSeleccionable e = Elemento(contexto, seleccionado);
		const glm::mat4 transformacion = proyeccion * vista * e.modelo;
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
		const ImU32 color = IM_COL32(255, 205, 40, 255);
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
