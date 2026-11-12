// PhysicsTests.cpp
// -----------------------------------------------------------------------------
// Pruebas de MundoFisico (Bullet). No necesitan ventana ni OpenGL.
//   1. Gravedad configurada con Z vertical.
//   2. Caída libre sin suelo.
//   3. Paso fijo de 1/60 s (acumulación, tiempos grandes, independencia de los FPS).
//   4. Pausa.
//   5. Colisión con el suelo y reposo (demo del cubo) y reinicio.
// -----------------------------------------------------------------------------
#include "EscenaFisica.h"
#include "MundoFisico.h"
#include "SceneLoader.h"
#include "VistaEstereo.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace fisica;
namespace fs = std::filesystem;

namespace
{
	int fallos = 0;

	void comprobar(bool condicion, const char* caso)
	{
		if (!condicion)
		{
			std::cerr << "FALLO: " << caso << '\n';
			++fallos;
		}
	}

	bool casi(float a, float b, float tolerancia) { return std::fabs(a - b) <= tolerancia; }

	// Simula 'segundos' de tiempo real en fotogramas de duración fija 'deltaFotograma'.
	template <typename Funcion>
	void Simular(float segundos, float deltaFotograma, Funcion&& porFotograma)
	{
		const int fotogramas = static_cast<int>(std::lround(segundos / deltaFotograma));
		for (int i = 0; i < fotogramas; ++i) porFotograma(deltaFotograma);
	}

	// La demo ya no tiene mundo propio (usa el mundo único del juego), así que las pruebas
	// de la Fase 1 crean un mundo y le pasan la demo. Es el único cambio respecto a la Fase 1.
	struct Escenario
	{
		MundoFisico mundo;
		DemoCuboCaida demo{ mundo };
		void Actualizar(float dt, bool pausado) { mundo.FijarPausa(pausado); mundo.Actualizar(dt); }
		PoseCuerpo PoseCubo() const { return demo.PoseCubo(); }
	};

	void PruebaGravedad()
	{
		MundoFisico mundo;
		const glm::vec3 g = mundo.Gravedad();
		comprobar(casi(g.x, 0.0f, 1e-6f) && casi(g.y, 0.0f, 1e-6f) && casi(g.z, -9.81f, 1e-5f),
			"la gravedad es (0, 0, -9.81): Z es el eje vertical");
	}

	void PruebaCaidaLibre()
	{
		MundoFisico mundo;
		const IdCuerpo cubo = mundo.CrearCajaDinamica(glm::vec3(1.0f, 2.0f, 100.0f), glm::vec3(0.25f), 1.0f);

		Simular(1.0f, MundoFisico::PasoFijo, [&](float dt) { mundo.Actualizar(dt); });

		const PoseCuerpo pose = mundo.PoseDe(cubo);
		// Teoría: z = 100 - 1/2 g t^2 = 95.095 m tras 1 s (Bullet integra con Euler semi-implícito, ~0.08 m menos).
		comprobar(casi(pose.posicion.z, 95.095f, 0.15f), "tras 1 s de caída libre el cubo ha bajado ~4.9 m");
		comprobar(casi(pose.posicion.x, 1.0f, 1e-4f) && casi(pose.posicion.y, 2.0f, 1e-4f),
			"la gravedad no desplaza el cubo en X ni en Y");
		comprobar(casi(mundo.VelocidadDe(cubo).z, -9.81f, 0.05f), "tras 1 s la velocidad vertical es ~-9.81 m/s");
	}

	void PruebaPasoFijo()
	{
		MundoFisico mundo;
		comprobar(mundo.Actualizar(0.005f) == 0, "un fotograma más corto que el paso fijo no simula nada");
		comprobar(mundo.Actualizar(0.005f) == 0, "...ni el segundo (acumulado 0.010 s)");
		comprobar(mundo.Actualizar(0.0075f) == 1, "al acumular más de 1/60 s se ejecuta exactamente un paso");
		comprobar(mundo.Actualizar(0.034f) == 2, "un fotograma de 34 ms ejecuta dos pasos de 1/60 s");
		comprobar(mundo.Actualizar(0.0f) == 0 && mundo.Actualizar(-1.0f) == 0, "un delta cero o negativo se ignora");

		MundoFisico congelado;
		const int pasos = congelado.Actualizar(10.0f);
		comprobar(pasos == MundoFisico::PasosMaximosPorFotograma, "un fotograma de 10 s se recorta al máximo de pasos");
		comprobar(congelado.Actualizar(0.0f) == 0, "tras un tiempo enorme no queda tiempo pendiente acumulado");
	}

	void PruebaIndependenciaDeLosFps()
	{
		Escenario a, b;
		a.demo.Iniciar();
		b.demo.Iniciar();
		Simular(3.0f, 1.0f / 30.0f, [&](float dt) { a.Actualizar(dt, false); });
		Simular(3.0f, 1.0f / 144.0f, [&](float dt) { b.Actualizar(dt, false); });
		comprobar(casi(a.PoseCubo().posicion.z, b.PoseCubo().posicion.z, 0.02f),
			"30 FPS y 144 FPS dan el mismo resultado final");
	}

	void PruebaPausa()
	{
		MundoFisico mundo;
		const IdCuerpo cubo = mundo.CrearCajaDinamica(glm::vec3(0.0f, 0.0f, 50.0f), glm::vec3(0.25f), 1.0f);

		Simular(0.5f, MundoFisico::PasoFijo, [&](float dt) { mundo.Actualizar(dt); });
		const float alturaAntes = mundo.PoseDe(cubo).posicion.z;
		const long long pasosAntes = mundo.PasosTotales();

		mundo.FijarPausa(true);
		comprobar(mundo.EstaEnPausa(), "la pausa queda activada");
		comprobar(mundo.Actualizar(1.0f) == 0, "en pausa no se ejecuta ningún paso");
		Simular(2.0f, 0.016f, [&](float dt) { mundo.Actualizar(dt); });
		comprobar(mundo.PoseDe(cubo).posicion.z == alturaAntes, "en pausa el cubo no se mueve");
		comprobar(mundo.PasosTotales() == pasosAntes, "en pausa el contador de pasos no cambia");

		mundo.FijarPausa(false);
		comprobar(mundo.Actualizar(MundoFisico::PasoFijo) == 1,
			"al reanudar solo cuenta el tiempo nuevo (el de la pausa se descartó)");
		comprobar(mundo.PoseDe(cubo).posicion.z < alturaAntes, "al reanudar el cubo vuelve a caer");
	}

	void PruebaSuelo()
	{
		Escenario escenario;
		DemoCuboCaida& demo = escenario.demo;
		demo.Iniciar();
		const float alturaInicial = demo.PoseCubo().posicion.z;
		const float alturaReposo = DemoCuboCaida::AlturaSuperiorSuelo + DemoCuboCaida::MitadCubo;   // -1.45

		comprobar(casi(alturaInicial, DemoCuboCaida::AlturaInicialCentro, 1e-5f), "el cubo empieza a la altura inicial");

		float alturaMinima = alturaInicial;
		float alturaTras03s = 0.0f;
		float tiempo = 0.0f;
		Simular(6.0f, MundoFisico::PasoFijo, [&](float dt) {
			escenario.Actualizar(dt, false);
			tiempo += dt;
			alturaMinima = std::min(alturaMinima, demo.PoseCubo().posicion.z);
			if (alturaTras03s == 0.0f && tiempo >= 0.3f) alturaTras03s = demo.PoseCubo().posicion.z;
		});

		comprobar(alturaTras03s < alturaInicial - 0.3f, "a los 0.3 s el cubo ya ha caído");
		// Al impactar a ~7 m/s el cubo recorre ~12 cm por paso, así que Bullet permite una
		// penetración momentánea (~5 cm medidos) y lo empuja fuera. Atravesar de verdad sería
		// hundirse más de la mitad del cubo.
		comprobar(alturaMinima > alturaReposo - 0.10f, "el cubo nunca atraviesa el suelo (solo penetración momentánea < 10 cm)");
		const PoseCuerpo pose = demo.PoseCubo();
		comprobar(casi(pose.posicion.z, alturaReposo, 0.02f), "el cubo se apoya sobre el suelo (centro a 0.25 m de él)");
		comprobar(glm::length(demo.VelocidadCubo()) < 0.01f, "el cubo termina quieto");
		comprobar(demo.CuboEnReposo(), "Bullet duerme el cubo cuando lleva un rato quieto");
		comprobar(casi(pose.posicion.x, 6.0f, 0.01f) && casi(pose.posicion.y, -1.0f, 0.01f),
			"caer plano no desplaza el cubo en horizontal");

		// Reiniciar vuelve a empezar desde arriba.
		demo.Iniciar();
		comprobar(casi(demo.PoseCubo().posicion.z, alturaInicial, 1e-5f), "Iniciar() reinicia la demo");
		comprobar(escenario.mundo.NumeroDeCuerpos() == 2, "tras reiniciar solo hay suelo y cubo (no se acumulan)");

		demo.Detener();
		comprobar(!demo.EstaActiva() && escenario.mundo.NumeroDeCuerpos() == 0, "Detener() elimina los cuerpos");
	}

	void PruebaMasaInvalida()
	{
		MundoFisico mundo;
		comprobar(mundo.CrearCajaDinamica(glm::vec3(0.0f), glm::vec3(1.0f), 0.0f) == CuerpoInvalido,
			"un cuerpo dinámico con masa 0 se rechaza");
		comprobar(mundo.PoseDe(CuerpoInvalido).posicion == glm::vec3(0.0f), "pedir la pose de un id inválido no falla");
	}
	// ======================================================================
	// Fase 2: escena JSON -> cuerpos de Bullet
	// ======================================================================

	SceneObject ObjetoCaja(const char* id, const glm::vec3& posicion, const glm::vec3& rotacion,
		const glm::vec3& escala, TipoCuerpo tipo, const glm::vec3& mitad, const glm::vec3& desplazamiento, float masa)
	{
		SceneObject o{};
		o.id = id;
		o.posicion = posicion;
		o.rotacion = rotacion;
		o.escala = escala;
		o.fisica.presente = true;
		o.fisica.tipo = tipo;
		o.fisica.mitadTamano = mitad;
		o.fisica.desplazamiento = desplazamiento;
		o.fisica.masa = masa;
		return o;
	}

	bool Crear(EscenaFisica& escena, const std::vector<SceneObject>& objetos, std::string& error)
	{
		const glb::EscenaAnimada sinAnimados;
		return escena.Crear(objetos, sinAnimados, {}, error);
	}

	void Avanzar(MundoFisico& mundo, float segundos)
	{
		Simular(segundos, MundoFisico::PasoFijo, [&](float dt) { mundo.Actualizar(dt); });
	}

	// Rotación de un cuerpo aplicada a un vector (para comprobar orientaciones).
	glm::vec3 Gira(const PoseCuerpo& pose, const glm::vec3& v) { return pose.rotacion * v; }

	bool Cerca(const glm::vec3& a, const glm::vec3& b, float tolerancia = 1e-3f) { return glm::length(a - b) <= tolerancia; }

	// Colisionador trasladado, girado y escalado: no solo "correcto en el origen".
	void PruebaColisionadorTransformado()
	{
		MundoFisico mundo;
		EscenaFisica escena(mundo);
		// Objeto en (3,-2,1), girado 90 grados en Z, escala (2,1,1); caja de ±0.5 desplazada 1 m en X del modelo.
		//   offset escalado = (2,0,0) -> girado 90 grados en Z = (0,2,0) -> centro = (3,0,1)
		//   mitad final = (1, 0.5, 0.5); el eje largo (X del modelo) queda a lo largo de Y.
		std::string error;
		comprobar(Crear(escena, { ObjetoCaja("t", { 3, -2, 1 }, { 0, 0, 90 }, { 2, 1, 1 }, TipoCuerpo::Estatico,
			{ 0.5f, 0.5f, 0.5f }, { 1, 0, 0 }, 0.0f) }, error), "crear colisionador transformado");
		const auto cajas = mundo.CajasDeColision();
		comprobar(cajas.size() == 1, "se crea exactamente un cuerpo");
		if (cajas.size() != 1) return;
		comprobar(Cerca(cajas[0].pose.posicion, { 3, 0, 1 }), "centro: posicion + R * (escala * offset)");
		comprobar(Cerca(cajas[0].mitadTamano, { 1.0f, 0.5f, 0.5f }), "mitad: |escala| * halfSize (escala aplicada una vez)");
		comprobar(Cerca(Gira(cajas[0].pose, { 1, 0, 0 }), { 0, 1, 0 }), "la caja gira con el objeto (X del modelo -> Y del mundo)");

		// Las esquinas reales de la caja: x en [2.5, 3.5], y en [-1, 1] (largo a lo largo de Y), z en [0.5, 1.5].
		float minimo[3] = { 1e9f, 1e9f, 1e9f }, maximo[3] = { -1e9f, -1e9f, -1e9f };
		for (const glm::vec3& p : AristasDeCaja(cajas[0]))
			for (int e = 0; e < 3; ++e) { minimo[e] = std::min(minimo[e], p[e]); maximo[e] = std::max(maximo[e], p[e]); }
		comprobar(casi(minimo[0], 2.5f, 1e-3f) && casi(maximo[0], 3.5f, 1e-3f) && casi(minimo[1], -1.0f, 1e-3f) &&
			casi(maximo[1], 1.0f, 1e-3f) && casi(minimo[2], 0.5f, 1e-3f) && casi(maximo[2], 1.5f, 1e-3f),
			"las aristas dibujadas ocupan exactamente el volumen esperado");

		// Escala negativa (modelo espejado): el centro la respeta y la mitad sigue positiva.
		EscenaFisica espejo(mundo);
		comprobar(Crear(espejo, { ObjetoCaja("e", { 0, 0, 0 }, { 0, 0, 0 }, { -2, 1, 1 }, TipoCuerpo::Estatico,
			{ 1, 1, 1 }, { 1, 0, 0 }, 0.0f) }, error), "crear con escala negativa");
		bool encontrada = false;
		for (const CajaColision& c : mundo.CajasDeColision())
			if (Cerca(c.pose.posicion, { -2, 0, 0 }) && Cerca(c.mitadTamano, { 2, 1, 1 })) encontrada = true;
		comprobar(encontrada, "escala negativa: centro espejado y mitad positiva");
	}

	void PruebaEstaticoNoCae()
	{
		MundoFisico mundo;
		EscenaFisica escena(mundo);
		std::string error;
		comprobar(Crear(escena, { ObjetoCaja("flota", { 1, 2, 5 }, { 0, 0, 0 }, { 1, 1, 1 }, TipoCuerpo::Estatico,
			{ 0.5f, 0.5f, 0.5f }, { 0, 0, 0 }, 0.0f) }, error), "crear estatico en el aire");
		Avanzar(mundo, 3.0f);
		comprobar(Cerca(mundo.CajasDeColision()[0].pose.posicion, { 1, 2, 5 }, 1e-6f), "un cuerpo estatico no cae");
	}

	// Suelo + caja dinamica con offset y giro: cae, choca y se detiene.
	void PruebaDinamicoCaeYChoca()
	{
		MundoFisico mundo;
		EscenaFisica escena(mundo);
		std::string error;
		// Suelo: cubo de ±1 escalado (8,5,0.15), cara superior en z = -1.7.
		const SceneObject suelo = ObjetoCaja("suelo", { 5, 0, -1.85f }, { 0, 0, 0 }, { 8, 5, 0.15f }, TipoCuerpo::Estatico,
			{ 1, 1, 1 }, { 0, 0, 0 }, 0.0f);
		// Caja: modelo con el origen en su base (offset z = +1 con halfSize 1), escala 0.25, girada 30 grados en Z.
		const SceneObject caja = ObjetoCaja("caja", { 5, 0, 1 }, { 0, 0, 30 }, { 0.25f, 0.25f, 0.25f }, TipoCuerpo::Dinamico,
			{ 1, 1, 1 }, { 0, 0, 1 }, 2.0f);
		const std::vector<SceneObject> objetos = { suelo, caja };
		comprobar(Crear(escena, objetos, error), "crear suelo y caja");
		comprobar(escena.NumeroDeCuerpos() == 2 && escena.NumeroDeDinamicos() == 1, "dos cuerpos, uno dinamico");

		const glm::mat4 antes = escena.MatrizVisualDe(1, caja);
		comprobar(Cerca(glm::vec3(antes[3]), caja.posicion), "al empezar, el dibujo coincide con la posicion del JSON");

		Avanzar(mundo, 0.3f);
		comprobar(escena.MatrizVisualDe(1, caja)[3].z < 1.0f - 0.2f, "a los 0.3 s la caja ya ha caido");
		Avanzar(mundo, 5.0f);

		// En reposo: el centro de la caja esta a 0.25 m sobre el suelo y el ORIGEN del modelo (su base) toca el suelo.
		const glm::mat4 despues = escena.MatrizVisualDe(1, caja);
		comprobar(casi(despues[3].z, -1.7f, 0.02f), "el origen del modelo (su base) queda sobre el suelo");
		comprobar(casi(despues[3].x, 5.0f, 0.02f) && casi(despues[3].y, 0.0f, 0.02f), "cae en vertical, sin deriva horizontal");
		// Conserva el giro de 30 grados en Z: la columna X del dibujo apunta a (cos30, sin30, 0) y mide 0.25 (escala).
		comprobar(Cerca(glm::vec3(despues[0]), glm::vec3(0.25f * std::cos(glm::radians(30.0f)), 0.25f * std::sin(glm::radians(30.0f)), 0.0f), 0.02f),
			"la caja conserva su giro y su escala al caer");
		comprobar(mundo.CajasDeColision().size() == 2, "no se han duplicado cuerpos");
		for (const CajaColision& c : mundo.CajasDeColision())
			if (c.tipo == TipoCuerpo::Dinamico) comprobar(glm::length(mundo.VelocidadDe(0)) < 1.0f, "(sanidad) velocidades finitas");
		// Un objeto sin fisica se dibuja con la matriz de la escena.
		SceneObject sinFisica{};
		sinFisica.posicion = { 1, 2, 3 }; sinFisica.escala = { 1, 1, 1 };
		comprobar(Cerca(glm::vec3(escena.MatrizVisualDe(-1, sinFisica)[3]), { 1, 2, 3 }), "un objeto sin fisica conserva su matriz");

		// Pausa y reinicio con una escena real: en pausa nada se mueve.
		EscenaFisica otra(mundo);
		const SceneObject volando = ObjetoCaja("v", { 0, 20, 50 }, { 0, 0, 0 }, { 1, 1, 1 }, TipoCuerpo::Dinamico, { 0.5f, 0.5f, 0.5f }, { 0, 0, 0 }, 1.0f);
		comprobar(Crear(otra, { volando }, error), "crear caja para la prueba de pausa");
		mundo.FijarPausa(true);
		const glm::mat4 enPausa = otra.MatrizVisualDe(0, volando);
		Simular(1.0f, 0.016f, [&](float dt) { mundo.Actualizar(dt); });
		comprobar(otra.MatrizVisualDe(0, volando)[3] == enPausa[3], "con la pausa, el cuerpo de la escena no se mueve");
		mundo.FijarPausa(false);
		Avanzar(mundo, 0.2f);
		comprobar(otra.MatrizVisualDe(0, volando)[3].z < enPausa[3].z, "al reanudar vuelve a caer");
	}

	std::string RutaDeEscena(const char* nombre)
	{
		return (fs::path(ESCAPETHEUAB_SOURCE_DIR) / "EntornVGI/assets/scenes" / nombre).string();
	}

	// Carga una escena JSON real y la monta: datos, GLB y cuerpos.
	struct EscenaMontada
	{
		DatosEscena datos;
		glb::EscenaAnimada animados;
		std::unique_ptr<EscenaFisica> fisica;
		std::string error;
		bool ok = false;

		EscenaMontada(MundoFisico& mundo, const std::string& ruta)
		{
			fisica = std::make_unique<EscenaFisica>(mundo);
			if (!CargarDatosEscenaJSON(ruta, datos, error)) return;
			if (!CargarObjetosAnimadosDeEscena(datos.animados, fs::path(ruta).parent_path(), animados, error)) return;
			ok = fisica->Crear(datos.objetos, animados, datos.animados, error);
		}
	};

	void PruebaPuertasDesdeJson()
	{
		MundoFisico mundo;
		EscenaMontada habitacion(mundo, RutaDeEscena("habitacion_beta.json"));
		comprobar(habitacion.ok, "habitacion_beta monta con sus fisicas");
		if (!habitacion.ok) { std::cerr << habitacion.error << '\n'; return; }

		// Dos puertas del MISMO glb: un solo modelo cargado, dos estados independientes.
		glb::EscenaAnimada& anim = habitacion.animados;
		comprobar(anim.objetos.size() == 2 && anim.modelos.size() == 1, "dos puertas comparten un unico modelo GLB");
		comprobar(anim.objetos[0].modelo == anim.objetos[1].modelo, "ambas instancias apuntan al mismo ModeloGLB");
		comprobar(&anim.objetos[0].pose != &anim.objetos[1].pose, "cada instancia tiene su pose");

		// Cuerpos: 12 objetos OBJ con fisica + (3 marco + 1 hoja) * 2 puertas = 20.
		comprobar(habitacion.fisica->NumeroDeCuerpos() == 20 && mundo.NumeroDeCuerpos() == 20, "20 cuerpos: 12 OBJ + 8 de puertas");

		const auto cajasCinematicas = [&]() {
			std::vector<CajaColision> v;
			for (const CajaColision& c : mundo.CajasDeColision()) if (c.tipo == TipoCuerpo::Cinematico) v.push_back(c);
			return v;
		};
		const auto posicionesEstaticas = [&]() {
			std::vector<glm::vec3> v;
			for (const CajaColision& c : mundo.CajasDeColision()) if (c.tipo == TipoCuerpo::Estatico) v.push_back(c.pose.posicion);
			return v;
		};
		const std::vector<CajaColision> hojasCerradas = cajasCinematicas();
		const std::vector<glm::vec3> estaticasAntes = posicionesEstaticas();
		comprobar(hojasCerradas.size() == 2, "una hoja cinematica por puerta");
		const glm::vec3 ojosFrentePuerta(4.5f, 3.0f, -0.65f);
		const glm::vec3 miradaPuerta(1.0f, 0.0f, 0.0f);
		const IdCuerpo impactoPuerta = mundo.CuerpoAlFinalDelRayo(
			ojosFrentePuerta, miradaPuerta, glb::RADIO_INTERACCION);
		comprobar(habitacion.fisica->ObjetoAnimadoDeCuerpo(impactoPuerta) == 1 &&
			glb::BuscarObjetoInteractuable(anim, ojosFrentePuerta, miradaPuerta) == &anim.objetos[1],
			"el raycast visible y la consulta de puerta identifican el mismo objetivo");
		// Hoja cerrada: centro a 1.0 m del suelo (-1.7), entre los dos postes del marco, sobre el eje de la puerta.
		for (const CajaColision& h : hojasCerradas)
			comprobar(casi(h.pose.posicion.z, -1.7f + 1.009f, 0.01f) && casi(h.pose.posicion.x, 6.0f, 0.05f),
				"hoja cerrada: altura y plano correctos");

		// Se abre SOLO la puerta 1 durante 2 segundos.
		glb::AlternarPuerta(anim.objetos[0]);
		for (int i = 0; i < 120; ++i)
		{
			glb::ActualizarObjetos(anim, MundoFisico::PasoFijo);
			habitacion.fisica->SincronizarCinematicos(anim);
			mundo.Actualizar(MundoFisico::PasoFijo);
		}
		comprobar(anim.objetos[0].reproductor.tiempo > 1.0f && anim.objetos[1].reproductor.tiempo == 0.0f,
			"relojes independientes: solo se mueve la puerta 1");
		const std::vector<CajaColision> hojasAbiertas = cajasCinematicas();
		const bool primeraMovida = glm::length(hojasAbiertas[0].pose.posicion - hojasCerradas[0].pose.posicion) > 0.2f;
		const bool segundaQuieta = Cerca(hojasAbiertas[1].pose.posicion, hojasCerradas[1].pose.posicion, 1e-4f);
		const bool giraLaPrimera = glm::length(Gira(hojasAbiertas[0].pose, { 1, 0, 0 }) - Gira(hojasCerradas[0].pose, { 1, 0, 0 })) > 0.5f;
		// Las dos hojas estan guardadas en orden de creacion: puerta_1 primero.
		comprobar(primeraMovida && giraLaPrimera, "la hoja de la puerta 1 se desplaza y gira con la animacion");
		comprobar(segundaQuieta, "la hoja de la puerta 2 no se mueve");
		// La bisagra no se mueve: el borde de la hoja junto a la bisagra (x = 0 del nodo Door) queda en su sitio (hasta ~2 cm: la hoja tiene 3.4 cm de grosor),
		// mientras que el borde libre se desplaza varios decimetros.
		const auto bordeBisagra = [](const CajaColision& hoja) { return hoja.pose.posicion + Gira(hoja.pose, { hoja.mitadTamano.x, 0, 0 }); };
		const auto bordeLibre = [](const CajaColision& hoja) { return hoja.pose.posicion - Gira(hoja.pose, { hoja.mitadTamano.x, 0, 0 }); };
		comprobar(Cerca(bordeBisagra(hojasAbiertas[0]), bordeBisagra(hojasCerradas[0]), 0.03f), "la hoja gira alrededor de su bisagra");
		comprobar(glm::length(bordeLibre(hojasAbiertas[0]) - bordeLibre(hojasCerradas[0])) > 0.5f, "el borde libre se aparta de la posicion cerrada");
		// El marco (estatico) no ha cambiado ni un milimetro mientras la hoja giraba.
		const std::vector<glm::vec3> estaticasDespues = posicionesEstaticas();
		bool marcoIgual = estaticasAntes.size() == estaticasDespues.size();
		for (std::size_t i = 0; marcoIgual && i < estaticasAntes.size(); ++i)
			marcoIgual = Cerca(estaticasAntes[i], estaticasDespues[i], 1e-6f);
		comprobar(marcoIgual, "el marco permanece estatico mientras la hoja gira");
	}

	// Cambio de habitacion: sin residuos y sin tocar la demo; carga fallida sin efectos.
	void PruebaCambioDeHabitacion()
	{
		MundoFisico mundo;
		DemoCuboCaida demo(mundo);
		demo.Iniciar();
		comprobar(mundo.NumeroDeCuerpos() == 2, "la demo aporta 2 cuerpos");

		std::unique_ptr<EscenaMontada> actual = std::make_unique<EscenaMontada>(mundo, RutaDeEscena("habitacion_beta.json"));
		comprobar(actual->ok && mundo.NumeroDeCuerpos() == 22, "demo (2) + habitacion_beta (20) en el mismo mundo");

		// Intento de cargar una escena invalida: la habitacion activa debe seguir intacta.
		// (Un nodo inexistente en el colisionador se descubre al crear los cuerpos, a mitad del proceso.)
		const fs::path temp = fs::temp_directory_path() / "EscapeTheUAB-PhysicsTests-escena";
		fs::create_directories(temp);
		{
			std::string json = "{\"version\":1,\"name\":\"mala\",\"architecture\":[],\"objects\":["
				"{\"id\":\"a\",\"model\":\"" + (fs::path(ESCAPETHEUAB_SOURCE_DIR) / "EntornVGI/assets/models/test.obj").generic_string() +
				"\",\"position\":[0,0,0],\"rotation\":[0,0,0],\"scale\":[1,1,1],\"physics\":{\"type\":\"static\",\"halfSize\":[1,1,1]}}],"
				"\"animated\":[{\"id\":\"p\",\"model\":\"" + (fs::path(ESCAPETHEUAB_SOURCE_DIR) / "EntornVGI/assets/animations/Puerta.glb").generic_string() +
				"\",\"position\":[0,0,0],\"colliders\":[{\"node\":\"NodoQueNoExiste\",\"type\":\"static\",\"halfSize\":[1,1,1]}]}]}";
			std::ofstream(temp / "mala.json") << json;
		}
		const int antes = mundo.NumeroDeCuerpos();
		EscenaMontada mala(mundo, (temp / "mala.json").string());
		comprobar(!mala.ok, "una escena con un nodo inexistente falla");
		comprobar(mala.error.find("NodoQueNoExiste") != std::string::npos, "el error nombra el nodo que falta");
		comprobar(mundo.NumeroDeCuerpos() == antes, "la carga fallida no deja cuerpos a medio registrar (ni el objeto 'a')");
		comprobar(mala.fisica->NumeroDeCuerpos() == 0, "la EscenaFisica fallida queda vacia");
		comprobar(actual->fisica->NumeroDeCuerpos() == 20, "la habitacion activa sigue intacta");

		// Elegir la animacion por nombre: la que existe se acepta y la que no, no.
		{
			glb::EscenaAnimada una;
			std::string e;
			const std::string glbPuerta = (fs::path(ESCAPETHEUAB_SOURCE_DIR) / "EntornVGI/assets/animations/Puerta.glb").string();
			comprobar(glb::AnadirObjeto(una, "x", glbPuerta, glm::vec3(0.0f), 0.0f, e), "cargar la puerta para elegir animacion");
			const std::string nombreReal = una.objetos[0].modelo->animaciones[0].nombre;
			comprobar(glb::SeleccionarAnimacion(una.objetos[0], nombreReal, e), "elegir la animacion por su nombre real");
			comprobar(!glb::SeleccionarAnimacion(una.objetos[0], "otra", e) && e.find("otra") != std::string::npos,
				"una animacion inexistente da un error con su nombre");
		}

		// Escena con un animacion inexistente falla antes de crear nada.
		{
			std::string json = "{\"version\":1,\"name\":\"mala2\",\"architecture\":[],\"objects\":[],\"animated\":[{\"id\":\"p\",\"model\":\"" +
				(fs::path(ESCAPETHEUAB_SOURCE_DIR) / "EntornVGI/assets/animations/Puerta.glb").generic_string() +
				"\",\"position\":[0,0,0],\"animation\":\"NoExiste\"}]}";
			std::ofstream(temp / "mala2.json") << json;
		}
		EscenaMontada mala2(mundo, (temp / "mala2.json").string());
		comprobar(!mala2.ok && mala2.error.find("NoExiste") != std::string::npos, "animacion inexistente: error comprensible");

		// Cambio de habitacion como hace main.cpp: se crea la nueva y SOLO ENTONCES se vacia la anterior.
		EscenaMontada nueva(mundo, RutaDeEscena("ejemplo_fisica.json"));
		comprobar(nueva.ok, "ejemplo_fisica monta");
		const int cuerposNueva = nueva.fisica->NumeroDeCuerpos();
		comprobar(mundo.NumeroDeCuerpos() == 2 + 20 + cuerposNueva, "durante el cambio conviven las dos (aun no se ha vaciado la vieja)");
		actual->fisica->Vaciar();
		actual.reset();
		comprobar(mundo.NumeroDeCuerpos() == 2 + cuerposNueva, "tras el cambio solo quedan la demo y la habitacion nueva");
		comprobar(demo.EstaActiva() && mundo.NumeroDeCuerpos() > 0, "la demo no se ve afectada por el cambio de habitacion");

		// Reiniciar la demo no toca los cuerpos de la habitacion.
		demo.Iniciar();
		comprobar(mundo.NumeroDeCuerpos() == 2 + cuerposNueva, "reiniciar la demo no duplica ni borra cuerpos de la habitacion");
		demo.Detener();
		nueva.fisica->Vaciar();
		comprobar(mundo.NumeroDeCuerpos() == 0, "al vaciar todo no queda ningun cuerpo residual");

		// Las cajas dinamicas del ejemplo caen y terminan quietas sobre la mesa / el suelo.
		EscenaMontada ejemplo(mundo, RutaDeEscena("ejemplo_fisica.json"));
		Avanzar(mundo, 6.0f);
		int quietas = 0;
		for (const CajaColision& c : mundo.CajasDeColision())
			if (c.tipo == TipoCuerpo::Dinamico && c.pose.posicion.z > -1.8f && c.pose.posicion.z < 0.0f) ++quietas;
		comprobar(quietas == 3, "las tres cajas del ejemplo han caido y estan apoyadas");
		for (const CajaColision& c : mundo.CajasDeColision())
			if (c.tipo == TipoCuerpo::Dinamico) comprobar(c.dormido, "las cajas del ejemplo terminan en reposo");

		std::error_code ec;
		fs::remove_all(temp, ec);
	}
	// ======================================================================
	// Fase 3: controlador del jugador (cápsula de Bullet)
	// ======================================================================

	constexpr float SueloZ = -1.7f;   // cara superior del suelo de la habitación

	// Un suelo grande (cara superior en SueloZ).
	void CrearSuelo(MundoFisico& mundo) { mundo.CrearCajaEstatica({ 0, 0, SueloZ - 0.15f }, { 30, 30, 0.15f }); }

	// Una pared vertical: un plano a x = 'x' de 30 cm de grosor.
	void CrearParedX(MundoFisico& mundo, float x) { mundo.CrearCajaEstatica({ x + 0.15f, 0, 0 }, { 0.15f, 10, 2 }); }

	// El jugador anda con velocidad constante 'velocidad' durante 'segundos', en fotogramas de 'dt'.
	void Andar(MundoFisico& mundo, const glm::vec3& velocidad, float segundos, float dt)
	{
		mundo.FijarVelocidadJugador(velocidad);
		Simular(segundos, dt, [&](float paso) { mundo.Actualizar(paso); });
	}

	void PruebaJugadorSueloYGravedad()
	{
		MundoFisico mundo;
		CrearSuelo(mundo);
		comprobar(mundo.CrearJugador({ 0, 0, SueloZ + 1.0f }), "se crea el jugador");
		comprobar(!mundo.CrearJugador({ 0, 0, 0 }), "no se pueden crear dos jugadores");
		comprobar(mundo.HayJugador() && mundo.NumeroDeCuerpos() == 1, "el jugador no cuenta como cuerpo rigido");

		Avanzar(mundo, 0.1f);
		comprobar(mundo.PosicionPiesJugador().z < SueloZ + 1.0f - 0.01f, "con gravedad, el jugador empieza a caer");
		Avanzar(mundo, 2.0f);
		const glm::vec3 pies = mundo.PosicionPiesJugador();
		comprobar(casi(pies.z, SueloZ, 0.06f), "los pies se apoyan en el suelo");
		comprobar(mundo.JugadorEnSuelo(), "JugadorEnSuelo() es verdadero apoyado");
		comprobar(casi(pies.x, 0.0f, 1e-3f) && casi(pies.y, 0.0f, 1e-3f), "cae en vertical (sin deriva horizontal)");

		// Sin pedir movimiento, se queda quieto indefinidamente (no se hunde ni vibra).
		const float alturaAntes = pies.z;
		Avanzar(mundo, 3.0f);
		comprobar(casi(mundo.PosicionPiesJugador().z, alturaAntes, 0.005f), "quieto sobre el suelo no se hunde ni vibra");
	}

	void PruebaJugadorAndaLibre()
	{
		MundoFisico mundo;
		CrearSuelo(mundo);
		mundo.CrearJugador({ 0, 0, SueloZ + 0.02f });
		Avanzar(mundo, 0.5f);
		Andar(mundo, { 3, 0, 0 }, 2.0f, MundoFisico::PasoFijo);
		// 3 m/s * 2 s = 6 m (sobre suelo llano la distancia es la esperada).
		comprobar(casi(mundo.PosicionPiesJugador().x, 6.0f, 0.1f), "sin obstaculos, anda a 3 m/s");
		comprobar(casi(mundo.PosicionPiesJugador().z, SueloZ, 0.06f), "y se mantiene sobre el suelo");
	}

	void PruebaJugadorChocaConPared()
	{
		MundoFisico mundo;
		CrearSuelo(mundo);
		CrearParedX(mundo, 5.0f);   // cara en x = 5
		mundo.CrearJugador({ 3, 0, SueloZ + 0.02f });
		Avanzar(mundo, 0.5f);
		Andar(mundo, { 3, 0, 0 }, 3.0f, MundoFisico::PasoFijo);
		const float x = mundo.PosicionPiesJugador().x;
		// Se detiene con la cápsula (radio 0.3) pegada a la pared: x ~ 4.7.
		comprobar(x < 5.0f - MedidasJugador::Radio + 0.03f, "no atraviesa la pared");
		comprobar(x > 5.0f - MedidasJugador::Radio - 0.15f, "se detiene junto a la pared (no queda lejos)");
		comprobar(casi(mundo.PosicionPiesJugador().z, SueloZ, 0.06f), "chocar no lo levanta ni lo hunde");

		// Seguir empujando mucho tiempo no lo hace "entrar" poco a poco.
		Andar(mundo, { 3, 0, 0 }, 5.0f, MundoFisico::PasoFijo);
		comprobar(mundo.PosicionPiesJugador().x < 5.0f - MedidasJugador::Radio + 0.03f, "seguir empujando no atraviesa");
	}

	void PruebaJugadorSeAgachaYRespetaTecho()
	{
		MundoFisico mundo;
		CrearSuelo(mundo);
		mundo.CrearJugador({ 0, 0, SueloZ + 0.02f });
		Avanzar(mundo, 1.0f);

		const glm::vec3 piesDePie = mundo.PosicionPiesJugador();
		comprobar(mundo.ActualizarJugadorAgachado(true, 0.1f), "el jugador empieza a agacharse");
		comprobar(mundo.AlturaJugador() < MedidasJugador::Altura &&
			mundo.AlturaJugador() > MedidasJugador::AlturaAgachado,
			"la altura cambia gradualmente en lugar de instantaneamente");
		comprobar(mundo.AlturaOjosJugador() < MedidasJugador::AlturaOjos,
			"la altura de la camara tambien cambia gradualmente");
		for (int i = 0; i < 10; ++i)
			mundo.ActualizarJugadorAgachado(true, 0.1f);
		comprobar(mundo.JugadorAgachado(), "el estado del jugador indica que esta agachado");
		comprobar(casi(mundo.AlturaJugador(), MedidasJugador::AlturaAgachado, 1e-3f),
			"la transicion termina en la altura agachada configurada");
		comprobar(casi(mundo.PosicionPiesJugador().z, piesDePie.z, 1e-3f),
			"agacharse mantiene los pies en el suelo");

		// El techo deja espacio para la cápsula agachada, pero no para la de pie.
		const float techoInferior = SueloZ + MedidasJugador::AlturaAgachado + 0.15f;
		mundo.CrearCajaEstatica({ 0, 0, techoInferior + 0.1f }, { 2, 2, 0.1f });
		bool puedeSeguirLevantandose = true;
		for (int i = 0; i < 10 && puedeSeguirLevantandose; ++i)
			puedeSeguirLevantandose = mundo.ActualizarJugadorAgachado(false, 0.1f);
		comprobar(!puedeSeguirLevantandose, "el techo impide seguir levantandose");
		comprobar(mundo.JugadorAgachado(), "el jugador sigue agachado bajo el techo");

		MundoFisico espacioAbierto;
		CrearSuelo(espacioAbierto);
		espacioAbierto.CrearJugador({ 0, 0, SueloZ + 0.02f });
		Avanzar(espacioAbierto, 1.0f);
		for (int i = 0; i < 10; ++i)
			espacioAbierto.ActualizarJugadorAgachado(true, 0.1f);
		bool puedeLevantarse = true;
		for (int i = 0; i < 10 && puedeLevantarse; ++i)
			puedeLevantarse = espacioAbierto.ActualizarJugadorAgachado(false, 0.1f);
		comprobar(puedeLevantarse, "se puede levantar sin obstaculos");
		comprobar(!espacioAbierto.JugadorAgachado(), "el estado vuelve a estar de pie");
	}

	// Agachado suave: curva de Hermite (despacio al empezar y al llegar), pies fijos, sin hundirse ni saltar.
	void PruebaAgachadoSuaveYEstable()
	{
		MundoFisico mundo;
		CrearSuelo(mundo);
		mundo.CrearJugador({ 0, 0, SueloZ + 0.02f });
		Avanzar(mundo, 1.0f);
		const float piesZ = mundo.PosicionPiesJugador().z;
		comprobar(mundo.JugadorEnSuelo(), "de pie: en el suelo");

		// Con delta 0 (pausa) no cambia nada.
		mundo.ActualizarJugadorAgachado(true, 0.0f);
		comprobar(!mundo.JugadorAgachado() && mundo.FraccionAgachado() == 0.0f, "delta 0: la postura se congela");

		// Bajar: la altura es monotona, el primer y el ultimo tramo avanzan menos que el central.
		std::vector<float> alturas;
		for (int i = 0; i < 40; ++i)
		{
			mundo.ActualizarJugadorAgachado(true, MundoFisico::PasoFijo);
			mundo.Actualizar(MundoFisico::PasoFijo);
			alturas.push_back(mundo.AlturaJugador());
			comprobar(std::abs(mundo.PosicionPiesJugador().z - piesZ) < 0.01f, "los pies no se mueven al cambiar de postura");
		}
		bool monotona = true;
		for (std::size_t i = 1; i < alturas.size(); ++i) monotona = monotona && alturas[i] <= alturas[i - 1] + 1e-6f;
		comprobar(monotona, "la altura baja siempre, sin rebotes");
		const float primerPaso = MedidasJugador::Altura - alturas[0];
		const float pasoCentral = alturas[19] - alturas[20];
		comprobar(primerPaso < pasoCentral * 0.3f, "el arranque es mas lento que el tramo central (curva suave)");
		comprobar(mundo.JugadorEnSuelo(), "agachando sigue apoyado en el suelo");

		// Subir con el techo libre hasta de pie: misma altura de pies, y alturas de ojos coherentes.
		for (int i = 0; i < 60; ++i)
		{
			mundo.ActualizarJugadorAgachado(false, MundoFisico::PasoFijo);
			mundo.Actualizar(MundoFisico::PasoFijo);
		}
		comprobar(!mundo.JugadorAgachado() && casi(mundo.AlturaJugador(), MedidasJugador::Altura, 1e-3f), "vuelve a la altura de pie");
		comprobar(std::abs(mundo.PosicionPiesJugador().z - piesZ) < 0.01f, "tras agacharse y levantarse los pies estan donde estaban");
		comprobar(casi(mundo.AlturaOjosJugador(), MedidasJugador::AlturaOjos, 1e-3f), "los ojos vuelven a 1,70 m");
	}

	// Cambio de postura andando contra un muro y bajo un techo: nunca atraviesa colisionadores.
	void PruebaAgachadoAndandoYTecho()
	{
		MundoFisico mundo;
		CrearSuelo(mundo);
		CrearParedX(mundo, 5.0f);
		// Techo bajo (1,3 m) en x > 2: de pie (1,8 m) no cabe, agachado (1,0 m) si.
		mundo.CrearCajaEstatica({ 3.5f, 0, SueloZ + 1.3f + 0.1f }, { 1.5f, 3, 0.1f });
		mundo.CrearJugador({ 0, 0, SueloZ + 0.02f });
		Avanzar(mundo, 0.5f);

		for (int i = 0; i < 40; ++i)
		{
			mundo.ActualizarJugadorAgachado(true, MundoFisico::PasoFijo);
			mundo.FijarVelocidadJugador({ 1.4f, 0, 0 });
			mundo.Actualizar(MundoFisico::PasoFijo);
		}
		Andar(mundo, { 1.4f, 0, 0 }, 3.0f, MundoFisico::PasoFijo);
		comprobar(mundo.PosicionPiesJugador().x > 2.5f, "agachado pasa bajo el techo bajo");
		comprobar(mundo.PosicionPiesJugador().x < 5.0f - MedidasJugador::Radio + 0.03f, "agachado no atraviesa la pared");

		// Soltar Ctrl bajo el techo: no se levanta.
		bool sigue = true;
		for (int i = 0; i < 60; ++i)
		{
			sigue = mundo.ActualizarJugadorAgachado(false, MundoFisico::PasoFijo) && sigue;
			mundo.Actualizar(MundoFisico::PasoFijo);
		}
		comprobar(!sigue && mundo.JugadorAgachado(), "bajo el techo bajo se queda agachado");
		comprobar(mundo.FraccionAgachado() > 0.5f, "la fraccion de agachado lo refleja");

		// Sale a campo abierto y entonces se levanta.
		Andar(mundo, { -1.4f, 0, 0 }, 3.0f, MundoFisico::PasoFijo);
		for (int i = 0; i < 60; ++i)
		{
			mundo.ActualizarJugadorAgachado(false, MundoFisico::PasoFijo);
			mundo.Actualizar(MundoFisico::PasoFijo);
		}
		comprobar(!mundo.JugadorAgachado(), "fuera del techo se levanta");
	}

	// El mismo agachado a 30, 60 y 120 FPS acaba en la misma altura en el mismo tiempo.
	void PruebaAgachadoIndependienteDeLosFps()
	{
		float finales[3];
		int k = 0;
		for (int fps : { 30, 60, 120 })
		{
			MundoFisico mundo;
			CrearSuelo(mundo);
			mundo.CrearJugador({ 0, 0, SueloZ + 0.02f });
			Avanzar(mundo, 0.5f);
			for (int i = 0; i < fps / 5; ++i)   // 0,2 s
				mundo.ActualizarJugadorAgachado(true, 1.0f / fps);
			finales[k++] = mundo.AlturaJugador();
		}
		comprobar(casi(finales[0], finales[1], 2e-3f) && casi(finales[1], finales[2], 2e-3f), "el agachado no depende de los FPS");
	}

	void PruebaJugadorSeDesliza()
	{
		MundoFisico mundo;
		CrearSuelo(mundo);
		CrearParedX(mundo, 5.0f);
		mundo.CrearJugador({ 3, 0, SueloZ + 0.02f });
		Avanzar(mundo, 0.5f);
		// Diagonal contra la pared: la componente X se anula, la Y debe continuar (deslizar).
		Andar(mundo, { 2.1f, 2.1f, 0 }, 2.0f, MundoFisico::PasoFijo);
		const glm::vec3 pies = mundo.PosicionPiesJugador();
		comprobar(pies.x < 5.0f - MedidasJugador::Radio + 0.03f, "diagonal: no atraviesa la pared");
		comprobar(pies.y > 2.5f, "diagonal: sigue avanzando a lo largo de la pared (desliza)");
		comprobar(pies.y < 2.1f * 2.0f + 0.1f, "diagonal: no avanza mas de lo que pide");
	}

	void PruebaJugadorChocaConObjetoEstatico()
	{
		MundoFisico mundo;
		CrearSuelo(mundo);
		// Una mesa/armario: caja de 1 x 1 x 1 m con la base en el suelo, centrada en (5, 0).
		mundo.CrearCajaEstatica({ 5, 0, SueloZ + 0.5f }, { 0.5f, 0.5f, 0.5f });
		mundo.CrearJugador({ 2, 0, SueloZ + 0.02f });
		Avanzar(mundo, 0.5f);
		Andar(mundo, { 3, 0, 0 }, 3.0f, MundoFisico::PasoFijo);
		comprobar(mundo.PosicionPiesJugador().x < 4.5f - MedidasJugador::Radio + 0.03f, "no atraviesa un objeto estatico");
		comprobar(mundo.PosicionPiesJugador().z < SueloZ + 0.1f, "y no trepa encima (altura de escalon limitada)");

		// Rodearlo: yendo en diagonal acaba pasando por un lado.
		Andar(mundo, { 0, 3, 0 }, 1.0f, MundoFisico::PasoFijo);
		Andar(mundo, { 3, 0, 0 }, 2.0f, MundoFisico::PasoFijo);
		comprobar(mundo.PosicionPiesJugador().x > 6.0f, "se puede rodear el objeto");
	}

	// Mismo recorrido con distintos FPS: mismo resultado y nunca atraviesa.
	void PruebaJugadorIndependienteDeLosFps()
	{
		float finalX[4];
		const float dts[4] = { 1.0f / 144.0f, 1.0f / 60.0f, 1.0f / 15.0f, 0.25f };   // 0.25 s = fotograma de 4 FPS
		for (int i = 0; i < 4; ++i)
		{
			MundoFisico mundo;
			CrearSuelo(mundo);
			CrearParedX(mundo, 5.0f);
			mundo.CrearJugador({ 3, 0, SueloZ + 0.02f });
			Avanzar(mundo, 0.5f);
			Andar(mundo, { 3, 0, 0 }, 4.0f, dts[i]);
			finalX[i] = mundo.PosicionPiesJugador().x;
			comprobar(finalX[i] < 5.0f - MedidasJugador::Radio + 0.03f, "a cualquier FPS no atraviesa la pared");
		}
		comprobar(casi(finalX[0], finalX[3], 0.03f) && casi(finalX[1], finalX[2], 0.03f) && casi(finalX[0], finalX[1], 0.03f),
			"el punto donde se detiene no depende de los FPS");

		// Y en movimiento libre recorre la misma distancia (salvo el redondeo de fotogramas).
		MundoFisico lento, rapido;
		for (MundoFisico* m : { &lento, &rapido }) { CrearSuelo(*m); m->CrearJugador({ 0, 0, SueloZ + 0.02f }); Avanzar(*m, 0.5f); }
		Andar(lento, { 3, 0, 0 }, 2.0f, 0.1f);
		Andar(rapido, { 3, 0, 0 }, 2.0f, 1.0f / 144.0f);
		comprobar(casi(lento.PosicionPiesJugador().x, rapido.PosicionPiesJugador().x, 0.1f), "misma distancia a 10 FPS y a 144 FPS");

		// Un tirón enorme (10 s de fotograma) se recorta: ni se teletransporta ni atraviesa.
		MundoFisico tiron;
		CrearSuelo(tiron);
		CrearParedX(tiron, 5.0f);
		tiron.CrearJugador({ 4.0f, 0, SueloZ + 0.02f });
		Avanzar(tiron, 0.5f);
		tiron.FijarVelocidadJugador({ 3, 0, 0 });
		tiron.Actualizar(10.0f);
		comprobar(tiron.PosicionPiesJugador().x < 5.0f - MedidasJugador::Radio + 0.03f, "un fotograma de 10 s no lo lanza a traves de la pared");
	}

	void PruebaJugadorPausa()
	{
		MundoFisico mundo;
		CrearSuelo(mundo);
		mundo.CrearJugador({ 0, 0, SueloZ + 1.0f });   // en el aire: cae
		Avanzar(mundo, 0.1f);
		mundo.FijarPausa(true);
		const glm::vec3 enPausa = mundo.PosicionPiesJugador();
		mundo.FijarVelocidadJugador({ 3, 0, 0 });
		Simular(2.0f, 0.016f, [&](float dt) { mundo.Actualizar(dt); });
		comprobar(mundo.PosicionPiesJugador() == enPausa, "en pausa el jugador no se mueve ni cae");
		mundo.FijarPausa(false);
		Avanzar(mundo, 0.2f);
		comprobar(mundo.PosicionPiesJugador().x > enPausa.x + 0.3f && mundo.PosicionPiesJugador().z < enPausa.z, "al reanudar anda y cae");
		// Teletransporte: coloca al jugador y resetea la caida.
		mundo.TeletransportarJugador({ 7, 8, SueloZ + 0.02f });
		mundo.FijarVelocidadJugador({ 0, 0, 0 });
		Avanzar(mundo, 0.5f);
		comprobar(Cerca(mundo.PosicionPiesJugador(), { 7, 8, SueloZ }, 0.08f), "TeletransportarJugador lo coloca donde se pide");
		mundo.EliminarJugador();
		comprobar(!mundo.HayJugador(), "EliminarJugador lo quita");
	}

	// Recorre la puerta 'puerta_2' de habitacion_beta en el eje X (hueco en x = 6, y = 3). (La puerta_1 queda
	// medio tapada por table_right, asi que se usa la 2, que esta despejada.)
	float AndarPorLaPuerta(EscenaMontada& habitacion, MundoFisico& mundo, bool abierta, float dt)
	{
		glb::EscenaAnimada& anim = habitacion.animados;
		if (abierta)
		{
			glb::AlternarPuerta(anim.objetos[1]);
			for (int i = 0; i < 300; ++i)   // 5 s: de sobra para abrirse del todo
			{
				glb::ActualizarObjetos(anim, MundoFisico::PasoFijo);
				habitacion.fisica->SincronizarCinematicos(anim);
				mundo.Actualizar(MundoFisico::PasoFijo);
			}
		}
		mundo.CrearJugador({ 4.5f, abierta ? 3.04f : 3.0f, SueloZ + 0.02f });   // abierta: la hoja ocupa el lado y<2.68, se pasa por el centro del hueco libre
		Avanzar(mundo, 0.5f);
		mundo.FijarVelocidadJugador({ 3, 0, 0 });
		Simular(3.0f, dt, [&](float paso) {
			habitacion.fisica->SincronizarCinematicos(anim);
			mundo.Actualizar(paso);
		});
		return mundo.PosicionPiesJugador().x;
	}

	void PruebaJugadorYPuertas()
	{
		// Puerta CERRADA: la hoja bloquea el paso.
		{
			MundoFisico mundo;
			EscenaMontada habitacion(mundo, RutaDeEscena("habitacion_beta.json"));
			const float x = AndarPorLaPuerta(habitacion, mundo, false, MundoFisico::PasoFijo);
			comprobar(x < 6.0f - MedidasJugador::Radio + 0.05f, "puerta cerrada: el jugador no la atraviesa");
			comprobar(x > 5.0f, "puerta cerrada: llega hasta ella");
		}
		// Puerta ABIERTA: pasa por el hueco con holgura.
		{
			MundoFisico mundo;
			EscenaMontada habitacion(mundo, RutaDeEscena("habitacion_beta.json"));
			const float x = AndarPorLaPuerta(habitacion, mundo, true, MundoFisico::PasoFijo);
			comprobar(x > 7.5f, "puerta abierta: el jugador cruza el hueco");
		}
		// Lo mismo con 8 FPS: la hoja cerrada tampoco se atraviesa.
		{
			MundoFisico mundo;
			EscenaMontada habitacion(mundo, RutaDeEscena("habitacion_beta.json"));
			const float x = AndarPorLaPuerta(habitacion, mundo, false, 0.125f);
			comprobar(x < 6.0f - MedidasJugador::Radio + 0.05f, "puerta cerrada a 8 FPS: tampoco la atraviesa");
		}
		// Una puerta que se CIERRA sobre el jugador lo empuja con suavidad: sin saltos bruscos.
		{
			MundoFisico mundo;
			EscenaMontada habitacion(mundo, RutaDeEscena("habitacion_beta.json"));
			glb::EscenaAnimada& anim = habitacion.animados;
			glb::AlternarPuerta(anim.objetos[1]);
			for (int i = 0; i < 300; ++i) { glb::ActualizarObjetos(anim, MundoFisico::PasoFijo); habitacion.fisica->SincronizarCinematicos(anim); mundo.Actualizar(MundoFisico::PasoFijo); }
			// La puerta abierta queda tumbada hacia -X (x de 5.14 a 5.98, y ~ 2.6). Al cerrarse, la hoja
			// barre el cuarto de circulo que hay delante: ponemos al jugador en medio de ese recorrido.
			mundo.CrearJugador({ 5.6f, 3.1f, SueloZ + 0.02f });
			Avanzar(mundo, 0.5f);
			const glm::vec3 inicio = mundo.PosicionPiesJugador();
			comprobar(Cerca(inicio, { 5.6f, 3.1f, SueloZ }, 0.08f), "el jugador esta en el recorrido de la hoja y la puerta abierta no lo mueve");
			glb::AlternarPuerta(anim.objetos[1]);   // cerrar
			glm::vec3 anterior = mundo.PosicionPiesJugador();
			float saltoMaximo = 0.0f;
			for (int i = 0; i < 300; ++i)
			{
				glb::ActualizarObjetos(anim, MundoFisico::PasoFijo);
				habitacion.fisica->SincronizarCinematicos(anim);
				mundo.Actualizar(MundoFisico::PasoFijo);
				const glm::vec3 ahora = mundo.PosicionPiesJugador();
				saltoMaximo = std::max(saltoMaximo, glm::length(ahora - anterior));
				anterior = ahora;
			}
			// Al terminar, la hoja cerrada (x 5.973..6.053, y 2.6..3.4) y la capsula no se solapan.
			const float fueraX = std::max({ 5.973f - anterior.x, 0.0f, anterior.x - 6.053f });
			const float fueraY = std::max({ 2.6f - anterior.y, 0.0f, anterior.y - 3.4f });
			comprobar(std::hypot(fueraX, fueraY) > MedidasJugador::Radio - 0.04f, "la puerta que se cierra saca al jugador del volumen de la hoja");
			comprobar(glm::length(glm::vec3(anterior.x - inicio.x, anterior.y - inicio.y, 0.0f)) > 0.05f, "...porque efectivamente lo empuja");
			// Mientras la hoja barre, el empujon es de 2-5 cm por paso (a ~1.5-3 m/s). Al final, cuando el jugador
			// queda apretado entre la hoja y el poste, hay un unico pico (~11 cm medidos). Se exige que siempre
			// sea menor que el radio de la capsula (25 cm): nunca se salta paredes ni "se teletransporta".
			comprobar(saltoMaximo < MedidasJugador::Radio * 0.6f, "la puerta que se cierra no teletransporta al jugador (salto por paso < 15 cm)");
			comprobar(std::isfinite(anterior.x) && casi(anterior.z, SueloZ, 0.1f), "tras cerrarse, el jugador sigue en el suelo y en una posicion valida");
			std::cout << "  (info) cierre sobre el jugador: salto maximo " << saltoMaximo << " m, posicion final "
				<< anterior.x << ", " << anterior.y << "\n";
		}
	}

	// habitacion_beta completa: un recorrido hacia las paredes y objetos reales.
	void PruebaJugadorEnHabitacionBeta()
	{
		const float limitesX[2] = { -3.0f + 0.15f, 13.0f - 0.15f };   // caras interiores de las paredes laterales
		MundoFisico mundo;
		EscenaMontada habitacion(mundo, RutaDeEscena("habitacion_beta.json"));
		mundo.CrearJugador({ 10.0f, 0.0f, SueloZ + 0.02f });   // posicion de inicio del juego
		Avanzar(mundo, 0.5f);
		comprobar(mundo.JugadorEnSuelo(), "habitacion_beta: el jugador empieza apoyado en el suelo");
		Andar(mundo, { 3, 0, 0 }, 3.0f, 1.0f / 60.0f);
		comprobar(mundo.PosicionPiesJugador().x < limitesX[1] - MedidasJugador::Radio + 0.03f, "habitacion_beta: la pared derecha lo detiene");
		Andar(mundo, { 0, 3, 0 }, 4.0f, 1.0f / 60.0f);
		comprobar(mundo.PosicionPiesJugador().y < 5.0f - 0.15f - MedidasJugador::Radio + 0.03f, "habitacion_beta: la pared delantera lo detiene");
		// Hacia el armario (-1, 3.3): lo cruza en diagonal sin atravesarlo.
		Andar(mundo, { -3, 0, 0 }, 8.0f, 1.0f / 60.0f);
		const glm::vec3 pies = mundo.PosicionPiesJugador();
		comprobar(pies.x > -3.0f + 0.15f + MedidasJugador::Radio - 0.03f, "habitacion_beta: la pared izquierda lo detiene");
		comprobar(casi(pies.z, SueloZ, 0.1f), "habitacion_beta: sigue en el suelo tras el recorrido");
		std::cout << "  (info) habitacion_beta: posicion final tras el recorrido " << pies.x << ", " << pies.y << "\n";
	}
	// ======================================================================
	// Integracion VR/master: vista estereoscopica de prueba y agarre con Bullet
	// ======================================================================

	// Vista estereo (solo matematica: el dibujo en pantalla se comprueba a mano).
	void PruebaVistaEstereo()
	{
		const glm::mat4 central = glm::lookAt(glm::vec3(3, 2, 1), glm::vec3(8, 2, 0.5f), glm::vec3(0, 0, 1));
		const glm::mat4 izq = estereo::VistaDeOjo(central, estereo::Ojo::Izquierdo);
		const glm::mat4 der = estereo::VistaDeOjo(central, estereo::Ojo::Derecho);
		const glm::mat4 invC = glm::inverse(central), invI = glm::inverse(izq), invD = glm::inverse(der);
		const glm::vec3 derechaCamara = glm::normalize(glm::vec3(invC[0]));

		comprobar(Cerca(glm::vec3(invI[3]), glm::vec3(invC[3]) - derechaCamara * 0.032f, 1e-4f), "ojo izquierdo: 3,2 cm a la izquierda del centro");
		comprobar(Cerca(glm::vec3(invD[3]), glm::vec3(invC[3]) + derechaCamara * 0.032f, 1e-4f), "ojo derecho: 3,2 cm a la derecha del centro");
		comprobar(casi(glm::distance(glm::vec3(invI[3]), glm::vec3(invD[3])), 0.064f, 1e-4f), "separacion entre ojos de 6,4 cm");
		comprobar(Cerca(glm::vec3(invI[2]), glm::vec3(invC[2]), 1e-5f) && Cerca(glm::vec3(invD[2]), glm::vec3(invC[2]), 1e-5f),
			"ambos ojos miran en la misma direccion que la camara (el estereo no gira la cabeza)");
		comprobar(Cerca(glm::vec3(estereo::VistaDeOjo(central, estereo::Ojo::Izquierdo, 0.0f)[3]), glm::vec3(central[3]), 1e-6f),
			"con separacion 0 la vista del ojo es la central");
		comprobar(estereo::MitadDePantalla(estereo::Ojo::Izquierdo, 1280).x == 0 && estereo::MitadDePantalla(estereo::Ojo::Derecho, 1280).x == 640 &&
			estereo::MitadDePantalla(estereo::Ojo::Derecho, 1280).ancho == 640, "cada ojo ocupa una mitad de la pantalla");
	}

	// Indice de un objeto de la escena por su id (-1 si no esta).
	int IndiceDe(const EscenaMontada& e, const char* id)
	{
		for (std::size_t i = 0; i < e.datos.objetos.size(); ++i) if (e.datos.objetos[i].id == id) return static_cast<int>(i);
		return -1;
	}

	void PruebaAgarreConBullet()
	{
		MundoFisico mundo;
		EscenaMontada sala(mundo, RutaDeEscena("habitacion_beta.json"));
		comprobar(sala.ok, "habitacion_beta monta para las pruebas de agarre");
		if (!sala.ok) return;
		const int grande = IndiceDe(sala, "box_large");
		const int pared = IndiceDe(sala, "wall_front");
		comprobar(grande >= 0 && sala.fisica->NumeroDeDinamicos() == 2, "box_large y box_small son los objetos agarrables");
		const int cuerposIniciales = mundo.NumeroDeCuerpos();

		// Jugador a 1,2 m de la caja; los ojos quedan a 1,7 m sobre sus pies.
		mundo.CrearJugador({ 5.8f, 2.0f, SueloZ + 0.02f });
		Avanzar(mundo, 1.0f);
		const glm::vec3 ojos = mundo.PosicionPiesJugador() + glm::vec3(0, 0, MedidasJugador::AlturaOjos);
		const glm::vec3 haciaCaja = glm::vec3(7, 2, -1.3f) - ojos;

		// 1) Seleccion: solo lo agarrable, y solo si se ve de verdad.
		comprobar(sala.fisica->IntentarAgarrar(ojos, glm::vec3(-1, 0, -0.3f)) == -1, "mirando una pared/vacio no se agarra nada");
		comprobar(sala.fisica->IntentarAgarrar(ojos, glm::vec3(0, 0, -1)) == -1, "mirando al suelo (estatico) no se agarra nada");
		comprobar(!sala.fisica->HayAgarre(), "tras los intentos fallidos la mano sigue vacia");
		comprobar(sala.fisica->IntentarAgarrar(glm::vec3(5.8f, 2.0f, -1.2f) + glm::vec3(0, 0, 0), glm::vec3(0, 1, 0)) == -1,
			"mirando una mesa/mueble estatico o una puerta no se agarra nada");
		comprobar(sala.fisica->IntentarAgarrar(ojos, haciaCaja * 3.0f) == grande && sala.fisica->HayAgarre() && sala.fisica->ObjetoAgarrado() == grande,
			"mirando box_large a 1,8 m se agarra");
		comprobar(mundo.NumeroDeCuerpos() == cuerposIniciales, "agarrar no crea ni borra cuerpos");
		sala.fisica->SoltarAgarre();

		// Alcance: desde 4 m no llega.
		const glm::vec3 lejos = glm::vec3(3.0f, 2.0f, -0.3f);
		comprobar(sala.fisica->IntentarAgarrar(lejos, glm::vec3(7, 2, -1.3f) - lejos) == -1, "fuera de alcance (2 m) no se agarra");

		// 2) Agarre: la caja SIGUE a la mano siendo el mismo cuerpo de Bullet, sin gravedad.
		comprobar(sala.fisica->IntentarAgarrar(ojos, haciaCaja) == grande, "agarrar box_large");
		const glm::vec3 mirada(1, 0, 0);
		for (int i = 0; i < 90; ++i) { sala.fisica->ActualizarAgarre(ojos, mirada); mundo.Actualizar(MundoFisico::PasoFijo); }
		const IdCuerpo cuerpo = mundo.CuerpoSujeto();
		const glm::vec3 enLaMano = mundo.PoseDe(cuerpo).posicion;
		comprobar(Cerca(enLaMano, mundo.DestinoDeSujecion(), 0.05f), "la caja llega al punto de la mano");
		comprobar(enLaMano.z > -1.0f, "sujeta no cae por la gravedad (esta mas alta que en el suelo)");
		const glm::mat4 dibujo = sala.fisica->MatrizVisualDe(grande, sala.datos.objetos[grande]);
		comprobar(Cerca(glm::vec3(dibujo[3]), enLaMano, 0.01f), "lo que se dibuja es exactamente la posicion de Bullet");
		// El objeto de la mano no empuja al jugador (la capsula lo ve como obstaculo, pero esta a >0,6 m).
		comprobar(Cerca(mundo.PosicionPiesJugador(), glm::vec3(5.8f, 2.0f, SueloZ), 0.1f), "llevar un objeto no desplaza al jugador");

		// 3) Pausa: mundo congelado, el agarre no se pierde.
		mundo.FijarPausa(true);
		const glm::vec3 enPausa = mundo.PoseDe(cuerpo).posicion;
		for (int i = 0; i < 30; ++i) { sala.fisica->ActualizarAgarre(ojos, glm::vec3(0, 1, 0)); mundo.Actualizar(MundoFisico::PasoFijo); }
		comprobar(mundo.PoseDe(cuerpo).posicion == enPausa && sala.fisica->HayAgarre(), "en pausa el objeto no se mueve y sigue en la mano");
		mundo.FijarPausa(false);

		// 4) Soltar: recupera la gravedad y cae al suelo, quieto y sin atravesarlo.
		sala.fisica->SoltarAgarre();
		comprobar(!sala.fisica->HayAgarre() && mundo.CuerpoSujeto() == CuerpoInvalido, "soltar vacia la mano");
		Avanzar(mundo, 4.0f);
		const glm::vec3 reposo = mundo.PoseDe(cuerpo).posicion;
		comprobar(casi(reposo.z, SueloZ + 0.4f, 0.03f), "soltada, la caja cae y se apoya en el suelo (recupera la gravedad)");
		comprobar(mundo.EstaDormido(cuerpo) || glm::length(mundo.VelocidadDe(cuerpo)) < 0.05f, "y se queda quieta");
		comprobar(mundo.NumeroDeCuerpos() == cuerposIniciales, "ni duplicados ni perdidos tras agarrar y soltar");
		(void)pared;
	}

	// La mano atraviesa una pared: el objeto se queda en este lado (Bullet lo para), nunca la cruza.
	void PruebaAgarreContraPared()
	{
		MundoFisico mundo;
		EscenaFisica escena(mundo);
		std::string error;
		CrearSuelo(mundo);   // fuera de la escena: suelo propio
		const SceneObject pared = ObjetoCaja("pared", { 5.15f, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }, TipoCuerpo::Estatico, { 0.15f, 5, 2 }, { 0, 0, 0 }, 0.0f);
		const SceneObject caja = ObjetoCaja("caja", { 3.5f, 0, SueloZ + 0.2f }, { 0, 0, 0 }, { 0.2f, 0.2f, 0.2f }, TipoCuerpo::Dinamico, { 1, 1, 1 }, { 0, 0, 0 }, 2.0f);
		comprobar(Crear(escena, { pared, caja }, error), "crear pared y caja");
		Avanzar(mundo, 0.5f);
		const glm::vec3 ojos(2.5f, 0, SueloZ + 0.2f);   // ojos a media altura de la caja, solo para el rayo
		comprobar(escena.IntentarAgarrar(ojos, glm::vec3(1, 0, 0)) == 1, "la caja se agarra a 1 m");
		// La mano se mueve 3 m a traves de la pared (hasta x = 6.5): Bullet debe frenar la caja.
		const glm::vec3 manoDetrasDeLaPared(6.5f, 0, SueloZ + 0.2f);
		mundo.FijarDestinoDeSujecion(manoDetrasDeLaPared);
		float xMax = 0.0f;
		for (int i = 0; i < 120; ++i)
		{
			mundo.FijarDestinoDeSujecion(manoDetrasDeLaPared);
			mundo.Actualizar(MundoFisico::PasoFijo);
			xMax = std::max(xMax, mundo.PoseDe(mundo.CuerpoSujeto() == CuerpoInvalido ? 2 : mundo.CuerpoSujeto()).posicion.x);
		}
		comprobar(mundo.CuerpoSujeto() != CuerpoInvalido, "la caja sigue sujeta (el auto-soltado lo decide ActualizarAgarre)");
		const IdCuerpo cuerpo = mundo.CuerpoSujeto();
		const float x = mundo.PoseDe(cuerpo).posicion.x;
		// Cara de la pared: x = 5.0. La caja mide 0,4 (mitad 0,2): su centro no puede pasar de 4,8 (+ margen de contacto).
		comprobar(x < 5.0f - 0.2f + 0.05f, "la caja contra la pared no la atraviesa");
		std::cout << "  (info) agarre contra pared: x maximo del centro " << xMax << " (cara de la pared en 5.0, mitad de la caja 0.2)\n";
		comprobar(xMax < 4.95f, "ni siquiera un instante: la penetracion maxima es de pocos centimetros");

		// ActualizarAgarre suelta el objeto atascado cuando la mano queda a mas de 2 m.
		escena.ActualizarAgarre(glm::vec3(6.5f, 0, SueloZ + 0.2f), glm::vec3(1, 0, 0));   // mano a ~7,6 m: a 2,8 m de la caja, mas de 2 m
		comprobar(!escena.HayAgarre(), "un objeto atascado lejos de la mano se suelta solo");
	}

	void PruebaSeleccionInteraccionConObstruccion()
	{
		MundoFisico mundo;
		EscenaFisica escena(mundo);
		std::string error;
		const SceneObject pared = ObjetoCaja("pared", { 4.0f, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 },
			TipoCuerpo::Estatico, { 0.15f, 2, 2 }, { 0, 0, 0 }, 0.0f);
		const SceneObject caja = ObjetoCaja("caja", { 6.0f, 0, 0 }, { 0, 0, 0 }, { 0.25f, 0.25f, 0.25f },
			TipoCuerpo::Dinamico, { 1, 1, 1 }, { 0, 0, 0 }, 1.0f);
		comprobar(Crear(escena, { pared, caja }, error), "crear una caja interactuable tras una pared");

		const glm::vec3 origen(2.0f, 0.0f, 0.0f);
		const IdCuerpo primerImpacto = mundo.CuerpoAlFinalDelRayo(origen, glm::vec3(1, 0, 0), 5.0f);
		comprobar(primerImpacto != CuerpoInvalido && escena.ObjetoDinamicoDeCuerpo(primerImpacto) == -1,
			"el primer impacto es la pared, no el objeto dinamico que queda detras");
		comprobar(escena.IntentarAgarrar(origen, glm::vec3(1, 0, 0)) == -1 && !escena.HayAgarre(),
			"no se puede recoger un objeto a traves de la pared");

		const glm::vec3 origenVisible(4.2f, 0.0f, 0.0f);
		const IdCuerpo cajaVisible = mundo.CuerpoAlFinalDelRayo(origenVisible, glm::vec3(1, 0, 0), 2.0f);
		comprobar(escena.ObjetoDinamicoDeCuerpo(cajaVisible) == 1,
			"el mismo raycast identifica la caja cuando queda visible y en rango");
		comprobar(escena.IntentarAgarrar(origenVisible, glm::vec3(1, 0, 0)) == 1,
			"la caja visible se puede recoger");
	}

	// Cambiar de habitacion con un objeto en la mano: se suelta y no queda ninguna referencia colgando.
	void PruebaCambioDeHabitacionConObjetoAgarrado()
	{
		MundoFisico mundo;
		auto sala = std::make_unique<EscenaMontada>(mundo, RutaDeEscena("habitacion_beta.json"));
		const int grande = IndiceDe(*sala, "box_large");
		const glm::vec3 ojos(5.8f, 2.0f, 0.0f);
		comprobar(sala->fisica->IntentarAgarrar(ojos, glm::vec3(7, 2, -1.3f) - ojos) == grande, "agarrar antes de cambiar de sala");
		comprobar(mundo.CuerpoSujeto() != CuerpoInvalido, "hay un cuerpo sujeto");

		EscenaMontada nueva(mundo, RutaDeEscena("ejemplo_fisica.json"));   // como cargarYAplicarEscena: primero la nueva...
		comprobar(nueva.ok, "la sala nueva monta");
		sala->fisica->Vaciar();                                            // ...y luego se vacia la vieja
		comprobar(mundo.CuerpoSujeto() == CuerpoInvalido && !sala->fisica->HayAgarre(), "al vaciar la sala vieja el agarre desaparece");
		sala.reset();
		comprobar(!nueva.fisica->HayAgarre(), "la sala nueva empieza con la mano vacia");
		comprobar(mundo.NumeroDeCuerpos() == nueva.fisica->NumeroDeCuerpos(), "no quedan cuerpos de la sala anterior");
		Avanzar(mundo, 1.0f);   // simular sin referencias colgando no debe fallar ni mover nada raro
		comprobar(mundo.NumeroDeCuerpos() == nueva.fisica->NumeroDeCuerpos(), "sigue sin haber residuos");

		// Eliminar el cuerpo sujeto directamente tambien limpia el agarre (sin punteros colgando).
		const glm::vec3 ojos2(5.0f, -4.0f, 0.0f);
		MundoFisico otro;
		const IdCuerpo c = otro.CrearCajaDinamica({ 0, 0, 0 }, glm::vec3(0.2f), 1.0f);
		comprobar(otro.SujetarCuerpo(c) && otro.CuerpoSujeto() == c, "sujetar un cuerpo dinamico");
		otro.EliminarCuerpo(c);
		comprobar(otro.CuerpoSujeto() == CuerpoInvalido, "eliminar el cuerpo sujeto limpia el agarre");
		const IdCuerpo estatico = otro.CrearCajaEstatica({ 0, 0, 0 }, glm::vec3(1.0f));
		comprobar(!otro.SujetarCuerpo(estatico) && !otro.SujetarCuerpo(CuerpoInvalido), "no se puede sujetar un estatico ni un id invalido");
		(void)ojos2;
	}
}

int main()
{
	PruebaGravedad();
	PruebaCaidaLibre();
	PruebaPasoFijo();
	PruebaIndependenciaDeLosFps();
	PruebaPausa();
	PruebaSuelo();
	PruebaMasaInvalida();
	PruebaColisionadorTransformado();
	PruebaEstaticoNoCae();
	PruebaDinamicoCaeYChoca();
	PruebaPuertasDesdeJson();
	PruebaCambioDeHabitacion();
	PruebaJugadorSueloYGravedad();
	PruebaJugadorAndaLibre();
	PruebaJugadorChocaConPared();
	PruebaJugadorSeAgachaYRespetaTecho();
	PruebaAgachadoSuaveYEstable();
	PruebaAgachadoAndandoYTecho();
	PruebaAgachadoIndependienteDeLosFps();
	PruebaJugadorSeDesliza();
	PruebaJugadorChocaConObjetoEstatico();
	PruebaJugadorIndependienteDeLosFps();
	PruebaJugadorPausa();
	PruebaJugadorYPuertas();
	PruebaJugadorEnHabitacionBeta();
	PruebaVistaEstereo();
	PruebaAgarreConBullet();
	PruebaAgarreContraPared();
	PruebaSeleccionInteraccionConObstruccion();
	PruebaCambioDeHabitacionConObjetoAgarrado();

	if (fallos == 0) std::cout << "PhysicsTests: todas las pruebas pasan\n";
	else std::cerr << "PhysicsTests: " << fallos << " fallo(s)\n";
	return fallos == 0 ? 0 : 1;
}
