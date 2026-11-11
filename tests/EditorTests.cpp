// EditorTests.cpp
// Pruebas del editor sin ventana: historial (deshacer/rehacer), visibilidad temporal, selección por rayo,
// límites de modelos y persistencia (añadir OBJ, puertas GLB, guardar y recargar).
#include "Editor/CamaraEditor.h"
#include "Editor/HistorialEscena.h"
#include "Editor/LimitesModelo.h"
#include "Editor/SelectorObjetos.h"
#include "Editor/SerializadorEscenas.h"
#include "Editor/VisibilidadEditor.h"
#include "SceneLoader.h"

#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace editor;
namespace fs = std::filesystem;

namespace
{
	int fallos = 0;
	void comprobar(bool condicion, const char* caso)
	{
		if (!condicion) { std::cerr << "FALLO: " << caso << '\n'; ++fallos; }
	}
	bool casi(float a, float b, float eps = 1e-3f) { return std::abs(a - b) <= eps; }

	void escribir(const fs::path& ruta, const std::string& contenido)
	{
		fs::create_directories(ruta.parent_path());
		std::ofstream archivo(ruta, std::ios::binary | std::ios::trunc);
		archivo << contenido;
	}
	std::string leer(const fs::path& ruta)
	{
		std::ifstream archivo(ruta, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(archivo), std::istreambuf_iterator<char>());
	}

	SceneObject Objeto(const std::string& id, const std::string& categoria, glm::vec3 pos, glm::vec3 escala = glm::vec3(1.0f))
	{
		SceneObject o;
		o.id = id; o.categoria = categoria; o.modelo = "m.obj";
		o.posicion = pos; o.rotacion = glm::vec3(0.0f); o.escala = escala;
		return o;
	}

	EstadoEscena Estado(std::vector<SceneObject> objetos = {}, std::vector<SceneObjetoAnimado> animados = {})
	{
		return EstadoEscena{ std::move(objetos), std::move(animados) };
	}

	// Sala como habitacion_beta: suelo, 4 paredes y techo con test.obj (cubo de lado 2) escalado.
	std::vector<SceneObject> Sala()
	{
		auto fisica = [](SceneObject o) { o.fisica.presente = true; o.fisica.mitadTamano = glm::vec3(1.0f); return o; };
		return {
			fisica(Objeto("floor", "architecture", { 5, 0, -1.85f }, { 8, 5, 0.15f })),
			fisica(Objeto("wall_left", "architecture", { -3, 0, -0.075f }, { 0.15f, 5, 1.625f })),
			fisica(Objeto("wall_back", "architecture", { 5, -5, -0.075f }, { 8, 0.15f, 1.625f })),
			fisica(Objeto("ceiling", "architecture", { 5, 0, 1.7f }, { 8, 5, 0.15f })),
			Objeto("desk", "objects", { 2, 1, -1.7f }),
		};
	}
}

int main()
{
	// ---------------------------------------------------------------- Historial: deshacer / rehacer
	{
		std::vector<SceneObject> objetos = { Objeto("a", "objects", { 0, 0, 0 }) };
		HistorialEscena h;
		const EstadoEscena inicial = Estado(objetos);

		// Tres operaciones seguidas: mover, añadir, eliminar.
		h.Registrar(Estado(objetos)); objetos[0].posicion = { 1, 0, 0 };
		h.Registrar(Estado(objetos)); objetos.push_back(Objeto("b", "objects", { 5, 5, 0 }));
		h.Registrar(Estado(objetos)); objetos.erase(objetos.begin());
		comprobar(h.OperacionesParaDeshacer() == 3 && !h.PuedeRehacer(), "tres operaciones registradas, nada que rehacer");

		EstadoEscena actual = Estado(objetos);
		comprobar(h.Deshacer(actual) && actual.objetos.size() == 2 && actual.objetos[0].id == "a", "deshacer la eliminacion recupera el objeto");
		comprobar(h.Deshacer(actual) && actual.objetos.size() == 1 && actual.objetos[0].posicion.x == 1.0f, "deshacer la creacion lo quita");
		comprobar(h.Deshacer(actual) && Iguales(actual, inicial), "deshacer hasta el estado inicial");
		comprobar(!h.Deshacer(actual) && Iguales(actual, inicial), "sin mas historial no cambia nada");

		comprobar(h.Rehacer(actual) && actual.objetos[0].posicion.x == 1.0f, "rehacer el movimiento");
		comprobar(h.Rehacer(actual) && actual.objetos.size() == 2, "rehacer la creacion");
		comprobar(h.Rehacer(actual) && actual.objetos.size() == 1 && actual.objetos[0].id == "b", "rehacer la eliminacion");
		comprobar(!h.Rehacer(actual), "no se puede rehacer mas");

		// Una operacion nueva descarta la rama de rehacer.
		h.Deshacer(actual); h.Deshacer(actual);
		comprobar(h.PuedeRehacer(), "hay rama de rehacer tras deshacer");
		h.Registrar(Estado(actual.objetos));
		comprobar(!h.PuedeRehacer(), "una operacion nueva invalida la rama de rehacer");
	}

	// Un arrastre del gizmo = UNA operacion, aunque haya cientos de cambios intermedios.
	{
		std::vector<SceneObject> objetos = { Objeto("a", "objects", { 0, 0, 0 }) };
		HistorialEscena h;
		h.IniciarTransaccion(Estado(objetos));
		for (int i = 1; i <= 300; ++i)
		{
			objetos[0].posicion.x = 0.01f * static_cast<float>(i);
			h.IniciarTransaccion(Estado(objetos));   // llamadas repetidas no sustituyen el inicio
		}
		comprobar(h.CerrarTransaccion(Estado(objetos)), "el arrastre se registra al terminar");
		comprobar(h.OperacionesParaDeshacer() == 1, "300 cambios del arrastre son UNA sola operacion");
		EstadoEscena actual = Estado(objetos);
		h.Deshacer(actual);
		comprobar(actual.objetos[0].posicion.x == 0.0f, "deshacer devuelve la posicion previa al arrastre");

		// Un clic sin mover no ensucia el historial.
		HistorialEscena limpio;
		limpio.IniciarTransaccion(Estado(objetos));
		comprobar(!limpio.CerrarTransaccion(Estado(objetos)) && !limpio.PuedeDeshacer(), "una transaccion sin cambios no se registra");
	}

	// El historial tiene limite.
	{
		HistorialEscena h;
		for (std::size_t i = 0; i < HistorialEscena::LimiteOperaciones + 50; ++i) h.Registrar(Estado());
		comprobar(h.OperacionesParaDeshacer() == HistorialEscena::LimiteOperaciones, "el historial no crece sin limite");
	}

	// Las puertas GLB tambien entran en el historial.
	{
		SceneObjetoAnimado puerta; puerta.id = "puerta_1"; puerta.posicion = { 6, -2, -1.7f }; puerta.giroZGrados = -90.0f;
		std::vector<SceneObjetoAnimado> animados = { puerta };
		HistorialEscena h;
		h.Registrar(Estado({}, animados));
		animados[0].posicion.x = 9.0f; animados[0].giroZGrados = 0.0f;
		EstadoEscena actual = Estado({}, animados);
		h.Deshacer(actual);
		comprobar(actual.animados[0].posicion.x == 6.0f && actual.animados[0].giroZGrados == -90.0f, "deshacer mover/girar una puerta");
	}

	// ---------------------------------------------------------------- Visibilidad temporal
	{
		std::vector<SceneObject> sala = Sala();
		const auto clases = ClasificarElementos(sala);
		comprobar(clases[0] == ClaseElemento::Suelo, "el suelo se reconoce por forma y altura");
		comprobar(clases[1] == ClaseElemento::Pared && clases[2] == ClaseElemento::Pared, "las paredes se reconocen por forma");
		comprobar(clases[3] == ClaseElemento::Techo, "el techo se reconoce por forma y altura (no por el nombre)");
		comprobar(clases[4] == ClaseElemento::Objeto, "un mueble es un objeto");

		// Renombrar el techo no cambia su clasificacion.
		sala[3].id = "x_zyz_77";
		comprobar(ClasificarElementos(sala)[3] == ClaseElemento::Techo, "sin depender de las palabras 'techo' o 'ceiling'");
		sala[3].id = "ceiling";

		VisibilidadEditor v;
		comprobar(v.Visible(sala[3], clases[3], false), "por defecto el techo es visible");
		comprobar(!v.Visible(sala[3], clases[3], true), "vista superior: se oculta automaticamente");
		v.autoOcultarTechoEnVistaSuperior = false;
		comprobar(v.Visible(sala[3], clases[3], true), "se puede desactivar la ocultacion automatica");
		v.ocultarTecho = true;
		comprobar(!v.Visible(sala[3], clases[3], false) && v.Visible(sala[1], clases[1], false), "ocultar techo solo oculta el techo");
		v.ocultarParedes = true;
		comprobar(!v.Visible(sala[1], clases[1], false) && v.Visible(sala[4], clases[4], false), "ocultar paredes no oculta muebles");
		v.ocultos.insert("desk");
		comprobar(!v.Visible(sala[4], clases[4], false), "se puede ocultar un elemento concreto");
		v.MostrarTodo();
		comprobar(v.Visible(sala[3], clases[3], false) && v.Visible(sala[4], clases[4], false), "mostrar todo");
		v.aislar = true; v.idAislado = "desk";
		comprobar(v.Visible(sala[4], clases[4], false) && !v.Visible(sala[0], clases[0], false) && !v.AnimadoVisible("puerta_1"),
			"aislar deja solo el elegido");
	}

	// ---------------------------------------------------------------- Limites de modelos y seleccion
	const auto unico = std::chrono::steady_clock::now().time_since_epoch().count();
	const fs::path temp = fs::temp_directory_path() / ("EscapeTheUAB-EditorTests-" + std::to_string(unico));
	{
		escribir(temp / "caja.obj", "# cubo\nv -1 0 -2\nv 1 0 -2\nv 1 4 2\nv -1 4 2\nvt 9 9\nvn 0 1 0\nf 1 2 3\n");
		const CajaLocal caja = LimitesDeOBJ(temp / "caja.obj");
		comprobar(caja.valida && casi(caja.minimo.y, 0.0f) && casi(caja.maximo.y, 4.0f) && casi(caja.minimo.z, -2.0f) && casi(caja.maximo.z, 2.0f),
			"limites de un OBJ leidos de sus vertices (sin vt/vn)");
		comprobar(!LimitesDeOBJ(temp / "no_existe.obj").valida, "un OBJ inexistente no tiene limites");

		// Modelo GLB sintetico: un nodo con una malla en Y arriba (alto 2 en Y de glTF = 2 en Z del juego).
		glb::ModeloGLB modelo;
		glb::NodoGLB nodo; nodo.nombre = "Puerta"; nodo.indiceMalla = 0;
		modelo.nodos.push_back(nodo);
		glb::MallaGLB malla; glb::PrimitivaGLB prim;
		prim.posiciones = { { 0, 0, 0 }, { 1, 2, 0.1f } };
		malla.primitivas.push_back(prim);
		modelo.mallas.push_back(malla);
		const CajaLocal glbCaja = LimitesDeGLB(modelo);
		comprobar(glbCaja.valida && casi(glbCaja.maximo.z, 2.0f) && casi(glbCaja.maximo.x, 1.0f), "limites de un GLB en espacio Z-up");

		// OBB: el rayo respeta giro y escala.
		CajaLocal unidad; unidad.minimo = glm::vec3(-1.0f); unidad.maximo = glm::vec3(1.0f); unidad.valida = true;
		glm::mat4 modeloM = glm::translate(glm::mat4(1.0f), glm::vec3(10, 0, 0));
		modeloM = glm::rotate(modeloM, glm::radians(45.0f), glm::vec3(0, 0, 1));
		modeloM = glm::scale(modeloM, glm::vec3(0.5f, 2.0f, 1.0f));
		float t = 0.0f;
		comprobar(InterseccionRayoCaja({ 0, 0, 0 }, { 1, 0, 0 }, modeloM, unidad, t) && t > 8.0f && t < 10.0f, "rayo contra caja girada y escalada");
		comprobar(!InterseccionRayoCaja({ 0, 5, 0 }, { 1, 0, 0 }, modeloM, unidad, t), "rayo que pasa de largo");
		comprobar(!InterseccionRayoCaja({ 0, 0, 0 }, { -1, 0, 0 }, modeloM, unidad, t), "caja detras del rayo no cuenta");
	}

	// Seleccion por rayo desde la camara del editor (vista superior ortografica y perspectiva).
	{
		CajaLocal unidad; unidad.minimo = glm::vec3(-0.5f); unidad.maximo = glm::vec3(0.5f); unidad.valida = true;
		CajaLocal puerta; puerta.minimo = glm::vec3(-0.45f, -0.03f, 0.0f); puerta.maximo = glm::vec3(0.45f, 0.03f, 2.1f); puerta.valida = true;
		const std::vector<CandidatoSeleccion> candidatos = {
			{ 0, glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(6, 4, 0.1f)), unidad },   // suelo grande
			{ 1, glm::translate(glm::mat4(1.0f), glm::vec3(1, 1, 0.45f)), ConMitadMinima(unidad, 0.02f) },                              // mueble OBJ sin fisica
			{ 2, glm::translate(glm::mat4(1.0f), glm::vec3(-2, 1, 0.0f)), ConMitadMinima(puerta, 0.05f) },                              // puerta GLB (fina)
			{ 3, glm::translate(glm::mat4(1.0f), glm::vec3(1, 1, 1.1f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.06f)), unidad },     // objeto pequeno sobre el mueble
		};
		const glm::vec4 rect(0, 0, 1280, 720);
		// Camara superior ortografica como EditorEscenas::Matrices.
		auto ortoSuperior = [&](glm::vec3 objetivo, glm::mat4& vista, glm::mat4& proy) {
			vista = glm::lookAt(objetivo + glm::vec3(0, 0, 100), objetivo, glm::vec3(0, 1, 0));
			proy = glm::ortho(-7.0f * 1280.0f / 720.0f, 7.0f * 1280.0f / 720.0f, -7.0f, 7.0f, 0.05f, 400.0f);
		};
		glm::mat4 vista, proy;
		ortoSuperior(glm::vec3(0.0f), vista, proy);
		auto pantallaDe = [&](glm::vec3 mundo) {
			const glm::vec4 c = proy * vista * glm::vec4(mundo, 1.0f);
			return glm::vec2((c.x / c.w * 0.5f + 0.5f) * 1280.0f, (1.0f - (c.y / c.w * 0.5f + 0.5f)) * 720.0f);
		};
		comprobar(ElegirMasCercano(RayoDesdePantalla(vista, proy, pantallaDe({ -2, 1, 1.0f }), rect), candidatos) == 2, "vista superior: se selecciona la puerta GLB fina");
		comprobar(ElegirMasCercano(RayoDesdePantalla(vista, proy, pantallaDe({ 1, 1, 0.5f }), rect), candidatos) == 3, "se prefiere el objeto pequeno que esta encima del mueble");
		comprobar(ElegirMasCercano(RayoDesdePantalla(vista, proy, pantallaDe({ 1.4f, 1.3f, 0.0f }), rect), candidatos) == 1, "mueble OBJ sin colisionador seleccionable");
		comprobar(ElegirMasCercano(RayoDesdePantalla(vista, proy, pantallaDe({ -2.8f, -1.8f, 0.0f }), rect), candidatos) == 0, "suelo/arquitectura seleccionable");
		comprobar(ElegirMasCercano(RayoDesdePantalla(vista, proy, pantallaDe({ 6, 6, 0.0f }), rect), candidatos) == -1, "clic en vacio: sin seleccion");

		// Perspectiva: el mas cercano a la camara gana.
		const glm::mat4 vp = glm::lookAt(glm::vec3(-8, 1, 0.5f), glm::vec3(0, 1, 0.5f), glm::vec3(0, 0, 1));
		const glm::mat4 pp = glm::perspective(glm::radians(50.0f), 1280.0f / 720.0f, 0.05f, 500.0f);
		const glm::vec4 cp = pp * vp * glm::vec4(-2, 1, 1.0f, 1.0f);
		const glm::vec2 centroPuerta((cp.x / cp.w * 0.5f + 0.5f) * 1280.0f, (1.0f - (cp.y / cp.w * 0.5f + 0.5f)) * 720.0f);
		comprobar(ElegirMasCercano(RayoDesdePantalla(vp, pp, centroPuerta, rect), candidatos) == 2, "perspectiva: se selecciona la puerta");
		comprobar(!RayoDesdePantalla(vp, pp, centroPuerta, glm::vec4(0, 0, 0, 0)).valido, "viewport degenerado: rayo invalido");
	}

	// ---------------------------------------------------------------- Camara y flechas
	{
		OpcionesCamara o;
		CamaraEditor c;
		const glm::vec3 f0 = c.Adelante(), r0 = c.Derecha();
		c.Mirar(40.0f, 0.0f, o);
		comprobar(glm::dot(c.Adelante(), r0) > 0.0f, "raton a la derecha: se mira a la derecha");
		c = CamaraEditor();
		c.Mirar(-40.0f, 0.0f, o);
		comprobar(glm::dot(c.Adelante(), r0) < 0.0f, "raton a la izquierda: se mira a la izquierda");
		c = CamaraEditor();
		c.Mirar(0.0f, -40.0f, o);   // raton hacia ARRIBA (dy < 0 en coordenadas de pantalla)
		comprobar(c.Adelante().z > f0.z, "raton hacia arriba: se mira hacia arriba");
		c = CamaraEditor();
		c.Mirar(0.0f, 40.0f, o);
		comprobar(c.Adelante().z < f0.z, "raton hacia abajo: se mira hacia abajo");
		o.invertirY = true;
		c = CamaraEditor();
		c.Mirar(0.0f, -40.0f, o);
		comprobar(c.Adelante().z < f0.z, "con 'invertir Y' el eje vertical se invierte");
		o.invertirY = false;
		c = CamaraEditor();
		c.Mirar(0.0f, -1.0e6f, o);
		comprobar(casi(c.pitch, LimitePitch, 1e-3f) && std::abs(c.Adelante().z) < 1.0f, "el pitch se limita arriba");
		c.Mirar(0.0f, 1.0e6f, o);
		comprobar(casi(c.pitch, -LimitePitch, 1e-3f), "el pitch se limita abajo");
		c = CamaraEditor();
		c.Mirar(123.0f, -57.0f, o);
		comprobar(casi(glm::dot(c.Adelante(), c.Derecha()), 0.0f) && casi(glm::length(c.Arriba()), 1.0f) &&
			casi(glm::dot(c.Arriba(), c.Adelante()), 0.0f), "la base de la camara sigue siendo ortonormal");

		// Pan: la escena sigue al cursor (desplazamiento en pantalla = el del cursor).
		{
			glm::mat4 v, p;
			c = CamaraEditor();
			const glm::vec3 punto = c.Pivote();
			auto pantalla = [&](glm::vec3 w) {
				c.Matrices(16.0f / 9.0f, v, p);
				const glm::vec4 q = p * v * glm::vec4(w, 1.0f);
				return glm::vec2((q.x / q.w * 0.5f + 0.5f) * 1280.0f, (1.0f - (q.y / q.w * 0.5f + 0.5f)) * 720.0f);
			};
			const glm::vec2 antes = pantalla(punto);
			c.Pan(100.0f, 0.0f, 720.0f, o);
			const glm::vec2 despues = pantalla(punto);
			comprobar(despues.x > antes.x && std::abs((despues.x - antes.x) - 100.0f) < 1.0f, "pan perspectiva: arrastrar a la derecha mueve la escena 100 px a la derecha");
			c.Pan(0.0f, 60.0f, 720.0f, o);
			comprobar(pantalla(punto).y > despues.y, "pan perspectiva: arrastrar hacia abajo baja la escena");
			c = CamaraEditor();
			c.CambiarVista(Vista::Superior);
			const glm::vec2 a0 = pantalla(glm::vec3(0, 0, 0));
			c.Pan(100.0f, 0.0f, 720.0f, o);
			const glm::vec2 a1 = pantalla(glm::vec3(0, 0, 0));
			comprobar(std::abs((a1.x - a0.x) - 100.0f) < 1.0f, "pan ortografico superior: la escena sigue al cursor");
			c.Pan(0.0f, 50.0f, 720.0f, o);
			comprobar(std::abs((pantalla(glm::vec3(0, 0, 0)).y - a1.y) - 50.0f) < 1.0f, "pan ortografico: arrastrar hacia abajo baja la escena");
		}

		// Zoom y orbita.
		c = CamaraEditor();
		const glm::vec3 pivote = c.Pivote();
		c.Zoom(+2.0f, o);
		comprobar(c.distanciaFoco < 12.0f && glm::length(c.Pivote() - pivote) < 1e-3f, "rueda hacia arriba acerca al mismo pivote");
		c.Zoom(-4.0f, o);
		comprobar(c.distanciaFoco > 12.0f, "rueda hacia abajo aleja");
		c = CamaraEditor();
		const glm::vec3 piv = c.Pivote();
		c.Orbitar(piv, 80.0f, -20.0f, o);
		comprobar(glm::length(c.Pivote() - piv) < 1e-3f && glm::length(c.posicion - piv) > 1.0f, "orbitar mantiene el pivote y mueve la camara");

		// Multiplicadores: Ctrl prevalece sobre Shift.
		o.multiplicadorShift = 3.0f;
		o.multiplicadorCtrl = 0.25f;
		comprobar(MultiplicadorVelocidad(false, false, o) == 1.0f && MultiplicadorVelocidad(false, true, o) == 3.0f &&
			MultiplicadorVelocidad(true, false, o) == 0.25f && MultiplicadorVelocidad(true, true, o) == 0.25f,
			"x1 / Shift x3 / Ctrl x0,25 / Ctrl+Shift: prevalece Ctrl");
		c = CamaraEditor();
		c.yaw = 90.0f;
		c.pitch = 0.0f;
		c.posicion = glm::vec3(0.0f);
		c.Mover(1.0f, 0.0f, 0.0f, 0.05f, 1.0f, o);
		comprobar(casi(c.posicion.y, o.velocidad * 0.05f, 1e-4f), "W mueve hacia delante");

		// Flechas.
		c = CamaraEditor();
		c.yaw = 90.0f;
		c.pitch = -45.0f;   // mirando a +Y y hacia abajo
		comprobar(casi(glm::length(DireccionFlecha(Flecha::Arriba, c) - glm::vec3(0, 1, 0)), 0.0f, 1e-4f), "perspectiva: flecha arriba = hacia delante en XY");
		comprobar(casi(glm::length(DireccionFlecha(Flecha::Derecha, c) - glm::vec3(1, 0, 0)), 0.0f, 1e-4f), "perspectiva: flecha derecha = derecha de la camara");
		comprobar(DireccionFlecha(Flecha::Arriba, c).z == 0.0f && DireccionFlecha(Flecha::Izquierda, c).z == 0.0f, "las flechas no cambian la altura en perspectiva");
		comprobar(DireccionFlecha(Flecha::Subir, c).z == 1.0f && DireccionFlecha(Flecha::Bajar, c).z == -1.0f, "RePag/AvPag suben y bajan en Z");
		c.yaw = 0.0f;
		comprobar(casi(glm::length(DireccionFlecha(Flecha::Arriba, c) - glm::vec3(1, 0, 0)), 0.0f, 1e-4f), "mirando a +X, flecha arriba = +X");
		c.pitch = 89.0f;   // casi vertical: no se degenera
		comprobar(casi(glm::length(DireccionFlecha(Flecha::Arriba, c)), 1.0f, 1e-3f), "mirando casi recto abajo la direccion sigue siendo valida");
		c = CamaraEditor();
		c.CambiarVista(Vista::Superior);
		comprobar(DireccionFlecha(Flecha::Arriba, c) == glm::vec3(0, 1, 0) && DireccionFlecha(Flecha::Derecha, c) == glm::vec3(1, 0, 0),
			"vista Top: flechas sobre el plano XY (arriba = +Y, derecha = +X)");

		c = CamaraEditor();
		c.yaw = 0.0f;
		c.pitch = 0.0f;   // mirando a +X
		const glm::vec3 origen(2.0f, 3.0f, 1.0f);
		comprobar(casi(AplicarFlecha(origen, Flecha::Arriba, c, false, false, o).x, 2.10f, 1e-5f), "flecha: 10 cm");
		comprobar(casi(AplicarFlecha(origen, Flecha::Arriba, c, true, false, o).x, 2.01f, 1e-5f), "Ctrl + flecha: 1 cm");
		comprobar(casi(AplicarFlecha(origen, Flecha::Arriba, c, false, true, o).x, 2.50f, 1e-5f), "Shift + flecha: 50 cm");
		bool snap = false;
		comprobar(LongitudPasoFlecha(true, true, o, snap) == o.pasoCuadricula && snap, "Ctrl+Shift = snap a la cuadricula");
		const glm::vec3 conSnap = AplicarFlecha(glm::vec3(2.03f, 3.04f, 1.0f), Flecha::Arriba, c, true, true, o);
		comprobar(casi(conSnap.x, 2.10f, 1e-4f) && casi(conSnap.y, 3.04f, 1e-5f), "snap: queda en la cuadricula de 10 cm solo en el eje que se mueve");
		comprobar(casi(AplicarFlecha(origen, Flecha::Subir, c, false, false, o).z, 1.10f, 1e-5f), "RePag: +10 cm en Z");
		o.pasoCuadricula = 0.25f;
		comprobar(casi(AplicarFlecha(glm::vec3(0.0f), Flecha::Arriba, c, true, true, o).x, 0.25f, 1e-5f), "la cuadricula es configurable");

		// Estado de captura de la entrada.
		comprobar(ViewportLibre(false, false, false, false), "sin nada activo el viewport esta libre");
		comprobar(!ViewportLibre(true, false, false, false) && !ViewportLibre(false, true, false, false) &&
			!ViewportLibre(false, false, true, false) && !ViewportLibre(false, false, false, true),
			"panel, widget, gizmo o popup bloquean camara y seleccion");
		comprobar(GizmoHabilitado(false, false, false, false), "el gizmo esta habilitado por defecto");
		comprobar(!GizmoHabilitado(false, true, false, false) && !GizmoHabilitado(false, false, true, false) &&
			!GizmoHabilitado(false, false, false, true), "se deshabilita al navegar, con Alt o escalando una puerta");
		comprobar(GizmoHabilitado(true, true, true, true), "un arrastre en curso nunca se corta");

		// Una pulsacion mantenida de flecha (cientos de repeticiones) es UNA operacion de historial.
		std::vector<SceneObject> objetos = { Objeto("mesa", "objects", { 1, 1, 0 }) };
		HistorialEscena h;
		h.IniciarTransaccion(Estado(objetos));
		c = CamaraEditor();
		c.yaw = 90.0f;
		c.pitch = 0.0f;
		for (int i = 0; i < 30; ++i)
		{
			objetos[0].posicion = AplicarFlecha(objetos[0].posicion, Flecha::Derecha, c, false, false, OpcionesCamara());
			h.IniciarTransaccion(Estado(objetos));
		}
		comprobar(casi(objetos[0].posicion.x, 4.0f, 1e-3f), "30 repeticiones de 10 cm = 3 m");
		h.CerrarTransaccion(Estado(objetos));
		comprobar(h.OperacionesParaDeshacer() == 1, "las repeticiones del teclado forman una sola operacion");
		EstadoEscena actual = Estado(objetos);
		h.Deshacer(actual);
		comprobar(casi(actual.objetos[0].posicion.x, 1.0f), "Ctrl+Z devuelve la posicion previa a la pulsacion mantenida");
	}

	// ---------------------------------------------------------------- Persistencia: añadir OBJ, puertas, guardar y recargar
	{
		escribir(temp / "models" / "mesa.obj", "# mesa\nv 0 0 0\nv 1 0 0\nv 1 1 1\n");
		escribir(temp / "models" / "escritorio.obj", "# escritorio\nv 0 0 0\nv 2 0 0\nv 2 1 1\n");
		escribir(temp / "animations" / "Puerta.glb", "# glb de prueba\n");
		const fs::path escena = temp / "scenes" / "sala.json";
		const std::string original = R"json({
  "version": 1,
  "name": "sala",
  "architecture": [
    {"id":"floor","model":"../models/mesa.obj","position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]},
    {"id":"ceiling","model":"../models/mesa.obj","position":[0,0,3],"rotation":[0,0,0],"scale":[1,1,1]}
  ],
  "objects": [
    {"id":"mesa","model":"../models/mesa.obj","position":[1,2,3],"rotation":[0,0,0],"scale":[1,1,1]}
  ],
  "animated": [
    {"id":"puerta_1","model":"../animations/Puerta.glb","position":[6,-2,-1.7],"rotation":[0,0,-90],"animation":"Abrir",
     "colliders":[{"node":"DoorFrame","type":"static","halfSize":[0.05,1.05,0.04],"offset":[-0.45,1.05,-0.013]}]}
  ]
})json";
		escribir(escena, original);

		DatosEscena datos;
		std::string error;
		comprobar(CargarDatosEscenaJSON(escena, datos, error), "la escena de prueba carga");

		SerializadorEscenas serializador;
		comprobar(serializador.Cargar(escena, error), "el serializador carga la escena");

		// Lo que hace el editor al añadir un OBJ: ID unico, categoria 'objects', ESCALA 1 (no 0) y ruta absoluta que se guarda relativa.
		SceneObject nuevo = Objeto("obj_escritorio", "objects", { 4, 1, -1.7f });
		nuevo.modelo = (temp / "models" / "escritorio.obj").lexically_normal().string();
		nuevo.rotacion = { 90, 0, 0 };
		nuevo.fisica.presente = true; nuevo.fisica.tipo = fisica::TipoCuerpo::Estatico;
		nuevo.fisica.mitadTamano = { 1, 0.5f, 0.5f }; nuevo.fisica.desplazamiento = { 1, 0.5f, 0.5f };

		// Regresion del error 6: un objeto con escala 0 (como el que creaba el editor antes) es rechazado con un error claro.
		SceneObject cero = nuevo; cero.id = "obj_cero"; cero.escala = glm::vec3(0.0f);
		std::vector<SceneObject> conCero = datos.objetos; conCero.insert(conCero.end(), datos.objetos.begin(), datos.objetos.end());
		conCero.assign(datos.objetos.begin(), datos.objetos.end()); conCero.push_back(cero);
		comprobar(!serializador.Guardar(conCero, datos.animados, error) && !error.empty(), "escala 0 se rechaza al guardar (error visible)");
		comprobar(leer(escena) == original, "un guardado rechazado deja el JSON intacto");

		std::vector<SceneObject> objetos = datos.objetos;
		objetos.push_back(nuevo);
		std::vector<SceneObjetoAnimado> animados = datos.animados;
		animados[0].posicion = { 7.5f, -2.0f, -1.7f };
		animados[0].giroZGrados = 0.0f;
		comprobar(serializador.Guardar(objetos, animados, error), "guardar con un OBJ nuevo y una puerta movida");

		DatosEscena recargada;
		comprobar(CargarDatosEscenaJSON(escena, recargada, error), "el JSON guardado se recarga");
		bool encontrado = false;
		for (const SceneObject& o : recargada.objetos)
			if (o.id == "obj_escritorio")
			{
				encontrado = true;
				comprobar(casi(o.posicion.x, 4.0f) && casi(o.rotacion.x, 90.0f) && casi(o.escala.x, 1.0f), "el OBJ nuevo conserva su transformacion");
				comprobar(o.fisica.presente && casi(o.fisica.mitadTamano.x, 1.0f), "el OBJ nuevo conserva su colisionador de caja");
				comprobar(fs::equivalent(escena.parent_path() / o.modelo, temp / "models" / "escritorio.obj"), "la ruta se guarda relativa al JSON");
			}
		comprobar(encontrado, "el OBJ añadido sigue en la escena tras guardar y recargar");

		comprobar(recargada.animados.size() == 1, "la puerta sigue en la escena");
		if (!recargada.animados.empty())
		{
			const SceneObjetoAnimado& p = recargada.animados[0];
			comprobar(casi(p.posicion.x, 7.5f) && casi(p.giroZGrados, 0.0f), "la puerta guarda su nueva posicion y giro");
			comprobar(p.animacion == "Abrir" && p.colisionadores.size() == 1 && p.colisionadores[0].nodo == "DoorFrame" &&
				casi(p.colisionadores[0].mitadTamano.y, 1.05f), "animacion y colisionadores de la puerta se conservan");
		}

		// Mover con las flechas y guardar: la transformacion se conserva en el JSON.
		{
			CamaraEditor camara;
			camara.yaw = 0.0f;
			camara.pitch = 0.0f;   // mirando a +X: flecha arriba = +X
			SceneObject* mesa = nullptr;
			for (SceneObject& o : objetos) if (o.id == "mesa") mesa = &o;
			comprobar(mesa != nullptr, "la mesa del JSON original esta en la lista");
			if (mesa != nullptr)
			{
				mesa->posicion = AplicarFlecha(mesa->posicion, Flecha::Arriba, camara, false, true, OpcionesCamara());   // +50 cm
				comprobar(serializador.Guardar(objetos, animados, error), "guardar tras mover con las flechas");
				DatosEscena movida;
				CargarDatosEscenaJSON(escena, movida, error);
				bool ok = false;
				for (const SceneObject& o : movida.objetos) if (o.id == "mesa") ok = casi(o.posicion.x, 1.5f) && casi(o.posicion.y, 2.0f);
				comprobar(ok, "la posicion movida con las flechas se conserva en el JSON");
			}
		}

		// Ocultar elementos NO modifica el JSON: la visibilidad ni existe para el serializador.
		VisibilidadEditor v; v.ocultarTecho = true; v.ocultarParedes = true; v.ocultos.insert("mesa");
		const std::string tras = leer(escena);
		serializador.Guardar(objetos, animados, error);
		comprobar(leer(escena) == tras, "guardar con elementos ocultos escribe exactamente lo mismo (el techo sigue en el JSON)");
		comprobar(tras.find("\"ceiling\"") != std::string::npos, "el techo sigue en el JSON");

		// Deshacer hasta el inicio y guardar devuelve el mismo contenido logico que el original.
		HistorialEscena h;
		h.Registrar(Estado(datos.objetos, datos.animados));
		EstadoEscena actual = Estado(objetos, animados);
		h.Deshacer(actual);
		comprobar(serializador.Guardar(actual.objetos, actual.animados, error), "guardar despues de deshacer");
		DatosEscena trasDeshacer;
		CargarDatosEscenaJSON(escena, trasDeshacer, error);
		comprobar(trasDeshacer.objetos.size() == datos.objetos.size() && casi(trasDeshacer.animados[0].posicion.x, 6.0f),
			"tras deshacer y guardar, el JSON vuelve al estado anterior (sin el OBJ, puerta en su sitio)");
	}

	std::error_code ec;
	fs::remove_all(temp, ec);
	if (fallos == 0) std::cout << "EditorTests: todo correcto\n";
	return fallos == 0 ? 0 : 1;
}
