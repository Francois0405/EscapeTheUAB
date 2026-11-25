// PhysicsTests.cpp
// -----------------------------------------------------------------------------
// Pruebas de MundoFisico (Bullet). No necesitan ventana ni OpenGL.
//   1. Gravedad configurada con Z vertical.
//   2. Caída libre sin suelo.
//   3. Paso fijo de 1/60 s (acumulación, tiempos grandes, independencia de los FPS).
//   4. Pausa.
//   5. Colisión con el suelo y reposo (demo del cubo) y reinicio.
// -----------------------------------------------------------------------------
#include "src/physics/PhysicsScene.h"
#include "src/gameplay/hirity/HiritiAnimation.h"
#include "src/physics/PhysicsWorld.h"
#include "src/scene/SceneLoader.h"
#include "src/vr/StereoView.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

using namespace physics;
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

	void PruebaRecursoIconoInteraccion()
	{
		const fs::path fuente = fs::path(ESCAPETHEUAB_SOURCE_DIR) / "assets/ui/hand.png";
		const fs::path runtime = fs::path(ESCAPETHEUAB_RUNTIME_ASSET_ROOT) / "assets/ui/hand.png";
		comprobar(fs::is_regular_file(fuente), "hand.png existe en recursos fuente");
		comprobar(fs::is_regular_file(runtime), "hand.png se copia junto al ejecutable independientemente del cwd de CTest");
		if (!fs::is_regular_file(fuente) || !fs::is_regular_file(runtime)) return;
		std::ifstream a(fuente, std::ios::binary), b(runtime, std::ios::binary);
		const std::vector<unsigned char> bytesA((std::istreambuf_iterator<char>(a)), {});
		const std::vector<unsigned char> bytesB((std::istreambuf_iterator<char>(b)), {});
		comprobar(bytesA == bytesB, "la textura runtime es la misma que el icono hand.png original");
		const bool pngValido = bytesA.size() > 26 && bytesA[0] == 0x89 && bytesA[1] == 'P' &&
			bytesA[2] == 'N' && bytesA[3] == 'G' && bytesA[25] == 6 && bytesA[24] == 8;
		comprobar(pngValido, "hand.png es PNG RGBA de 8 bits con transparencia");
		if (pngValido)
		{
			const unsigned ancho = (static_cast<unsigned>(bytesA[16]) << 24) | (static_cast<unsigned>(bytesA[17]) << 16) |
				(static_cast<unsigned>(bytesA[18]) << 8) | bytesA[19];
			const unsigned alto = (static_cast<unsigned>(bytesA[20]) << 24) | (static_cast<unsigned>(bytesA[21]) << 16) |
				(static_cast<unsigned>(bytesA[22]) << 8) | bytesA[23];
			comprobar(ancho == 512 && alto == 512, "el icono conserva su resolución original de 512x512; el HUD lo escala a 34 px");
		}
	}

	// Simula 'segundos' de tiempo real en fotogramas de duración fija 'deltaFotograma'.
	template <typename Funcion>
	void Simular(float segundos, float deltaFotograma, Funcion&& porFotograma)
	{
		const int fotogramas = static_cast<int>(std::lround(segundos / deltaFotograma));
		for (int i = 0; i < fotogramas; ++i) porFotograma(deltaFotograma);
	}

	// La demo ya no tiene mundo propio (usa el mundo único del juego), así que las pruebas
	// de la Fase 1 crean un mundo y le pasan la demo. Es el único cambio respecto a la Fase 1.
	struct TestScenario
	{
		PhysicsWorld mundo;
		FallingCubeDemo demo{ mundo };
		void Update(float dt, bool pausado) { mundo.SetPaused(pausado); mundo.Update(dt); }
		BodyPose CubePose() const { return demo.CubePose(); }
	};

	void PruebaGravedad()
	{
		PhysicsWorld mundo;
		const glm::vec3 g = mundo.GravityValue();
		comprobar(casi(g.x, 0.0f, 1e-6f) && casi(g.y, 0.0f, 1e-6f) && casi(g.z, -9.81f, 1e-5f),
			"la gravedad es (0, 0, -9.81): Z es el eje vertical");
	}

	void PruebaAceleracionHorizontal()
	{
		PhysicsWorld mundo;
		const IdCuerpo esfera = mundo.CreateSphere(BodyType::Dynamic, glm::vec3(0.0f, 0.0f, 2.0f), 0.25f, 1.0f);
		comprobar(esfera != CuerpoInvalido, "se crea la esfera dinamica para probar aceleracion");
		if (esfera == CuerpoInvalido) return;

		comprobar(mundo.ApplyHorizontalAcceleration(esfera, glm::vec3(1.0f, 0.0f, 0.0f), 0.1f),
			"la aceleracion horizontal se aplica al rigid body dinamico");
		mundo.Update(0.1f);
		const glm::vec3 speed = mundo.VelocityOf(esfera);
		comprobar(speed.x > 0.05f && speed.z < 0.0f,
			"la aceleracion mueve la esfera horizontalmente y conserva la gravedad vertical");

		const IdCuerpo obstaculo = mundo.CreateStaticBox(
			glm::vec3(2.0f, 0.0f, 2.0f), glm::vec3(0.25f));
		comprobar(obstaculo != CuerpoInvalido, "se crea un obstaculo estatico para el raycast");
		const float distanciaObstaculo = mundo.ObstacleDistanceOnRay(
			glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(1.0f, 0.0f, 0.0f), 3.0f, esfera);
		comprobar(distanciaObstaculo > 1.0f && distanciaObstaculo < 2.0f,
			"el raycast de navegacion detecta hitboxes e ignora el cuerpo de Hiriti");
		const float caminoLibre = mundo.ObstacleDistanceOnRay(
			glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 1.0f, 0.0f), 3.0f, esfera);
		comprobar(casi(caminoLibre, 3.0f, 1.0e-5f),
			"el raycast devuelve el alcance completo cuando no hay hitboxes en la ruta");
		comprobar(!mundo.CanPlaceDynamicBody(esfera, glm::vec3(2.0f, 0.0f, 2.0f)) &&
			mundo.CanPlaceDynamicBody(esfera, glm::vec3(0.0f, 2.0f, 2.0f)),
			"la reaparicion comprueba solapamientos con hitboxes existentes");
		comprobar(mundo.TeleportDynamicBody(esfera, glm::vec3(0.0f, 2.0f, 2.0f)) &&
			casi(mundo.PoseOf(esfera).position.y, 2.0f, 1.0e-5f) &&
			glm::length(mundo.VelocityOf(esfera)) < 1.0e-5f,
			"la reaparicion actualiza Bullet y limpia la velocidad previa");
	}

	void PruebaCaidaLibre()
	{
		PhysicsWorld mundo;
		const IdCuerpo cubo = mundo.CreateDynamicBox(glm::vec3(1.0f, 2.0f, 100.0f), glm::vec3(0.25f), 1.0f);

		Simular(1.0f, PhysicsWorld::PasoFijo, [&](float dt) { mundo.Update(dt); });

		const BodyPose pose = mundo.PoseOf(cubo);
		// Teoría: z = 100 - 1/2 g t^2 = 95.095 m tras 1 s (Bullet integra con Euler semi-implícito, ~0.08 m menos).
		comprobar(casi(pose.position.z, 95.095f, 0.15f), "tras 1 s de caída libre el cubo ha bajado ~4.9 m");
		comprobar(casi(pose.position.x, 1.0f, 1e-4f) && casi(pose.position.y, 2.0f, 1e-4f),
			"la gravedad no desplaza el cubo en X ni en Y");
		comprobar(casi(mundo.VelocityOf(cubo).z, -9.81f, 0.05f), "tras 1 s la velocidad vertical es ~-9.81 m/s");
	}

	void PruebaPasoFijo()
	{
		PhysicsWorld mundo;
		comprobar(mundo.Update(0.005f) == 0, "un fotograma más corto que el paso fijo no simula nada");
		comprobar(mundo.Update(0.005f) == 0, "...ni el segundo (acumulado 0.010 s)");
		comprobar(mundo.Update(0.0075f) == 1, "al acumular más de 1/60 s se ejecuta exactamente un paso");
		comprobar(mundo.Update(0.034f) == 2, "un fotograma de 34 ms ejecuta dos pasos de 1/60 s");
		comprobar(mundo.Update(0.0f) == 0 && mundo.Update(-1.0f) == 0, "un delta cero o negativo se ignora");

		PhysicsWorld congelado;
		const int pasos = congelado.Update(10.0f);
		comprobar(pasos == PhysicsWorld::PasosMaximosPorFotograma, "un fotograma de 10 s se recorta al máximo de pasos");
		comprobar(congelado.Update(0.0f) == 0, "tras un tiempo enorme no queda tiempo pendiente acumulado");
	}

	void PruebaIndependenciaDeLosFps()
	{
		TestScenario a, b;
		a.demo.Start();
		b.demo.Start();
		Simular(3.0f, 1.0f / 30.0f, [&](float dt) { a.Update(dt, false); });
		Simular(3.0f, 1.0f / 144.0f, [&](float dt) { b.Update(dt, false); });
		comprobar(casi(a.CubePose().position.z, b.CubePose().position.z, 0.02f),
			"30 FPS y 144 FPS dan el mismo resultado final");
	}

	void PruebaPausa()
	{
		PhysicsWorld mundo;
		const IdCuerpo cubo = mundo.CreateDynamicBox(glm::vec3(0.0f, 0.0f, 50.0f), glm::vec3(0.25f), 1.0f);

		Simular(0.5f, PhysicsWorld::PasoFijo, [&](float dt) { mundo.Update(dt); });
		const float alturaAntes = mundo.PoseOf(cubo).position.z;
		const long long pasosAntes = mundo.PasosTotales();

		mundo.SetPaused(true);
		comprobar(mundo.IsPaused(), "la pausa queda activada");
		comprobar(mundo.Update(1.0f) == 0, "en pausa no se ejecuta ningún paso");
		Simular(2.0f, 0.016f, [&](float dt) { mundo.Update(dt); });
		comprobar(mundo.PoseOf(cubo).position.z == alturaAntes, "en pausa el cubo no se mueve");
		comprobar(mundo.PasosTotales() == pasosAntes, "en pausa el contador de pasos no cambia");

		mundo.SetPaused(false);
		comprobar(mundo.Update(PhysicsWorld::PasoFijo) == 1,
			"al reanudar solo cuenta el tiempo nuevo (el de la pausa se descartó)");
		comprobar(mundo.PoseOf(cubo).position.z < alturaAntes, "al reanudar el cubo vuelve a caer");
	}

	void PruebaSuelo()
	{
		TestScenario escenario;
		FallingCubeDemo& demo = escenario.demo;
		demo.Start();
		const float alturaInicial = demo.CubePose().position.z;
		const float alturaReposo = FallingCubeDemo::AlturaSuperiorSuelo + FallingCubeDemo::MitadCubo;   // -1.45

		comprobar(casi(alturaInicial, FallingCubeDemo::AlturaInicialCentro, 1e-5f), "el cubo empieza a la altura inicial");

		float alturaMinima = alturaInicial;
		float alturaTras03s = 0.0f;
		float tiempo = 0.0f;
		Simular(6.0f, PhysicsWorld::PasoFijo, [&](float dt) {
			escenario.Update(dt, false);
			tiempo += dt;
			alturaMinima = std::min(alturaMinima, demo.CubePose().position.z);
			if (alturaTras03s == 0.0f && tiempo >= 0.3f) alturaTras03s = demo.CubePose().position.z;
		});

		comprobar(alturaTras03s < alturaInicial - 0.3f, "a los 0.3 s el cubo ya ha caído");
		// Al impactar a ~7 m/s el cubo recorre ~12 cm por paso, así que Bullet permite una
		// penetración momentánea (~5 cm medidos) y lo empuja fuera. Atravesar de verdad sería
		// hundirse más de la mitad del cubo.
		comprobar(alturaMinima > alturaReposo - 0.10f, "el cubo nunca atraviesa el suelo (solo penetración momentánea < 10 cm)");
		const BodyPose pose = demo.CubePose();
		comprobar(casi(pose.position.z, alturaReposo, 0.02f), "el cubo se apoya sobre el suelo (centro a 0.25 m de él)");
		comprobar(glm::length(demo.CubeVelocity()) < 0.01f, "el cubo termina quieto");
		comprobar(demo.CubeAtRest(), "Bullet duerme el cubo cuando lleva un rato quieto");
		comprobar(casi(pose.position.x, 6.0f, 0.01f) && casi(pose.position.y, -1.0f, 0.01f),
			"caer plano no desplaza el cubo en horizontal");

		// Reiniciar vuelve a empezar desde arriba.
		demo.Start();
		comprobar(casi(demo.CubePose().position.z, alturaInicial, 1e-5f), "Iniciar() reinicia la demo");
		comprobar(escenario.mundo.BodyCount() == 2, "tras reiniciar solo hay suelo y cubo (no se acumulan)");

		demo.Stop();
		comprobar(!demo.IsRunningNow() && escenario.mundo.BodyCount() == 0, "Detener() elimina los cuerpos");
	}

	void PruebaMasaInvalida()
	{
		PhysicsWorld mundo;
		comprobar(mundo.CreateDynamicBox(glm::vec3(0.0f), glm::vec3(1.0f), 0.0f) == CuerpoInvalido,
			"un cuerpo dinámico con masa 0 se rechaza");
		comprobar(mundo.PoseOf(CuerpoInvalido).position == glm::vec3(0.0f), "pedir la pose de un id inválido no falla");
	}

	void PruebaLanzamientoDeObjetoAgarrado()
	{
		PhysicsWorld mundo;
		const IdCuerpo cubo = mundo.CreateDynamicBox(glm::vec3(0.0f), glm::vec3(0.2f), 1.0f);
		comprobar(mundo.HoldBody(cubo), "se puede sujetar el objeto antes de lanzarlo");
		comprobar(mundo.ThrowHeldBody(glm::vec3(8.0f, 0.0f, 3.0f)),
			"lanzar devuelve verdadero cuando habia un objeto sujeto");
		comprobar(mundo.HeldBody() == CuerpoInvalido, "lanzar libera la sujecion");
		const glm::vec3 speed = mundo.VelocityOf(cubo);
		comprobar(casi(speed.x, 8.0f, 1e-4f) && casi(speed.y, 0.0f, 1e-4f) &&
			casi(speed.z, 3.0f, 1e-4f), "el objeto conserva la velocidad del lanzamiento");

		mundo.Update(PhysicsWorld::PasoFijo);
		comprobar(mundo.VelocityOf(cubo).z < 3.0f, "el objeto lanzado vuelve a recibir gravedad");
		comprobar(!mundo.ThrowHeldBody(glm::vec3(1.0f, 0.0f, 0.0f)),
			"lanzar sin objeto sujeto informa que no hubo lanzamiento");
	}
	// ======================================================================
	// Fase 2: escena JSON -> cuerpos de Bullet
	// ======================================================================

	SceneObject ObjetoCaja(const char* id, const glm::vec3& position, const glm::vec3& eulerRotation,
		const glm::vec3& escala, BodyType tipo, const glm::vec3& mitad, const glm::vec3& desplazamiento, float masa)
	{
		SceneObject o{};
		o.id = id;
		o.position = position;
		o.eulerRotation = eulerRotation;
		o.escala = escala;
		o.fisica.presente = true;
		o.fisica.tipo = tipo;
		o.fisica.halfExtents = mitad;
		o.fisica.desplazamiento = desplazamiento;
		o.fisica.masa = masa;
		return o;
	}

	bool Create(PhysicsScene& escena, const std::vector<SceneObject>& objetos, std::string& error)
	{
		const glb::AnimatedScene sinAnimados;
		return escena.Create(objetos, sinAnimados, {}, error);
	}

	void Avanzar(PhysicsWorld& mundo, float segundos)
	{
		Simular(segundos, PhysicsWorld::PasoFijo, [&](float dt) { mundo.Update(dt); });
	}

	// Rotación de un cuerpo aplicada a un vector (para comprobar orientaciones).
	glm::vec3 Gira(const BodyPose& pose, const glm::vec3& v) { return pose.eulerRotation * v; }

	bool Cerca(const glm::vec3& a, const glm::vec3& b, float tolerancia = 1e-3f) { return glm::length(a - b) <= tolerancia; }

	// Colisionador trasladado, girado y escalado: no solo "correcto en el origen".
	void PruebaColisionadorTransformado()
	{
		PhysicsWorld mundo;
		PhysicsScene escena(mundo);
		// Objeto en (3,-2,1), girado 90 grados en Z, escala (2,1,1); caja de ±0.5 desplazada 1 m en X del modelo.
		//   offset escalado = (2,0,0) -> girado 90 grados en Z = (0,2,0) -> centro = (3,0,1)
		//   mitad final = (1, 0.5, 0.5); el eje largo (X del modelo) queda a lo largo de Y.
		std::string error;
		comprobar(Create(escena, { ObjetoCaja("t", { 3, -2, 1 }, { 0, 0, 90 }, { 2, 1, 1 }, BodyType::Static,
			{ 0.5f, 0.5f, 0.5f }, { 1, 0, 0 }, 0.0f) }, error), "crear colisionador transformado");
		const auto cajas = mundo.CollisionBoxes();
		comprobar(cajas.size() == 1, "se crea exactamente un cuerpo");
		if (cajas.size() != 1) return;
		comprobar(Cerca(cajas[0].pose.position, { 3, 0, 1 }), "centro: posicion + R * (escala * offset)");
		comprobar(Cerca(cajas[0].halfExtents, { 1.0f, 0.5f, 0.5f }), "mitad: |escala| * halfSize (escala aplicada una vez)");
		comprobar(Cerca(Gira(cajas[0].pose, { 1, 0, 0 }), { 0, 1, 0 }), "la caja gira con el objeto (X del modelo -> Y del mundo)");

		// Las esquinas reales de la caja: x en [2.5, 3.5], y en [-1, 1] (largo a lo largo de Y), z en [0.5, 1.5].
		float minimo[3] = { 1e9f, 1e9f, 1e9f }, maximo[3] = { -1e9f, -1e9f, -1e9f };
		for (const glm::vec3& p : BoxEdges(cajas[0]))
			for (int e = 0; e < 3; ++e) { minimo[e] = std::min(minimo[e], p[e]); maximo[e] = std::max(maximo[e], p[e]); }
		comprobar(casi(minimo[0], 2.5f, 1e-3f) && casi(maximo[0], 3.5f, 1e-3f) && casi(minimo[1], -1.0f, 1e-3f) &&
			casi(maximo[1], 1.0f, 1e-3f) && casi(minimo[2], 0.5f, 1e-3f) && casi(maximo[2], 1.5f, 1e-3f),
			"las aristas dibujadas ocupan exactamente el volumen esperado");

		// Escala negativa (modelo espejado): el centro la respeta y la mitad sigue positiva.
		PhysicsScene espejo(mundo);
		comprobar(Create(espejo, { ObjetoCaja("e", { 0, 0, 0 }, { 0, 0, 0 }, { -2, 1, 1 }, BodyType::Static,
			{ 1, 1, 1 }, { 1, 0, 0 }, 0.0f) }, error), "crear con escala negativa");
		bool encontrada = false;
		for (const CollisionBox& c : mundo.CollisionBoxes())
			if (Cerca(c.pose.position, { -2, 0, 0 }) && Cerca(c.halfExtents, { 2, 1, 1 })) encontrada = true;
		comprobar(encontrada, "escala negativa: centro espejado y mitad positiva");
	}

	void PruebaEstaticoNoCae()
	{
		PhysicsWorld mundo;
		PhysicsScene escena(mundo);
		std::string error;
		comprobar(Create(escena, { ObjetoCaja("flota", { 1, 2, 5 }, { 0, 0, 0 }, { 1, 1, 1 }, BodyType::Static,
			{ 0.5f, 0.5f, 0.5f }, { 0, 0, 0 }, 0.0f) }, error), "crear estatico en el aire");
		Avanzar(mundo, 3.0f);
		comprobar(Cerca(mundo.CollisionBoxes()[0].pose.position, { 1, 2, 5 }, 1e-6f), "un cuerpo estatico no cae");
	}

	void PruebaReconstruccionCambioEstaticoDinamico()
	{
		PhysicsWorld mundo;
		std::string error;
		SceneObject objeto = ObjetoCaja("replicable", { 0, 0, 3 }, { 0, 0, 0 }, { 1, 1, 1 },
			BodyType::Static, { 0.5f, 0.5f, 0.5f }, { 0, 0, 0 }, 0.0f);
		auto actual = std::make_unique<PhysicsScene>(mundo);
		comprobar(Create(*actual, { objeto }, error), "crear la caja estática original");
		comprobar(actual->BodyCount() == 1 && actual->DynamicCount() == 0, "estado inicial con un cuerpo estático");

		objeto.fisica.tipo = BodyType::Dynamic; objeto.fisica.masa = 2.0f;
		auto dinamica = std::make_unique<PhysicsScene>(mundo);
		comprobar(Create(*dinamica, { objeto }, error), "construir la versión dinámica antes de retirar la anterior");
		actual->ClearBodies(); actual = std::move(dinamica);
		comprobar(mundo.BodyCount() == 1 && actual->BodyCount() == 1 && actual->DynamicCount() == 1,
			"estático a dinámico deja un solo cuerpo registrado");
		const auto dinamicos = mundo.CollisionBoxes();
		comprobar(dinamicos.size() == 1 && dinamicos[0].tipo == BodyType::Dynamic,
			"Bullet contiene el tipo dinámico solicitado");

		objeto.fisica.tipo = BodyType::Static; objeto.fisica.masa = 0.0f;
		auto estatica = std::make_unique<PhysicsScene>(mundo);
		comprobar(Create(*estatica, { objeto }, error), "construir la versión estática antes de retirar la anterior");
		actual->ClearBodies(); actual = std::move(estatica);
		comprobar(mundo.BodyCount() == 1 && actual->BodyCount() == 1 && actual->DynamicCount() == 0,
			"dinámico a estático deja un solo cuerpo registrado");
		const auto estaticos = mundo.CollisionBoxes();
		comprobar(estaticos.size() == 1 && estaticos[0].tipo == BodyType::Static,
			"Bullet contiene el tipo estático solicitado");
	}

	void PruebaSujecionesMultiplesYReplicaCinematica()
	{
		PhysicsWorld mundo;
		const auto cuerpoA = mundo.CreateBox(BodyType::Dynamic, { 0, 0, 3 }, glm::quat(1, 0, 0, 0), { 0.2f, 0.2f, 0.2f }, 1.0f);
		const auto cuerpoB = mundo.CreateBox(BodyType::Dynamic, { 1, 0, 3 }, glm::quat(1, 0, 0, 0), { 0.2f, 0.2f, 0.2f }, 1.0f);
		comprobar(mundo.HoldBodyAuthoritative(cuerpoA) && mundo.HoldBodyAuthoritative(cuerpoB),
			"Bullet permite sujetar dos objetos a propietarios distintos");
		mundo.SetHoldDestination(cuerpoA, { 0, 1, 3 });
		mundo.SetHoldDestination(cuerpoB, { 1, 1, 3 });
		Avanzar(mundo, 0.25f);
		comprobar(mundo.IsHeld(cuerpoA) && mundo.IsHeld(cuerpoB) &&
			mundo.PoseOf(cuerpoA).position.y > 0.5f && mundo.PoseOf(cuerpoB).position.y > 0.5f,
			"cada sujecion aplica su propio destino durante los pasos Bullet");
		comprobar(mundo.ReleaseAuthoritativeBody(cuerpoA, { 0, 0, 4 }) && !mundo.IsHeld(cuerpoA) && mundo.IsHeld(cuerpoB),
			"soltar un objeto no libera el objeto del otro jugador");
		comprobar(Cerca(mundo.VelocityOf(cuerpoA), { 0, 0, 4 }), "la velocidad de lanzamiento se aplica al cuerpo autoritativo");

		for (const BodyType tipo : { BodyType::Dynamic, BodyType::Bouncer })
		{
			PhysicsWorld cliente;
			const char* id = tipo == BodyType::Bouncer ? "hiriti-replica" : "replica";
			SceneObject objeto = ObjetoCaja(id, { 0, 0, 2 }, { 0, 0, 0 }, { 1, 1, 1 }, tipo,
				{ 0.25f, 0.25f, 0.25f }, { 0, 0, 0 }, tipo == BodyType::Bouncer ? 0.25f : 1.0f);
			PhysicsScene escena(cliente);
			std::string error;
			comprobar(escena.Create({ objeto }, {}, {}, error, false),
				"la réplica cliente crea dynamic y Rebotador como cinemáticos");
			auto cuerpos = cliente.CollisionBoxes();
			comprobar(cuerpos.size() == 1 && cuerpos[0].tipo == BodyType::Kinematic,
				"el cliente no ejecuta una segunda simulacion del objeto con autoridad");
			PhysicsScene::DynamicState estado;
			estado.objectIndex = 0; estado.pose.position = { 3, 2, 4 }; estado.pose.eulerRotation = glm::quat(1, 0, 0, 0);
			comprobar(escena.ApplyReplicatedState(estado),
				"snapshot acepta la réplica cinemática de dynamic y Rebotador");
			const auto pose = cliente.CollisionBoxes()[0].pose;
			comprobar(Cerca(pose.position, { 3, 2, 4 }), "pose replicada actualiza el AABB de Bullet");
			comprobar(Cerca(glm::vec3(escena.VisualMatrixOf(0, objeto)[3]), { 3, 2, 4 }),
				"la representacion visual toma la misma pose del cuerpo replicado");
			const IdCuerpo cuerpoMirado = cliente.BodyAtRayEnd({ 3, 0, 4 }, { 0, 1, 0 }, 3.0f);
			comprobar(escena.DynamicObjectOfBody(cuerpoMirado) == 0 &&
				cliente.CollisionBoxes()[static_cast<std::size_t>(cuerpoMirado)].tipo == BodyType::Kinematic,
				"el raycast del cliente selecciona su réplica cinemática de objeto dynamic o Rebotador");
		}
	}

	// Suelo + caja dinamica con offset y giro: cae, choca y se detiene.
	void PruebaDinamicoCaeYChoca()
	{
		PhysicsWorld mundo;
		PhysicsScene escena(mundo);
		std::string error;
		// Suelo: cubo de ±1 escalado (8,5,0.15), cara superior en z = -1.7.
		const SceneObject suelo = ObjetoCaja("suelo", { 5, 0, -1.85f }, { 0, 0, 0 }, { 8, 5, 0.15f }, BodyType::Static,
			{ 1, 1, 1 }, { 0, 0, 0 }, 0.0f);
		// Caja: modelo con el origen en su base (offset z = +1 con halfSize 1), escala 0.25, girada 30 grados en Z.
		const SceneObject caja = ObjetoCaja("caja", { 5, 0, 1 }, { 0, 0, 30 }, { 0.25f, 0.25f, 0.25f }, BodyType::Dynamic,
			{ 1, 1, 1 }, { 0, 0, 1 }, 2.0f);
		const std::vector<SceneObject> objetos = { suelo, caja };
		comprobar(Create(escena, objetos, error), "crear suelo y caja");
		comprobar(escena.BodyCount() == 2 && escena.DynamicCount() == 1, "dos cuerpos, uno dinamico");
		comprobar(!escena.HasDynamicObjectContact(1), "la caja no tiene contacto antes de caer");

		const glm::mat4 antes = escena.VisualMatrixOf(1, caja);
		comprobar(Cerca(glm::vec3(antes[3]), caja.position), "al empezar, el dibujo coincide con la posicion del JSON");

		Avanzar(mundo, 0.3f);
		comprobar(escena.VisualMatrixOf(1, caja)[3].z < 1.0f - 0.2f, "a los 0.3 s la caja ya ha caido");
		Avanzar(mundo, 5.0f);
		comprobar(escena.HasDynamicObjectContact(1), "se detecta el contacto de la caja con el suelo");

		// En reposo: el centro de la caja esta a 0.25 m sobre el suelo y el ORIGEN del modelo (su base) toca el suelo.
		const glm::mat4 despues = escena.VisualMatrixOf(1, caja);
		comprobar(casi(despues[3].z, -1.7f, 0.02f), "el origen del modelo (su base) queda sobre el suelo");
		comprobar(casi(despues[3].x, 5.0f, 0.02f) && casi(despues[3].y, 0.0f, 0.02f), "cae en vertical, sin deriva horizontal");
		// Conserva el giro de 30 grados en Z: la columna X del dibujo apunta a (cos30, sin30, 0) y mide 0.25 (escala).
		comprobar(Cerca(glm::vec3(despues[0]), glm::vec3(0.25f * std::cos(glm::radians(30.0f)), 0.25f * std::sin(glm::radians(30.0f)), 0.0f), 0.02f),
			"la caja conserva su giro y su escala al caer");
		comprobar(mundo.CollisionBoxes().size() == 2, "no se han duplicado cuerpos");
		for (const CollisionBox& c : mundo.CollisionBoxes())
			if (c.tipo == BodyType::Dynamic) comprobar(glm::length(mundo.VelocityOf(0)) < 1.0f, "(sanidad) velocidades finitas");
		// Un objeto sin fisica se dibuja con la matriz de la escena.
		SceneObject sinFisica{};
		sinFisica.position = { 1, 2, 3 }; sinFisica.escala = { 1, 1, 1 };
		comprobar(Cerca(glm::vec3(escena.VisualMatrixOf(-1, sinFisica)[3]), { 1, 2, 3 }), "un objeto sin fisica conserva su matriz");

		// Pausa y reinicio con una escena real: en pausa nada se mueve.
		PhysicsScene otra(mundo);
		const SceneObject volando = ObjetoCaja("v", { 0, 20, 50 }, { 0, 0, 0 }, { 1, 1, 1 }, BodyType::Dynamic, { 0.5f, 0.5f, 0.5f }, { 0, 0, 0 }, 1.0f);
		comprobar(Create(otra, { volando }, error), "crear caja para la prueba de pausa");
		mundo.SetPaused(true);
		const glm::mat4 enPausa = otra.VisualMatrixOf(0, volando);
		Simular(1.0f, 0.016f, [&](float dt) { mundo.Update(dt); });
		comprobar(otra.VisualMatrixOf(0, volando)[3] == enPausa[3], "con la pausa, el cuerpo de la escena no se mueve");
		mundo.SetPaused(false);
		Avanzar(mundo, 0.2f);
		comprobar(otra.VisualMatrixOf(0, volando)[3].z < enPausa[3].z, "al reanudar vuelve a caer");
	}

	std::string RutaDeEscena(const char* nombre)
	{
		return (fs::path(ESCAPETHEUAB_SOURCE_DIR) / "assets/scenes" / nombre).string();
	}

	// Carga una escena JSON real y la monta: datos, GLB y cuerpos.
	struct EscenaMontada
	{
		SceneData datos;
		glb::AnimatedScene animados;
		std::unique_ptr<PhysicsScene> fisica;
		std::string error;
		bool ok = false;

		EscenaMontada(PhysicsWorld& mundo, const std::string& ruta)
		{
			fisica = std::make_unique<PhysicsScene>(mundo);
			if (!LoadSceneDataJson(ruta, datos, error)) return;
			if (!LoadSceneAnimatedObjects(datos.animados, fs::path(ruta).parent_path(), animados, error)) return;
			ok = fisica->Create(datos.objetos, animados, datos.animados, error);
		}
	};

	void PruebaPuertasDesdeJson()
	{
		PhysicsWorld mundo;
		EscenaMontada habitacion(mundo, RutaDeEscena("habitacion_beta.json"));
		comprobar(habitacion.ok, "habitacion_beta monta con sus fisicas");
		if (!habitacion.ok) { std::cerr << habitacion.error << '\n'; return; }

		// Dos puertas del MISMO glb: un solo modelo cargado, dos estados independientes.
		glb::AnimatedScene& anim = habitacion.animados;
		comprobar(anim.objetos.size() == 2 && anim.modelos.size() == 1, "dos puertas comparten un unico modelo GLB");
		comprobar(anim.objetos[0].modelo == anim.objetos[1].modelo, "ambas instancias apuntan al mismo ModeloGLB");
		comprobar(&anim.objetos[0].pose != &anim.objetos[1].pose, "cada instancia tiene su pose");

		// Cuerpos: 12 objetos OBJ con fisica + (3 marco + 1 hoja) * 2 puertas = 20.
		comprobar(habitacion.fisica->BodyCount() == 20 && mundo.BodyCount() == 20, "20 cuerpos: 12 OBJ + 8 de puertas");

		const auto cajasCinematicas = [&]() {
			std::vector<CollisionBox> v;
			for (const CollisionBox& c : mundo.CollisionBoxes()) if (c.tipo == BodyType::Kinematic) v.push_back(c);
			return v;
		};
		const auto posicionesEstaticas = [&]() {
			std::vector<glm::vec3> v;
			for (const CollisionBox& c : mundo.CollisionBoxes()) if (c.tipo == BodyType::Static) v.push_back(c.pose.position);
			return v;
		};
		const std::vector<CollisionBox> hojasCerradas = cajasCinematicas();
		const std::vector<glm::vec3> estaticasAntes = posicionesEstaticas();
		comprobar(hojasCerradas.size() == 2, "una hoja cinematica por puerta");
		const glm::vec3 ojosFrentePuerta(4.5f, 3.0f, -0.65f);
		const glm::vec3 miradaPuerta(1.0f, 0.0f, 0.0f);
		float distanciaPuerta = 0.0f;
		const IdCuerpo impactoPuerta = mundo.BodyAtRayEnd(
			ojosFrentePuerta, miradaPuerta, glb::RADIO_INTERACCION, &distanciaPuerta);
		comprobar(habitacion.fisica->AnimatedObjectOfBody(impactoPuerta) == 1 &&
			glb::FindInteractableObject(anim, ojosFrentePuerta, miradaPuerta) == &anim.objetos[1],
			"el raycast visible y la consulta de puerta identifican el mismo objetivo");
		comprobar(std::isfinite(distanciaPuerta) && distanciaPuerta > 0.0f && distanciaPuerta < glb::RADIO_INTERACCION,
			"la consulta de interacción conserva la distancia real del primer impacto de Bullet");
		// Hoja cerrada: centro a 1.0 m del suelo (-1.7), entre los dos postes del marco, sobre el eje de la puerta.
		for (const CollisionBox& h : hojasCerradas)
			comprobar(casi(h.pose.position.z, -1.7f + 1.009f, 0.01f) && casi(h.pose.position.x, 6.0f, 0.05f),
				"hoja cerrada: altura y plano correctos");

		// Se abre SOLO la puerta 1 durante 2 segundos.
		glb::ToggleDoor(anim.objetos[0]);
		for (int i = 0; i < 120; ++i)
		{
			glb::UpdateAnimatedObjects(anim, PhysicsWorld::PasoFijo);
			habitacion.fisica->SyncKinematics(anim);
			mundo.Update(PhysicsWorld::PasoFijo);
		}
		comprobar(anim.objetos[0].reproductor.tiempo > 1.0f && anim.objetos[1].reproductor.tiempo == 0.0f,
			"relojes independientes: solo se mueve la puerta 1");
		const std::vector<CollisionBox> hojasAbiertas = cajasCinematicas();
		const bool primeraMovida = glm::length(hojasAbiertas[0].pose.position - hojasCerradas[0].pose.position) > 0.2f;
		const bool segundaQuieta = Cerca(hojasAbiertas[1].pose.position, hojasCerradas[1].pose.position, 1e-4f);
		const bool giraLaPrimera = glm::length(Gira(hojasAbiertas[0].pose, { 1, 0, 0 }) - Gira(hojasCerradas[0].pose, { 1, 0, 0 })) > 0.5f;
		// Las dos hojas estan guardadas en orden de creacion: puerta_1 primero.
		comprobar(primeraMovida && giraLaPrimera, "la hoja de la puerta 1 se desplaza y gira con la animacion");
		comprobar(segundaQuieta, "la hoja de la puerta 2 no se mueve");
		// La bisagra no se mueve: el borde de la hoja junto a la bisagra (x = 0 del nodo Door) queda en su sitio (hasta ~2 cm: la hoja tiene 3.4 cm de grosor),
		// mientras que el borde libre se desplaza varios decimetros.
		const auto bordeBisagra = [](const CollisionBox& hoja) { return hoja.pose.position + Gira(hoja.pose, { hoja.halfExtents.x, 0, 0 }); };
		const auto bordeLibre = [](const CollisionBox& hoja) { return hoja.pose.position - Gira(hoja.pose, { hoja.halfExtents.x, 0, 0 }); };
		comprobar(Cerca(bordeBisagra(hojasAbiertas[0]), bordeBisagra(hojasCerradas[0]), 0.03f), "la hoja gira alrededor de su bisagra");
		comprobar(glm::length(bordeLibre(hojasAbiertas[0]) - bordeLibre(hojasCerradas[0])) > 0.5f, "el borde libre se aparta de la posicion cerrada");
		// El marco (estatico) no ha cambiado ni un milimetro mientras la hoja giraba.
		const std::vector<glm::vec3> estaticasDespues = posicionesEstaticas();
		bool marcoIgual = estaticasAntes.size() == estaticasDespues.size();
		for (std::size_t i = 0; marcoIgual && i < estaticasAntes.size(); ++i)
			marcoIgual = Cerca(estaticasAntes[i], estaticasDespues[i], 1e-6f);
		comprobar(marcoIgual, "el marco permanece estatico mientras la hoja gira");
	}

	void PruebaParedParametricaConPuerta()
	{
		PhysicsWorld mundo;
		SceneObject pared;
		pared.id = "wall"; pared.categoria = "architecture"; pared.position = glm::vec3(0.0f);
		pared.eulerRotation = glm::vec3(0.0f); pared.escala = glm::vec3(5.0f, 0.15f, 1.5f);
		pared.fisica.presente = true; pared.fisica.tipo = BodyType::Static;
		pared.fisica.halfExtents = glm::vec3(1.0f); pared.pared.presente = true;
		pared.pared.aperturas.push_back({ "door", 0.0f, 1.04f, 2.1f, 0.0f, "door-opening" });
		SceneAnimatedObject door;
		door.id = "door";
		door.modelo = (fs::path(ESCAPETHEUAB_SOURCE_DIR) / "assets/animations/Puerta_Lab.glb").string();
		door.position = glm::vec3(0.0f, 0.0f, -1.5f);
		door.colisionadores = {
			{"DoorFrame", BodyType::Static, {0.05f,1.05f,0.04f}, {-0.45f,1.05f,-0.013f}},
			{"DoorFrame", BodyType::Static, {0.05f,1.05f,0.04f}, {0.45f,1.05f,-0.013f}},
			{"DoorFrame", BodyType::Static, {0.4f,0.05f,0.04f}, {0.0f,2.05f,-0.013f}},
			{"Door", BodyType::Kinematic, {0.418f,1.005f,0.017f}, {-0.418f,-0.041f,-0.017f}}
		};
		door.paredId = "wall"; door.openingId = "door-opening";
		glb::AnimatedScene animados;
		std::string error;
		comprobar(glb::AddAnimatedObject(animados, door.id, door.modelo, door.position, 0.0f, error), "carga GLB para pared paramétrica");
		physics::PhysicsScene escena(mundo);
		comprobar(escena.Create({pared}, animados, {door}, error), "Bullet construye colliders segmentados para el vano");
		comprobar(escena.BodyCount() == 7, "tres paños de pared sustituyen al collider completo y se suman cuatro colliders de puerta");
		if (escena.BodyCount() != 7) return;
		const glm::vec3 origen(0.0f, -1.0f, -0.5f);
		const glm::vec3 direccion(0.0f, 1.0f, 0.0f);
		comprobar(mundo.BodyAtRayEnd(origen, direccion, 2.0f) != CuerpoInvalido,
			"la hoja cerrada bloquea el cruce en el centro del vano");
		glb::ToggleDoor(animados.objetos[0]);
		for (int i = 0; i < 240; ++i)
		{
			glb::UpdateAnimatedObjects(animados, PhysicsWorld::PasoFijo);
			escena.SyncKinematics(animados);
			mundo.Update(PhysicsWorld::PasoFijo);
		}
		comprobar(mundo.BodyAtRayEnd(origen, direccion, 2.0f) == CuerpoInvalido,
			"con la hoja abierta el rayo atraviesa el vano sin chocar con la pared");
	}

	void PruebaSueloLaboratorioCalibrado()
	{
		PhysicsWorld mundo;
		EscenaMontada laboratorio(mundo, RutaDeEscena("aula_informatica_calibrada.json"));
		comprobar(laboratorio.ok, "el laboratorio calibrado monta con todos sus cuerpos Bullet");
		if (!laboratorio.ok) { std::cerr << laboratorio.error << '\n'; return; }

		int colisionables = static_cast<int>(std::count_if(laboratorio.datos.objetos.begin(), laboratorio.datos.objetos.end(),
			[](const SceneObject& objeto) { return objeto.fisica.presente; }));
		for (const SceneAnimatedObject& animado : laboratorio.datos.animados)
			colisionables += static_cast<int>(animado.colisionadores.size());
		comprobar(laboratorio.fisica->BodyCount() == colisionables,
			"suelo y arquitectura conservan colliders unicos; se cuentan tambien los marcos y hojas de puerta GLB");
		const SceneObject* suelo = nullptr;
		for (const SceneObject& objeto : laboratorio.datos.objetos)
			if (objeto.id == "suelo") suelo = &objeto;
		comprobar(suelo != nullptr && suelo->fisica.presente && suelo->fisica.tipo == BodyType::Static,
			"el suelo principal conserva un unico cuerpo estatico");
		if (suelo == nullptr) return;

		const float alturaPavimento = suelo->position.z + suelo->escala.z * suelo->fisica.halfExtents.z;
		comprobar(mundo.CreatePlayerBody({ 0.0f, 0.0f, alturaPavimento + 0.02f }),
			"se crea el jugador sobre el pavimento modular");
		Simular(2.0f, PhysicsWorld::PasoFijo, [&](float dt) { mundo.Update(dt); });
		const float pies = mundo.PlayerFeetPosition().z;
		comprobar(mundo.IsPlayerGrounded() && pies > alturaPavimento - 0.08f && pies < alturaPavimento + 0.08f,
			"el jugador permanece apoyado sobre la altura del suelo calibrado");

		// Integración geométrica: abrir la hoja GLB permite cruzar el vano rotado
		// entre recepción y laboratorio, sin atravesar los dos tramos de pared.
		glb::AnimatedObject* puerta = glb::FindAnimatedObject(laboratorio.animados, "puerta_acceso_laboratorio");
		comprobar(puerta != nullptr, "la escena compacta incluye la puerta recepción/laboratorio");
		if (puerta == nullptr) return;
		glb::BeginAnimation(*puerta);
		glb::UpdateAnimatedObjects(laboratorio.animados, puerta->reproductor.duracion);
		laboratorio.fisica->SyncKinematics(laboratorio.animados);
		mundo.TeleportPlayer({ -3.7f, 4.5f, alturaPavimento });
		mundo.SetPlayerVelocity({ 1.5f, 0.0f, 0.0f });
		Simular(1.2f, PhysicsWorld::PasoFijo, [&](float dt) { mundo.Update(dt); });
		mundo.SetPlayerVelocity({ 0.0f, 0.0f, 0.0f });
		comprobar(mundo.PlayerFeetPosition().x > -2.2f,
			"el vano de la puerta abierta deja pasar al jugador y la pared maciza contigua lo sigue bloqueando");
		glb::ToggleDoor(*puerta);
		glb::UpdateAnimatedObjects(laboratorio.animados, puerta->reproductor.duracion);
		laboratorio.fisica->SyncKinematics(laboratorio.animados);
		mundo.TeleportPlayer({ -3.7f, 4.5f, alturaPavimento });
		mundo.SetPlayerVelocity({ 1.5f, 0.0f, 0.0f });
		Simular(1.2f, PhysicsWorld::PasoFijo, [&](float dt) { mundo.Update(dt); });
		mundo.SetPlayerVelocity({ 0.0f, 0.0f, 0.0f });
		comprobar(mundo.PlayerFeetPosition().x < -2.7f,
			"la hoja cinemática cerrada bloquea el vano y no permite atravesar la pared");
	}

	void PruebaArranqueEnSpawnDelLaboratorio()
	{
		PhysicsWorld mundo;
		EscenaMontada laboratorio(mundo, RutaDeEscena("aula_informatica_calibrada.json"));
		comprobar(laboratorio.ok, "se monta la misma escena que carga el inicio del juego");
		if (!laboratorio.ok) { std::cerr << laboratorio.error << '\n'; return; }
		comprobar(laboratorio.datos.spawn.presente, "la escena proporciona spawn explícito para PLAY");
		if (!laboratorio.datos.spawn.presente) return;

		const glm::vec3 spawn = laboratorio.datos.spawn.pies;
		comprobar(spawn.x > -7.25f && spawn.x < -2.75f && spawn.y > 1.75f && spawn.y < 7.25f,
			"el spawn está dentro de la recepción y separado de sus paredes");
		glm::vec3 pies, normal;
		std::string error;
		const bool valido = mundo.ValidatePlayerSpawn(spawn, pies, normal, error);
		comprobar(valido, "Bullet confirma suelo bajo los pies y espacio para la cápsula");
		if (!valido) { std::cerr << "Spawn laboratorio: " << error << '\n'; return; }
		comprobar(normal.z > 0.95f && std::abs(pies.z - spawn.z) < 0.02f,
			"el ajuste de spawn coincide con la cara superior del pavimento");
		comprobar(mundo.CreatePlayerBody(pies), "se crea el controlador Bullet desde el spawn validado");
		const glm::vec3 ojos = mundo.PlayerFeetPosition() + glm::vec3(0, 0, mundo.PlayerEyeHeight());
		comprobar(casi(ojos.z, pies.z + 1.70f, 0.001f), "la altura de ojos se deriva de los pies y la altura FPS");
		Simular(2.0f, PhysicsWorld::PasoFijo, [&](float dt) { mundo.Update(dt); });
		const glm::vec3 trasDosSegundos = mundo.PlayerFeetPosition();
		std::cout << "[inicio laboratorio] spawn feet=" << pies.x << ',' << pies.y << ',' << pies.z
			<< " eyes=" << ojos.x << ',' << ojos.y << ',' << ojos.z << " feet+2s="
			<< trasDosSegundos.x << ',' << trasDosSegundos.y << ',' << trasDosSegundos.z << '\n';
		comprobar(mundo.IsPlayerGrounded() && casi(trasDosSegundos.z, pies.z, 0.06f),
			"tras dos segundos el jugador sigue apoyado sobre el suelo del laboratorio");
		comprobar(glm::length(glm::vec2(trasDosSegundos.x - pies.x, trasDosSegundos.y - pies.y)) < 0.01f,
			"el jugador de inicio no deriva hacia fuera de la recepción sin entrada");

		const fs::path runtimeRoot = ESCAPETHEUAB_RUNTIME_ASSET_ROOT;
		const fs::path runtimeScene = runtimeRoot / "assets/scenes/aula_informatica_calibrada.json";
		comprobar(fs::is_regular_file(runtimeScene), "la copia de ejecución incluye el JSON de laboratorio actualizado");
		if (fs::is_regular_file(runtimeScene))
		{
			SceneData runtimeData;
			comprobar(LoadSceneDataJson(runtimeScene, runtimeData, error) && runtimeData.spawn.presente &&
				runtimeData.spawn.pies == spawn,
				"la aplicación y CTest encuentran la misma configuración de spawn en el directorio de ejecución");
		}
		for (const char* recurso : { "assets/models/escape_pack/arquitectura/suelo_base.obj", "assets/animations/Puerta_Lab.glb" })
			comprobar(fs::is_regular_file(runtimeRoot / recurso), "los recursos actualizados de la escena están copiados junto al ejecutable");
	}

	// Cambio de habitacion: sin residuos y sin tocar la demo; carga fallida sin efectos.
	void PruebaCambioDeHabitacion()
	{
		PhysicsWorld mundo;
		FallingCubeDemo demo(mundo);
		demo.Start();
		comprobar(mundo.BodyCount() == 2, "la demo aporta 2 cuerpos");

		std::unique_ptr<EscenaMontada> actual = std::make_unique<EscenaMontada>(mundo, RutaDeEscena("habitacion_beta.json"));
		comprobar(actual->ok && mundo.BodyCount() == 22, "demo (2) + habitacion_beta (20) en el mismo mundo");

		// Intento de cargar una escena invalida: la habitacion activa debe seguir intacta.
		// (Un nodo inexistente en el colisionador se descubre al crear los cuerpos, a mitad del proceso.)
		const fs::path temp = fs::temp_directory_path() / "EscapeTheUAB-PhysicsTests-escena";
		fs::create_directories(temp);
		{
			std::string json = "{\"version\":1,\"name\":\"mala\",\"architecture\":[],\"objects\":["
				"{\"id\":\"a\",\"model\":\"" + (fs::path(ESCAPETHEUAB_SOURCE_DIR) / "assets/models/test.obj").generic_string() +
				"\",\"position\":[0,0,0],\"rotation\":[0,0,0],\"scale\":[1,1,1],\"physics\":{\"type\":\"static\",\"halfSize\":[1,1,1]}}],"
				"\"animated\":[{\"id\":\"p\",\"model\":\"" + (fs::path(ESCAPETHEUAB_SOURCE_DIR) / "assets/animations/Puerta.glb").generic_string() +
				"\",\"position\":[0,0,0],\"colliders\":[{\"node\":\"NodoQueNoExiste\",\"type\":\"static\",\"halfSize\":[1,1,1]}]}]}";
			std::ofstream(temp / "mala.json") << json;
		}
		const int antes = mundo.BodyCount();
		EscenaMontada mala(mundo, (temp / "mala.json").string());
		comprobar(!mala.ok, "una escena con un nodo inexistente falla");
		comprobar(mala.error.find("NodoQueNoExiste") != std::string::npos, "el error nombra el nodo que falta");
		comprobar(mundo.BodyCount() == antes, "la carga fallida no deja cuerpos a medio registrar (ni el objeto 'a')");
		comprobar(mala.fisica->BodyCount() == 0, "la EscenaFisica fallida queda vacia");
		comprobar(actual->fisica->BodyCount() == 20, "la habitacion activa sigue intacta");

		// Elegir la animacion por nombre: la que existe se acepta y la que no, no.
		{
			glb::AnimatedScene una;
			std::string e;
			const std::string glbPuerta = (fs::path(ESCAPETHEUAB_SOURCE_DIR) / "assets/animations/Puerta.glb").string();
			comprobar(glb::AddAnimatedObject(una, "x", glbPuerta, glm::vec3(0.0f), 0.0f, e), "cargar la puerta para elegir animacion");
			const std::string nombreReal = una.objetos[0].modelo->animaciones[0].nombre;
			comprobar(glb::SelectAnimation(una.objetos[0], nombreReal, e), "elegir la animacion por su nombre real");
			comprobar(!glb::SelectAnimation(una.objetos[0], "otra", e) && e.find("otra") != std::string::npos,
				"una animacion inexistente da un error con su nombre");
		}

		// Escena con un animacion inexistente falla antes de crear nada.
		{
			std::string json = "{\"version\":1,\"name\":\"mala2\",\"architecture\":[],\"objects\":[],\"animated\":[{\"id\":\"p\",\"model\":\"" +
				(fs::path(ESCAPETHEUAB_SOURCE_DIR) / "assets/animations/Puerta.glb").generic_string() +
				"\",\"position\":[0,0,0],\"animation\":\"NoExiste\"}]}";
			std::ofstream(temp / "mala2.json") << json;
		}
		EscenaMontada mala2(mundo, (temp / "mala2.json").string());
		comprobar(!mala2.ok && mala2.error.find("NoExiste") != std::string::npos, "animacion inexistente: error comprensible");

		// Cambio de habitacion como hace main.cpp: se crea la nueva y SOLO ENTONCES se vacia la anterior.
		EscenaMontada nueva(mundo, RutaDeEscena("ejemplo_fisica.json"));
		comprobar(nueva.ok, "ejemplo_fisica monta");
		const int cuerposNueva = nueva.fisica->BodyCount();
		comprobar(mundo.BodyCount() == 2 + 20 + cuerposNueva, "durante el cambio conviven las dos (aun no se ha vaciado la vieja)");
		actual->fisica->ClearBodies();
		actual.reset();
		comprobar(mundo.BodyCount() == 2 + cuerposNueva, "tras el cambio solo quedan la demo y la habitacion nueva");
		comprobar(demo.IsRunningNow() && mundo.BodyCount() > 0, "la demo no se ve afectada por el cambio de habitacion");

		// Reiniciar la demo no toca los cuerpos de la habitacion.
		demo.Start();
		comprobar(mundo.BodyCount() == 2 + cuerposNueva, "reiniciar la demo no duplica ni borra cuerpos de la habitacion");
		demo.Stop();
		nueva.fisica->ClearBodies();
		comprobar(mundo.BodyCount() == 0, "al vaciar todo no queda ningun cuerpo residual");

		// Las cajas dinamicas del ejemplo caen y terminan quietas sobre la mesa / el suelo.
		EscenaMontada ejemplo(mundo, RutaDeEscena("ejemplo_fisica.json"));
		Avanzar(mundo, 6.0f);
		int quietas = 0;
		for (const CollisionBox& c : mundo.CollisionBoxes())
			if (c.tipo == BodyType::Dynamic && c.pose.position.z > -1.8f && c.pose.position.z < 0.0f) ++quietas;
		comprobar(quietas == 3, "las tres cajas del ejemplo han caido y estan apoyadas");
		for (const CollisionBox& c : mundo.CollisionBoxes())
			if (c.tipo == BodyType::Dynamic) comprobar(c.dormido, "las cajas del ejemplo terminan en reposo");

		std::error_code ec;
		fs::remove_all(temp, ec);
	}
	// ======================================================================
	// Fase 3: controlador del jugador (cápsula de Bullet)
	// ======================================================================

	constexpr float SueloZ = -1.7f;   // cara superior del suelo de la habitación

	// Un suelo grande (cara superior en SueloZ).
	void CrearSuelo(PhysicsWorld& mundo) { mundo.CreateStaticBox({ 0, 0, SueloZ - 0.15f }, { 30, 30, 0.15f }); }

	// Una pared vertical: un plano a x = 'x' de 30 cm de grosor.
	void CrearParedX(PhysicsWorld& mundo, float x) { mundo.CreateStaticBox({ x + 0.15f, 0, 0 }, { 0.15f, 10, 2 }); }

	// El jugador anda con velocidad constante 'velocidad' durante 'segundos', en fotogramas de 'dt'.
	void Andar(PhysicsWorld& mundo, const glm::vec3& speed, float segundos, float dt)
	{
		mundo.SetPlayerVelocity(speed);
		Simular(segundos, dt, [&](float paso) { mundo.Update(paso); });
	}

	void PruebaJugadorSueloYGravedad()
	{
		PhysicsWorld mundo;
		CrearSuelo(mundo);
		comprobar(mundo.CreatePlayerBody({ 0, 0, SueloZ + 1.0f }), "se crea el jugador");
		comprobar(!mundo.CreatePlayerBody({ 0, 0, 0 }), "no se pueden crear dos jugadores");
		comprobar(mundo.HasPlayerBody() && mundo.BodyCount() == 1, "el jugador no cuenta como cuerpo rigido");

		Avanzar(mundo, 0.1f);
		comprobar(mundo.PlayerFeetPosition().z < SueloZ + 1.0f - 0.01f, "con gravedad, el jugador empieza a caer");
		Avanzar(mundo, 2.0f);
		const glm::vec3 pies = mundo.PlayerFeetPosition();
		comprobar(casi(pies.z, SueloZ, 0.06f), "los pies se apoyan en el suelo");
		comprobar(mundo.IsPlayerGrounded(), "JugadorEnSuelo() es verdadero apoyado");
		comprobar(casi(pies.x, 0.0f, 1e-3f) && casi(pies.y, 0.0f, 1e-3f), "cae en vertical (sin deriva horizontal)");

		// Sin pedir movimiento, se queda quieto indefinidamente (no se hunde ni vibra).
		const float alturaAntes = pies.z;
		Avanzar(mundo, 3.0f);
		comprobar(casi(mundo.PlayerFeetPosition().z, alturaAntes, 0.005f), "quieto sobre el suelo no se hunde ni vibra");
	}

	void PruebaJugadorAndaLibre()
	{
		PhysicsWorld mundo;
		CrearSuelo(mundo);
		mundo.CreatePlayerBody({ 0, 0, SueloZ + 0.02f });
		Avanzar(mundo, 0.5f);
		Andar(mundo, { 3, 0, 0 }, 2.0f, PhysicsWorld::PasoFijo);
		// 3 m/s * 2 s = 6 m (sobre suelo llano la distancia es la esperada).
		comprobar(casi(mundo.PlayerFeetPosition().x, 6.0f, 0.1f), "sin obstaculos, anda a 3 m/s");
		comprobar(casi(mundo.PlayerFeetPosition().z, SueloZ, 0.06f), "y se mantiene sobre el suelo");
	}

	void PruebaJugadorChocaConPared()
	{
		PhysicsWorld mundo;
		CrearSuelo(mundo);
		CrearParedX(mundo, 5.0f);   // cara en x = 5
		mundo.CreatePlayerBody({ 3, 0, SueloZ + 0.02f });
		Avanzar(mundo, 0.5f);
		Andar(mundo, { 3, 0, 0 }, 3.0f, PhysicsWorld::PasoFijo);
		const float x = mundo.PlayerFeetPosition().x;
		// Se detiene con la cápsula (radio 0.3) pegada a la pared: x ~ 4.7.
		comprobar(x < 5.0f - PlayerDimensions::Radio + 0.03f, "no atraviesa la pared");
		comprobar(x > 5.0f - PlayerDimensions::Radio - 0.15f, "se detiene junto a la pared (no queda lejos)");
		comprobar(casi(mundo.PlayerFeetPosition().z, SueloZ, 0.06f), "chocar no lo levanta ni lo hunde");

		// Seguir empujando mucho tiempo no lo hace "entrar" poco a poco.
		Andar(mundo, { 3, 0, 0 }, 5.0f, PhysicsWorld::PasoFijo);
		comprobar(mundo.PlayerFeetPosition().x < 5.0f - PlayerDimensions::Radio + 0.03f, "seguir empujando no atraviesa");
	}

	void PruebaJugadorSeAgachaYRespetaTecho()
	{
		PhysicsWorld mundo;
		CrearSuelo(mundo);
		mundo.CreatePlayerBody({ 0, 0, SueloZ + 0.02f });
		Avanzar(mundo, 1.0f);

		const glm::vec3 piesDePie = mundo.PlayerFeetPosition();
		comprobar(mundo.UpdatePlayerCrouch(true, 0.1f), "el jugador empieza a agacharse");
		comprobar(mundo.PlayerHeight() < PlayerDimensions::Altura &&
			mundo.PlayerHeight() > PlayerDimensions::AlturaAgachado,
			"la altura cambia gradualmente en lugar de instantaneamente");
		comprobar(mundo.PlayerEyeHeight() < PlayerDimensions::AlturaOjos,
			"la altura de la camara tambien cambia gradualmente");
		for (int i = 0; i < 10; ++i)
			mundo.UpdatePlayerCrouch(true, 0.1f);
		comprobar(mundo.IsPlayerCrouched(), "el estado del jugador indica que esta agachado");
		comprobar(casi(mundo.PlayerHeight(), PlayerDimensions::AlturaAgachado, 1e-3f),
			"la transicion termina en la altura agachada configurada");
		comprobar(casi(mundo.PlayerFeetPosition().z, piesDePie.z, 1e-3f),
			"agacharse mantiene los pies en el suelo");

		// El techo deja espacio para la cápsula agachada, pero no para la de pie.
		const float techoInferior = SueloZ + PlayerDimensions::AlturaAgachado + 0.15f;
		mundo.CreateStaticBox({ 0, 0, techoInferior + 0.1f }, { 2, 2, 0.1f });
		bool puedeSeguirLevantandose = true;
		for (int i = 0; i < 10 && puedeSeguirLevantandose; ++i)
			puedeSeguirLevantandose = mundo.UpdatePlayerCrouch(false, 0.1f);
		comprobar(!puedeSeguirLevantandose, "el techo impide seguir levantandose");
		comprobar(mundo.IsPlayerCrouched(), "el jugador sigue agachado bajo el techo");

		PhysicsWorld espacioAbierto;
		CrearSuelo(espacioAbierto);
		espacioAbierto.CreatePlayerBody({ 0, 0, SueloZ + 0.02f });
		Avanzar(espacioAbierto, 1.0f);
		for (int i = 0; i < 10; ++i)
			espacioAbierto.UpdatePlayerCrouch(true, 0.1f);
		bool puedeLevantarse = true;
		for (int i = 0; i < 10 && puedeLevantarse; ++i)
			puedeLevantarse = espacioAbierto.UpdatePlayerCrouch(false, 0.1f);
		comprobar(puedeLevantarse, "se puede levantar sin obstaculos");
		comprobar(!espacioAbierto.IsPlayerCrouched(), "el estado vuelve a estar de pie");
	}

	// Agachado suave: curva de Hermite (despacio al empezar y al llegar), pies fijos, sin hundirse ni saltar.
	void PruebaAgachadoSuaveYEstable()
	{
		PhysicsWorld mundo;
		CrearSuelo(mundo);
		mundo.CreatePlayerBody({ 0, 0, SueloZ + 0.02f });
		Avanzar(mundo, 1.0f);
		const float piesZ = mundo.PlayerFeetPosition().z;
		comprobar(mundo.IsPlayerGrounded(), "de pie: en el suelo");

		// Con delta 0 (pausa) no cambia nada.
		mundo.UpdatePlayerCrouch(true, 0.0f);
		comprobar(!mundo.IsPlayerCrouched() && mundo.CrouchFraction() == 0.0f, "delta 0: la postura se congela");

		// Bajar: la altura es monotona, el primer y el ultimo tramo avanzan menos que el central.
		std::vector<float> alturas;
		for (int i = 0; i < 40; ++i)
		{
			mundo.UpdatePlayerCrouch(true, PhysicsWorld::PasoFijo);
			mundo.Update(PhysicsWorld::PasoFijo);
			alturas.push_back(mundo.PlayerHeight());
			comprobar(std::abs(mundo.PlayerFeetPosition().z - piesZ) < 0.01f, "los pies no se mueven al cambiar de postura");
		}
		bool monotona = true;
		for (std::size_t i = 1; i < alturas.size(); ++i) monotona = monotona && alturas[i] <= alturas[i - 1] + 1e-6f;
		comprobar(monotona, "la altura baja siempre, sin rebotes");
		const float primerPaso = PlayerDimensions::Altura - alturas[0];
		const float pasoCentral = alturas[19] - alturas[20];
		comprobar(primerPaso < pasoCentral * 0.3f, "el arranque es mas lento que el tramo central (curva suave)");
		comprobar(mundo.IsPlayerGrounded(), "agachando sigue apoyado en el suelo");

		// Subir con el techo libre hasta de pie: misma altura de pies, y alturas de ojos coherentes.
		for (int i = 0; i < 60; ++i)
		{
			mundo.UpdatePlayerCrouch(false, PhysicsWorld::PasoFijo);
			mundo.Update(PhysicsWorld::PasoFijo);
		}
		comprobar(!mundo.IsPlayerCrouched() && casi(mundo.PlayerHeight(), PlayerDimensions::Altura, 1e-3f), "vuelve a la altura de pie");
		comprobar(std::abs(mundo.PlayerFeetPosition().z - piesZ) < 0.01f, "tras agacharse y levantarse los pies estan donde estaban");
		comprobar(casi(mundo.PlayerEyeHeight(), PlayerDimensions::AlturaOjos, 1e-3f), "los ojos vuelven a 1,70 m");
	}

	// Cambio de postura andando contra un muro y bajo un techo: nunca atraviesa colisionadores.
	void PruebaAgachadoAndandoYTecho()
	{
		PhysicsWorld mundo;
		CrearSuelo(mundo);
		CrearParedX(mundo, 5.0f);
		// Techo bajo (1,3 m) en x > 2: de pie (1,8 m) no cabe, agachado (1,0 m) si.
		mundo.CreateStaticBox({ 3.5f, 0, SueloZ + 1.3f + 0.1f }, { 1.5f, 3, 0.1f });
		mundo.CreatePlayerBody({ 0, 0, SueloZ + 0.02f });
		Avanzar(mundo, 0.5f);

		for (int i = 0; i < 40; ++i)
		{
			mundo.UpdatePlayerCrouch(true, PhysicsWorld::PasoFijo);
			mundo.SetPlayerVelocity({ 1.4f, 0, 0 });
			mundo.Update(PhysicsWorld::PasoFijo);
		}
		Andar(mundo, { 1.4f, 0, 0 }, 3.0f, PhysicsWorld::PasoFijo);
		comprobar(mundo.PlayerFeetPosition().x > 2.5f, "agachado pasa bajo el techo bajo");
		comprobar(mundo.PlayerFeetPosition().x < 5.0f - PlayerDimensions::Radio + 0.03f, "agachado no atraviesa la pared");

		// Soltar Ctrl bajo el techo: no se levanta.
		bool sigue = true;
		for (int i = 0; i < 60; ++i)
		{
			sigue = mundo.UpdatePlayerCrouch(false, PhysicsWorld::PasoFijo) && sigue;
			mundo.Update(PhysicsWorld::PasoFijo);
		}
		comprobar(!sigue && mundo.IsPlayerCrouched(), "bajo el techo bajo se queda agachado");
		comprobar(mundo.CrouchFraction() > 0.5f, "la fraccion de agachado lo refleja");

		// Sale a campo abierto y entonces se levanta.
		Andar(mundo, { -1.4f, 0, 0 }, 3.0f, PhysicsWorld::PasoFijo);
		for (int i = 0; i < 60; ++i)
		{
			mundo.UpdatePlayerCrouch(false, PhysicsWorld::PasoFijo);
			mundo.Update(PhysicsWorld::PasoFijo);
		}
		comprobar(!mundo.IsPlayerCrouched(), "fuera del techo se levanta");
	}

	// El mismo agachado a 30, 60 y 120 FPS acaba en la misma altura en el mismo tiempo.
	void PruebaAgachadoIndependienteDeLosFps()
	{
		float finales[3];
		int k = 0;
		for (int fps : { 30, 60, 120 })
		{
			PhysicsWorld mundo;
			CrearSuelo(mundo);
			mundo.CreatePlayerBody({ 0, 0, SueloZ + 0.02f });
			Avanzar(mundo, 0.5f);
			for (int i = 0; i < fps / 5; ++i)   // 0,2 s
				mundo.UpdatePlayerCrouch(true, 1.0f / fps);
			finales[k++] = mundo.PlayerHeight();
		}
		comprobar(casi(finales[0], finales[1], 2e-3f) && casi(finales[1], finales[2], 2e-3f), "el agachado no depende de los FPS");
	}

	void PruebaJugadorSeDesliza()
	{
		PhysicsWorld mundo;
		CrearSuelo(mundo);
		CrearParedX(mundo, 5.0f);
		mundo.CreatePlayerBody({ 3, 0, SueloZ + 0.02f });
		Avanzar(mundo, 0.5f);
		// Diagonal contra la pared: la componente X se anula, la Y debe continuar (deslizar).
		Andar(mundo, { 2.1f, 2.1f, 0 }, 2.0f, PhysicsWorld::PasoFijo);
		const glm::vec3 pies = mundo.PlayerFeetPosition();
		comprobar(pies.x < 5.0f - PlayerDimensions::Radio + 0.03f, "diagonal: no atraviesa la pared");
		comprobar(pies.y > 2.5f, "diagonal: sigue avanzando a lo largo de la pared (desliza)");
		comprobar(pies.y < 2.1f * 2.0f + 0.1f, "diagonal: no avanza mas de lo que pide");
	}

	void PruebaJugadorChocaConObjetoEstatico()
	{
		PhysicsWorld mundo;
		CrearSuelo(mundo);
		// Una mesa/armario: caja de 1 x 1 x 1 m con la base en el suelo, centrada en (5, 0).
		mundo.CreateStaticBox({ 5, 0, SueloZ + 0.5f }, { 0.5f, 0.5f, 0.5f });
		mundo.CreatePlayerBody({ 2, 0, SueloZ + 0.02f });
		Avanzar(mundo, 0.5f);
		Andar(mundo, { 3, 0, 0 }, 3.0f, PhysicsWorld::PasoFijo);
		comprobar(mundo.PlayerFeetPosition().x < 4.5f - PlayerDimensions::Radio + 0.03f, "no atraviesa un objeto estatico");
		comprobar(mundo.PlayerFeetPosition().z < SueloZ + 0.1f, "y no trepa encima (altura de escalon limitada)");

		// Rodearlo: yendo en diagonal acaba pasando por un lado.
		Andar(mundo, { 0, 3, 0 }, 1.0f, PhysicsWorld::PasoFijo);
		Andar(mundo, { 3, 0, 0 }, 2.0f, PhysicsWorld::PasoFijo);
		comprobar(mundo.PlayerFeetPosition().x > 6.0f, "se puede rodear el objeto");
	}

	// Mismo recorrido con distintos FPS: mismo resultado y nunca atraviesa.
	void PruebaJugadorIndependienteDeLosFps()
	{
		float finalX[4];
		const float dts[4] = { 1.0f / 144.0f, 1.0f / 60.0f, 1.0f / 15.0f, 0.25f };   // 0.25 s = fotograma de 4 FPS
		for (int i = 0; i < 4; ++i)
		{
			PhysicsWorld mundo;
			CrearSuelo(mundo);
			CrearParedX(mundo, 5.0f);
			mundo.CreatePlayerBody({ 3, 0, SueloZ + 0.02f });
			Avanzar(mundo, 0.5f);
			Andar(mundo, { 3, 0, 0 }, 4.0f, dts[i]);
			finalX[i] = mundo.PlayerFeetPosition().x;
			comprobar(finalX[i] < 5.0f - PlayerDimensions::Radio + 0.03f, "a cualquier FPS no atraviesa la pared");
		}
		comprobar(casi(finalX[0], finalX[3], 0.03f) && casi(finalX[1], finalX[2], 0.03f) && casi(finalX[0], finalX[1], 0.03f),
			"el punto donde se detiene no depende de los FPS");

		// Y en movimiento libre recorre la misma distancia (salvo el redondeo de fotogramas).
		PhysicsWorld lento, rapido;
		for (PhysicsWorld* m : { &lento, &rapido }) { CrearSuelo(*m); m->CreatePlayerBody({ 0, 0, SueloZ + 0.02f }); Avanzar(*m, 0.5f); }
		Andar(lento, { 3, 0, 0 }, 2.0f, 0.1f);
		Andar(rapido, { 3, 0, 0 }, 2.0f, 1.0f / 144.0f);
		comprobar(casi(lento.PlayerFeetPosition().x, rapido.PlayerFeetPosition().x, 0.1f), "misma distancia a 10 FPS y a 144 FPS");

		// Un tirón enorme (10 s de fotograma) se recorta: ni se teletransporta ni atraviesa.
		PhysicsWorld tiron;
		CrearSuelo(tiron);
		CrearParedX(tiron, 5.0f);
		tiron.CreatePlayerBody({ 4.0f, 0, SueloZ + 0.02f });
		Avanzar(tiron, 0.5f);
		tiron.SetPlayerVelocity({ 3, 0, 0 });
		tiron.Update(10.0f);
		comprobar(tiron.PlayerFeetPosition().x < 5.0f - PlayerDimensions::Radio + 0.03f, "un fotograma de 10 s no lo lanza a traves de la pared");
	}

	void PruebaJugadorPausa()
	{
		PhysicsWorld mundo;
		CrearSuelo(mundo);
		mundo.CreatePlayerBody({ 0, 0, SueloZ + 1.0f });   // en el aire: cae
		Avanzar(mundo, 0.1f);
		mundo.SetPaused(true);
		const glm::vec3 enPausa = mundo.PlayerFeetPosition();
		mundo.SetPlayerVelocity({ 3, 0, 0 });
		Simular(2.0f, 0.016f, [&](float dt) { mundo.Update(dt); });
		comprobar(mundo.PlayerFeetPosition() == enPausa, "en pausa el jugador no se mueve ni cae");
		mundo.SetPaused(false);
		Avanzar(mundo, 0.2f);
		comprobar(mundo.PlayerFeetPosition().x > enPausa.x + 0.3f && mundo.PlayerFeetPosition().z < enPausa.z, "al reanudar anda y cae");
		// Teletransporte: coloca al jugador y resetea la caida.
		mundo.TeleportPlayer({ 7, 8, SueloZ + 0.02f });
		mundo.SetPlayerVelocity({ 0, 0, 0 });
		Avanzar(mundo, 0.5f);
		comprobar(Cerca(mundo.PlayerFeetPosition(), { 7, 8, SueloZ }, 0.08f), "TeletransportarJugador lo coloca donde se pide");
		mundo.RemovePlayerBody();
		comprobar(!mundo.HasPlayerBody(), "EliminarJugador lo quita");
	}

	// Recorre la puerta 'puerta_2' de habitacion_beta en el eje X (hueco en x = 6, y = 3). (La puerta_1 queda
	// medio tapada por table_right, asi que se usa la 2, que esta despejada.)
	float AndarPorLaPuerta(EscenaMontada& habitacion, PhysicsWorld& mundo, bool abierta, float dt)
	{
		glb::AnimatedScene& anim = habitacion.animados;
		if (abierta)
		{
			glb::ToggleDoor(anim.objetos[1]);
			for (int i = 0; i < 300; ++i)   // 5 s: de sobra para abrirse del todo
			{
				glb::UpdateAnimatedObjects(anim, PhysicsWorld::PasoFijo);
				habitacion.fisica->SyncKinematics(anim);
				mundo.Update(PhysicsWorld::PasoFijo);
			}
		}
		mundo.CreatePlayerBody({ 4.5f, abierta ? 3.04f : 3.0f, SueloZ + 0.02f });   // abierta: la hoja ocupa el lado y<2.68, se pasa por el centro del hueco libre
		Avanzar(mundo, 0.5f);
		mundo.SetPlayerVelocity({ 3, 0, 0 });
		Simular(3.0f, dt, [&](float paso) {
			habitacion.fisica->SyncKinematics(anim);
			mundo.Update(paso);
		});
		return mundo.PlayerFeetPosition().x;
	}

	void PruebaJugadorYPuertas()
	{
		// Puerta CERRADA: la hoja bloquea el paso.
		{
			PhysicsWorld mundo;
			EscenaMontada habitacion(mundo, RutaDeEscena("habitacion_beta.json"));
			const float x = AndarPorLaPuerta(habitacion, mundo, false, PhysicsWorld::PasoFijo);
			comprobar(x < 6.0f - PlayerDimensions::Radio + 0.05f, "puerta cerrada: el jugador no la atraviesa");
			comprobar(x > 5.0f, "puerta cerrada: llega hasta ella");
		}
		// Puerta ABIERTA: pasa por el hueco con holgura.
		{
			PhysicsWorld mundo;
			EscenaMontada habitacion(mundo, RutaDeEscena("habitacion_beta.json"));
			const float x = AndarPorLaPuerta(habitacion, mundo, true, PhysicsWorld::PasoFijo);
			comprobar(x > 7.5f, "puerta abierta: el jugador cruza el hueco");
		}
		// Lo mismo con 8 FPS: la hoja cerrada tampoco se atraviesa.
		{
			PhysicsWorld mundo;
			EscenaMontada habitacion(mundo, RutaDeEscena("habitacion_beta.json"));
			const float x = AndarPorLaPuerta(habitacion, mundo, false, 0.125f);
			comprobar(x < 6.0f - PlayerDimensions::Radio + 0.05f, "puerta cerrada a 8 FPS: tampoco la atraviesa");
		}
		// Una puerta que se CIERRA sobre el jugador lo empuja con suavidad: sin saltos bruscos.
		{
			PhysicsWorld mundo;
			EscenaMontada habitacion(mundo, RutaDeEscena("habitacion_beta.json"));
			glb::AnimatedScene& anim = habitacion.animados;
			glb::ToggleDoor(anim.objetos[1]);
			for (int i = 0; i < 300; ++i) { glb::UpdateAnimatedObjects(anim, PhysicsWorld::PasoFijo); habitacion.fisica->SyncKinematics(anim); mundo.Update(PhysicsWorld::PasoFijo); }
			// La puerta abierta queda tumbada hacia -X (x de 5.14 a 5.98, y ~ 2.6). Al cerrarse, la hoja
			// barre el cuarto de circulo que hay delante: ponemos al jugador en medio de ese recorrido.
			mundo.CreatePlayerBody({ 5.6f, 3.1f, SueloZ + 0.02f });
			Avanzar(mundo, 0.5f);
			const glm::vec3 inicio = mundo.PlayerFeetPosition();
			comprobar(Cerca(inicio, { 5.6f, 3.1f, SueloZ }, 0.08f), "el jugador esta en el recorrido de la hoja y la puerta abierta no lo mueve");
			glb::ToggleDoor(anim.objetos[1]);   // cerrar
			glm::vec3 anterior = mundo.PlayerFeetPosition();
			float saltoMaximo = 0.0f;
			for (int i = 0; i < 300; ++i)
			{
				glb::UpdateAnimatedObjects(anim, PhysicsWorld::PasoFijo);
				habitacion.fisica->SyncKinematics(anim);
				mundo.Update(PhysicsWorld::PasoFijo);
				const glm::vec3 ahora = mundo.PlayerFeetPosition();
				saltoMaximo = std::max(saltoMaximo, glm::length(ahora - anterior));
				anterior = ahora;
			}
			// Al terminar, la hoja cerrada (x 5.973..6.053, y 2.6..3.4) y la capsula no se solapan.
			const float fueraX = std::max({ 5.973f - anterior.x, 0.0f, anterior.x - 6.053f });
			const float fueraY = std::max({ 2.6f - anterior.y, 0.0f, anterior.y - 3.4f });
			comprobar(std::hypot(fueraX, fueraY) > PlayerDimensions::Radio - 0.04f, "la puerta que se cierra saca al jugador del volumen de la hoja");
			comprobar(glm::length(glm::vec3(anterior.x - inicio.x, anterior.y - inicio.y, 0.0f)) > 0.05f, "...porque efectivamente lo empuja");
			// Mientras la hoja barre, el empujon es de 2-5 cm por paso (a ~1.5-3 m/s). Al final, cuando el jugador
			// queda apretado entre la hoja y el poste, hay un unico pico (~11 cm medidos). Se exige que siempre
			// sea menor que el radio de la capsula (25 cm): nunca se salta paredes ni "se teletransporta".
			comprobar(saltoMaximo < PlayerDimensions::Radio * 0.6f, "la puerta que se cierra no teletransporta al jugador (salto por paso < 15 cm)");
			comprobar(std::isfinite(anterior.x) && casi(anterior.z, SueloZ, 0.1f), "tras cerrarse, el jugador sigue en el suelo y en una posicion valida");
			std::cout << "  (info) cierre sobre el jugador: salto maximo " << saltoMaximo << " m, posicion final "
				<< anterior.x << ", " << anterior.y << "\n";
		}
	}

	// habitacion_beta completa: un recorrido hacia las paredes y objetos reales.
	void PruebaJugadorEnHabitacionBeta()
	{
		const float limitesX[2] = { -3.0f + 0.15f, 13.0f - 0.15f };   // caras interiores de las paredes laterales
		PhysicsWorld mundo;
		EscenaMontada habitacion(mundo, RutaDeEscena("habitacion_beta.json"));
		mundo.CreatePlayerBody({ 10.0f, 0.0f, SueloZ + 0.02f });   // posicion de inicio del juego
		Avanzar(mundo, 0.5f);
		comprobar(mundo.IsPlayerGrounded(), "habitacion_beta: el jugador empieza apoyado en el suelo");
		Andar(mundo, { 3, 0, 0 }, 3.0f, 1.0f / 60.0f);
		comprobar(mundo.PlayerFeetPosition().x < limitesX[1] - PlayerDimensions::Radio + 0.03f, "habitacion_beta: la pared derecha lo detiene");
		Andar(mundo, { 0, 3, 0 }, 4.0f, 1.0f / 60.0f);
		comprobar(mundo.PlayerFeetPosition().y < 5.0f - 0.15f - PlayerDimensions::Radio + 0.03f, "habitacion_beta: la pared delantera lo detiene");
		// Hacia el armario (-1, 3.3): lo cruza en diagonal sin atravesarlo.
		Andar(mundo, { -3, 0, 0 }, 8.0f, 1.0f / 60.0f);
		const glm::vec3 pies = mundo.PlayerFeetPosition();
		comprobar(pies.x > -3.0f + 0.15f + PlayerDimensions::Radio - 0.03f, "habitacion_beta: la pared izquierda lo detiene");
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
		const glm::mat4 izq = stereo::EyeViewMatrix(central, stereo::Eye::Left);
		const glm::mat4 der = stereo::EyeViewMatrix(central, stereo::Eye::Right);
		const glm::mat4 invC = glm::inverse(central), invI = glm::inverse(izq), invD = glm::inverse(der);
		const glm::vec3 derechaCamara = glm::normalize(glm::vec3(invC[0]));

		comprobar(Cerca(glm::vec3(invI[3]), glm::vec3(invC[3]) - derechaCamara * 0.032f, 1e-4f), "ojo izquierdo: 3,2 cm a la izquierda del centro");
		comprobar(Cerca(glm::vec3(invD[3]), glm::vec3(invC[3]) + derechaCamara * 0.032f, 1e-4f), "ojo derecho: 3,2 cm a la derecha del centro");
		comprobar(casi(glm::distance(glm::vec3(invI[3]), glm::vec3(invD[3])), 0.064f, 1e-4f), "separacion entre ojos de 6,4 cm");
		comprobar(Cerca(glm::vec3(invI[2]), glm::vec3(invC[2]), 1e-5f) && Cerca(glm::vec3(invD[2]), glm::vec3(invC[2]), 1e-5f),
			"ambos ojos miran en la misma direccion que la camara (el estereo no gira la cabeza)");
		comprobar(Cerca(glm::vec3(stereo::EyeViewMatrix(central, stereo::Eye::Left, 0.0f)[3]), glm::vec3(central[3]), 1e-6f),
			"con separacion 0 la vista del ojo es la central");
		comprobar(stereo::ScreenHalf(stereo::Eye::Left, 1280).x == 0 && stereo::ScreenHalf(stereo::Eye::Right, 1280).x == 640 &&
			stereo::ScreenHalf(stereo::Eye::Right, 1280).ancho == 640, "cada ojo ocupa una mitad de la pantalla");
	}

	// Indice de un objeto de la escena por su id (-1 si no esta).
	int IndexOf(const EscenaMontada& e, const char* id)
	{
		for (std::size_t i = 0; i < e.datos.objetos.size(); ++i) if (e.datos.objetos[i].id == id) return static_cast<int>(i);
		return -1;
	}

	void PruebaAgarreConBullet()
	{
		PhysicsWorld mundo;
		EscenaMontada sala(mundo, RutaDeEscena("habitacion_beta.json"));
		comprobar(sala.ok, "habitacion_beta monta para las pruebas de agarre");
		if (!sala.ok) return;
		const int grande = IndexOf(sala, "box_large");
		const int pared = IndexOf(sala, "wall_front");
		comprobar(grande >= 0 && sala.fisica->DynamicCount() == 2, "box_large y box_small son los objetos agarrables");
		const int cuerposIniciales = mundo.BodyCount();

		// Jugador a 1,2 m de la caja; los ojos quedan a 1,7 m sobre sus pies.
		mundo.CreatePlayerBody({ 5.8f, 2.0f, SueloZ + 0.02f });
		Avanzar(mundo, 1.0f);
		const glm::vec3 ojos = mundo.PlayerFeetPosition() + glm::vec3(0, 0, PlayerDimensions::AlturaOjos);
		const glm::vec3 haciaCaja = glm::vec3(7, 2, -1.3f) - ojos;

		// 1) Seleccion: solo lo agarrable, y solo si se ve de verdad.
		comprobar(sala.fisica->TryGrab(ojos, glm::vec3(-1, 0, -0.3f)) == -1, "mirando una pared/vacio no se agarra nada");
		comprobar(sala.fisica->TryGrab(ojos, glm::vec3(0, 0, -1)) == -1, "mirando al suelo (estatico) no se agarra nada");
		comprobar(!sala.fisica->HasGrab(), "tras los intentos fallidos la mano sigue vacia");
		comprobar(sala.fisica->TryGrab(glm::vec3(5.8f, 2.0f, -1.2f) + glm::vec3(0, 0, 0), glm::vec3(0, 1, 0)) == -1,
			"mirando una mesa/mueble estatico o una puerta no se agarra nada");
		comprobar(sala.fisica->TryGrab(ojos, haciaCaja * 3.0f) == grande && sala.fisica->HasGrab() && sala.fisica->GrabbedObject() == grande,
			"mirando box_large a 1,8 m se agarra");
		comprobar(mundo.BodyCount() == cuerposIniciales, "agarrar no crea ni borra cuerpos");
		sala.fisica->ReleaseGrab();

		// El agarre de un cliente vive en la sujeción autoritativa del host, no en cuerpoSujeto local.
		const glm::vec3 destinoAgarre = PhysicsScene::GrabDestination(ojos, haciaCaja);
		comprobar(sala.fisica->HoldRemoteDynamic(grande, ojos, haciaCaja),
			"el host sujeta remotamente el mismo cuerpo dinámico que seleccionó el rayo del cliente");
		comprobar(sala.fisica->HeldDynamicObject(grande) && !sala.fisica->HasGrab() &&
			 sala.fisica->GrabbedObject() == -1,
			"la sujeción remota se detecta sin fingir que pertenece a la mano local del host");
		sala.fisica->UpdateRemoteGrab(grande, destinoAgarre);
		Avanzar(mundo, 1.5f);
		glm::vec3 posicionRemota(0.0f);
		comprobar(sala.fisica->DynamicObjectPosition(grande, posicionRemota) &&
			 Cerca(posicionRemota, destinoAgarre, 0.12f),
			"el agarre remoto usa el mismo punto delante del rayo que el agarre local");
		comprobar(sala.fisica->ReleaseRemoteDynamic(grande, glm::vec3(0.0f)) &&
			!sala.fisica->HeldDynamicObject(grande),
			"soltar el agarre remoto recupera la gravedad y elimina la restricción autoritativa");

		// Alcance: desde 4 m no llega.
		const glm::vec3 lejos = glm::vec3(3.0f, 2.0f, -0.3f);
		comprobar(sala.fisica->TryGrab(lejos, glm::vec3(7, 2, -1.3f) - lejos) == -1, "fuera de alcance (2 m) no se agarra");

		// 2) Agarre: la caja SIGUE a la mano siendo el mismo cuerpo de Bullet, sin gravedad.
		comprobar(sala.fisica->TryGrab(ojos, haciaCaja) == grande, "agarrar box_large");
		const glm::vec3 mirada(1, 0, 0);
		for (int i = 0; i < 90; ++i) { sala.fisica->UpdateGrab(ojos, mirada); mundo.Update(PhysicsWorld::PasoFijo); }
		const IdCuerpo cuerpo = mundo.HeldBody();
		const glm::vec3 enLaMano = mundo.PoseOf(cuerpo).position;
		comprobar(Cerca(enLaMano, mundo.HoldDestination(), 0.05f), "la caja llega al punto de la mano");
		comprobar(enLaMano.z > -1.0f, "sujeta no cae por la gravedad (esta mas alta que en el suelo)");
		const glm::mat4 dibujo = sala.fisica->VisualMatrixOf(grande, sala.datos.objetos[grande]);
		comprobar(Cerca(glm::vec3(dibujo[3]), enLaMano, 0.01f), "lo que se dibuja es exactamente la posicion de Bullet");
		// El objeto de la mano no empuja al jugador (la capsula lo ve como obstaculo, pero esta a >0,6 m).
		comprobar(Cerca(mundo.PlayerFeetPosition(), glm::vec3(5.8f, 2.0f, SueloZ), 0.1f), "llevar un objeto no desplaza al jugador");

		// 3) Pausa: mundo congelado, el agarre no se pierde.
		mundo.SetPaused(true);
		const glm::vec3 enPausa = mundo.PoseOf(cuerpo).position;
		for (int i = 0; i < 30; ++i) { sala.fisica->UpdateGrab(ojos, glm::vec3(0, 1, 0)); mundo.Update(PhysicsWorld::PasoFijo); }
		comprobar(mundo.PoseOf(cuerpo).position == enPausa && sala.fisica->HasGrab(), "en pausa el objeto no se mueve y sigue en la mano");
		mundo.SetPaused(false);

		// 4) Soltar: recupera la gravedad y cae al suelo, quieto y sin atravesarlo.
		sala.fisica->ReleaseGrab();
		comprobar(!sala.fisica->HasGrab() && mundo.HeldBody() == CuerpoInvalido, "soltar vacia la mano");
		Avanzar(mundo, 4.0f);
		const glm::vec3 reposo = mundo.PoseOf(cuerpo).position;
		comprobar(casi(reposo.z, SueloZ + 0.4f, 0.03f), "soltada, la caja cae y se apoya en el suelo (recupera la gravedad)");
		comprobar(mundo.IsAsleep(cuerpo) || glm::length(mundo.VelocityOf(cuerpo)) < 0.05f, "y se queda quieta");
		comprobar(mundo.BodyCount() == cuerposIniciales, "ni duplicados ni perdidos tras agarrar y soltar");
		(void)pared;
	}

	// La mano atraviesa una pared: el objeto se queda en este lado (Bullet lo para), nunca la cruza.
	void PruebaAgarreContraPared()
	{
		PhysicsWorld mundo;
		PhysicsScene escena(mundo);
		std::string error;
		CrearSuelo(mundo);   // fuera de la escena: suelo propio
		const SceneObject pared = ObjetoCaja("pared", { 5.15f, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }, BodyType::Static, { 0.15f, 5, 2 }, { 0, 0, 0 }, 0.0f);
		const SceneObject caja = ObjetoCaja("caja", { 3.5f, 0, SueloZ + 0.2f }, { 0, 0, 0 }, { 0.2f, 0.2f, 0.2f }, BodyType::Dynamic, { 1, 1, 1 }, { 0, 0, 0 }, 2.0f);
		comprobar(Create(escena, { pared, caja }, error), "crear pared y caja");
		Avanzar(mundo, 0.5f);
		const glm::vec3 ojos(2.5f, 0, SueloZ + 0.2f);   // ojos a media altura de la caja, solo para el rayo
		comprobar(escena.TryGrab(ojos, glm::vec3(1, 0, 0)) == 1, "la caja se agarra a 1 m");
		// La mano se mueve 3 m a traves de la pared (hasta x = 6.5): Bullet debe frenar la caja.
		const glm::vec3 manoDetrasDeLaPared(6.5f, 0, SueloZ + 0.2f);
		mundo.SetHoldDestination(manoDetrasDeLaPared);
		float xMax = 0.0f;
		for (int i = 0; i < 120; ++i)
		{
			mundo.SetHoldDestination(manoDetrasDeLaPared);
			mundo.Update(PhysicsWorld::PasoFijo);
			xMax = std::max(xMax, mundo.PoseOf(mundo.HeldBody() == CuerpoInvalido ? 2 : mundo.HeldBody()).position.x);
		}
		comprobar(mundo.HeldBody() != CuerpoInvalido, "la caja sigue sujeta (el auto-soltado lo decide ActualizarAgarre)");
		const IdCuerpo cuerpo = mundo.HeldBody();
		const float x = mundo.PoseOf(cuerpo).position.x;
		// Cara de la pared: x = 5.0. La caja mide 0,4 (mitad 0,2): su centro no puede pasar de 4,8 (+ margen de contacto).
		comprobar(x < 5.0f - 0.2f + 0.05f, "la caja contra la pared no la atraviesa");
		std::cout << "  (info) agarre contra pared: x maximo del centro " << xMax << " (cara de la pared en 5.0, mitad de la caja 0.2)\n";
		comprobar(xMax < 4.95f, "ni siquiera un instante: la penetracion maxima es de pocos centimetros");

		// ActualizarAgarre suelta el objeto atascado cuando la mano queda a mas de 2 m.
		escena.UpdateGrab(glm::vec3(6.5f, 0, SueloZ + 0.2f), glm::vec3(1, 0, 0));   // mano a ~7,6 m: a 2,8 m de la caja, mas de 2 m
		comprobar(!escena.HasGrab(), "un objeto atascado lejos de la mano se suelta solo");
	}

	void PruebaSeleccionInteraccionConObstruccion()
	{
		PhysicsWorld mundo;
		PhysicsScene escena(mundo);
		std::string error;
		const SceneObject pared = ObjetoCaja("pared", { 4.0f, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 },
			BodyType::Static, { 0.15f, 2, 2 }, { 0, 0, 0 }, 0.0f);
		const SceneObject caja = ObjetoCaja("caja", { 6.0f, 0, 0 }, { 0, 0, 0 }, { 0.25f, 0.25f, 0.25f },
			BodyType::Dynamic, { 1, 1, 1 }, { 0, 0, 0 }, 1.0f);
		comprobar(Create(escena, { pared, caja }, error), "crear una caja interactuable tras una pared");

		const glm::vec3 origen(2.0f, 0.0f, 0.0f);
		const IdCuerpo primerImpacto = mundo.BodyAtRayEnd(origen, glm::vec3(1, 0, 0), 5.0f);
		comprobar(primerImpacto != CuerpoInvalido && escena.DynamicObjectOfBody(primerImpacto) == -1,
			"el primer impacto es la pared, no el objeto dinamico que queda detras");
		comprobar(escena.TryGrab(origen, glm::vec3(1, 0, 0)) == -1 && !escena.HasGrab(),
			"no se puede recoger un objeto a traves de la pared");

		const glm::vec3 origenVisible(4.2f, 0.0f, 0.0f);
		const IdCuerpo cajaVisible = mundo.BodyAtRayEnd(origenVisible, glm::vec3(1, 0, 0), 2.0f);
		comprobar(escena.DynamicObjectOfBody(cajaVisible) == 1,
			"el mismo raycast identifica la caja cuando queda visible y en rango");
		comprobar(escena.TryGrab(origenVisible, glm::vec3(1, 0, 0)) == 1,
			"la caja visible se puede recoger");
	}

	// Cambiar de habitacion con un objeto en la mano: se suelta y no queda ninguna referencia colgando.
	void PruebaCambioDeHabitacionConObjetoAgarrado()
	{
		PhysicsWorld mundo;
		auto sala = std::make_unique<EscenaMontada>(mundo, RutaDeEscena("habitacion_beta.json"));
		const int grande = IndexOf(*sala, "box_large");
		const glm::vec3 ojos(5.8f, 2.0f, 0.0f);
		comprobar(sala->fisica->TryGrab(ojos, glm::vec3(7, 2, -1.3f) - ojos) == grande, "agarrar antes de cambiar de sala");
		comprobar(mundo.HeldBody() != CuerpoInvalido, "hay un cuerpo sujeto");

		EscenaMontada nueva(mundo, RutaDeEscena("ejemplo_fisica.json"));   // como cargarYAplicarEscena: primero la nueva...
		comprobar(nueva.ok, "la sala nueva monta");
		sala->fisica->ClearBodies();                                            // ...y luego se vacia la vieja
		comprobar(mundo.HeldBody() == CuerpoInvalido && !sala->fisica->HasGrab(), "al vaciar la sala vieja el agarre desaparece");
		sala.reset();
		comprobar(!nueva.fisica->HasGrab(), "la sala nueva empieza con la mano vacia");
		comprobar(mundo.BodyCount() == nueva.fisica->BodyCount(), "no quedan cuerpos de la sala anterior");
		Avanzar(mundo, 1.0f);   // simular sin referencias colgando no debe fallar ni mover nada raro
		comprobar(mundo.BodyCount() == nueva.fisica->BodyCount(), "sigue sin haber residuos");

		// Eliminar el cuerpo sujeto directamente tambien limpia el agarre (sin punteros colgando).
		const glm::vec3 ojos2(5.0f, -4.0f, 0.0f);
		PhysicsWorld otro;
		const IdCuerpo c = otro.CreateDynamicBox({ 0, 0, 0 }, glm::vec3(0.2f), 1.0f);
		comprobar(otro.HoldBody(c) && otro.HeldBody() == c, "sujetar un cuerpo dinamico");
		otro.RemoveBody(c);
		comprobar(otro.HeldBody() == CuerpoInvalido, "eliminar el cuerpo sujeto limpia el agarre");
		const IdCuerpo estatico = otro.CreateStaticBox({ 0, 0, 0 }, glm::vec3(1.0f));
		comprobar(!otro.HoldBody(estatico) && !otro.HoldBody(CuerpoInvalido), "no se puede sujetar un estatico ni un id invalido");
		(void)ojos2;
	}

	void PruebaRebotadorYEmpujeDelJugador()
	{
		PhysicsWorld mundo;
		mundo.CreateStaticBox({ 0.0f, 0.0f, -0.15f }, { 3.0f, 3.0f, 0.15f });
		const IdCuerpo esfera = mundo.CreateSphere(BodyType::Bouncer, { 0.0f, 0.0f, 0.2f }, 0.2f, 0.25f);
		comprobar(esfera != CuerpoInvalido, "se crea la esfera rebotadora");
		mundo.ApplyImpulseToBody(esfera, { 0.0f, 0.0f, 0.75f });
		int botes = 0;
		float velocidadZAnterior = mundo.VelocityOf(esfera).z;
		Simular(6.0f, PhysicsWorld::PasoFijo, [&](float delta) {
			mundo.Update(delta);
			const float velocidadZ = mundo.VelocityOf(esfera).z;
			if (velocidadZAnterior < -0.1f && velocidadZ > 0.1f) ++botes;
			velocidadZAnterior = velocidadZ;
		});
		comprobar(botes >= game::HiritiAnimator::NumeroBotes,
			"Hiriti conserva siete botes perceptibles con restitucion alta");

		PhysicsWorld mundoConJugador;
		mundoConJugador.CreateStaticBox({ 0.0f, 0.0f, -0.15f }, { 3.0f, 3.0f, 0.15f });
		const IdCuerpo empujable = mundoConJugador.CreateSphere(
			BodyType::Bouncer, { 0.0f, 0.0f, 0.2f }, 0.2f, 0.25f);
		comprobar(mundoConJugador.CreatePlayerBody({ -0.8f, 0.0f, 0.0f }),
			"se crea el jugador para probar el empuje");
		mundoConJugador.SetPlayerVelocity({ 2.0f, 0.0f, 0.0f });
		Simular(0.8f, PhysicsWorld::PasoFijo, [&](float delta) { mundoConJugador.Update(delta); });
		comprobar(mundoConJugador.PoseOf(empujable).position.x > 0.1f,
			"caminar hacia Hiriti lo empuja y lo desplaza");
	}

	void PruebaEsferaDinamicaConOffsetYRotacion()
	{
		PhysicsWorld mundo;
		PhysicsScene escena(mundo);
		SceneObject esfera = ObjetoCaja("esfera", { 1.0f, 2.0f, 3.0f }, { 20.0f, 35.0f, 10.0f },
			{ 0.2f, 0.2f, 0.2f }, BodyType::Bouncer, glm::vec3(1.0f), { 0.0f, 0.0f, 0.75f }, 0.25f);
		esfera.fisica.forma = BodyShape::Sphere;
		std::string error;
		const bool creada = Create(escena, { esfera }, error);
		comprobar(creada, "se crea una esfera dinámica rotada con offset");
		if (!creada) return;
		const auto cuerpos = mundo.CollisionBoxes();
		comprobar(cuerpos.size() == 1 && cuerpos[0].tipo == BodyType::Bouncer,
			"la autoridad conserva el tipo Rebotador de Hiriti");

		const glm::mat4 modelo = escena.VisualMatrixOf(0, esfera);
		const glm::mat4 esperado = ObjectMatrix(esfera);
		comprobar(glm::length(glm::vec3(modelo[0]) - glm::vec3(esperado[0])) < 1e-5f &&
			glm::length(glm::vec3(modelo[1]) - glm::vec3(esperado[1])) < 1e-5f &&
			glm::length(glm::vec3(modelo[2]) - glm::vec3(esperado[2])) < 1e-5f &&
			glm::length(glm::vec3(modelo[3]) - glm::vec3(esperado[3])) < 1e-5f,
			"la esfera conserva la orientación y posición inicial del modelo");
	}
}

int main()
{
	PruebaRecursoIconoInteraccion();
	PruebaGravedad();
	PruebaAceleracionHorizontal();
	PruebaCaidaLibre();
	PruebaPasoFijo();
	PruebaIndependenciaDeLosFps();
	PruebaPausa();
	PruebaSuelo();
	PruebaMasaInvalida();
	PruebaLanzamientoDeObjetoAgarrado();
	PruebaColisionadorTransformado();
	PruebaEstaticoNoCae();
	PruebaReconstruccionCambioEstaticoDinamico();
	PruebaSujecionesMultiplesYReplicaCinematica();
	PruebaDinamicoCaeYChoca();
	PruebaPuertasDesdeJson();
	PruebaParedParametricaConPuerta();
	PruebaSueloLaboratorioCalibrado();
	PruebaArranqueEnSpawnDelLaboratorio();
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
	PruebaRebotadorYEmpujeDelJugador();
	PruebaEsferaDinamicaConOffsetYRotacion();

	if (fallos == 0) std::cout << "PhysicsTests: todas las pruebas pasan\n";
	else std::cerr << "PhysicsTests: " << fallos << " fallo(s)\n";
	return fallos == 0 ? 0 : 1;
}
