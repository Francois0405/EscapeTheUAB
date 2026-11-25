
//******** PRACTICA VISUALITZACIÓ GRÀFICA INTERACTIVA (Escola Enginyeria - UAB)
//******** Entorn bàsic VS2026 MONOFINESTRA amb OpenGL 4.6, interfície GLFW 3.4, ImGui i llibreries GLM
//******** Ferran Poveda, Marc Vivet, Carme Julià, Débora Gil, Enric Martí (Setembre 2026)
// main.cpp : Este archivo contiene la función "main". La ejecución del programa comienza y termina ahí.
//    Versió 0.5:	- Interficie ImGui
//					- Per a dialeg de cerca de fitxers, s'utilitza la llibreria NativeFileDialog
//

// Entorn VGI.ImGui: Includes llibreria ImGui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "file_dialog_compat.h"
#include "framework.h"
#include "src/rendering/shader.h"
#include "src/rendering/visualitzacio.h"
#include "src/rendering/LegacyVgiGeometryControls.h"
#include "src/rendering/LegacyVgiSceneRenderer.h"
#include "src/rendering/LegacyVgiViewControls.h"
#include "src/rendering/RuntimeRenderControls.h"
#include "src/rendering/RuntimeRenderer.h"
#include "src/scene/escena.h"
#include "main.h"
#include "src/scene/SceneLoader.h"
#include "src/scene/SceneRuntime.h"
#include "src/animation/AnimacionPuerta.h"
#include "src/rendering/ModeloGLB.h"
#include "src/animation/ObjetosAnimadosGLB.h"
#include "src/rendering/DibujoGLB.h"
#include "src/physics/MundoFisico.h"
#include "src/physics/EscenaFisica.h"
#include "src/physics/DibujoColisionadores.h"
#include "src/vr/VistaEstereo.h"
#include "src/vr/OpenXRManager.h"
#include "src/gameplay/player/MovimientoFPS.h"
#include "src/gameplay/player/PlayerRuntime.h"
#include "src/audio/AudioManager.h"
#include "src/gameplay/hirity/AnimacionHiriti.h"
#include "src/gameplay/hirity/HiritiRuntime.h"
#include "src/gameplay/interaction/InteractionRuntime.h"
#include "TemporizadorAmbiente.h"
#include "src/networking/replication/NetJuego.h"
#include "src/networking/replication/NetAdaptadores.h"
#include "src/networking/replication/NetAvatar.h"
#include "Network/PreferenciasRed.h"
#include "Network/GrabDiagnostics.h"
#include "src/networking/session/NetworkSessionRuntime.h"
#include "src/editor/EditorEscenas.h"
#include "src/editor/SerializadorEscenas.h"
#include "src/ui/RuntimeUi.h"
#include "src/ui/StatusBar.h"
#include <iostream>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <map>
#include <random>
#include <limits>
#include <chrono>
#include <sstream>
#include "src/rendering/UniformCache.h"
#include "src/rendering/RenderMetrics.h"

// CODI NOSTRE

#include <SOIL2.h>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ---------------------------

// Objetos de la escena cargada.
std::vector<SceneObject> objetosEscena;
std::unordered_map<std::string, std::unique_ptr<COBJModel>> modelosHabitacion;
editor::EditorEscenas editorEscenas;
editor::SerializadorEscenas serializadorEscena;
std::vector<SceneObjetoAnimado> datosAnimadosEscena;
COBJModel* phoneOBJ = nullptr;
std::unordered_map<std::string, GLuint> texturasMaterialEscena;

// Locate copied resources beside the executable or in the source tree.
static void SetWorkingDirectoryToAssetRoot(const char* executablePath)
{
	namespace fs = std::filesystem;
	std::error_code ec;
	std::vector<fs::path> candidates;
	if (executablePath != nullptr && executablePath[0] != '\0')
	{
		fs::path executable = fs::weakly_canonical(fs::path(executablePath), ec);
		if (!ec)
		{
			candidates.push_back(executable.parent_path());
			candidates.push_back(executable.parent_path().parent_path());
			candidates.push_back(executable.parent_path().parent_path().parent_path());
			candidates.push_back(executable.parent_path().parent_path().parent_path().parent_path());
		}
	}
	candidates.push_back(fs::current_path(ec));

	for (const auto& candidate : candidates)
	{
		ec.clear();
		if (fs::exists(candidate / "assets" / "shaders" / "gouraud_shdrML.vert", ec) &&
			fs::exists(candidate / "assets" / "scenes" / "aula_informatica_calibrada.json", ec))
		{
			fs::current_path(candidate, ec);
			if (!ec)
				return;
		}
	}
}

namespace
{
	GLuint texturaDeMaterial(const SceneMaterial& material)
	{
		if (!material.presente || material.textura.empty() || !serializadorEscena.Cargado()) return 0;
		std::error_code ec;
		const auto absoluta = std::filesystem::absolute(serializadorEscena.Ruta().parent_path() / material.textura, ec).lexically_normal();
		if (ec) return 0;
		const std::string clave = absoluta.string();
		if (const auto it = texturasMaterialEscena.find(clave); it != texturasMaterialEscena.end()) return it->second;
		const GLint id = loadIMA_SOIL(clave.c_str());
		if (id <= 0) return 0;
		texturasMaterialEscena.emplace(clave, static_cast<GLuint>(id));
		return static_cast<GLuint>(id);
	}

	unsigned int texturaParaEditor(const std::filesystem::path& ruta)
	{
		std::error_code ec;
		const auto absoluta = std::filesystem::absolute(ruta, ec).lexically_normal();
		if (ec || !std::filesystem::is_regular_file(absoluta, ec)) return 0;
		const std::string clave = absoluta.string();
		if (const auto it = texturasMaterialEscena.find(clave); it != texturasMaterialEscena.end()) return it->second;
		const GLint id = loadIMA_SOIL(clave.c_str());
		if (id <= 0) return 0;
		texturasMaterialEscena.emplace(clave, static_cast<GLuint>(id));
		return static_cast<unsigned int>(id);
	}

	void aplicarIntensidadMaterial(GLuint programa, bool ambienteActivo = true)
	{
		if (programa == 0) return;
		const GLint localizacion = glcache::UniformLocation(programa, "sw_intensity");
		if (localizacion < 0) return;
		GLint programaAnterior = 0;
		glGetIntegerv(GL_CURRENT_PROGRAM, &programaAnterior);
		if (static_cast<GLuint>(programaAnterior) != programa) glUseProgram(programa);
		glUniform4i(localizacion, sw_material[0] ? GL_TRUE : GL_FALSE,
			(sw_material[1] && ambienteActivo) ? GL_TRUE : GL_FALSE, sw_material[2] ? GL_TRUE : GL_FALSE,
			sw_material[3] ? GL_TRUE : GL_FALSE);
		if (static_cast<GLuint>(programaAnterior) != programa) glUseProgram(static_cast<GLuint>(programaAnterior));
	}

	// F2: vista estereoscópica DE PRUEBA en pantalla (dos mitades). No es VR real: no hay OpenXR ni visor.
	bool modoEstereoPrueba = false;
	// OpenXR (VR real): SOLO se inicia cuando el usuario lo pide (F3 o el botón del panel de física).
	// No tiene nada que ver con F2. Si no hay runtime o visor, el juego sigue en pantalla.
	vr::GestorOpenXR gestorOpenXR;
	GLFWwindow* ventanaPrincipal = nullptr;   // la necesita OpenXR para enlazar su sesión con el contexto OpenGL
	int versionGLMayor = 0, versionGLMenor = 0;

	void alternarOpenXR()
	{
		if (gestorOpenXR.TieneRecursos()) gestorOpenXR.Desactivar();
		else gestorOpenXR.Activar(ventanaPrincipal, versionGLMayor, versionGLMenor);
	}
	bool fpsPausado = false;
	bool fpsCapturaSolicitada = true;
	bool fpsCursorCapturado = false;
	bool fpsTienePosicionAnterior = false;
	bool ventanaConFoco = true;
	bool mostrarPanelesDebug = false;
	bool mostrarContadorFPS = false;
	bool renderizadoOptimizado = true;
	bool oclusoresPhongInvalidos = true;
	GLuint consultasGpu[3]{};
	bool consultaGpuPendiente[3]{};
	int consultaGpuActual = 0;
	double ultimoGpuMs = 0.0;
	bool consultaGpuActiva = false;
	void iniciarConsultaGpu()
	{
		if (!renderMetrics::enabled) return;
		if (consultasGpu[0] == 0) glGenQueries(3, consultasGpu);
		const int slot = consultaGpuActual;
		if (consultaGpuPendiente[slot])
		{
			GLint disponible = GL_FALSE;
			glGetQueryObjectiv(consultasGpu[slot], GL_QUERY_RESULT_AVAILABLE, &disponible);
			if (disponible) { GLuint64 ns = 0; glGetQueryObjectui64v(consultasGpu[slot], GL_QUERY_RESULT, &ns); ultimoGpuMs = static_cast<double>(ns) / 1.0e6; consultaGpuPendiente[slot] = false; }
		}
		consultaGpuActiva = !consultaGpuPendiente[slot];
		if (consultaGpuActiva) glBeginQuery(GL_TIME_ELAPSED, consultasGpu[slot]);
	}
	void finalizarConsultaGpu()
	{
		if (!renderMetrics::enabled || consultasGpu[0] == 0) return;
		if (consultaGpuActiva) { glEndQuery(GL_TIME_ELAPSED); consultaGpuPendiente[consultaGpuActual] = true; }
		consultaGpuActual = (consultaGpuActual + 1) % 3;
	}
	int modoDiagnosticoIluminacion = 0;
	double fpsUltimoX = 0.0;
	double fpsUltimoY = 0.0;
	bool mostrarMovil = true;
	glm::vec3 movilPosicion(-0.30f, -0.28f, -0.65f);
	glm::vec3 movilRotacion(90.0f, 0.0f, -12.0f);
	float movilEscala = 0.09f;
	float intensidadIdle = 0.002f;
	float intensidadWalking = 0.018f;
	float intensidadMouse = 0.0008f;
	float pesoCaminata = 0.0f;
	float faseCaminata = 0.0f;
	float mouseSwayX = 0.0f;
	float mouseSwayY = 0.0f;
	double mouseDeltaX = 0.0;
	double mouseDeltaY = 0.0;
	float tiempoFPS = 0.0f;
	float deltaFPS = 0.0f;
	AudioManager audioManager;
	float volumenGeneralPorcentaje = 100.0f;
	juego::AnimacionHiriti animacionHiriti;
	juego::TemporizadorAudioHiriti temporizadorAudioHiriti;

	// --- Movimiento FPS: sprint, estamina y balanceo visual (MovimientoFPS.h) ---
	fps::MovimientoJugador movimientoJugador;
	fps::BalanceoCamara balanceoCamara;
	fps::BarraEstamina barraEstamina;
	bool mostrarPorcentajeEstamina = true;
	glm::vec3 piesFotogramaAnterior(0.0f);
	bool hayPiesFotogramaAnterior = false;

	// --- DEMO FASE 1: puerta que se abre girando sobre su bisagra ---
	// Keyframes: en 0 s la puerta está cerrada (0 grados), en 2 s abierta (90 grados).
	// Para cambiar la duración o el ángulo basta con editar esta lista.
	std::vector<animacion::KeyframeAngulo> keyframesPuertaDemo = { {0.0f, 0.0f}, {2.0f, 90.0f} };
	animacion::Reproductor reproductorPuertaDemo;
	float anguloPuertaDemo = 0.0f;                              // ángulo actual (se recalcula cada fotograma)
	const glm::vec3 mitadTamanoPuertaDemo(0.05f, 0.5f, 1.0f);   // puerta de 1 m de ancho, 10 cm de grosor y 2 m de alto
	const glm::vec3 bisagraPuertaDemo(6.0f, -0.5f, -1.7f);      // punto del mundo (a ras de suelo)

	// Se llama una vez por fotograma: avanza el reloj y calcula el ángulo con los keyframes.
	void actualizarPuertaDemo(float deltaSegundos)
	{
		reproductorPuertaDemo.Actualizar(deltaSegundos);
		anguloPuertaDemo = animacion::AnguloEnTiempo(keyframesPuertaDemo, reproductorPuertaDemo.tiempo);
	}

	// Arranca la animación desde el principio (la puerta vuelve a estar cerrada).
	void iniciarPuertaDemo()
	{
		reproductorPuertaDemo.Iniciar(keyframesPuertaDemo.back().tiempo);
	}

	// --- OBJETOS ANIMADOS GLB: las animaciones vienen de Blender (assets/animations/*.glb) ---
	// No hay keyframes ni puertas escritas aquí: la lista "animated" del JSON de la habitación
	// declara cada instancia (puerta, cajón...) y cargarYAplicarEscena() las crea.
	// Varias instancias pueden usar el mismo .glb y se abren de forma independiente.
	glb::EscenaAnimada objetosAnimados;
	// --- FÍSICA (Bullet, Fase 1): cubo que cae sobre un suelo estático. Ver MundoFisico.h ---
	// UN solo mundo físico para toda la partida. La habitación activa y la demo ponen cuerpos
	// en él y cada una quita solo los suyos. No depende de GLFW ni de la cámara (válido para VR).
	fisica::MundoFisico mundoFisico;
	std::unique_ptr<fisica::EscenaFisica> escenaFisica = std::make_unique<fisica::EscenaFisica>(mundoFisico);
	fisica::DemoCuboCaida demoFisica(mundoFisico);
	bool mostrarColisionadores = false;   // opción de depuración (panel "Fisica (Bullet)")
	bool hiritiFollowPlayerEnabled = false;
	float hiritiFollowStopDistance = 1.8f;
	float hiritiFollowMaxSpeed = 2.4f;
	bool objetosAnimadosListos = false;   // true cuando los modelos ya están en la GPU
	GLuint texturaManoInteraccion = 0;
	bool cargaLanzamientoActiva = false;
	float tiempoCargaLanzamiento = 0.0f;
	constexpr float TiempoMaximoCargaLanzamiento = juego::interaction::kMaxThrowChargeSeconds;

	void cargarTexturaManoInteraccion()
	{
		std::error_code ec;
		const std::filesystem::path rutaAbs = std::filesystem::absolute("assets/ui/hand.png", ec).lexically_normal();
		if (ec || !std::filesystem::is_regular_file(rutaAbs, ec))
		{
			std::fprintf(stderr, "No se encuentra assets/ui/hand.png desde la raiz de recursos '%s'. "
				"Comprueba que EntornVGI_runtime_resources se haya ejecutado.\n",
				std::filesystem::current_path(ec).string().c_str());
			return;
		}

		int ancho = 0, alto = 0, canales = 0;
		unsigned char* pixels = SOIL_load_image(rutaAbs.string().c_str(), &ancho, &alto, &canales, SOIL_LOAD_RGBA);
		if (pixels == nullptr)
		{
			std::fprintf(stderr, "No se pudo cargar el icono de interaccion '%s': %s\n",
				rutaAbs.string().c_str(), SOIL_last_result());
			return;
		}
		if (ancho <= 0 || alto <= 0)
		{
			std::fprintf(stderr, "El icono de interaccion tiene dimensiones invalidas (%d x %d): %s\n",
				ancho, alto, rutaAbs.string().c_str());
			SOIL_free_image_data(pixels);
			return;
		}

		for (int i = 0; i < ancho * alto; ++i)
		{
			const unsigned char opacidad = static_cast<unsigned char>(
				(static_cast<unsigned int>(pixels[i * 4]) * pixels[i * 4 + 3] + 127u) / 255u);
			pixels[i * 4] = pixels[i * 4 + 1] = pixels[i * 4 + 2] = 255;
			pixels[i * 4 + 3] = opacidad;
		}

		glGenTextures(1, &texturaManoInteraccion);
		if (texturaManoInteraccion != 0)
		{
			glBindTexture(GL_TEXTURE_2D, texturaManoInteraccion);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ancho, alto, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		else
			std::fprintf(stderr, "No se pudo crear la textura del HUD de interaccion.\n");
		SOIL_free_image_data(pixels);
	}

	// ÚNICO sitio que conoce la cámara FPS: convierte "lo que ve el jugador" en una posición y una
	// dirección del mundo. Todo el sistema de interacción (puertas y agarre) solo recibe esos dos
	// vectores; con VR se llamaría igual, pero con los de la mano o el mando.

	bool modoJuegoFPS(); // Declaración anticipada
	bool entradaFPSActiva();

	bool linternaMovilActiva = true;
	bool lamparasLaboratorioActivas = true;

	// Slots 4-16 pertenecen a las luces declaradas en JSON. Los slots 0-3 siguen
	// reservados para las fuentes generales y la linterna móvil.
	void actualizarLucesDeEscena()
	{
		constexpr int primerSlot = 4;
		const bool hayLucesDeEscena = std::any_of(objetosEscena.begin(), objetosEscena.end(),
			[](const SceneObject& objeto) { return objeto.luz.presente; });
		for (int slot = primerSlot; slot < NUM_MAX_LLUMS; ++slot)
		{
			llumGL[slot].encesa = false;
			llumGL[slot].areaSource = false;
			llumGL[slot].areaHalfSizeU = llumGL[slot].areaHalfSizeV = 0.0f;
		}
		if (!hayLucesDeEscena) return;

		if (!lamparasLaboratorioActivas) return;

		int slot = primerSlot;
		std::size_t lucesSinSlot = 0;
		for (const SceneObject& objeto : objetosEscena)
		{
			if (!objeto.luz.presente || !objeto.luz.encendida) continue;
			if (slot >= NUM_MAX_LLUMS) { ++lucesSinSlot; continue; }

			auto& luz = llumGL[slot++];
			const glm::mat4 modelo = fisica::MatrizDeObjeto(objeto);
			const glm::vec3 ejeU = glm::normalize(glm::vec3(modelo[0]));
			const glm::vec3 ejeV = glm::normalize(glm::vec3(modelo[2]));
			const glm::vec3 direccion = glm::normalize(glm::mat3(modelo) * glm::vec3(0.0f, -1.0f, 0.0f));
			const glm::vec3 posicion(modelo * glm::vec4(objeto.luz.desplazamiento, 1.0f));
			luz.posicio = { posicion.x, posicion.y, posicion.z, 1.0 };
			luz.difusa = { objeto.luz.color.r * objeto.luz.intensidad,
				objeto.luz.color.g * objeto.luz.intensidad,
				objeto.luz.color.b * objeto.luz.intensidad, 1.0f };
			luz.especular = { 0.14f, 0.16f, 0.16f, 1.0f };
			luz.atenuacio.a = 0.018;
			luz.atenuacio.b = 0.08;
			luz.atenuacio.c = 1.0;
			luz.restringida = true;
			luz.spotdirection = { direccion.x, direccion.y, direccion.z, 0.0 };
			luz.spotcoscutoff = std::cos(glm::radians(68.0f));
			luz.spotInnerCosCutoff = std::cos(glm::radians(48.0f));
			luz.spotexponent = 1.0f;
			luz.areaSource = objeto.luz.areaSize.x > 0.0f && objeto.luz.areaSize.y > 0.0f;
			luz.areaAxisU = { ejeU.x, ejeU.y, ejeU.z, 0.0 };
			luz.areaAxisV = { ejeV.x, ejeV.y, ejeV.z, 0.0 };
			luz.areaHalfSizeU = objeto.luz.areaSize.x * glm::length(glm::vec3(modelo[0])) * 0.5f;
			luz.areaHalfSizeV = objeto.luz.areaSize.y * glm::length(glm::vec3(modelo[2])) * 0.5f;
			luz.encesa = true;
		}
		static std::size_t ultimoAviso = 0;
		if (lucesSinSlot != ultimoAviso)
		{
			ultimoAviso = lucesSinSlot;
			if (lucesSinSlot != 0)
				std::cerr << "Iluminacion Phong limitada a " << (NUM_MAX_LLUMS - primerSlot)
					<< " lamparas de escena; quedan " << lucesSinSlot << " sin slot.\n";
		}
	}

	void actualizarOclusoresPhong()
	{
		const auto inicio = std::chrono::steady_clock::now();
		static std::size_t firmaAnterior = 0;
		static GLuint programaAnterior = 0;
		static GLuint programaBloqueAnterior = 0;
		static GLuint indiceBloqueOclusores = GL_INVALID_INDEX;
		static GLuint bufferOclusores = 0;
		size_t firma = 1469598103934665603ull;
		auto mezclar = [&firma](size_t valor) { firma ^= valor; firma *= 1099511628211ull; };
		for (const SceneObject& objeto : objetosEscena)
		{
			if (!objeto.fisica.presente || objeto.fisica.tipo != fisica::TipoCuerpo::Estatico) continue;
			for (int eje = 0; eje < 3; ++eje)
			{
				mezclar(std::hash<float>{}(objeto.posicion[eje])); mezclar(std::hash<float>{}(objeto.rotacion[eje]));
				mezclar(std::hash<float>{}(objeto.escala[eje])); mezclar(std::hash<float>{}(objeto.fisica.mitadTamano[eje]));
				mezclar(std::hash<float>{}(objeto.fisica.desplazamiento[eje]));
			}
			mezclar(std::hash<std::string>{}(objeto.categoria));
		}
		constexpr GLuint bindingOclusores = 0;
		constexpr int maxOclusores = 12;
		constexpr int maxOclusoresIndirectos = 24;
		struct alignas(16) OclusorGpu
		{
			GLfloat mundoALocal[16]{};
			GLfloat mitad[4]{};
		};
		struct alignas(16) BloqueOclusoresGpu
		{
			std::array<OclusorGpu, maxOclusores> directos{};
			std::array<OclusorGpu, maxOclusoresIndirectos> indirectos{};
		};
		static_assert(sizeof(OclusorGpu) == 80, "std140: mat4 + vec4 ocupan 80 bytes");
		static_assert(sizeof(BloqueOclusoresGpu) == 2880, "12+24 oclusores std140 ocupan 2880 bytes");
		// GL 3.3 garantiza 16 KiB por UBO: quedan 13504 bytes.
		// En uniforms default activos del fragment shader: 17*32 + (16 view + 17 material + 4 ambient +
		// 21 escalares/bool/int + 6 colores) = 608; sampler/matrices no usadas no cuentan; margen 416/1024.
		// El GLuint static vive con el contexto principal, que OpenGL libera al destruirse.
		if (bufferOclusores == 0)
		{
			glGenBuffers(1, &bufferOclusores);
			if (bufferOclusores != 0)
			{
				glBindBuffer(GL_UNIFORM_BUFFER, bufferOclusores);
				glBufferData(GL_UNIFORM_BUFFER, sizeof(BloqueOclusoresGpu), nullptr, GL_DYNAMIC_DRAW);
			}
		}
		if (bufferOclusores == 0)
		{
			static bool avisoBuffer = false;
			if (!avisoBuffer) { std::cerr << "No se pudo crear el UBO de oclusores Phong.\n"; avisoBuffer = true; }
			if (renderMetrics::enabled) renderMetrics::oclusoresMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicio).count();
			return;
		}
		// El binding point pertenece al contexto y se reafirma incluso si la firma de escena no cambió.
		glBindBufferBase(GL_UNIFORM_BUFFER, bindingOclusores, bufferOclusores);
		if (renderizadoOptimizado && !oclusoresPhongInvalidos && firma == firmaAnterior && programaAnterior == shader_programID)
		{
			if (renderMetrics::enabled) renderMetrics::oclusoresMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicio).count();
			return;
		}
		BloqueOclusoresGpu bloque{};
		int cantidad = 0;
		int cantidadIndirecta = 0;
		for (const SceneObject& objeto : objetosEscena)
		{
			if (!objeto.fisica.presente ||
				objeto.fisica.tipo != fisica::TipoCuerpo::Estatico) continue;
			const glm::mat4 matrizObjeto = fisica::MatrizDeObjeto(objeto);
			glm::mat4 giro(1.0f);
			giro = glm::rotate(giro, glm::radians(objeto.rotacion.z), glm::vec3(0, 0, 1));
			giro = glm::rotate(giro, glm::radians(objeto.rotacion.y), glm::vec3(0, 1, 0));
			giro = glm::rotate(giro, glm::radians(objeto.rotacion.x), glm::vec3(1, 0, 0));
			const glm::vec3 centro = glm::vec3(matrizObjeto * glm::vec4(objeto.fisica.desplazamiento, 1.0f));
			const glm::mat4 mundoCollider = glm::translate(glm::mat4(1.0f), centro) * giro;
			const glm::vec3 mitad = glm::abs(objeto.escala) * objeto.fisica.mitadTamano;
			if (cantidad < maxOclusores)
			{
				const auto indice = static_cast<std::size_t>(cantidad);
				const auto matriz = glm::inverse(mundoCollider);
				std::copy_n(glm::value_ptr(matriz), 16, bloque.directos[indice].mundoALocal);
				for (int eje = 0; eje < 3; ++eje) bloque.directos[indice].mitad[eje] = mitad[eje];
				++cantidad;
			}
			// El rebote se bloquea por arquitectura, no por mesas/sillas: así una silla bajo
			// una mesa recibe relleno local sin que la luz pueda saltar a otra habitación.
			if (objeto.categoria == "architecture" && cantidadIndirecta < maxOclusoresIndirectos)
			{
				const auto matriz = glm::inverse(mundoCollider);
				std::copy_n(glm::value_ptr(matriz), 16,
					bloque.indirectos[static_cast<std::size_t>(cantidadIndirecta)].mundoALocal);
				for (int eje = 0; eje < 3; ++eje)
					bloque.indirectos[static_cast<std::size_t>(cantidadIndirecta)].mitad[eje] = mitad[eje];
				++cantidadIndirecta;
			}
		}

		glUseProgram(shader_programID);
		if (shader_programID != 0 && (programaBloqueAnterior != shader_programID || oclusoresPhongInvalidos))
		{
			indiceBloqueOclusores = glGetUniformBlockIndex(shader_programID, "PhongOccludersBlock");
			if (indiceBloqueOclusores != GL_INVALID_INDEX)
				glUniformBlockBinding(shader_programID, indiceBloqueOclusores, bindingOclusores);
			programaBloqueAnterior = shader_programID;
		}
		glUniform1i(glcache::UniformLocation(shader_programID, "uOccluderCount"), cantidad);
		glUniform1i(glcache::UniformLocation(shader_programID, "uIndirectOccluderCount"), cantidadIndirecta);
		if ((cantidad > 0 || cantidadIndirecta > 0) && indiceBloqueOclusores != GL_INVALID_INDEX)
		{
			glBindBuffer(GL_UNIFORM_BUFFER, bufferOclusores);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(BloqueOclusoresGpu), &bloque);
		}
		firmaAnterior = firma; programaAnterior = shader_programID; oclusoresPhongInvalidos = false;
		if (renderMetrics::enabled) renderMetrics::oclusoresMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicio).count();
	}

	glm::mat4 matrizLocalMovilActual()
	{
		const float idle = intensidadIdle;
		const float bob = pesoCaminata * intensidadWalking;
		glm::mat4 matriz(1.0f);
		matriz = glm::translate(matriz, movilPosicion + glm::vec3(
			std::sin(faseCaminata) * bob * 0.45f,
			std::sin(tiempoFPS * 1.4f) * idle + std::abs(std::sin(faseCaminata)) * bob, 0.0f));
		matriz = glm::rotate(matriz, glm::radians(movilRotacion.x) + std::sin(tiempoFPS * 1.4f) * idle + mouseSwayY, glm::vec3(1, 0, 0));
		matriz = glm::rotate(matriz, glm::radians(movilRotacion.y) - std::sin(faseCaminata) * bob * 0.8f - mouseSwayX, glm::vec3(0, 1, 0));
		matriz = glm::rotate(matriz, glm::radians(movilRotacion.z) + std::cos(faseCaminata) * bob * 0.4f, glm::vec3(0, 0, 1));
		return glm::scale(matriz, glm::vec3(movilEscala));
	}

	void actualizarLinternaMovil(const glm::mat4& matrizMovilMundo)
	{
		constexpr int idx = 3;
		auto& luz = llumGL[idx];
		if (!linternaMovilActiva || !mostrarMovil || !modoJuegoFPS() || editorEscenas.Activo())
		{ luz.encesa = false; return; }

		// Phone.obj coloca el conjunto cámara/flash trasero en el saliente superior de la cara +Y.
		const glm::vec3 ledLocal(0.614f, 0.068f, -1.412f);
		const glm::vec3 origen = glm::vec3(matrizMovilMundo * glm::vec4(ledLocal, 1.0f));
		const glm::vec3 frenteTelefono = glm::normalize(glm::vec3(matrizMovilMundo * glm::vec4(0, -1, 0, 0)));
		// También sigue la orientación del ojo actual cuando el renderizado usa OpenXR.
		glm::vec3 mirada = glm::vec3(glm::inverse(ViewMatrix) * glm::vec4(0, 0, -1, 0));
		if (glm::length(mirada) < 0.0001f) { luz.encesa = false; return; }
		mirada = glm::normalize(mirada);
		const glm::vec3 direccion = glm::normalize(glm::mix(frenteTelefono, mirada, 0.68f));

		luz.encesa = true;
		luz.posicio = { origen.x, origen.y, origen.z, 1.0 };
		luz.difusa = { 0.88f, 0.90f, 0.84f, 1.0f };
		luz.especular = { 1.0f, 1.0f, 1.0f, 1.0f };
		luz.atenuacio.a = 0.02; luz.atenuacio.b = 0.08; luz.atenuacio.c = 1.0;
		luz.restringida = true;
		luz.spotdirection = { direccion.x, direccion.y, direccion.z, 0.0 };
		luz.spotcoscutoff = std::cos(glm::radians(48.0f));
		luz.spotInnerCosCutoff = std::cos(glm::radians(28.0f));
		luz.spotexponent = 1.15f;
		luz.areaSource = false;
	}

	// --- Multijugador (OFFLINE por defecto: no hay hilo, ni socket, ni ENet hasta pulsar Iniciar o usar --host/--connect) ---
	netjuego::RedJuego redJuego;
	// El panel puede cerrarse y recuperarse con F4, sin depender de los paneles F1.
	bool mostrarPanelRed = true;
	std::unique_ptr<netjuego::PuertasGLB> puertasRed;       // adaptador de las puertas GLB (IDs de escena)
	netjuego::ConfigRed configRedPendiente;                 // lo que editan el menu y la linea de comandos
	netjuego::session::SessionRuntimeState estadoSesionRed;  // estado propio de src/networking/session
	vr::PoseXR ultimaCabezaVR;                              // pose real del visor, la rellena el render VR
	bool hayCabezaVR = false;
	void iniciarRed(const netjuego::ConfigRed& cfg);
	void detenerRed();

	using juego::interaction::AccionInteraccion;
	using juego::interaction::ObjetivoInteraccion;

	bool modoJuegoFPS();
	void origenYDireccionDeLaMirada(glm::vec3& origen, glm::vec3& direccion);

	ObjetivoInteraccion objetivoInteraccionActual;

	bool diagnosticoAgarreActivo()
	{
		static const bool activo = []
		{
			const char* valor = std::getenv("ENTORNVGI_DEBUG_GRAB");
			return valor != nullptr && valor[0] != '\0' && valor[0] != '0';
		}();
		return activo;
	}

	net::GrabDiagnosticRole rolDiagnosticoAgarre()
	{
		return redJuego.EsHost() ? net::GrabDiagnosticRole::Host :
			redJuego.EsCliente() ? net::GrabDiagnosticRole::Client : net::GrabDiagnosticRole::Offline;
	}

	void persistirDiagnosticoAgarre(const std::string& registro)
	{
		if (!net::GrabPersistenceEnabledFromEnvironment()) return;
		static std::array<std::unique_ptr<net::GrabDiagnostics>, 3> sinks;
		const auto rol = rolDiagnosticoAgarre();
		const auto indice = static_cast<std::size_t>(rol);
		auto& sink = sinks[indice];
		if (!sink)
			sink = std::make_unique<net::GrabDiagnostics>(net::DefaultGrabDiagnosticsDirectory(),
				rol, net::CurrentProcessId());
		sink->Write(registro);
	}

	void emitirDiagnosticoAgarre(const std::string& registro)
	{
		std::clog << registro << '\n';
		persistirDiagnosticoAgarre(registro);
	}

	void trazarAgarre(const char* evento, uint32_t id, net::PeerId peer, net::PeerId propietario,
		const glm::vec3& posicion, const glm::vec3& punto, const char* detalle)
	{
		if (!diagnosticoAgarreActivo()) return;
		std::ostringstream registro;
		registro << "[grab] " << evento << " id=" << id << " peer=" << peer << " owner=" << propietario
			<< " detail=" << detalle << " pos=(" << posicion.x << ',' << posicion.y << ',' << posicion.z
			<< ") point=(" << punto.x << ',' << punto.y << ',' << punto.z << ')';
		emitirDiagnosticoAgarre(registro.str());
	}

	// Contexto prestado a src/gameplay/hirity: el estado sigue siendo de main.cpp.
	const juego::hiriti::HiritiContext& contextoHiriti()
	{
		static const juego::hiriti::HiritiContext contexto{
			audioManager, animacionHiriti, temporizadorAudioHiriti, objetosEscena, escenaFisica, mundoFisico,
			redJuego, hiritiFollowPlayerEnabled, hiritiFollowStopDistance, hiritiFollowMaxSpeed,
			[](glm::vec3& origen, glm::vec3& direccion) { origenYDireccionDeLaMirada(origen, direccion); },
			[] { return glm::vec2(static_cast<float>(n[0] - opvN.x), static_cast<float>(n[1] - opvN.y)); },
			[] { return glfwGetTime(); },
			[] { return diagnosticoAgarreActivo(); },
			[](const std::string& registro) { emitirDiagnosticoAgarre(registro); },
			[](const char* evento, uint32_t id, net::PeerId peer, net::PeerId propietario,
				const glm::vec3& posicion, const glm::vec3& punto, const char* detalle)
				{ trazarAgarre(evento, id, peer, propietario, posicion, punto, detalle); }
		};
		return contexto;
	}

	void reproducirSonidoHiriti(const std::string& ruta, float volumenBase = 1.0f)
	{
		juego::hiriti::PlayHiritiSound(contextoHiriti(), ruta, volumenBase);
	}

	void actualizarHiriti(bool puedeDescubrir, bool puedeProcesarColisiones, float delta)
	{
		juego::hiriti::UpdateHiriti(contextoHiriti(), puedeDescubrir, puedeProcesarColisiones, delta);
	}

	void actualizarAudioAmbiente(bool activo, float delta)
	{
		juego::hiriti::UpdateAmbientAudio(contextoHiriti(), activo, delta);
	}

	// Contexto prestado a src/gameplay/interaction: el estado sigue siendo de main.cpp.
	const juego::interaction::InteractionContext& contextoInteraccion()
	{
		static const juego::interaction::InteractionContext contexto{
			objetosEscena, escenaFisica, mundoFisico, redJuego, objetosAnimados, puertasRed,
			objetivoInteraccionActual, cargaLanzamientoActiva, tiempoCargaLanzamiento, mostrarPanelesDebug,
			[] { return entradaFPSActiva(); },
			[] { return editorEscenas.Activo(); },
			[] { return gestorOpenXR.SesionEnMarcha(); },
			[](glm::vec3& origen, glm::vec3& direccion) { origenYDireccionDeLaMirada(origen, direccion); },
			[] { return glfwGetTime(); },
			[](const char* evento, uint32_t id, net::PeerId peer, net::PeerId propietario,
				const glm::vec3& posicion, const glm::vec3& punto, const char* detalle)
				{ trazarAgarre(evento, id, peer, propietario, posicion, punto, detalle); },
			[]
			{
				animacionHiriti.DescubrirPorInteraccion();
				if (animacionHiriti.MarcarAgarrado())
					reproducirSonidoHiriti("assets/audio/hirity/story/lohashechobien.ogg");
			},
			[](float carga) { animacionHiriti.MarcarLanzadoPorJugador(carga); }
		};
		return contexto;
	}

	bool jugadorTieneAgarre()
	{
		return juego::interaction::PlayerHasGrab(contextoInteraccion());
	}

	ObjetivoInteraccion buscarObjetivoInteraccion()
	{
		return juego::interaction::FindInteractionTarget(contextoInteraccion());
	}

	void actualizarObjetivoInteraccion(bool permitido)
	{
		juego::interaction::UpdateInteractionTarget(contextoInteraccion(), permitido);
	}

	void iniciarCargaLanzamiento()
	{
		juego::interaction::StartThrowCharge(contextoInteraccion());
	}

	void interactuar()
	{
		juego::interaction::Interact(contextoInteraccion());
	}

	void actualizarAgarreDelJugador(bool puedeJugar)
	{
		juego::interaction::UpdatePlayerGrab(contextoInteraccion(), puedeJugar);
	}

	gameplay::player::PlayerContext& contextoJugador();
	bool modoJuegoFPS() { return gameplay::player::isFpsMode(contextoJugador()); }

	void fijarCapturaCursor(GLFWwindow* window, bool capturar)
	{
		if (fpsCursorCapturado == capturar) return;
		fpsCursorCapturado = capturar;
		fpsTienePosicionAnterior = false;
		glfwSetInputMode(window, GLFW_CURSOR, capturar ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
		// Al cambiar GLFW_CURSOR se reinicia el delta antes de aplicar giro FPS.
	}

	void sincronizarCursorFPS(GLFWwindow* window)
	{
		const bool capturar = modoJuegoFPS() && !editorEscenas.Activo() && !fpsPausado && !mostrarPanelesDebug && ventanaConFoco && fpsCapturaSolicitada;
		fijarCapturaCursor(window, capturar);
	}
}


namespace
{
	std::string nombreEscenaActiva;
	SceneSpawn spawnEscenaActiva;
	float intensidadIndirectaEscena = 0.16f;
	float intensidadAmbientalBaseEscena = 0.025f;
	float escalaLuzDirectaEscena = 1.0f;
	glm::vec3 colorReboteSueloEscena{ 0.42f, 0.46f, 0.43f };
	glm::vec3 colorReboteCieloEscena{ 0.24f, 0.28f, 0.34f };
	bool escenaTieneGeometriaValida = false;
	bool entradaFPSActiva();

	gameplay::player::PlayerContext& contextoJugador()
	{
		static gameplay::player::PlayerContext context{
			mundoFisico, escenaFisica, escenaTieneGeometriaValida, spawnEscenaActiva, nombreEscenaActiva,
			opvN, n, angleZ, camera, projeccio, CAM_NAVEGA, PERSPECT,
			show_EntornVGI_window, oProjeccio, oCamera, oObjecte, oIlumina, oShader, ifixe, SkyBoxCube,
			[] { OnProjeccioPerspectiva(); }, [] { OnCameraNavega(); }, [] { OnObjecteCub(); },
			[] { OnShaderPhong(); }, [] { OnVistaSkyBox(); },
			[] {
				llumGL[0].encesa = true;
				llumGL[0].difusa.r = 0.12f; llumGL[0].difusa.g = 0.15f; llumGL[0].difusa.b = 0.18f;
				llumGL[0].especular.r = 0.03f; llumGL[0].especular.g = 0.03f; llumGL[0].especular.b = 0.03f;
			},
			 movimientoJugador, balanceoCamara, hayPiesFotogramaAnterior, piesFotogramaAnterior,
			 pesoCaminata, faseCaminata, mouseSwayX, mouseSwayY, mouseDeltaX, mouseDeltaY, intensidadMouse,
			 tiempoFPS, deltaFPS, [] { return entradaFPSActiva(); }
		};
		return context;
	}

	void fijarCamaraAlSpawn(const SceneSpawn& spawn) { gameplay::player::fixCameraAtSpawn(contextoJugador(), spawn); }

	void aplicarPresetJuego() { gameplay::player::applyGamePreset(contextoJugador()); }

	void inclinarJugadorFPS(double deltaY) { gameplay::player::look(contextoJugador(), deltaY); }

	// --- JUGADOR FÍSICO (Fase 3) ---------------------------------------------------------
	// Reparto de trabajo (el mismo que se usará con VR):
	//   * El teclado (o el mando, en VR) solo dice "quiero ir en esta dirección a 3 m/s".
	//   * Bullet (la cápsula del jugador) decide hasta dónde se puede llegar: choca, desliza, cae.
	//   * La cámara se COLOCA donde quedó la cápsula (ojos = pies + 1,7 m). La cámara nunca
	//     mueve a la cápsula, salvo al salir del modo FPS (ver sincronizarJugadorYCamara).
	// La orientación (ratón) NO pasa por Bullet: sigue siendo cosa de la cámara.
	// (las velocidades -caminar, agachado, sprint- viven en fps::ParametrosMovimiento, editables en F1)

	bool hayTeclaDeMovimiento(GLFWwindow* window)
	{
		return glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
			glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
	}

	glm::vec3 posicionPiesDesdeCamara()
	{
		return glm::vec3(opvN.x, opvN.y, opvN.z - mundoFisico.AlturaOjosJugador());
	}

	void origenYDireccionDeLaMirada(glm::vec3& origen, glm::vec3& direccion)
	{
		if (gestorOpenXR.SesionEnMarcha())
		{
			vr::PoseXR poseMando;
			if (gestorOpenXR.PoseApuntadoMando(1, poseMando)) // Solo mando derecho (1)
			{
				const glm::vec3 pies = mundoFisico.HayJugador() ? mundoFisico.PosicionPiesJugador() : posicionPiesDesdeCamara();
				const glm::vec3 frenteFPS(static_cast<float>(n[0] - opvN.x), static_cast<float>(n[1] - opvN.y), static_cast<float>(n[2] - opvN.z));
				const vr::Anclaje anclaje = vr::AnclajeDelJugador(pies, frenteFPS,
					gestorOpenXR.Espacio() == vr::EspacioReferencia::Suelo, mundoFisico.AlturaOjosJugador());

				const glm::mat4 vistaMando = vr::VistaDeOjoVR(poseMando, anclaje);
				const glm::mat4 mundoMando = glm::inverse(vistaMando);

				origen = glm::vec3(mundoMando[3]);
				direccion = glm::normalize(-glm::vec3(mundoMando[2]));
				return;
			}
		}

		// Modo escritorio (PC): cámara FPS
		origen = glm::vec3(opvN.x, opvN.y, opvN.z);
		direccion = glm::vec3(n[0] - opvN.x, n[1] - opvN.y, n[2] - opvN.z);
	}

	bool matrizMandoIzquierdoMundo(glm::mat4& matrizMundo)
	{
		if (!gestorOpenXR.SesionEnMarcha()) return false;

		vr::PoseXR poseMandoIzq;
		if (!gestorOpenXR.PoseApuntadoMando(0, poseMandoIzq)) return false;

		const glm::vec3 pies = mundoFisico.HayJugador() ? mundoFisico.PosicionPiesJugador() : posicionPiesDesdeCamara();
		const glm::vec3 frenteFPS(static_cast<float>(n[0] - opvN.x), static_cast<float>(n[1] - opvN.y), static_cast<float>(n[2] - opvN.z));
		const vr::Anclaje anclaje = vr::AnclajeDelJugador(pies, frenteFPS,
			gestorOpenXR.Espacio() == vr::EspacioReferencia::Suelo, mundoFisico.AlturaOjosJugador());

		const glm::mat4 vistaMando = vr::VistaDeOjoVR(poseMandoIzq, anclaje);
		matrizMundo = glm::inverse(vistaMando);
		return true;
	}


	// Contexto prestado a src/networking/session: el estado sigue siendo de main.cpp.
	bool reconstruirFisicaTrasEdicion(std::string& error, bool autoridadFisica = true);

	const netjuego::session::SessionContext& contextoSesionRed()
	{
		static const netjuego::session::SessionContext contexto{
			objetosEscena, escenaFisica, mundoFisico, redJuego, objetosAnimados, puertasRed, estadoSesionRed,
			gestorOpenXR, ultimaCabezaVR, hayCabezaVR,
			[] { return glm::vec3(static_cast<float>(opvN.x), static_cast<float>(opvN.y), static_cast<float>(opvN.z)); },
			[] { return glm::vec3(static_cast<float>(n[0] - opvN.x), static_cast<float>(n[1] - opvN.y), static_cast<float>(n[2] - opvN.z)); },
			[] { return mundoFisico.HayJugador() ? mundoFisico.PosicionPiesJugador() : posicionPiesDesdeCamara(); },
			[] { return serializadorEscena.Cargado() ? serializadorEscena.Ruta().stem().string() : std::string(); },
			[](std::string& error, bool autoridadFisica) { return reconstruirFisicaTrasEdicion(error, autoridadFisica); },
			[] { return entradaFPSActiva(); },
			[](glm::vec3& origen, glm::vec3& direccion) { origenYDireccionDeLaMirada(origen, direccion); },
			[] { return diagnosticoAgarreActivo(); },
			[](const char* evento, uint32_t id, net::PeerId peer, net::PeerId propietario,
				const glm::vec3& posicion, const glm::vec3& punto, const char* detalle)
				{ trazarAgarre(evento, id, peer, propietario, posicion, punto, detalle); },
			[]
			{
				animacionHiriti.DescubrirPorInteraccion();
				if (animacionHiriti.MarcarAgarrado())
					reproducirSonidoHiriti("assets/audio/hirity/story/lohashechobien.ogg");
			},
			[](float carga) { animacionHiriti.MarcarLanzadoPorJugador(carga); }
		};
		return contexto;
	}

	bool capturarEstadoEscenaRed(std::vector<netjuego::EstadoObjetoEscena>& salida)
	{
		return netjuego::session::CaptureSceneState(contextoSesionRed(), salida);
	}

	void iniciarRed(const netjuego::ConfigRed& cfg)
	{
		netjuego::session::StartSession(contextoSesionRed(), cfg);
	}

	void detenerRed()
	{
		netjuego::session::StopSession(contextoSesionRed());
	}

	// Una vez por fotograma, en el hilo de juego. Offline: no hace nada (coste cero).
	void actualizarRed(double ahora, bool jugando)
	{
		netjuego::session::UpdateSession(contextoSesionRed(), ahora, jugando);
	}

	// Crea la cápsula bajo la cámara actual. Se llama tras cargar la primera habitación, para que
	// el suelo ya exista y el jugador no caiga al vacío.
	bool crearJugadorFisico() { return gameplay::player::createPhysicsPlayer(contextoJugador()); }

	// Convierte las teclas WASD en una velocidad horizontal DEL MUNDO (sin tocar la posición).
	void moverJugadorFPS(GLFWwindow* window, double velocidadJugador) { gameplay::player::moveFpsPlayer(contextoJugador(), window, velocidadJugador); }

	// Después de simular: la cámara va a donde está la cápsula (conservando hacia dónde mira).
	// Si NO estamos en modo FPS (cámaras de depuración), ocurre lo contrario: la cápsula sigue a la
	// cámara, para que al volver al modo FPS esté donde toca y no haya un salto.
	void sincronizarJugadorYCamara() { gameplay::player::synchronizePlayerAndCamera(contextoJugador()); }

	void actualizarMovimientoMovil(GLFWwindow* window, float deltaTime) { (void)window; gameplay::player::updateMobile(contextoJugador(), deltaTime); }

	// Estado del juego FPS que permite la entrada (no hay pausa, ni foco perdido, ni paneles de depuracion).
	bool entradaFPSActiva()
	{
		return modoJuegoFPS() && !editorEscenas.Activo() && !fpsPausado && !mostrarPanelesDebug &&
			(ventanaConFoco || gestorOpenXR.SesionEnMarcha());
	}

	void cancelarCargaLanzamiento()
	{
		juego::interaction::CancelThrowCharge(contextoInteraccion());
	}

	void lanzarObjetoCargado()
	{
		juego::interaction::ThrowChargedObject(contextoInteraccion());
	}

	void actualizarCargaLanzamiento(GLFWwindow* window, float delta)
	{
		juego::interaction::UpdateThrowCharge(contextoInteraccion(), delta, [window]
			{ return glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS || gestorOpenXR.GatilloPulsado(); });
	}

	// Despues de simular: el balanceo visual sigue el desplazamiento REAL de la capsula (no la tecla pulsada),
	// asi que la camara y el movil comparten la misma fase y se quedan quietos al chocar con una pared.
	void actualizarBalanceoVisual(float delta) { gameplay::player::updateVisualBob(contextoJugador(), delta); }

	// Desplazamiento SOLO VISUAL del ojo en pantalla (derecha y arriba de la camara). No modifica opvN/n, que usan la
	// fisica, las puertas y el agarre. No se aplica en VR: alli manda la cabeza real.
	glm::dvec3 desplazamientoVisualDeCamara() { return gameplay::player::visualCameraDisplacement(contextoJugador()); }

	// Contexto prestado a src/scene/SceneRuntime: el estado sigue siendo de main.cpp.
	const scene::runtime::SceneRuntimeContext& contextoEscena()
	{
		static const scene::runtime::SceneRuntimeContext contexto{
			objetosEscena, modelosHabitacion, texturasMaterialEscena, datosAnimadosEscena, objetosAnimados,
			objetosAnimadosListos, serializadorEscena, mundoFisico, escenaFisica,
			nombreEscenaActiva, spawnEscenaActiva, intensidadIndirectaEscena, intensidadAmbientalBaseEscena,
			escalaLuzDirectaEscena, colorReboteSueloEscena, colorReboteCieloEscena,
			escenaTieneGeometriaValida, oclusoresPhongInvalidos, editorEscenas, animacionHiriti,
			objecte, textura, tFlag_invert_Y, shader_programID,
			[] { return posicionPiesDesdeCamara(); },
			[] { return glm::vec3(static_cast<float>(n[0] - opvN.x), static_cast<float>(n[1] - opvN.y), static_cast<float>(n[2] - opvN.z)); },
			[](const SceneSpawn& spawn) { fijarCamaraAlSpawn(spawn); },
			[] { return crearJugadorFisico(); },
			[] { return redJuego.EnRed(); },
			[] { detenerRed(); }
		};
		return contexto;
	}

	void liberarObjetosAnimados() { scene::runtime::ReleaseAnimatedObjects(contextoEscena()); }

	bool cargarYAplicarEscena(const std::filesystem::path& ruta)
	{
		return scene::runtime::LoadAndApplyScene(contextoEscena(), ruta);
	}

	bool reconstruirFisicaTrasEdicion(std::string& error, bool autoridadFisica)
	{
		return scene::runtime::RebuildPhysicsAfterEdit(contextoEscena(), error, autoridadFisica);
	}

	bool cargarModeloParaEditor(const std::filesystem::path& ruta, std::string& modeloCargado, std::string& error)
	{
		return scene::runtime::LoadModelForEditor(contextoEscena(), ruta, modeloCargado, error);
	}
}


namespace
{
	void OnWindowFocus(GLFWwindow* window, int focused)
	{
		ventanaConFoco = focused == GLFW_TRUE;
		if (!ventanaConFoco)
		{
			cancelarCargaLanzamiento();
			fijarCapturaCursor(window, false);
		}
		else
		{
			fpsTienePosicionAnterior = false;
		}
	}

	void OnWindowCloseRequest(GLFWwindow* window)
	{
		if (editorEscenas.HayCambiosSinGuardar())
		{
			glfwSetWindowShouldClose(window, GLFW_FALSE);
			editorEscenas.SolicitarConfirmacionCierre();
		}
	}
}

void InitGL()
{
// TODO: agregar aquí el código de construcción

//------ Entorn VGI: Inicialització de les variables globals de CEntornVGIView
	int i;

	// Entorn VGI: Variable de control per a Status Bar (consola) 
	statusB = false;

// Entorn VGI: Variables de control per Menú CÀMERA: Esfèrica, Navega, Mòbil, Zoom, Satelit, Polars... 
	camera = CAM_ESFERICA;	oCamera = 0;
	mobil = true;			zzoom = true;		zzoomO = false;		satelit = false;

// Entorn VGI: Variables de control de l'opció CAMERA->ESFÈRICA?
	cam_Esferica[0] = 15.0f;	cam_Esferica[1] = 0.0f;			cam_Esferica[2] = 0.0f;
	OPV.R = cam_Esferica[0];	OPV.alfa = cam_Esferica[1];		OPV.beta = cam_Esferica[2];		// Origen PV en esfèriques
	Vis_Polar = POLARZ;			
	ViewMatrix = glm::mat4(1.0);		// Inicialitzar a identitat
	oPolars = -1;

// Entorn VGI: Variables de control de l'opció CÀMERA->NAVEGA?
	n[0] = -5.5;	n[1] = 4.0;		n[2] = -1.35;
	opvN.x = -6.5;	opvN.y = 4.0;	opvN.z = -1.35;
	angleZ = 0.0;
	
// Entorn VGI: Variables de control de l'opció CÀMERA->GEODE?
	OPV_G.R = 15.0;		OPV_G.alfa = 0.0;	OPV_G.beta = 0.0;	// Origen PV en esfèriques per a Vista_Geode

// Entorn VGI: Variables de control per Menú VISTA: Pantalla Completa, Pan, dibuixar eixos i grids 
	fullscreen = false;	// La finestra s'inicia en mode finestra; F11 alterna pantalla completa
	pan = false;
	eixos = true;	eixos_programID = 0;  eixos_Id = 0;
	sw_grid = false;
	grid.x = false;	grid.y = false;		grid.z = false;		grid.w = false;
	hgrid.x = 0.0;	hgrid.y = 0.0;		hgrid.z = 0.0;		hgrid.w = 0.0;

// Entorn VGI: Variables opció VISTA->PAN?
	fact_pan = 1;
	tr_cpv.x = 0;	tr_cpv.y = 0;	tr_cpv.z = 0;		tr_cpvF.x = 0;	tr_cpvF.y = 0;	tr_cpvF.z = 0;

// Entorn VGI: Variables de control per les opcions de menú Projecció, Objecte
	projeccio = CAP;	// projeccio = PERSPECT;
	oProjeccio = 3;		// ImGui: Variable que controla desplegable TIPUS PROJECCIO d'ImGui (opció Perspectiva)
	ProjectionMatrix = glm::mat4(1.0);	// Inicialitzar matriu a identitat
	objecte = CAP;		// objecte = TETERA;
	oObjecte = 0;		// ImGui: Variable que controla desplegable TIPUS OBJECTE d'ImGui (opció cap objecte)

// Entorn VGI: Variables de control Skybox Cube
	SkyBoxCube = false;		
	skC_programID = 0;		//shader_SkyBoxC = NULL;
	skC_VAOID.vaoId = 0;	skC_VAOID.vboId = 0;	skC_VAOID.nVertexs = 0;
	cubemapTexture = 0;		//shaderSkyBox = NULL;

// Entorn VGI: Variables de control del menú TRANFORMA
	transf = false;		trasl = false;		rota = false;		escal = false;
	fact_Tras = 1;		fact_Rota = 90;
	TG.VTras.x = 0.0;	TG.VTras.y = 0.0;	TG.VTras.z = 0;	TGF.VTras.x = 0.0;	TGF.VTras.y = 0.0;	TGF.VTras.z = 0;
	TG.VRota.x = 0;		TG.VRota.y = 0;		TG.VRota.z = 0;	TGF.VRota.x = 0;	TGF.VRota.y = 0;	TGF.VRota.z = 0;
	TG.VScal.x = 1;		TG.VScal.y = 1;		TG.VScal.z = 1;	TGF.VScal.x = 1;	TGF.VScal.y = 1;	TGF.VScal.z = 1;
	transX = false;		transY = false;		transZ = false;
	GTMatrix = glm::mat4(1.0);		// Inicialitzar matriu a identitat

// Entorn VGI: Variables de control per les opcions de menú OCULTACIONS
	front_faces = true;	test_vis = false;	oculta = false;		back_line = false;

// Entorn VGI: Variables de control del menú ILUMINACIÓ
	ifixe = false;					ilum2sides = false;				ilumina = FILFERROS;
	oIlumina = 1;					// Variable que controla desplegable TIPUS ILUMINACIO d'ImGui (opció Filferros)
	// Reflexions actives: Ambient [1], Difusa [2] i Especular [3]. No actives: Emission [0]. 
	sw_material[0] = false;			sw_material[1] = true;			sw_material[2] = true;			sw_material[3] = true;	sw_material[4] = true;
	sw_material_old[0] = false;		sw_material_old[1] = true;		sw_material_old[2] = true;		sw_material_old[3] = true;	sw_material_old[4] = true;
	textura = false;				t_textura = CAP;				textura_map = true;
	for (i = 0; i < NUM_MAX_TEXTURES; i++) texturesID[i] = 0;
	tFlag_invert_Y = false;

// ----------------- Entorn VGI: Variables de control del menú LLUMS
	// Entorn VGI: Inicialització variables Llums
	llum_ambient = true;		llumGL[0].encesa = true;
	for (i = 1; i < NUM_MAX_LLUMS; i++) llumGL[i].encesa = false;
	for (i = 0; i < NUM_MAX_LLUMS; i++) 
		{	llumGL[i].posicio.x = 0.0;		llumGL[i].posicio.y = 0.0;		llumGL[i].posicio.z = 0.0;		llumGL[i].posicio.w = 1.0;
			llumGL[i].difusa.r = 1.0f;		llumGL[i].difusa.g = 1.0f;		llumGL[i].difusa.b = 1.0f;		llumGL[i].difusa.a = 1.0f;
			llumGL[i].especular.r = 1.0f;	llumGL[i].especular.g = 1.0f;	llumGL[i].especular.b = 1.0f;	llumGL[i].especular.a = 1.0f;
			llumGL[i].atenuacio.a = 0.0;	llumGL[i].atenuacio.b = 0.0;	llumGL[i].atenuacio.c = 1.0;
			llumGL[i].restringida = false;
			llumGL[i].spotdirection.x = 0.0;	llumGL[i].spotdirection.y = 0.0;	llumGL[i].spotdirection.z = 0.0; llumGL[i].spotdirection.w = 0.0;
			llumGL[i].spotcoscutoff = 0.0;		llumGL[i].spotInnerCosCutoff = 0.0;		llumGL[i].spotexponent = 0.0;
			llumGL[i].areaSource = false;
			llumGL[i].areaHalfSizeU = llumGL[i].areaHalfSizeV = 0.0f;
		}

// ---------------- LLUM #0 - (+Z) no restringida, amb atenuació constant (a,b,c) = (0,0,1)
// Posició de la font de llum (x,y,z)=(0,200,0):
	llumGL[0].posicio.x = 0.0;			llumGL[0].posicio.y = 0.0;			llumGL[0].posicio.z = 200.0;	llumGL[0].posicio.w = 1.0;

// Intensitats difusa i especular de la font de llum (r,g,b) = (1,1,1):
	llumGL[0].difusa.r = 1.0f;			llumGL[0].difusa.g = 1.0f;			llumGL[0].difusa.b = 1.0f;		llumGL[0].difusa.a = 1.0f;
	llumGL[0].especular.r = 1.0f;		llumGL[0].especular.g = 1.0f;		llumGL[0].especular.b = 1.0f;	llumGL[0].especular.a = 1.0f;

// Coeficients factor atenuació f_att=1/(ad2+bd+c). Llum sense atenuació per distància (a,b,c)=(0,0,1):
	llumGL[0].atenuacio.a = 0.0;		llumGL[0].atenuacio.b = 0.0;		llumGL[0].atenuacio.c = 1.0;

// Paràmetres font de llum restringida:
	llumGL[0].restringida = false;
	llumGL[0].spotdirection.x = 0.0;	llumGL[0].spotdirection.y = 0.0;	llumGL[0].spotdirection.z = -1.0;
	llumGL[0].spotcoscutoff = cos(25.0 * PI / 180);		llumGL[0].spotexponent = 1.0;		// llumGL[0].spotexponent = 45.0; Model de Warn (10, 500)

// Activació font de llum: ENCESA
	llumGL[0].encesa = true;

// ---------------- LLUM #1 - (+X) no restringida, amb atenuació constant (a,b,c) = (0,0,1)
// Posició de la font de llum (x,y,z)=(75,0,0):
	llumGL[1].posicio.x = 75.0;			llumGL[1].posicio.y = 0.0;			llumGL[1].posicio.z = 0.0;		llumGL[1].posicio.w = 1.0;

// Intensitats difusa i especular de la font de llum (r,g,b) = (1,1,1):
	llumGL[1].difusa.r = 1.0f;			llumGL[1].difusa.g = 1.0f;			llumGL[1].difusa.b = 1.0f;		llumGL[1].difusa.a = 1.0f;
	llumGL[1].especular.r = 1.0f;		llumGL[1].especular.g = 1.0f;		llumGL[1].especular.b = 1.0f;	llumGL[1].especular.a = 1.0f;

// Coeficients factor atenuació f_att=1/(ad2+bd+c). Llum sense atenuació per distància (a,b,c)=(0,0,1):
	llumGL[1].atenuacio.a = 0.0;		llumGL[1].atenuacio.b = 0.0;		llumGL[1].atenuacio.c = 1.0;

// Paràmetres font de llum restringida:
	llumGL[1].restringida = false;
	llumGL[1].spotdirection.x = 0.0;	llumGL[1].spotdirection.y = 0.0;	llumGL[1].spotdirection.z = 0.0;
	llumGL[1].spotcoscutoff = 0.0;		llumGL[1].spotexponent = 0.0;

// Activació font de llum: APAGADA
	llumGL[1].encesa = false;

// ---------------- LLUM #2 - (+Y) no restringida, amb atenuació constant (a,b,c) = (0,0,1)
// Posició de la font de llum (x,y,z)=(0,75,0):
	llumGL[2].posicio.x = 0.0;			llumGL[2].posicio.y = 75.0;			llumGL[2].posicio.z = 0.0;		llumGL[2].posicio.w = 1.0;

// Intensitats difusa i especular de la font de llum (r,g,b) = (1,1,1):
	llumGL[2].difusa.r = 1.0f;			llumGL[2].difusa.g = 1.0f;			llumGL[2].difusa.b = 1.0f;		llumGL[2].difusa.a = 1.0f;
	llumGL[2].especular.r = 1.0f;		llumGL[2].especular.b = 1.0f;		llumGL[2].especular.b = 1.0f;	llumGL[2].especular.a = 1.0f;

// Coeficients factor atenuació f_att=1/(ad2+bd+c). Llum amb atenuació per distància (a,b,c)=(0,0.025,1):
	llumGL[2].atenuacio.a = 0.0;		llumGL[2].atenuacio.b = 0.0;		llumGL[2].atenuacio.c = 1.0;

// Paràmetres font de llum restringida:
	llumGL[2].restringida = false;
	llumGL[2].spotdirection.x = 0.0;	llumGL[2].spotdirection.y = -1.0;	llumGL[2].spotdirection.z = 0.0;
	llumGL[2].spotcoscutoff = cos(2.5 * PI / 180);							llumGL[2].spotexponent = 1.0;

// Activació font de llum: APAGADA
	llumGL[2].encesa = false;

// ---------------- LLUM #3 - (Y=X), restringida amb 25 graus obertura i exponent 45, amb atenuació constant (a,b,c) = (0,0,1)
// Posició de la font de llum (x,y,z)=(75,75,75):
	llumGL[3].posicio.x = 75.0;			llumGL[3].posicio.y = 75.0;			llumGL[3].posicio.z = 75.0;		llumGL[3].posicio.w = 1.0;

// Intensitats difusa i especular de la font de llum (r,g,b) = (0,1,0):
	llumGL[3].difusa.r = 0.0f;			llumGL[2].difusa.g = 1.0f;			llumGL[3].difusa.b = 0.0f;		llumGL[3].difusa.a = 1.0f;
	llumGL[3].especular.r = 0.0f;		llumGL[2].especular.g = 1.0f;		llumGL[3].especular.b = 0.0f;	llumGL[3].especular.a = 1.0f;

// Coeficients factor atenuació f_att=1/(ad2+bd+c). Llum sense atenuació per distància (a,b,c)=(0,0,1):
	llumGL[3].atenuacio.a = 0.0;		llumGL[3].atenuacio.b = 0.0;		llumGL[3].atenuacio.c = 1.0;

// Paràmetres font de llum restringida:
	llumGL[3].restringida = true;
	llumGL[3].spotdirection.x = -1.0;	llumGL[3].spotdirection.y = -1.0;	llumGL[3].spotdirection.z = -1.0;
	llumGL[3].spotcoscutoff = cos(25.0 * PI / 180);							llumGL[3].spotexponent = 45.0;

// Activació font de llum: APAGADA
	llumGL[3].encesa = false;

// ---------------- LLUM #4 - (-Z), no restringida, amb atenuació constant (a,b,c) = (0,0,1)
// Posició de la font de llum (x,y,z)=(0,0,-75):
	llumGL[4].posicio.x = 0.0;			llumGL[4].posicio.y = 0.0;			llumGL[4].posicio.z = -75.0;	llumGL[4].posicio.w = 1.0;

// Intensitats difusa i especular de la font de llum (r,g,b) = (1,1,1):
	llumGL[4].difusa.r = 1.0f;			llumGL[4].difusa.g = 1.0f;			llumGL[4].difusa.b = 1.0f;		llumGL[4].difusa.a = 1.0f;
	llumGL[4].especular.r = 1.0f;		llumGL[4].especular.g = 1.0f;		llumGL[4].especular.b = 1.0f;	llumGL[4].especular.a = 1.0f;

// Coeficients factor atenuació f_att=1/(ad2+bd+c). Llum sense atenuació per distància (a,b,c)=(0,0,1):
	llumGL[4].atenuacio.a = 0.0;		llumGL[4].atenuacio.b = 0.0;		llumGL[4].atenuacio.c = 1.0;

// Paràmetres font de llum restringida:
	llumGL[4].restringida = false;
	llumGL[4].spotdirection.x = 0.0;	llumGL[4].spotdirection.y = 0.0;	llumGL[4].spotdirection.z = -1.0;
	llumGL[4].spotcoscutoff = cos(5 * PI / 180);							llumGL[4].spotexponent = 30.0;

// Activació font de llum: APAGADA
	llumGL[4].encesa = false;

// ---------------- LLUM #5 - (-X,-Y,-Z), direccional, no restringida, amb atenuació constant (a,b,c) = (0,0,1)
// Vector de la font de llum direccional (x,y,z)=(-1,-1,-1):
	llumGL[5].posicio.x = -1.0;			llumGL[5].posicio.y = -1.0;			llumGL[5].posicio.z = -1.0;		llumGL[5].posicio.w = 0.0;

// Intensitats difusa i especular de la font de llum (r,g,b) = (1,0,0):
	llumGL[5].difusa.r = 1.0f;			llumGL[5].difusa.g = 0.0f;			llumGL[5].difusa.b = 0.0f;		llumGL[5].difusa.a = 1.0f;
	llumGL[5].especular.r = 1.0f;		llumGL[5].especular.g = 0.0f;		llumGL[5].especular.b = 0.0f;	llumGL[5].especular.a = 1.0f;

// Coeficients factor atenuació f_att=1/(ad2+bd+c). Llum sense atenuació per distància (a,b,c)=(0,0,1):
	llumGL[5].atenuacio.a = 0.0;		llumGL[5].atenuacio.b = 0.0;		llumGL[5].atenuacio.c = 1.0;

// Paràmetres font de llum restringida:
	llumGL[5].restringida = false;
	llumGL[5].spotdirection.x = 0.0;	llumGL[5].spotdirection.y = 0.0;	llumGL[5].spotdirection.z = 0.0;
	llumGL[5].spotcoscutoff = 0.0;		llumGL[5].spotexponent = 0.0;

// Activació font de llum: APAGADA
	llumGL[5].encesa = false;

// ---------------- LLUM #6 - Llum Vaixell, configurada a la funció vaixell() en escena.cpp.
// Posició de la font de llum (x,y,z)=(-75,75,75):
	llumGL[6].posicio.x = -75.0;		llumGL[6].posicio.y = 75.0;			llumGL[6].posicio.z = 75.0;		llumGL[6].posicio.w = 1.0;

// Intensitats difusa i especular de la font de llum (r,g,b) = (1,1,1):
	llumGL[6].difusa.r = 1.0f;			llumGL[6].difusa.g = 1.0f;			llumGL[6].difusa.b = 1.0f;		llumGL[6].difusa.a = 1.0f;
	llumGL[6].especular.r = 1.0f;		llumGL[6].especular.g = 1.0f;		llumGL[6].especular.b = 1.0f;	llumGL[6].especular.a = 1;

// Coeficients factor atenuació f_att=1/(ad2+bd+c). Llum sense atenuació per distància (a,b,c)=(0,0,1):
	llumGL[6].atenuacio.a = 0.0;		llumGL[6].atenuacio.b = 0.0;		llumGL[6].atenuacio.c = 1.0;

// Paràmetres font de llum restringida:
	llumGL[6].restringida = false;
	llumGL[6].spotdirection.x = 1.0;	llumGL[6].spotdirection.y = -1.0;	llumGL[6].spotdirection.z = -1.0;
	llumGL[6].spotcoscutoff = sqrt(2.0) / 2.0;		llumGL[6].spotexponent = 10.0;

// Activació font de llum: APAGADA
	llumGL[6].encesa = false;

// ---------------- LLUM #7 - Llum Far, configurada a la funció faro() en escena.cpp.
// Posició de la font de llum (x,y,z)=(50,50,-50), però cap posició definida, definida en funció faro() en escena.cpp:
	llumGL[7].posicio.x = 50.0;			llumGL[7].posicio.y = 50.0;			llumGL[7].posicio.z = -50.0;	llumGL[7].posicio.w = 1.0;

	// Intensitats difusa i especular de la font de llum (r,g,b) = (0,1,1):
	llumGL[7].difusa.r = 0.0f;			llumGL[7].difusa.g = 1.0f;			llumGL[7].difusa.b = 1.0f;		llumGL[7].difusa.a = 1.0f;
	llumGL[7].especular.r = 0.0f;		llumGL[7].especular.g = 1.0f;		llumGL[7].especular.b = 1.0f;	llumGL[7].especular.a = 1;

	// Coeficients factor atenuació f_att=1/(ad2+bd+c). Llum sense atenuació per distància (a,b,c)=(0,0,1):
	llumGL[7].atenuacio.a = 0.0;		llumGL[7].atenuacio.b = 0.0;		llumGL[7].atenuacio.c = 1.0;

	// Paràmetres font de llum restringida:
	llumGL[7].restringida = true;
	llumGL[7].spotdirection.x = -1.0;	llumGL[7].spotdirection.y = -1.0;	llumGL[7].spotdirection.z = 1.0;
	llumGL[7].spotcoscutoff = 0.5;		llumGL[7].spotexponent = 5.0;

// Activació font de llum: APAGADA
	llumGL[7].encesa = false;
// ---------------- FI DEFINICIÓ LLUMS
	sw_llambient = false;
	col_llambient.r = 1.0;	col_llambient.g = 1.0;	col_llambient.b = 1.0; col_llambient.a = 1.0;

// Entorn VGI: Variables de control del menú SHADERS
	shader = CAP_SHADER;	
	oShader = 1;				// Variable que controla desplegable SHADER d'ImGui (opció Gouraud)
	shader_programID = 0;
	shaderLighting.releaseAllShaders();

// Entorn VGI: Càrrega SHADERS ---------------------------------------------------
// Càrrega Shader Eixos
	fprintf(stderr, "Eixos: \n");
	if (!eixos_programID) eixos_programID = shaderEixos.loadFileShaders("./assets/shaders/eixos.vert", "./assets/shaders/eixos.frag");

// VAO dels eixos: es crea aqui perque OnPaint() nomes el creava a la branca 'default', que mai
// s'executa (els shaders ja es carreguen en aquesta inicialitzacio). Sense VAO es dibuixava amb
// el VAO 0 i Core Profile generava GL_INVALID_OPERATION (1282) "Array object is not active".
	if (!eixos_Id) eixos_Id = deixos();

// Entorn VGI: Càrrega Shader Skybox
	fprintf(stderr, "SkyBox: \n");
	if (!skC_programID) skC_programID = shader_SkyBoxC.loadFileShaders("./assets/shaders/skybox.VERT", "./assets/shaders/skybox.FRAG");

// Entorn VGI: Càrrega VAO Skybox Cube
	if (skC_VAOID.vaoId == 0) skC_VAOID = loadCubeSkybox_VAO();
	Set_VAOList(CUBE_SKYBOX, skC_VAOID);

	if (!cubemapTexture)
	{	// load Skybox textures
		// -------------
		std::vector<std::string> faces =
		{ "./assets/textures/skybox/right.jpg",
			"./assets/textures/skybox/left.jpg",
			"./assets/textures/skybox/top.jpg",
			"./assets/textures/skybox/bottom.jpg",
			"./assets/textures/skybox/front.jpg",
			"./assets/textures/skybox/back.jpg"
		};
		cubemapTexture = loadCubemap(faces);
	}

// Càrrega Shader de Gouraud
	shader_programID = 0;
	fprintf(stderr, "Gouraud_shdrML: \n");
	if (!shader_programID) shader_programID = shaderLighting.loadFileShaders("./assets/shaders/gouraud_shdrML.vert", "./assets/shaders/gouraud_shdrML.frag");
	shader = GOURAUD_SHADER;
// Entorn VGI: FI Càrrega SHADERS ---------------------------------------------------

// Entorn VGI: Variables de control dels botons de mouse
	m_PosEAvall.x = 0;			m_PosEAvall.y = 0;			m_PosDAvall.x = 0;			m_PosDAvall.y = 0;
	m_ButoEAvall = false;		m_ButoDAvall = false;
	m_EsfeEAvall.R = 0.0;		m_EsfeEAvall.alfa = 0.0;	m_EsfeEAvall.beta = 0.0;
	m_EsfeIncEAvall.R = 0.0;	m_EsfeIncEAvall.alfa = 0.0;	m_EsfeIncEAvall.beta = 0.0;

// Entorn VGI: Variables que controlen paràmetres visualització: Mides finestra Windows i PV (càmera)
	w = 640;			h = 480;			// Mides de la finestra Windows (w-amplada,h-alçada)
	width_old = 640;	height_old = 480;	// Mides de la resolució actual de la pantalla (finestra Windows)
	w_old = 640;		h_old = 480;		// Mides de la finestra Windows (w-amplada,h-alçada) per restaurar Finestra des de fullscreen

// Entorn VGI: Color de fons i de l'objecte
	fonsR = true;		fonsG = true;		fonsB = true;
	c_fons.r = clear_colorB.x;		c_fons.g = clear_colorB.y;		c_fons.b = clear_colorB.z;			c_fons.b = clear_colorB.w;
	sw_color = false;
	clear_colorB = ImVec4(0.0f, 0.0f, 0.0f, 1.00f);	// ImGui: Color de fons
	clear_colorO = ImVec4(1.0f, 1.0f, 1.0f, 1.00f);	// ImGui: Color de l'objecte.
	col_obj.r = clear_colorO.x;	col_obj.g = clear_colorO.y;	col_obj.b = clear_colorO.z;		col_obj.a = clear_colorO.w;

// Entorn VGI: Objecte OBJ
	ObOBJ = NULL;		
	vao_OBJ.vaoId = 0;		vao_OBJ.vboId = 0;		vao_OBJ.nVertexs = 0;

// Entorn VGI: OBJECTE --> Corba B-Spline i Bezier
	npts_T = 0;
	for (i = 0; i < MAX_PATCH_CORBA; ++i)
		{	PC_t[i].x = 0.0;
			PC_t[i].y = 0.0;
			PC_t[i].z = 0.0;
		}
	pas_Corba = PAS_BSPLINE;	pas_CS = PAS_BSPLINE;
	sw_Punts_Control = false;

// TRIEDRE DE FRENET / DARBOUX: VT: vector Tangent, VNP: Vector Normal Principal, VBN: vector BiNormal
	dibuixa_TriedreFrenet = false;		dibuixa_TriedreDarboux = false;
	VT = { 0.0, 0.0, 1.0 };		VNP = { 1.0, 0.0, 0.0 };	VBN = { 0.0, 1.0, 0.0 };

// Entorn VGI: Variables del Timer
	t = 0;			anima = false;

// Entorn VGI : QUATERNIONS: Animació de cossos rígids per QUATERNIONS
	animaQ = false;		rotaQ = false;
/*
	for (i = 0; i < MAX_KEYFRAMES_Q; i++)
		{	keyf_Rota[i].x = 0.0f;	keyf_Rota[i].y = 0.0f;	keyf_Rota[i].z = 0.0f;
			//			keyf_Tras[i].x = 0.0f;	keyf_Tras[i].y = 0.0f;	keyf_Tras[i].z = 0.0f;
			keyf_Scal[i].x = 1.0f;	keyf_Scal[i].y = 1.0f;	keyf_Scal[i].z = 1.0f;
			keyf_Quat[i].w = 0.0f;	keyf_Quat[i].y = 0.0f;	keyf_Quat[i].y = 0.0f; keyf_Quat[i].z = 0.0f;
		}
	keyf_Tras[0].x = 10.0f;		keyf_Tras[0].y = -20.0f;	keyf_Tras[0].z = 30.0f;
	keyf_Tras[1].x = 0.0f;		keyf_Tras[1].y = 0.0f;		keyf_Tras[1].z = 0.0f;
	keyf_Tras[2].x = -10.0f;	keyf_Tras[2].y = 20.0f;		keyf_Tras[2].z = 30.0f;
	eix_Rota.x = 0;		eix_Rota.y = 0;		eix_Rota.z = 0;		angle_Rota = 0;
	for (i = 0; i < 16; i++) QMatrix[i] = 0;	QMatrix[0] = QMatrix[5] = QMatrix[10] = QMatrix[15] = 1.0;
	int_Tras.x = 0;		int_Tras.y = 0;		int_Tras.z = 0;
	int_Scal.x = 1;		int_Scal.y = 1;		int_Scal.z = 1;
	np_InQ = 0;
	keyf_RotaI.x = 0.0;	keyf_RotaI.y = 0.0; keyf_RotaI.z = 0.0;
	keyf_RotaF.x = 0.0;	keyf_RotaF.y = 0.0; keyf_RotaF.z = 0.0;
	keyf_RotaT.x = 0.0;	keyf_RotaT.y = 0.0; keyf_RotaT.z = 0.0;
	qI.w = 1.0;			qI.x = 0.0;				qI.y = 0.0;				qI.z = 0.0;
	qF.w = 1.0;			qF.x = 0.0;				qF.y = 0.0;				qF.z = 0.0;
	qT.w = 1.0;			qT.x = 0.0;				qT.y = 0.0;				qT.z = 0.0;
	qT_L.w = 1.0;		qT_L.x = 0.0;			qT_L.y = 0.0;			qT_L.z = 0.0;
	qT_S.w = 1.0;		qT_S.x = 0.0;			qT_S.y = 0.0;			qT_S.z = 0.0;
	qI_E.w = 1.0;		qI_E.x = 0.0;			qI_E.y = 0.0;			qI_E.z = 0.0;
	qF_E.w = 1.0;		qF_E.x = 0.0;			qF_E.y = 0.0;			qF_E.z = 0.0;
	qT_E.w = 1.0;		qT_E.x = 0.0;			qT_E.y = 0.0;			qT_E.z = 0.0;
	lerp = false;		slerp = false;
*/

// Entorn VGI: Variables de l'objecte FRACTAL
	t_fractal = CAP;	soroll = 'C';
	pas = 64;			pas_ini = 64;
	sw_il = true;		palcolFractal = false;
/*
	zzoomF = false;		paletaFractal = CAP_PALETA;
	vaoFractal.vaoId = 0;		vaoFractal.vboId = 0;		vaoFractal.nVertexs = 0;
	vaoMar.vaoId = 0;			vaoMar.vboId = 0;			vaoMar.nVertexs = 0;
*/

// Entorn VGI: ROBOT ARTICULAT
	animaR = false;		sw_robot = false;	sh_rmov = false;
/*
	angles_R[0] = 0.0;	angles_R[1] = 0.0;
	angles_R[2] = 0.0;	angles_R[3] = 0.0;	angles_R[4] = 0.0;
	angles_R[5] = 0.0;
	sh_rmov = true;

	pos_RI[0] = 0.0;		pos_RI[1] = 0.0;
	pos_RI[2] = 0.0;		pos_RI[3] = 0.0;		pos_RI[4] = 0.0;
	pos_RI[5] = 0.0;

	pos_RF[0] = 0.0;		pos_RF[1] = 0.0;
	pos_RF[2] = 0.0;		pos_RF[3] = 0.0;		pos_RF[4] = 0.0;
	pos_RF[5] = 0.0;

	for (i = 0; i < MAX_KEYFRAMES; i = i++)
		for (j = 0; j < 6; j = j++)		keyf_R[i][j] = 0.0f;
	np_In = 0;				num_frame = 0;			incr_frame = 1.0;
*/

// Entorn VGI: Altres variables
	mida = 1.0;			nom = "";		buffer = "";
	
// Entorn VGI: // Inicialtzar llista de VAO's.
	initVAOList();
}


// Viewport i dibuix del fotograma: implementacions a src/rendering/RuntimeRenderer.cpp.
static RuntimeRenderer& renderizadorRuntime()
{
	static RuntimeRenderer renderizador{
		.w = w,
		.h = h,
		.width_old = width_old,
		.height_old = height_old,
		.projeccio = projeccio,
		.camera = camera,
		.Vis_Polar = Vis_Polar,
		.OPV = OPV,
		.OPV_G = OPV_G,
		.opvN = opvN,
		.n = n,
		.pan = pan,
		.tr_cpv = tr_cpv,
		.tr_cpvF = tr_cpvF,
		.ViewMatrix = ViewMatrix,
		.ProjectionMatrix = ProjectionMatrix,
		.c_fons = c_fons,
		.col_obj = col_obj,
		.objecte = objecte,
		.mida = mida,
		.pas = pas,
		.front_faces = front_faces,
		.oculta = oculta,
		.test_vis = test_vis,
		.back_line = back_line,
		.ilumina = ilumina,
		.llum_ambient = llum_ambient,
		.llumGL = llumGL,
		.ifixe = ifixe,
		.ilum2sides = ilum2sides,
		.eixos = eixos,
		.grid = grid,
		.hgrid = hgrid,
		.statusB = statusB,
		.modoEstereoPrueba = modoEstereoPrueba,
		.shader_programID = shader_programID,
		.eixos_programID = eixos_programID,
		.eixos_Id = eixos_Id,
		.shaderEixos = shaderEixos,
		.shaderLighting = shaderLighting,
		.editorEscenas = editorEscenas,
		.gestorOpenXR = gestorOpenXR,
		.ultimaCabezaVR = ultimaCabezaVR,
		.hayCabezaVR = hayCabezaVR,
		.configura_Escena = [] { configura_Escena(); },
		.dibuixa_Escena = [] { dibuixa_Escena(); },
		.Barra_Estat = [] { Barra_Estat(); },
		.desplazamientoVisualDeCamara = [] { return desplazamientoVisualDeCamara(); },
		.piesJugador = [] { return mundoFisico.HayJugador() ? mundoFisico.PosicionPiesJugador() : posicionPiesDesdeCamara(); },
		.alturaOjosJugador = [] { return mundoFisico.AlturaOjosJugador(); }
	};
	return renderizador;
}

void OnSize(GLFWwindow* window, int width, int height) { renderizadorRuntime().OnSize(window, width, height); }
void OnFramebufferSize(GLFWwindow* window, int width, int height) { renderizadorRuntime().OnFramebufferSize(window, width, height); }
static bool dibujarEscenaVR() { return renderizadorRuntime().dibujarEscenaVR(); }
void OnPaint(GLFWwindow* window) { renderizadorRuntime().OnPaint(window); }

// configura_Escena: Funcio que configura els parametres de Model i dibuixa les
//                   primitives OpenGL dins classe Model
void configura_Escena() {

// Aplicar Transformacions Geometriques segons persiana Transformacio i Quaternions
	GTMatrix = instancia(transf, TG, TGF);
}

namespace
{
	GLuint rayoVao = 0, rayoVbo = 0;

	void dibujarRayoMandoVR()
	{
		if (!gestorOpenXR.SesionEnMarcha() || eixos_programID == 0) return;

		vr::PoseXR poseMando;
		if (!gestorOpenXR.PoseApuntadoMando(1, poseMando)) return; // Solo mando derecho (1)

		glm::vec3 origen, direccion;
		origenYDireccionDeLaMirada(origen, direccion);
		direccion = glm::normalize(direccion);

		// Longitud del láser (mismo alcance que el agarre/interacción)
		const float alcance = std::max(fisica::EscenaFisica::AlcanceDeAgarre, glb::RADIO_INTERACCION);
		const glm::vec3 destino = origen + direccion * alcance;

		// Color RGBA: verde si estás agarrando o apuntando a algo interactuable; cian por defecto
		const ObjetivoInteraccion objetivo = buscarObjetivoInteraccion();
		const bool resaltado = escenaFisica->HayAgarre() || objetivo.accion != AccionInteraccion::Ninguna;
		const float r = resaltado ? 0.1f : 0.0f;
		const float g = resaltado ? 1.0f : 0.85f;
		const float b = resaltado ? 0.2f : 1.0f;
		const float a = 1.0f;

		// 7 floats por vértice: pos (vec3) + color (vec4)
		const float vertices[] = {
			origen.x,  origen.y,  origen.z,  r, g, b, a,
			destino.x, destino.y, destino.z, r, g, b, a
		};

		if (rayoVao == 0)
		{
			glGenVertexArrays(1, &rayoVao);
			glGenBuffers(1, &rayoVbo);
			glBindVertexArray(rayoVao);
			glBindBuffer(GL_ARRAY_BUFFER, rayoVbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

			// location = 0 -> vec3 posición
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);

			// location = 1 -> vec4 color
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
		}
		else
		{
			glBindVertexArray(rayoVao);
			glBindBuffer(GL_ARRAY_BUFFER, rayoVbo);
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
		}

		glUseProgram(eixos_programID);
		glUniformMatrix4fv(glcache::UniformLocation(eixos_programID, "projectionMatrix"), 1, GL_FALSE, &ProjectionMatrix[0][0]);
		glUniformMatrix4fv(glcache::UniformLocation(eixos_programID, "viewMatrix"), 1, GL_FALSE, &ViewMatrix[0][0]);

		glLineWidth(2.5f);
		renderMetrics::DrawArrays(GL_LINES, 0, 2);
		glLineWidth(1.0f);

		glBindVertexArray(0);
		glUseProgram(shader_programID);
	}
}

void dibuixa_Escena()
{
	RenderLegacyVgiScene({
		.shader_programID = shader_programID,
		.eixos_programID = eixos_programID,
		.eixos_Id = eixos_Id,
		.ProjectionMatrix = ProjectionMatrix,
		.ViewMatrix = ViewMatrix,
		.SkyBoxCube = SkyBoxCube,
		.skC_programID = skC_programID,
		.cubemapTexture = cubemapTexture,
		.Vis_Polar = Vis_Polar,
		.eixos = eixos,
		.grid = grid,
		.hgrid = hgrid,
		.objecte = objecte,
		.col_obj = col_obj,
		.sw_material = sw_material,
		.textura = textura,
		.texturesID = texturesID,
		.textura_map = textura_map,
		.tFlag_invert_Y = tFlag_invert_Y,
		.npts_T = npts_T,
		.PC_t = PC_t,
		.pas_CS = pas_CS,
		.sw_Punts_Control = sw_Punts_Control,
		.dibuixa_TriedreFrenet = dibuixa_TriedreFrenet,
		.ObOBJ = ObOBJ,
		.GTMatrix = GTMatrix,
		.renderizadoOptimizado = renderizadoOptimizado,
		.intensidadIndirectaEscena = intensidadIndirectaEscena,
		.intensidadAmbientalBaseEscena = intensidadAmbientalBaseEscena,
		.escalaLuzDirectaEscena = escalaLuzDirectaEscena,
		.modoDiagnosticoIluminacion = modoDiagnosticoIluminacion,
		.colorReboteSueloEscena = colorReboteSueloEscena,
		.colorReboteCieloEscena = colorReboteCieloEscena,
		.lamparasLaboratorioActivas = lamparasLaboratorioActivas,
		.llumGL = llumGL,
		.mostrarMovil = mostrarMovil,
		.phoneOBJ = phoneOBJ,
		.movilEscala = movilEscala,
		.objetosEscena = objetosEscena,
		.modelosHabitacion = modelosHabitacion,
		.editorEscenas = editorEscenas,
		.escenaFisica = escenaFisica,
		.mitadTamanoPuertaDemo = mitadTamanoPuertaDemo,
		.bisagraPuertaDemo = bisagraPuertaDemo,
		.anguloPuertaDemo = anguloPuertaDemo,
		.demoFisica = demoFisica,
		.objetosAnimadosListos = objetosAnimadosListos,
		.objetosAnimados = objetosAnimados,
		.mostrarColisionadores = mostrarColisionadores,
		.mundoFisico = mundoFisico,
		.redJuego = redJuego,
		.iniciarConsultaGpu = iniciarConsultaGpu,
		.actualizarOclusoresPhong = actualizarOclusoresPhong,
		.modoJuegoFPS = modoJuegoFPS,
		.matrizMandoIzquierdoMundo = matrizMandoIzquierdoMundo,
		.matrizLocalMovilActual = matrizLocalMovilActual,
		.actualizarLinternaMovil = actualizarLinternaMovil,
		.texturaDeMaterial = texturaDeMaterial,
		.dibujarRayoMandoVR = dibujarRayoMandoVR,
		.finalizarConsultaGpu = finalizarConsultaGpu,
	});
}


// Barra_Estat: Actualitza la barra d'estat (Status Bar) de l'aplicació en la consola amb els
//      valors R,A,B,PVx,PVy,PVz en Visualització Interactiva.
void Barra_Estat()
{
	const std::string estado = runtimeui::FormatStatusBar({ nom, projeccio, camera, objecte, Vis_Polar,
		OPV, OPV_G, opvN, pan, tr_cpv, sw_grid, sw_color, transf, rota, trasl, escal,
		fonsR, fonsG, fonsB, TG, c_fons, col_obj, pas });
	std::fputs(estado.c_str(), stderr);
}

/* ------------------------------------------------------------------------- */
/*                            MENUS ImGui                                    */
/* ------------------------------------------------------------------------- */

// Entorn VGI: Dibuixa el menú principal ImGui que controla colors i permet activar altres menús:
//		- menú standard Imgui [show_demo_window()]
//		- menú propi de l'aplicació EntornVGI [show_EntornVGI_window()]

GLuint texturaFondoMenu = 0;

EstadoJuego estadoActual = ESTADO_MENU_PRINCIPAL;

// Ventanas ImGui heredadas del entorno VGI (demo, EntornVGI y "Status Menu").
static void dibujarVentanasLegacyImGui()
{
	if (!modoJuegoFPS() || mostrarPanelesDebug)
	{

// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
	if (show_demo_window)
		ImGui::ShowDemoWindow(&show_demo_window);

// 2.a. Show another simple window.
	if (show_another_window)
	{
		ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
		ImGui::Text("Hello from another window!");
		if (ImGui::Button("Close Me"))
			show_another_window = false;
		ImGui::End();
	}

// 2.b. Show the EntornVGI window. Finestra amb totes les opcions de l'aplicació.
	if (show_EntornVGI_window)
		ShowEntornVGIWindow(&show_EntornVGI_window); //ShowEntornVGIWindow(&show_EntornVGI_window);

// 3. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
	{	static float f = 0.0f;
		static int counter = 0;
		static float PV[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

		ImGui::Begin("Status Menu");                          // Create a window called "Status Menu" and append into it.

		ImGui::Text("Finestres EntornVGI:");               // Display some text (you can use a format strings too)
		ImGui::SameLine();
		ImGui::Checkbox("EntornVGI Window", &show_EntornVGI_window);
		static bool preset = true;
		ImGui::Separator();
		ImGui::Spacing();

		// Transformació PV de Coord. esfèriques (R,anglev,angleh) --> Coord. cartesianes (PVx,PVy,PVz)
		if (camera == CAM_NAVEGA) { PV[0] = opvN.x; PV[1] = opvN.y; PV[2] = opvN.z; }
		else {
			cam_Esferica[0] = OPV.R;	cam_Esferica[1] = OPV.alfa; cam_Esferica[2] = OPV.beta;
			if (Vis_Polar == POLARZ)
				{	PV[0] = OPV.R * cos(OPV.beta * PI / 180) * cos(OPV.alfa * PI / 180);
					PV[1] = OPV.R * sin(OPV.beta * PI / 180) * cos(OPV.alfa * PI / 180);
					PV[2] = OPV.R * sin(OPV.alfa * PI / 180);
				}
			else if (Vis_Polar == POLARY)
				{	PV[0] = OPV.R * sin(OPV.beta * PI / 180) * cos(OPV.alfa * PI / 180);
					PV[1] = OPV.R * sin(OPV.alfa * PI / 180);
					PV[2] = OPV.R * cos(OPV.beta * PI / 180) * cos(OPV.alfa * PI / 180);
			}
			else {	PV[0] = OPV.R * sin(OPV.alfa * PI / 180);
					PV[1] = OPV.R * cos(OPV.beta * PI / 180) * cos(OPV.alfa * PI / 180);
					PV[2] = OPV.R * sin(OPV.beta * PI / 180) * cos(OPV.alfa * PI / 180);
				}
		}

		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("CAMERA:");
		ImGui::PopStyleColor();

		ImGui::InputFloat3("Esferiques (R,alfa,beta)", cam_Esferica);
		ImGui::InputFloat3("Cartesianes (PVx,PVy,PVz)", PV);
		//ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f

		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("COLORS:");
		ImGui::PopStyleColor();
		ImGui::ColorEdit3("Color de Fons", (float*)&clear_colorB); // Edit 3 floats representing a background color
		ImGui::ColorEdit3("Color d'Objecte", (float*)&clear_colorO); // Edit 3 floats representing a object color
		c_fons.r = clear_colorB.x;	c_fons.g = clear_colorB.y;	c_fons.b = clear_colorB.z;	c_fons.a = clear_colorB.w;
		col_obj.r = clear_colorO.x;	col_obj.g = clear_colorO.y;	col_obj.b = clear_colorO.z;		col_obj.a = clear_colorO.w;
		ImGui::Separator();


		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("TRANSFORMA:");
		ImGui::PopStyleColor();
		float tras_ImGui[3] = { (float)TG.VTras.x,(float)TG.VTras.y,(float)TG.VTras.z };
		ImGui::InputFloat3("Traslacio (Tx,Ty,Tz)", tras_ImGui);
		float rota_ImGui[3] = { (float)TG.VRota.x,(float)TG.VRota.y,(float)TG.VRota.z };
		ImGui::InputFloat3("Rotacio (Rx,Ry,Rz)", rota_ImGui);
		float scal_ImGui[3] = { (float)TG.VScal.x,(float)TG.VScal.y,(float)TG.VScal.z };
		ImGui::InputFloat3("Escala (Sx, Sy, Sz)", scal_ImGui);

		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("ImGui:");               // Display some text (you can use a format strings too)
		ImGui::PopStyleColor();
		ImGui::Checkbox("Demo ImGui Window", &show_demo_window);      // Edit bools storing our window open/close state
		ImGui::SameLine();
		ImGui::Checkbox("Another ImGui Window", &show_another_window);
		ImGui::Spacing();

		ImGui::Text("imgui versions: (%s) (%d)", IMGUI_VERSION, IMGUI_VERSION_NUM);
		ImGui::Spacing();

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}
	}
}

void draw_Menu_ImGui()
{
	const auto fpsMode = [] { return modoJuegoFPS(); };
	const runtimeui::FrameContext contexto{
		// HUD
		{ fpsMode, [] { return entradaFPSActiva(); }, [] { return editorEscenas.Activo(); },
			barraEstamina, movimientoJugador, mostrarPorcentajeEstamina,
			cargaLanzamientoActiva, tiempoCargaLanzamiento, TiempoMaximoCargaLanzamiento, escenaFisica,
			[]() -> const char* { return objetivoInteraccionActual.accion == AccionInteraccion::Ninguna
				? nullptr : objetivoInteraccionActual.texto; },
			texturaManoInteraccion },
		// Menus
		{ window, estadoActual, texturaFondoMenu, volumenGeneralPorcentaje, audioManager,
			[] { fpsTienePosicionAnterior = false; mouseDeltaX = mouseDeltaY = 0.0; } },
		// Multijugador
		{ redJuego, configRedPendiente, mostrarPanelRed,
			[](const netjuego::ConfigRed& cfg) { iniciarRed(cfg); }, [] { detenerRed(); } },
		// Objetos dinamicos (F4)
		{ redJuego, escenaFisica, objetosEscena, hiritiFollowPlayerEnabled, animacionHiriti },
		// Paneles de depuracion (F1)
		{ mostrarMovil, movilPosicion, movilRotacion, movilEscala, intensidadIdle, intensidadWalking, intensidadMouse, deltaFPS,
			movimientoJugador, balanceoCamara, mostrarPorcentajeEstamina, mostrarContadorFPS, renderizadoOptimizado, ultimoGpuMs, modoDiagnosticoIluminacion,
			reproductorPuertaDemo, anguloPuertaDemo, mostrarColisionadores, hiritiFollowPlayerEnabled, hiritiFollowStopDistance, hiritiFollowMaxSpeed,
			modoEstereoPrueba, gestorOpenXR,
			mundoFisico, escenaFisica, demoFisica, redJuego, objetosEscena, objetosAnimados,
			[] { iniciarPuertaDemo(); }, [] { alternarOpenXR(); } },
		(c_fons.r < 0.5) || (c_fons.g < 0.5) || (c_fons.b < 0.5),
		mostrarPanelesDebug, mostrarContadorFPS,
		dibujarVentanasLegacyImGui,
		[](int ancho, int alto) { editorEscenas.DibujarInterfaz(editor::ContextoEscena{ objetosEscena, datosAnimadosEscena, objetosAnimados }, ancho, alto); }
	};
	runtimeui::DrawRuntimeUiFrame(contexto);
}

// Entorn VGI: Funció que mostra el menú principal de l'Entorn VGI amb els seus desplegables.
void MostraEntornVGIWindow(bool* p_open)
{	// Exceptionally add an extra assert here for people confused about initial Dear ImGui setup
	// Most functions would normally just crash if the context is missing.
	IM_ASSERT(ImGui::GetCurrentContext() != NULL && "Missing dear imgui context. Refer to examples app!");

// Examples Apps (accessible from the "Examples" menu)
//static bool show_window_about = false;

	if (show_window_about)       ShowAboutWindow(&show_window_about);

// We specify a default position/size in case there's no data in the .ini file.
// We only do it to make the demo applications a little more welcoming, but typically this isn't required.
	const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 650, main_viewport->WorkPos.y + 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);

	ImGuiWindowFlags window_flags = 0;
	// Main body of the Demo window starts here.
	if (!ImGui::Begin("EntornVGI Menu", p_open, window_flags))
	{	// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

// Most "big" widgets share a common width settings by default. See 'Demo->Layout->Widgets Width' for details.
// e.g. Use 2/3 of the space for widgets and 1/3 for labels (right align)
//ImGui::PushItemWidth(-ImGui::GetWindowWidth() * 0.35f);
// e.g. Leave a fixed amount of width for labels (by passing a negative value), the rest goes to widgets.
	ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

// Menu Bar
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("Arxius"))
		{
			//IMGUI_DEMO_MARKER("Menu/File");
			ShowArxiusOptions();
			ImGui::EndMenu();
		}
		if (modoJuegoFPS() && ImGui::BeginMenu("Juego"))
		{
			if (ImGui::MenuItem(
				estadoActual == ESTADO_PAUSA ? "Reanudar juego FPS" : "Pausar juego FPS"))
			{
				if (estadoActual == ESTADO_JUGANDO)
				{
					estadoActual = ESTADO_PAUSA;
					cancelarCargaLanzamiento();
				}
				else if (estadoActual == ESTADO_PAUSA)
				{
					estadoActual = ESTADO_JUGANDO;
					fpsTienePosicionAnterior = false;
					mouseDeltaX = mouseDeltaY = 0.0;
				}

				fpsPausado = (estadoActual != ESTADO_JUGANDO);
				sincronizarCursorFPS(window);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Sobre EntornVGI"))
			{	//IMGUI_DEMO_MARKER("Menu/Examples");
				//ShowAboutOptions(&show_window_about);
				ImGui::EndMenu();
			}
		ImGui::EndMenuBar();
	}

// End of ShowEntornVGIWindow()
	ImGui::PopItemWidth();
	ImGui::End();
}

// Entorn VGI: Funció que mostra els menús popUp "Arxius" per a obrir fitxers i el pop Up "About".
void ShowArxiusOptions()
{
//IMGUI_DEMO_MARKER("Examples/Menu");
	ImGui::MenuItem("(Arxius menu)", NULL, false, false);
	if (ImGui::MenuItem("New")) {}
	if (ImGui::MenuItem("Cargar habitacion JSON"))
		cargarYAplicarEscena("assets/scenes/habitacion_beta.json");
	if (ImGui::MenuItem("Cargar ejemplo fisica JSON"))
		cargarYAplicarEscena("assets/scenes/ejemplo_fisica.json");

	if (ImGui::MenuItem("Obrir Fitxer OBJ", "(<Shift>+#)")) {
		nfdchar_t* nomFitxer = NULL;
		nfdresult_t result = NFD_OpenDialog(NULL, NULL, &nomFitxer);

		if (result == NFD_OKAY) {
			puts("Success!");
			puts(nomFitxer);

			objecte = OBJOBJ;	textura = true;		tFlag_invert_Y = false; oObjecte = 9;
			//char* nomfitx = nomfitxer;
			if (ObOBJ == NULL) ObOBJ = ::new COBJModel;
			else { // Si instància ja s'ha utilitzat en un objecte OBJ
				ObOBJ->netejaVAOList_OBJ();		// Netejar VAO, EBO i VBO
				ObOBJ->netejaTextures_OBJ();	// Netejar buffers de textures
			}

			//int error = ObOBJ->LoadModel(nomfitx);			// Carregar objecte OBJ amb textura com a varis VAO's
			int error = ObOBJ->LoadModel(nomFitxer);			// Carregar objecte OBJ amb textura com a varis VAO's

			//	Pas de paràmetres textura al shader
			glUniform1i(glcache::UniformLocation(shader_programID, "textur"), textura);
			glUniform1i(glcache::UniformLocation(shader_programID, "flag_invert_y"), tFlag_invert_Y);

			free(nomFitxer);
		}

	}

	if (ImGui::MenuItem("Obrir Fractal", "(<Shift>+#)")) {
		nfdchar_t* nomFitxer = NULL;
		nfdresult_t result = NFD_OpenDialog(NULL, NULL, &nomFitxer);

		if (result == NFD_OKAY) {	puts("Success!");
									puts(nomFitxer);
									free(nomFitxer);
								}
		else if (result == NFD_CANCEL) puts("User pressed cancel.");
		else printf("Error: %s\n", NFD_GetError());
	}

	if (ImGui::BeginMenu("Open Recent"))
	{	ImGui::MenuItem("fish_hat.c");
		ImGui::MenuItem("fish_hat.inl");
		ImGui::MenuItem("fish_hat.h");
		if (ImGui::BeginMenu("More.."))
		{	ImGui::MenuItem("Hello");
			ImGui::MenuItem("Sailor");
			if (ImGui::BeginMenu("Recurse.."))
			{	ShowArxiusOptions();
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("Save", "(<Shift>+#))")) {}
	if (ImGui::MenuItem("Save As..")) {}

	if (ImGui::BeginMenu("Disabled", false)) // Disabled
		{	IM_ASSERT(0);
		}
	if (ImGui::MenuItem("Checked", NULL, true)) {}
	ImGui::Separator();
	if (ImGui::MenuItem("Quit", "Alt+F4"))
		glfwSetWindowShouldClose(window, GL_TRUE);
}


// Entorn VGI: Funció que mostra la finestra "About" de l'aplicació.
void ShowAboutWindow(bool* p_open)
{
// For the demo: add a debug button _BEFORE_ the normal log window contents
// We take advantage of a rarely used feature: multiple calls to Begin()/End() are appending to the _same_ window.
// Most of the contents of the window will be added by the log.Draw() call.
	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("About", p_open);
	//IMGUI_DEMO_MARKER("Window/About");
	ImGui::Text("VISUALITZACIO GRAFICA INTERACTIVA (Escola d'Enginyeria - UAB");
	ImGui::Text("Entorn Grafic VS2026 amb interficie GLFW 3.4, ImGui i OpenGL 4.6 per a practiques i ABP");
	ImGui::Text("AUTOR: Enric Marti Godia");
	ImGui::Text("Copyright (C) 2026");
	if (ImGui::Button("Acceptar"))
		show_window_about = false;
	ImGui::End();
}


// Entorn VGI: Funció que retorna opció de menú TIPUS CAMERA segons variable camera (si modificada per teclat)
int shortCut_Camera()
{	int auxCamera;

// Entorn VGI: Gestió opcions desplegable TIPUS CAMERA segons el valor de la variable selected
	switch (camera)
	{	case CAM_ESFERICA:	// Càmera ESFÈRICA
			auxCamera = 0;
			break;
		case CAM_NAVEGA:	// Càmera NAVEGA
			auxCamera = 1;
			break;
		case CAM_GEODE:		// Càmera GEODE
			auxCamera = 2;
			break;
		default:			// Opció CÀMERA <Altres Càmeres>
			auxCamera = -1;
			break;
	}
	return auxCamera;
}


// Entorn VGI: Funció que retorna opció de menú TIPUS CAMERA segons variable camera (si modificada per teclat)
int shortCut_Polars()
{
	int auxPolars;

// Entorn VGI: Gestió opcions desplegable TIPUS CAMERA segons el valor de la variable selected
	switch (Vis_Polar)
	{	case POLARX:	// Polars Eix X
			auxPolars = 0;
			break;
		case POLARY:	// Polars Eix Y
			auxPolars = 1;
			break;
		case POLARZ:	// Polars Eix Z
			auxPolars = 2;
			break;
		default:			// Opció CÀMERA <Altres Càmeres>
			auxPolars = -1;
			break;
	}
	return auxPolars;
}


// Entorn VGI: Funció que retorna opció de menú TIPUS PROJECCIO segons variable projecte (si modificada per teclat)
int shortCut_Projeccio()
{
	int auxProjeccio;

// Entorn VGI: Gestió opcions desplegable TIPUS PROJECCIO segons el valor de la variable selected
	switch (projeccio)
	{	case CAP:	// Projeccio CAP
			auxProjeccio = 0;
			break;
		case AXONOM:		// Projeccio AXONOMÈTRICA
			auxProjeccio = 1;
			break;
		case ORTO:			// Projeccio ORTOGRÀFICA
			auxProjeccio = 2;
			break;
		case PERSPECT:		// Projeccio PERSPECTIVA
			auxProjeccio = 3;
			break;
		default:			// Opció PROJECCIÓ <Altres Projeccions>
			auxProjeccio = 0;
			break;
	}
	return auxProjeccio;
}


// Entorn VGI: Funció que retorna opció de menú Objecte segons variable objecte (si modificada per teclat)
int shortCut_Objecte()
{
	int auxObjecte;

// Entorn VGI: Gestió opcions desplegable OBJECTES segons el valor de la variable selected
	switch (objecte)
	{	case CAP:			// Objecte CAP
		auxObjecte = 0;
		break;
		case CUB:			// Objecte CUB
			auxObjecte = 1;
			break;
		case CUB_RGB:		// Objecte CUB_RGB
			auxObjecte = 2;
			break;
		case ESFERA:		// Objecte ESFERA
			auxObjecte = 3;
			break;
		case TETERA:		// Objecte TETERA
			auxObjecte = 4;
			break;
		case ARC:			// Objecte ARC
			auxObjecte = 5;
			break;
		case MATRIUP:		// Objecte MATRIU PRIMITIVES
			auxObjecte = 6;
			break;
		case MATRIUP_VAO:	// Objecte MATRIU PRIMITIVES VAO
			auxObjecte = 7;
			break;
		case TIE:			// Objecte TIE
			auxObjecte = 8;
			break;
		case C_BEZIER:		// Objecte CORBA BEZIER
			auxObjecte = 10;
			break;
		case C_BSPLINE:		// Objecte CORBA B-SPLINE
			auxObjecte = 11;
			break;
		case C_LEMNISCATA:	// Objecte CORBA LEMNISCATA
			auxObjecte = 12;
			break;
		case C_HERMITTE:	// Objecte CORBA LEMNISCATA
			auxObjecte = 13;
			break;
		case C_CATMULL_ROM:	// Objecte CORBA LEMNISCATA
			auxObjecte = 14;
			break;
		case OBJOBJ:	// Objecte Arxiu OBJ
			auxObjecte = 9;
			break;
		default:			// Opció OBJECTE <Altres Objectes>
			auxObjecte = 0;
			break;
	}
	return auxObjecte;
}

// Entorn VGI: Funció que retorna opció de menú TIPUS ILUMINACIO segons variable ilumina (si modificada per teclat)
int shortCut_Iluminacio()
{
	int auxIlumina;

	// Entorn VGI: Gestió opcions desplegable OBJECTES segons el valor de la variable selected
	switch (ilumina)
	{	case PUNTS:		// Ilumninacio PUNTS
		auxIlumina = 0;
		break;
		case FILFERROS:	// Iluminacio FILFERROS
			auxIlumina = 1;
			break;
		case PLANA:		// Iluminacio PLANA
			auxIlumina = 2;
			break;
		case SUAU:		// Iluminacio SUAU
			auxIlumina = 3;
			break;
		default:		 // Opció Iluminacio <Altres Iluminacio>
			auxIlumina = 0;
			break;
	}
	return auxIlumina;
}

// Entorn VGI: Funció que retorna opció de menú TIPUS SHADER segons variable shader (si modificada per teclat)
int shortCut_Shader()
{
	int auxShader;

// Entorn VGI: Gestió opcions desplegable OBJECTES segons el valor de la variable selected
	switch (shader)
	{	case FLAT_SHADER:		// Shader FLAT_SHADER
			auxShader = 0;
			break;
		case GOURAUD_SHADER:	// Shader GOURAUD_SHADER
			auxShader = 1;
			break;
		case PHONG_SHADER:		// Shader PHONG
			auxShader = 2;
			break;
		case FILE_SHADER:		// Shader FILE_SHADER
			auxShader = 3;
			break;
		default:		 // Opció Iluminacio <Altres Iluminacio>
			auxShader = 0;
			break;
	}
	return auxShader;
}


// Demonstrate most Dear ImGui features (this is big function!)
// You may execute this function to experiment with the UI and understand what it does.
// You may then search for keywords in the code when you are interested by a specific feature.
void ShowEntornVGIWindow(bool* p_open)
{
	int i = 0; // Variable per a menús de selecció.
	static int selected = -1;

// Exceptionally add an extra assert here for people confused about initial Dear ImGui setup
// Most functions would normally just crash if the context is missing.
	IM_ASSERT(ImGui::GetCurrentContext() != NULL && "Missing dear imgui context. Refer to examples app!");

// Examples Apps(accessible from the "Examples" menu)
	static bool show_app_main_menu_bar = false;
	static bool show_app_documents = false;
	static bool show_app_console = false;
	static bool show_app_log = false;
	static bool show_app_layout = false;
	static bool show_app_property_editor = false;
	static bool show_app_long_text = false;
	static bool show_app_auto_resize = false;
	static bool show_app_constrained_resize = false;
	static bool show_app_simple_overlay = false;
	static bool show_app_fullscreen = false;
	static bool show_app_window_titles = false;
	static bool show_app_custom_rendering = false;

// Examples Apps (accessible from the "Examples" menu)
	//static bool show_window_about = false;

	if (show_window_about)       ShowAboutWindow(&show_window_about);

/*
	if (show_app_main_menu_bar)       ShowExampleAppMainMenuBar();
	if (show_app_documents)           ShowExampleAppDocuments(&show_app_documents);
	if (show_app_console)             ShowExampleAppConsole(&show_app_console);
	if (show_app_log)                 ShowExampleAppLog(&show_app_log);
	if (show_app_layout)              ShowExampleAppLayout(&show_app_layout);
	if (show_app_property_editor)     ShowExampleAppPropertyEditor(&show_app_property_editor);
	if (show_app_long_text)           ShowExampleAppLongText(&show_app_long_text);
	if (show_app_auto_resize)         ShowExampleAppAutoResize(&show_app_auto_resize);
	if (show_app_constrained_resize)  ShowExampleAppConstrainedResize(&show_app_constrained_resize);
	if (show_app_simple_overlay)      ShowExampleAppSimpleOverlay(&show_app_simple_overlay);
	if (show_app_fullscreen)          ShowExampleAppFullscreen(&show_app_fullscreen);
	if (show_app_window_titles)       ShowExampleAppWindowTitles(&show_app_window_titles);
	if (show_app_custom_rendering)    ShowExampleAppCustomRendering(&show_app_custom_rendering);
*/

// Dear ImGui Tools/Apps (accessible from the "Tools" menu)
	static bool show_app_metrics = false;
	static bool show_app_debug_log = false;
	static bool show_app_stack_tool = false;
	static bool show_app_about = false;
	static bool show_app_style_editor = false;

	if (show_app_metrics)
		ImGui::ShowMetricsWindow(&show_app_metrics);
	if (show_app_debug_log)
		ImGui::ShowDebugLogWindow(&show_app_debug_log);
	if (show_app_stack_tool)
		ImGui::ShowStackToolWindow(&show_app_stack_tool);
	if (show_app_about)
		ImGui::ShowAboutWindow(&show_app_about);
	if (show_app_style_editor)
	{	ImGui::Begin("Dear ImGui Style Editor", &show_app_style_editor);
		ImGui::ShowStyleEditor();
		ImGui::End();
	}

// Demonstrate the various window flags. Typically you would just use the default!
	static bool no_titlebar = false;
	static bool no_scrollbar = false;
	static bool no_menu = false;
	static bool no_move = false;
	static bool no_resize = false;
	static bool no_collapse = false;
	static bool no_close = false;
	static bool no_nav = false;
	static bool no_background = false;
	static bool no_bring_to_front = false;
	static bool unsaved_document = false;

	ImGuiWindowFlags window_flags = 0;
	if (no_titlebar)        window_flags |= ImGuiWindowFlags_NoTitleBar;
	if (no_scrollbar)       window_flags |= ImGuiWindowFlags_NoScrollbar;
	if (!no_menu)           window_flags |= ImGuiWindowFlags_MenuBar;
	if (no_move)            window_flags |= ImGuiWindowFlags_NoMove;
	if (no_resize)          window_flags |= ImGuiWindowFlags_NoResize;
	if (no_collapse)        window_flags |= ImGuiWindowFlags_NoCollapse;
	if (no_nav)             window_flags |= ImGuiWindowFlags_NoNav;
	if (no_background)      window_flags |= ImGuiWindowFlags_NoBackground;
	if (no_bring_to_front)  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	if (unsaved_document)   window_flags |= ImGuiWindowFlags_UnsavedDocument;
	if (no_close)           p_open = NULL; // Don't pass our bool* to Begin

// We specify a default position/size in case there's no data in the .ini file.
// We only do it to make the demo applications a little more welcoming, but typically this isn't required.
	const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 650, main_viewport->WorkPos.y + 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);

// Main body of the Demo window starts here.
	if (!ImGui::Begin("EntornVGI Menu", p_open, window_flags))
		{	// Early out if the window is collapsed, as an optimization.
			ImGui::End();
			return;
		}

// Most "big" widgets share a common width settings by default. See 'Demo->Layout->Widgets Width' for details.
// e.g. Use 2/3 of the space for widgets and 1/3 for labels (right align)
//ImGui::PushItemWidth(-ImGui::GetWindowWidth() * 0.35f);
// e.g. Leave a fixed amount of width for labels (by passing a negative value), the rest goes to widgets.
	ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

// Entorn VGI: Menu Bar (Pop Ups desplagables (Arxius, About)
	if (ImGui::BeginMenuBar())
	{	if (ImGui::BeginMenu("Arxius"))
			{	//IMGUI_DEMO_MARKER("Menu/File");
				//ShowExampleMenuFile();
				//ImGui::Text("Desplegable Arxius");
				ShowArxiusOptions();
				ImGui::EndMenu();
			}

		if (ImGui::BeginMenu("About"))
			{	//IMGUI_DEMO_MARKER("Menu/Examples");
				//ShowAboutOptions(&show_window_about);
				ImGui::MenuItem("Sobre EntornVGI", NULL, &show_window_about);
				ImGui::EndMenu();
			}
		ImGui::EndMenuBar();
	}

	ImGui::Text("Finestra de menus d'EntornVGI");
	ImGui::Spacing();

// Entorn VGI: DESPLEGABLES ---------------------------------------------------
// DESPLEGABLE CAMERA
//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("CAMERA"))
	{	// Entorn VGI. Mostrar Opcions desplegable TIPUS CAMERA
		//IMGUI_DEMO_MARKER("Widgets/Basic/RadioButton");

		ImGui::RadioButton("Esferica (<Shift>+L)", &oCamera, 0); ImGui::SameLine();
		static int clickCE = 0;
		if (ImGui::Button("Origen Esferica (<Shift>+O)")) clickCE++;
		if (ImGui::BeginTable("split", 3))
			{	ImGui::TableNextColumn(); ImGui::Checkbox("Mobil (<Shift>+M)", &mobil);
				ImGui::TableNextColumn(); ImGui::Checkbox("Zoom? (<Shift>+Z)", &zzoom);
				ImGui::TableNextColumn(); ImGui::Checkbox("Zoom Orto? (<Shift>+O)", &zzoomO);
				ImGui::Separator();
				ImGui::TableNextColumn(); ImGui::Checkbox("Satelit? (<Shift>+T)", &satelit);
				ImGui::EndTable();
			}
		ImGui::Separator();
		ImGui::Spacing();

		// EntornVGI: Si s'ha apretat el botó "Origen Esfèrica"
		if (clickCE)
			{	clickCE = 0;
				if (camera == CAM_ESFERICA) OnCameraOrigenEsferica();
			}

		static int clickCN = 0;
		ImGui::RadioButton("Navega (<Shift>+N)", &oCamera, 1); ImGui::SameLine();
		if (ImGui::Button("Origen Navega (<Shift>+Inici)")) clickCN++;
		ImGui::Separator();
		ImGui::Spacing();

		// EntornVGI: Si s'ha apretat el botó "Origen Navega"
		if (clickCN)
			{	clickCN = 0;
				if (camera == CAM_NAVEGA) OnCameraOrigenNavega();
			}

		static int clickCG = 0;
		ImGui::RadioButton("Geode (<Shift>+J)", &oCamera, 2); ImGui::SameLine();
		if (ImGui::Button("Origen Geode (<Shift>+Inici)")) clickCG++;

		// EntornVGI: Si s'ha apretat el botó "Origen Geode"
		if (clickCG)
			{	clickCG = 0;
				if (camera == CAM_GEODE) OnCameraOrigenGeode();
			}

		// Entorn VGI. Gestió opcions desplegable CAMERA segons el valor de la variable selected
		switch (oCamera)
		{	case 0: // Opció CAMERA Esfèrica
				if (camera != CAM_ESFERICA) OnCameraEsferica();
				break;
			case 1: // Opció CAMERA Navega
				if (camera != CAM_NAVEGA) OnCameraNavega();
				break;
			case 2:	// Opció CAMERA Geode
				if (camera != CAM_GEODE) OnCameraGeode();
				break;
			default:
				// Opció per defecte: CAMERA Esfèrica
				OnCameraEsferica();
				break;
		}

		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("POLARS");
		ImGui::PopStyleColor();
		
		// Entorn VGI. Mostrar Opcions desplegable POLARS
		//IMGUI_DEMO_MARKER("Widgets/Basic/RadioButton");
		oPolars = shortCut_Polars();	//static int oPolars = -1;
		ImGui::RadioButton("Polars X (<Shift>+X)", &oPolars, 0); ImGui::SameLine();
		ImGui::RadioButton("Polars Y (<Shift>+Y)", &oPolars, 1); ImGui::SameLine();
		ImGui::RadioButton("Polars Z (<Shift>+Z)", &oPolars, 2);

		// Entorn VGI. Gestió opcions desplegable OBJECTES segons el valor de la variable selected
		switch (oPolars)
		{	case 0: // Opció POLARS X
				if (Vis_Polar != POLARX) OnVistaPolarsX();
				break;
			case 1: // Opció POLARS Y
				if (Vis_Polar != POLARY) OnVistaPolarsY();
				break;
			case 2:	// Opció POLARS Z
				if (Vis_Polar != POLARZ) OnVistaPolarsZ();
				break;
			default:// Opció per defecte: POLARS Z
				OnVistaPolarsZ();
				break;
		}
	}

// DESPLEGABLE VISTA
//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("VISTA"))
	{	if (ImGui::BeginTable("split", 2))
			{	ImGui::TableNextColumn(); ImGui::Checkbox("Full Screen? (<Shift>+F)", &fullscreen);
				ImGui::TableNextColumn(); ImGui::Checkbox("Eixos? (<Shift>+E)", &eixos);
				ImGui::TableNextColumn(); ImGui::Checkbox("SkyBox? (<Shift>+S)", &SkyBoxCube);
				ImGui::EndTable();
			}
		ImGui::Spacing();
		ImGui::Separator();

		// Configurar opció Vista -> SkyBox?
		if (SkyBoxCube) {	// Càrrega Shader Skybox
							OnVistaSkyBox();
						}

		static int clicked = 0;
		if (ImGui::BeginTable("split", 2))
			{	ImGui::TableNextColumn(); ImGui::Checkbox("Pan? (<Shift>+P)", &pan);
				ImGui::TableNextColumn(); //ImGui::Checkbox("Origen Pan", &pan);
				//IMGUI_DEMO_MARKER("Widgets/Basic/Button");
				if (ImGui::Button("Origen Pan (<Shift>+Inici)")) clicked++;
				ImGui::EndTable();
			}
		ImGui::Spacing();
		ImGui::Separator();

		// Configurar opció Vista -> Pan?
		if (pan) OnVistaPan();
		if (clicked)
			{	if (pan) {	clicked = 0;
							OnVistaOrigenPan();
						}
			}
	}

	// DESPLEGABLE PROJECCIO
			//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("PROJECCIO"))
	{	//IMGUI_DEMO_MARKER("Widgets/Selectables/Single Selection PROJECCIO");
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("TIPUS PROJECCIO");
		ImGui::PopStyleColor();

		// Entorn VGI: Mostrar Opcions desplegable PROJECCIO
		//IMGUI_DEMO_MARKER("Widgets/Basic/RadioButton");

		ImGui::RadioButton("Axonometrica (<Shift>+A)", &oProjeccio, 1); ImGui::SameLine();
		ImGui::RadioButton("Ortografica (<Shift>+O)", &oProjeccio, 2); ImGui::SameLine();
		ImGui::RadioButton("Perspectiva (<Shift>+P)", &oProjeccio, 3);

		// Entorn VGI. Gestió opcions desplegable OBJECTES segons el valor de la variable selected
		switch (oProjeccio)
			{	case 1: // Opció PROJECCIÓ Axonomètrica
					if (projeccio != AXONOM) OnProjeccioAxonometrica();
					break;
				case 2: // Opció PROJECCIÓ Ortogràfica
					if (projeccio != ORTO) OnProjeccioOrtografica();
					break;
				case 3:	// Opció PROJECCIÓ Perspectiva
					if (projeccio != PERSPECT) OnProjeccioPerspectiva();
					break;
				default:
					// Opció per defecte: Projecció Perspectiva
					OnProjeccioPerspectiva();
					break;
				}
	}

// DESPLEGABLE VISTA
//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("OBJECTE"))
	{	// Entorn VGI. Mostrar Opcions desplegable OBJECTES
		//IMGUI_DEMO_MARKER("Widgets/Basic/RadioButton");

		// Using the _simplified_ one-liner Combo() api here
		// See "Combo" section for examples of how to use the more flexible BeginCombo()/EndCombo() api.
		//IMGUI_DEMO_MARKER("Widgets/Basic/Combo");
		const char* items[] = { "Cap(<Shift>+B)", "Cub (<Shift>+C)", "Cub RGB (<Shift>+D)", "Esfera (<Shift>+E)", "Tetera (<Shift>+T)",
			"Arc (<Shift>+R)", "Matriu Primitives (<Shift>+H)", "Matriu Primitives VAO (<Shift>+V)", "Tie (<Shift>+I)", "Arxiu OBJ",
			"Bezier (<Shift>+F5)", "B-spline (<Shift>+F6)", "Lemniscata (<Shift>+F7)", "Hermitte (<Shift>+F8)", "Catmull-Rom (<Shift>+F9)" };
		//static int item_current = 0;
		ImGui::Combo(" ", &oObjecte, items, IM_ARRAYSIZE(items));
		ImGui::Spacing();

		// Mostrar fram PARAMETRES CORBES si Objectes Corbes seleccionats
		if ((oObjecte > 9) && (oObjecte < 16)) {
			//IMGUI_DEMO_MARKER("Widgets/Selectables/Single Selection CORBES");
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
			ImGui::SeparatorText("PARAMETRES CORBES:");
			ImGui::PopStyleColor();

			if (ImGui::BeginTable("split", 2))
				{	ImGui::TableNextColumn(); ImGui::Checkbox("Triedre de Frenet? (<Shift>+F11)", &dibuixa_TriedreFrenet);
					ImGui::TableNextColumn(); ImGui::Checkbox("Triedre de Darboux? (<Shift>+F12)", &dibuixa_TriedreDarboux);
					ImGui::TableNextColumn(); ImGui::Checkbox("Punts de Control? (<Shift>+F10)", &sw_Punts_Control);
					ImGui::EndTable();
				}
			}

		// EntornVGI: Variables associades a Pop Up OBJECTE
		bool testA = false;
		int error = 0;

		// Entorn VGI. Gestió opcions desplegable OBJECTES segons el valor de la variable selected
		switch (oObjecte)
		{	case 0: // Opció OBJECTE Cap
				if (objecte != CAP) OnObjecteCap();
				break;
			case 1: // Opció OBJECTE Cub
				if (objecte != CUB) OnObjecteCub();
				break;
			case 2:	// Opció OBJECTE Cub RGB
				if (objecte != CUB_RGB) OnObjecteCubRGB();
				break;
			case 3: // Opció OBJECTE Esfera
				if (objecte != ESFERA) OnObjecteEsfera();
				break;
			case 4: // Opció OBJECTE Tetera
				if (objecte != TETERA) OnObjecteTetera();
				break;
			case 5: // Opció OBJECTE Arc
				if (objecte != ARC) OnObjecteArc();
				break;
			case 6: // Opció OBJECTE MatrVAOiu Primitives
				if (objecte != MATRIUP) OnObjecteMatriuPrimitives();
				break;
			case 7: // Opció OBJECTE Matriu Primitives VAO
				if (objecte != MATRIUP_VAO) OnObjecteMatriuPrimitivesVAO();
				break;
			case 8: // Opció OBJECTE Tie
				if (objecte != TIE) OnObjecteTie();
				break;
			case 9: // Opció OBJECTE Arxiu OBJ
				if (objecte != OBJOBJ) OnArxiuObrirFitxerObj();
				break;
			case 10: // Opció OBJECTE CORBA BEZIER
				if (objecte != C_BEZIER) OnObjecteCorbaBezier();
				break;
			case 11: // Opció OBJECTE CORBA B-SPLINE
				if (objecte != C_BSPLINE) OnObjecteCorbaBSpline();
				break;
			case 12: // Opció OBJECTE CORBA LEMNISCATA
				if (objecte != C_LEMNISCATA) OnObjecteCorbaLemniscata();
				break;
			case 13: // Opció OBJECTE CORBA HERMITTE
				if (objecte != C_HERMITTE) OnObjecteCorbaHermitte();
				break;
			case 14: // Opció OBJECTE CORBA CATMULL-ROM
				if (objecte != C_CATMULL_ROM) OnObjecteCorbaCatmullRom();
				break;
			default:
				// Opció per defecte: CAP
				OnObjecteCap();
				break;
		}
	}

// DESPLEGABLE VISTA
//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("TRANSFORMA"))
	{	//IMGUI_DEMO_MARKER("Widgets/Basic/RadioButton");
		static int clickTT = 0;
		ImGui::Checkbox("Traslacio (<Ctrl>+T)", &trasl); ImGui::SameLine();
		if (ImGui::Button("Origen Traslacio (<Ctrl>+O)")) clickTT++;
		ImGui::Spacing();

		// Si traslació activa, inhibir rotació.
		if (trasl) rota = false;
		transf = trasl || rota || escal;	// Actualitzar variable transf, si hi ha alguna transformació activa.

		// EntornVGI: Si s'ha apretat el botó "Origen Traslacio"
		if (clickTT)
			{	clickTT = 0;
				OnTransformaOrigenTraslacio();
			}

		static int clickTRota = 0;
		ImGui::Checkbox("Rotacio (<Ctrl>+R)", &rota); ImGui::SameLine();
		if (ImGui::Button("Origen Rotacio (<Ctrl>+O)")) clickTRota++;
		ImGui::Spacing();

		// Si rotació activa, inhibir traslació.
		if (rota) trasl = false;
		transf = trasl || rota || escal;	// Actualitzar variable transf, si hi ha alguna transformació activa.

		// EntornVGI: Si s'ha apretat el botó "Origen Rotacio"
		if (clickTRota)
			{	clickTRota = 0;
				OnTransformaOrigenRotacio();
			}

		static int clickTE = 0;
		ImGui::Checkbox("Escalat (<Ctrl>+S)", &escal); ImGui::SameLine();
		if (ImGui::Button("Origen Escalat (<Ctrl>+O)")) clickTE++;

		// Actualitzar variable transf, si hi ha alguna transformació activa.
		transf = trasl || rota || escal;

		// EntornVGI: Si s'ha apretat el botó "Origen Escalat"
		if (clickTE)
			{	clickTE = 0;
				OnTransformaOrigenEscalat();
			}

		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("MOBIL TRANSFORMA:");
		ImGui::PopStyleColor();

		// Guardar valors de les variables prèvies.
		bool transX_old = transX;
		bool transY_old = transY;
		bool transZ_old = transZ;

		ImGui::Checkbox("Mobil Eix X? (<Ctrl>+X)", &transX); ImGui::SameLine();
		ImGui::Checkbox("Mobil Eix y? (<Ctrl>+Y)", &transY); ImGui::SameLine();
		ImGui::Checkbox("Mobil Eix Z? (<Ctrl>+Z)", &transZ);

		if (transX) OnTransformaMobilX();
		if (transY) OnTransformaMobilY();
		if (transZ) OnTransformaMobilZ();
	}

// DESPLEGABLE VISTA
//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("OCULTACIONS"))
	{	//IMGUI_DEMO_MARKER("Widgets/Selectables/Single Selection OCULTACIONS");
		if (ImGui::BeginTable("split", 2))
			{	ImGui::TableNextColumn(); ImGui::Checkbox("Test Visibilitat? (<Ctrl>+C)", &test_vis);
				ImGui::TableNextColumn(); ImGui::Checkbox("Z-Buffer? (<Ctrl>+B)", &oculta);
				ImGui::EndTable();
			}
		ImGui::Separator();
		if (ImGui::BeginTable("split", 2))
			{	ImGui::TableNextColumn(); ImGui::Checkbox("Front Faces? (<Ctrl>+D)", &front_faces);
				//ImGui::TableNextColumn(); ImGui::Checkbox("Back Line? (<Ctrl>+B)", &back_line);
				ImGui::EndTable();
			}
	}

// DESPLEGABLE ILUMINACIO
//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("ILUMINACIO"))
	{	ImGui::Checkbox("Llum Fixe (T) / Llum lligada a camera (F) - (<Ctrl>+F)", &ifixe);
		ImGui::Spacing();

		// Entorn VGI. Mostrar Opcions desplegable TIPUS ILUMINACIO
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("TIPUS ILUMINACIO:");
		ImGui::PopStyleColor();

		// Entorn VGI. Llegir el valor actual de la variable objecte
		//int oldIlumina = shortCut_Iluminacio();	//static int oIlumina = 1;

		// Combo Boxes are also called "Dropdown" in other systems
		// Expose flags as checkbox for the demo
		static ImGuiComboFlags flags = (0 && ImGuiComboFlags_PopupAlignLeft && ImGuiComboFlags_NoPreview && ImGuiComboFlags_NoArrowButton);

		// Using the generic BeginCombo() API, you have full control over how to display the combo contents.
		// (your selection data could be an index, a pointer to the object, an id for the object, a flag intrusively
		// stored in the object itself, etc.)
		const char* items[] = { "Punts (<Ctrl>+F1)", "Filferros (<Ctrl>+F2)", "Plana (<Ctrl>+F3)",
			"Suau (<Ctrl>+F4)" };
		const char* combo_preview_value = items[oIlumina];  // Pass in the preview value visible before opening the combo (it could be anything)
		if (ImGui::BeginCombo("     ", combo_preview_value, flags))
		{	for (int n = 0; n < IM_ARRAYSIZE(items); n++)
			{	const bool is_selected = (oIlumina == n);
				if (ImGui::Selectable(items[n], is_selected)) oIlumina = n;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (is_selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		// Entorn VGI. Gestió opcions desplegable TIPUS ILUMINACIO segons el valor de la variable selected
		switch (oIlumina)
		{	case 0:
				// Opció ILUMINACIO Punts
				if (ilumina != PUNTS) OnIluminacioPunts();
				break;
			case 1:
				// Opció ILUMINACIO Filferros
				if (ilumina != FILFERROS) OnIluminacioFilferros();
				break;
			case 2:
				// Opció ILUMINACIO Plana
				if (ilumina != PLANA) OnIluminacioPlana();
				break;
			case 3:
				// Opció ILUMINACIO Suau
				if (ilumina != SUAU) OnIluminacioSuau();
				break;
			default:
				// Opció per defecte: FILFERROS
				OnIluminacioFilferros();
				break;
		}

		//IMGUI_DEMO_MARKER("Widgets/Selectables/Single Selection REFLEXIO MATERIAL");
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("REFLEXIO MATERIAL:");
		ImGui::PopStyleColor();
		ImGui::Checkbox("Material (T) / Color (F) - (<Ctrl>+F10)", &sw_material[4]);
		ImGui::Separator();
		ImGui::Spacing();

		// Pas de color material o color VAO al shader
		glUniform1i(glcache::UniformLocation(shader_programID, "sw_material"), sw_material[4]);

		if (ImGui::BeginTable("split", 2))
		{	ImGui::TableNextColumn(); ImGui::Checkbox("Emissio? (<Ctrl>+F6)", &sw_material[0]);
			ImGui::TableNextColumn(); ImGui::Checkbox("Ambient? (<Ctrl>+F7)", &sw_material[1]);
			ImGui::TableNextColumn(); ImGui::Checkbox("Difusa? (<Ctrl>+F8)", &sw_material[2]);
			ImGui::TableNextColumn(); ImGui::Checkbox("Especular? (<Ctrl>+F9)", &sw_material[3]);
			ImGui::EndTable();
		}

		// Pas de propietats de materials al shader
		aplicarIntensidadMaterial(shader_programID, llum_ambient);

		//IMGUI_DEMO_MARKER("Widgets/Selectables/Single Selection TEXTURES");
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SeparatorText("TEXTURES:");
		ImGui::PopStyleColor();
		ImGui::Checkbox("Textura? (<Ctrl>+I)", &textura);
		ImGui::SameLine();


		static int clickITS = 0;
		if (ImGui::Button("Imatge Textura SOIL (<Ctrl>+J)")) clickITS++;
		// EntornVGI: Si s'ha apretat el botó "Image Textura SOIL"
		if (clickITS) {	clickITS = 0;
						// Mostrar diàleg de fitxer i carregar imatge com a textura.
						OnIluminacioTexturaFitxerimatge();
						}

		ImGui::Spacing();
		ImGui::Checkbox("Flag_Invert_Y? (<Ctrl>+K)", &tFlag_invert_Y);
		if (tFlag_invert_Y && shader_programID) glUniform1i(glcache::UniformLocation(shader_programID, "flag_invert_y"), tFlag_invert_Y);
	}

// DESPLEGABLE LLUMS
//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("LLUMS"))
	{	ImGui::Checkbox("Llum Ambient? (<Ctrl>+A)", &llum_ambient);
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::BeginTable("split", 2))
		{	ImGui::TableNextColumn();
			ImGui::Checkbox("Llum #0 (+Z) (<Ctrl>+0)?", &llumGL[0].encesa);
			ImGui::TableNextColumn();
			ImGui::Checkbox("Llum #1 (+X) (<Ctrl>+1)?", &llumGL[1].encesa);
			ImGui::TableNextColumn();
			ImGui::Checkbox("Llum #2 (+Y) (<Ctrl>+2)?", &llumGL[2].encesa);
			ImGui::TableNextColumn();
			ImGui::Checkbox("Llum #3 (Z=Y=X) (<Ctrl>+3)?", &llumGL[3].encesa);
			ImGui::TableNextColumn();
			ImGui::Checkbox("Llum #4 (-Z)?(<Ctrl>+4)", &llumGL[4].encesa);
			ImGui::TableNextColumn();
			ImGui::Checkbox("Llum #5?(<Ctrl>+5)", &llumGL[5].encesa);
			ImGui::TableNextColumn();
			ImGui::Checkbox("Llum #6?(<Ctrl>+6)", &llumGL[6].encesa);
			ImGui::TableNextColumn();
			ImGui::Checkbox("Llum #7?(<Ctrl>+7)", &llumGL[7].encesa);
			ImGui::EndTable();
		}
		ImGui::Checkbox("Lamparas de escena", &lamparasLaboratorioActivas);
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted("Activa o apaga las luces declaradas en el JSON. La linterna F permanece independiente.");
			ImGui::EndTooltip();
		}

		// Activar llum ambient i llums (GL_LIGHT0-GL_LIGTH7)
		if (llum_ambient) OnLlumsLlumAmbient();

		if (llumGL[0].encesa)	OnLlumsLlum0();

		if (llumGL[1].encesa)	OnLlumsLlum1();

		if (llumGL[2].encesa)	OnLlumsLlum2();

		if (llumGL[3].encesa)	OnLlumsLlum3();

		if (llumGL[4].encesa)	OnLlumsLlum4();

		if (llumGL[5].encesa)	OnLlumsLlum5();

		if (llumGL[6].encesa)	OnLlumsLlum6();

		if (llumGL[7].encesa)	OnLlumsLlum7();
	}

// DESPLEGABLE SHADERS
//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("SHADERS"))
	{	// Combo Boxes are also called "Dropdown" in other systems
		// Expose flags as checkbox for the demo
		static ImGuiComboFlags flagsS = (0 && ImGuiComboFlags_PopupAlignLeft && ImGuiComboFlags_NoPreview && ImGuiComboFlags_NoArrowButton);

		// Using the generic BeginCombo() API, you have full control over how to display the combo contents.
		// (your selection data could be an index, a pointer to the object, an id for the object, a flag intrusively
		// stored in the object itself, etc.)
		const char* itemsS[] = { "Flat (<Ctrl>+L)", "Gouraud (<Ctrl>+G)", "Phong (<Ctrl>+P)",
			"Carregar fitxers Shader [.vert,.frag] (<Ctrl>+V)" };
		const char* combo_preview_value = itemsS[oShader];  // Pass in the preview value visible before opening the combo (it could be anything)
		if (ImGui::BeginCombo("Tipus de Shader", combo_preview_value, flagsS))
		{	for (int n = 0; n < IM_ARRAYSIZE(itemsS); n++)
			{	const bool is_selected = (oShader == n);
				if (ImGui::Selectable(itemsS[n], is_selected))
					oShader = n;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (is_selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		// Entorn VGI. Definició de nou Program que substituirà l'actual
		GLuint newShaderID = 0;
		// Entorn VGI. Gestió opcions desplegable TIPUS ILUMINACIO segons el valor de la variable selected
		switch (oShader)
		{	case 0: // Opció SHADER Flat
				if (shader != FLAT_SHADER)	OnShaderFlat();
				break;

			case 1:	// Opció SHADER Gouraud
				if (shader != GOURAUD_SHADER)	OnShaderGouraud();
				break;

			case 2:	// Opció SHADER Phong
				if (shader != PHONG_SHADER)	OnShaderPhong();
				break;

			case 3:	// Opció SHADER Carregar fitxers shader (.vert, .frag)
				if (shader != FILE_SHADER) OnShaderLoadFiles();
				break;

		}
	}

// DESPLEGABLE SHADERS
//IMGUI_DEMO_MARKER("Help");
	if (ImGui::CollapsingHeader("ABOUT"))
	{	//IMGUI_DEMO_MARKER("Window/About");
		ImGui::Text("VISUALITZACIO GRAFICA INTERACTIVA (Escola d'Enginyeria - UAB");
		ImGui::Text("Entorn Grafic VS2022 amb interficie GLFW 3.4, ImGui i OpenGL 4.6 per a practiques i ABP");
		ImGui::Text("AUTOR: Enric Marti Godia");
		ImGui::Text("Copyright (C) 2026");
		ImGui::Separator();
	}

	// End of ShowDemoWindow()
	ImGui::PopItemWidth();
	ImGui::End();
}


/* ------------------------------------------------------------------------- */
/*   RECURSOS DE MENU (persianes) DE L'APLICACIO:                            */
/*					1. ARXIUS												 */
/*					4. CÀMERA: Esfèrica (Mobil, Zoom, ZoomO, Satelit), Navega*/
/*					5. VISTA: Pan, Eixos i Grid							     */
/*					6. PROJECCIÓ                                             */
/*					7. OBJECTE					                             */
/*					8. TRANSFORMA											 */
/*					9. OCULTACIONS											 */
/*				   10. IL.LUMINACIÓ											 */
/*				   11. LLUMS												 */
/*				   12. SHADERS												 */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/*					1. ARXIUS 												 */
/* ------------------------------------------------------------------------- */

// Obrir fitxer Fractal
void OnArxiuObrirFractal()
{
// TODO: Agregue aquí su código de controlador de comandos

	nfdchar_t* nomFitxer = NULL;
	nfdresult_t result; // = NFD_OpenDialog(NULL, NULL, &nomFitxer);

// Entorn VGI: Obrir diàleg de lectura de fitxer (fitxers (*.MNT)
	result = NFD_OpenDialog(".mnt", NULL, &nomFitxer);

	if (result == NFD_OKAY) {
		puts("Fitxer Fractal Success!");
		puts(nomFitxer);

		objecte = O_FRACTAL;
// Entorn VGI: TO DO -> Llegir fitxer fractal (nomFitxer) i inicialitzar alçades

		free(nomFitxer);
	}

}

// OnArchivoObrirFitxerObj: Obrir fitxer en format gràfic OBJ
void OnArxiuObrirFitxerObj()
{
// TODO: Agregue aquí su código de controlador de comandos
	nfdchar_t* nomFitxer = NULL;
	nfdresult_t result = NFD_OpenDialog(NULL, NULL, &nomFitxer);

	if (result == NFD_OKAY) {
		// Entorn VGI: Variable de tipus nfdchar_t* 'nomFitxer' conté el nom del fitxer seleccionat
		puts("Success!");
		puts(nomFitxer);

		//if (ObOBJ != NULL) delete ObOBJ;
		objecte = OBJOBJ;	textura = true;		tFlag_invert_Y = false;

		if (ObOBJ == NULL) ObOBJ = ::new COBJModel;
		else {	// Si instància ja s'ha utilitzat en un objecte OBJ
			ObOBJ->netejaVAOList_OBJ();		// Netejar VAO, EBO i VBO
			ObOBJ->netejaTextures_OBJ();	// Netejar buffers de textures
		}

		//int error = ObOBJ->LoadModel(nomfitx);			// Carregar objecte OBJ amb textura com a varis VAO's
		int error = ObOBJ->LoadModel(nomFitxer);			// Carregar objecte OBJ amb textura com a varis VAO's

		//	Pas de paràmetres textura al shader
		glUniform1i(glcache::UniformLocation(shader_programID, "textur"), textura);
		glUniform1i(glcache::UniformLocation(shader_programID, "flag_invert_y"), tFlag_invert_Y);
		free(nomFitxer);
	}
}

// Obrir fitxer que conté paràmetres Font de Llum (fitxers .lght)
void OnArxiuObrirFitxerFontLlum()
{
// TODO: Agregue aquí su código de controlador de comandos
	nfdchar_t* nomFitxer = NULL;
	nfdresult_t result = NFD_OpenDialog(NULL, NULL, &nomFitxer);

	if (result == NFD_OKAY) {
		// Entorn VGI: Variable de tipus nfdchar_t* 'nomFitxer' conté el nom del fitxer seleccionat
		puts("Success!");
		puts(nomFitxer);

		bool err = llegir_FontLlum(nomFitxer); // Llegir Fitxer de Paràmetres Font de Llum
		}
}

// llegir_FontLlum: Llegir fitxer .lght que conté paràmetres de la font de lluym i-èssima. 
//				Retorna booleà a TRUE si s'ha fet la lectura correcte, FALSE en cas contrari.
//bool llegir_FontLlum(CString nomf)
bool llegir_FontLlum(char* nomf)
{
	int i, j;
	FILE* fd;

//	ifstream f("altinicials.dat",ios::in);
//    f>>i; f>>j;
	if ((fd = fopen(nomf, "rt")) == NULL)
		{	fprintf(stderr, "%s", "ERROR: File .lght was not opened");
			return false;
	}
	fscanf(fd, "%i \n", &i);
	if (i < 0 || i >= NUM_MAX_LLUMS) return false;
	else {
		fscanf(fd, "%i \n", &j); // Lectura llum encesa
		if (j == 0) llumGL[i].encesa = false; else llumGL[i].encesa = true;
		fscanf(fd, "%lf %lf %lf %lf\n", &llumGL[i].posicio.x, &llumGL[i].posicio.y, &llumGL[i].posicio.z, &llumGL[i].posicio.w);
		fscanf(fd, "%lf %lf %lf \n", &llumGL[i].difusa.r, &llumGL[i].difusa.g, &llumGL[i].difusa.b);
		fscanf(fd, "%lf %lf %lf \n", &llumGL[i].especular.r, &llumGL[i].especular.g, &llumGL[i].especular.b);
		fscanf(fd, "%lf %lf %lf \n", &llumGL[i].atenuacio.a, &llumGL[i].atenuacio.b, &llumGL[i].atenuacio.c);
		fscanf(fd, "%i \n", &j); // Lectura booleana font de llum restringida.
		if (j == 0) llumGL[i].restringida = false; else llumGL[i].restringida = true;
		fscanf(fd, "%lf %lf %lf \n", &llumGL[i].spotdirection.x, &llumGL[i].spotdirection.y, &llumGL[i].spotdirection.z);
		fscanf(fd, "%f \n", &llumGL[i].spotcoscutoff);
		fscanf(fd, "%f \n", &llumGL[i].spotexponent);
		}
	fclose(fd);

	return true;
}


// Obrir fitxers del SkyBox
void OnArxiuObrirSkybox()
{
	// TODO: Agregue aquí su código de controlador de comandos
	nfdchar_t* folderPath = NULL;
	nfdresult_t result = NFD_OpenDialog(NULL, NULL, &folderPath);
	std::vector<std::string> faces;

/*
// TODO: Agregue aquí su código de controlador de comandos
	CString folderPath;

// Càrrega VAO Skybox Cube
	if (skC_VAOID.vaoId == 0) skC_VAOID = loadCubeSkybox_VAO();
	Set_VAOList(CUBE_SKYBOX, skC_VAOID);

// Càrrega del fitxer right (+X <--> posx <--> right)
	CString facesCS = folderPath;
	facesCS += "\\right.jpg";
	// Convert a TCHAR string to a LPCSTR
	CT2CA pszConvertedAnsiString(facesCS);
// construct a std::string using the LPCSTR input
	std::string facesS1(pszConvertedAnsiString);
	faces.push_back(facesS1);

// Càrrega del fitxer left (-X <--> negx <--> left)
	facesCS = folderPath;
	facesCS += "\\left.jpg";
// Convert a TCHAR string to a LPCSTR
	CT2CA pszConvertedAnsiString2(facesCS);
// Construct a std::string using the LPCSTR input
	std::string facesS2(pszConvertedAnsiString2);
	faces.push_back(facesS2);

// Càrrega del fitxer top (+Y <--> posy <--> top)
	facesCS = folderPath;
	facesCS += "\\top.jpg";
// Convert a TCHAR string to a LPCSTR
	CT2CA pszConvertedAnsiString3(facesCS);
// Construct a std::string using the LPCSTR input
	std::string facesS3(pszConvertedAnsiString3);
	faces.push_back(facesS3);

// Càrrega del fitxer bottom (-Y <--> negy <--> bottom)
	facesCS = folderPath;
	facesCS += "\\bottom.jpg";
// Convert a TCHAR string to a LPCSTR
	CT2CA pszConvertedAnsiString4(facesCS);
// Construct a std::string using the LPCSTR input
	std::string facesS4(pszConvertedAnsiString4);
	faces.push_back(facesS4);
// Càrrega del fitxer front (+Z <--> posz <--> front)
	facesCS = folderPath;
	facesCS += "\\front.jpg";
// Convert a TCHAR string to a LPCSTR
	CT2CA pszConvertedAnsiString5(facesCS);
// Construct a std::string using the LPCSTR input
	std::string facesS5(pszConvertedAnsiString5);
	faces.push_back(facesS5);

// Càrrega del fitxer back (-Z <--> negz <--> back)
	facesCS = folderPath;
	facesCS += "\\back.jpg";
// Convert a TCHAR string to a LPCSTR
	CT2CA pszConvertedAnsiString6(facesCS);
// construct a std::string using the LPCSTR input
	std::string facesS6(pszConvertedAnsiString6);
	faces.push_back(facesS6);

//
//	if (!cubemapTexture)
//	{	// load Skybox textures
//		// -------------
//		std::vector<std::string> faces =
//		{ "./assets/textures/skybox/right.jpg",
//		"./assets/textures/skybox/left.jpg",
//		"./assets/textures/skybox/top.jpg",
//		"./assets/textures/skybox/bottom.jpg",
//		"./assets/textures/skybox/front.jpg",
//		"./assets/textures/skybox/back.jpg"
//	};

	cubemapTexture = loadCubemap(faces);
//	}

*/
}

static LegacyVgiViewControls& controlesVistaVgi()
{
	static LegacyVgiViewControls controles{
		.camera = camera,
		.OPV = OPV,
		.OPV_G = OPV_G,
		.Vis_Polar = Vis_Polar,
		.mobil = mobil,
		.zzoom = zzoom,
		.zzoomO = zzoomO,
		.satelit = satelit,
		.m_EsfeIncEAvall = m_EsfeIncEAvall,
		.anima = anima,
		.opvN = opvN,
		.n = n,
		.angleZ = angleZ,
		.spawnEscenaActiva = spawnEscenaActiva,
		.crearJugadorFisico = crearJugadorFisico,
		.fullscreen = fullscreen,
		.pan = pan,
		.fact_pan = fact_pan,
		.tr_cpv = tr_cpv,
		.tr_cpvF = tr_cpvF,
		.transX = transX,
		.transY = transY,
		.transZ = transZ,
		.eixos = eixos,
		.llumGL = llumGL,
		.shader_SkyBoxC = shader_SkyBoxC,
		.skC_programID = skC_programID,
		.skC_VAOID = skC_VAOID,
		.cubemapTexture = cubemapTexture,
		.projeccio = projeccio,
	};
	return controles;
}

void OnCameraEsferica() { controlesVistaVgi().OnCameraEsferica(); }
void OnCameraOrigenEsferica() { controlesVistaVgi().OnCameraOrigenEsferica(); }
void OnVistaMobil() { controlesVistaVgi().OnVistaMobil(); }
void OnVistaZoom() { controlesVistaVgi().OnVistaZoom(); }
void OnVistaZoomOrto() { controlesVistaVgi().OnVistaZoomOrto(); }
void OnVistaSatelit() { controlesVistaVgi().OnVistaSatelit(); }
void OnVistaPolarsX() { controlesVistaVgi().OnVistaPolarsX(); }
void OnVistaPolarsY() { controlesVistaVgi().OnVistaPolarsY(); }
void OnVistaPolarsZ() { controlesVistaVgi().OnVistaPolarsZ(); }
void OnCameraNavega() { controlesVistaVgi().OnCameraNavega(); }
void OnCameraOrigenNavega() { controlesVistaVgi().OnCameraOrigenNavega(); }
void OnCameraGeode() { controlesVistaVgi().OnCameraGeode(); }
void OnCameraOrigenGeode() { controlesVistaVgi().OnCameraOrigenGeode(); }
void OnVistaFullscreen() { controlesVistaVgi().OnVistaFullscreen(); }
void OnVistaPan() { controlesVistaVgi().OnVistaPan(); }
void OnVistaOrigenPan() { controlesVistaVgi().OnVistaOrigenPan(); }
void OnVistaEixos() { controlesVistaVgi().OnVistaEixos(); }
void OnVistaSkyBox() { controlesVistaVgi().OnVistaSkyBox(); }
void OnProjeccioPerspectiva() { controlesVistaVgi().OnProjeccioPerspectiva(); }
void OnProjeccioOrtografica() { controlesVistaVgi().OnProjeccioOrtografica(); }
void OnProjeccioAxonometrica() { controlesVistaVgi().OnProjeccioAxonometrica(); }


static LegacyVgiGeometryControls& controlesGeometriaVgi()
{
	static LegacyVgiGeometryControls controles{
		.objecte = objecte,
		.col_obj = col_obj,
		.texturesID = texturesID,
		.ObOBJ = ObOBJ,
		.sw_material = sw_material,
		.npts_T = npts_T,
		.PC_t = PC_t,
		.pas_CS = pas_CS,
		.sw_Punts_Control = sw_Punts_Control,
		.dibuixa_TriedreFrenet = dibuixa_TriedreFrenet,
		.dibuixa_TriedreDarboux = dibuixa_TriedreDarboux,
		.llegir_ptsC = llegir_ptsC,
		.transf = transf,
		.trasl = trasl,
		.rota = rota,
		.escal = escal,
		.fact_Tras = fact_Tras,
		.fact_Rota = fact_Rota,
		.TG = TG,
		.transX = transX,
		.transY = transY,
		.transZ = transZ,
		.mobil = mobil,
		.zzoom = zzoom,
		.pan = pan,
		.front_faces = front_faces,
		.test_vis = test_vis,
		.oculta = oculta,
	};
	return controles;
}

void OnObjecteCap() { controlesGeometriaVgi().OnObjecteCap(); }
void OnObjecteCub() { controlesGeometriaVgi().OnObjecteCub(); }
void OnObjecteCubRGB() { controlesGeometriaVgi().OnObjecteCubRGB(); }
void OnObjecteEsfera() { controlesGeometriaVgi().OnObjecteEsfera(); }
void OnObjecteTetera() { controlesGeometriaVgi().OnObjecteTetera(); }
void OnObjecteArc() { controlesGeometriaVgi().OnObjecteArc(); }
void OnObjecteTie() { controlesGeometriaVgi().OnObjecteTie(); }
void OnObjecteCorbaBezier() { controlesGeometriaVgi().OnObjecteCorbaBezier(); }
void OnObjecteCorbaLemniscata() { controlesGeometriaVgi().OnObjecteCorbaLemniscata(); }
void OnObjecteCorbaHermitte() { controlesGeometriaVgi().OnObjecteCorbaHermitte(); }
void OnObjecteCorbaCatmullRom() { controlesGeometriaVgi().OnObjecteCorbaCatmullRom(); }
void OnObjecteCorbaBSpline() { controlesGeometriaVgi().OnObjecteCorbaBSpline(); }
void OnObjectePuntsControl() { controlesGeometriaVgi().OnObjectePuntsControl(); }
void OnCorbesTriedreFrenet() { controlesGeometriaVgi().OnCorbesTriedreFrenet(); }
void OnCorbesTriedreDarboux() { controlesGeometriaVgi().OnCorbesTriedreDarboux(); }
void OnObjecteMatriuPrimitives() { controlesGeometriaVgi().OnObjecteMatriuPrimitives(); }
void OnObjecteMatriuPrimitivesVAO() { controlesGeometriaVgi().OnObjecteMatriuPrimitivesVAO(); }

void OnTransformaTraslacio() { controlesGeometriaVgi().OnTransformaTraslacio(); }
void OnTransformaOrigenTraslacio() { controlesGeometriaVgi().OnTransformaOrigenTraslacio(); }
void OnTransformaRotacio() { controlesGeometriaVgi().OnTransformaRotacio(); }
void OnTransformaOrigenRotacio() { controlesGeometriaVgi().OnTransformaOrigenRotacio(); }
void OnTransformaEscalat() { controlesGeometriaVgi().OnTransformaEscalat(); }
void OnTransformaOrigenEscalat() { controlesGeometriaVgi().OnTransformaOrigenEscalat(); }
void OnTransformaMobilX() { controlesGeometriaVgi().OnTransformaMobilX(); }
void OnTransformaMobilY() { controlesGeometriaVgi().OnTransformaMobilY(); }
void OnTransformaMobilZ() { controlesGeometriaVgi().OnTransformaMobilZ(); }

void OnOcultacionsFrontFaces() { controlesGeometriaVgi().OnOcultacionsFrontFaces(); }
void OnOcultacionsTestvis() { controlesGeometriaVgi().OnOcultacionsTestvis(); }
void OnOcultacionsZBuffer() { controlesGeometriaVgi().OnOcultacionsZBuffer(); }

/* ------------------------------------------------------------------------- */
/*		10-12. IL.LUMINACIÓ, LLUMS, SHADERS								 */
/* ------------------------------------------------------------------------- */

// Implementacions a src/rendering/RuntimeRenderControls.cpp.
static RuntimeRenderControls& controlesRender()
{
	static RuntimeRenderControls controles{
		.ifixe = ifixe,
		.ilum2sides = ilum2sides,
		.ilumina = ilumina,
		.oIlumina = oIlumina,
		.test_vis = test_vis,
		.oculta = oculta,
		.sw_material = sw_material,
		.sw_il = sw_il,
		.llum_ambient = llum_ambient,
		.llumGL = llumGL,
		.textura = textura,
		.t_textura = t_textura,
		.tFlag_invert_Y = tFlag_invert_Y,
		.texturesID = texturesID,
		.shader = shader,
		.shader_programID = shader_programID,
		.shaderLighting = shaderLighting,
		.oclusoresPhongInvalidos = oclusoresPhongInvalidos,
		.aplicarIntensidadMaterial = [](GLuint programa, bool ambienteActivo)
			{ aplicarIntensidadMaterial(programa, ambienteActivo); }
	};
	return controles;
}

void OnIluminacioLlumfixe() { controlesRender().OnIluminacioLlumfixe(); }
void OnIluminacio2Sides() { controlesRender().OnIluminacio2Sides(); }
void OnIluminacioPunts() { controlesRender().OnIluminacioPunts(); }
void OnIluminacioFilferros() { controlesRender().OnIluminacioFilferros(); }
void OnIluminacioPlana() { controlesRender().OnIluminacioPlana(); }
void OnIluminacioSuau() { controlesRender().OnIluminacioSuau(); }
void OnMaterialReflmaterial() { controlesRender().OnMaterialReflmaterial(); }
void OnMaterialEmissio() { controlesRender().OnMaterialEmissio(); }
void OnMaterialAmbient() { controlesRender().OnMaterialAmbient(); }
void OnMaterialDifusa() { controlesRender().OnMaterialDifusa(); }
void OnMaterialEspecular() { controlesRender().OnMaterialEspecular(); }
void OnIluminacioTextures() { controlesRender().OnIluminacioTextures(); }
void OnIluminacioTexturaFitxerimatge() { controlesRender().OnIluminacioTexturaFitxerimatge(); }
void OnIluminacioTexturaFlagInvertY() { controlesRender().OnIluminacioTexturaFlagInvertY(); }
void OnLlumsLlumAmbient() { controlesRender().OnLlumsLlumAmbient(); }
void OnLlumsLlum0() { controlesRender().OnLlumsLlum0(); }
void OnLlumsLlum1() { controlesRender().OnLlumsLlum1(); }
void OnLlumsLlum2() { controlesRender().OnLlumsLlum2(); }
void OnLlumsLlum3() { controlesRender().OnLlumsLlum3(); }
void OnLlumsLlum4() { controlesRender().OnLlumsLlum4(); }
void OnLlumsLlum5() { controlesRender().OnLlumsLlum5(); }
void OnLlumsLlum6() { controlesRender().OnLlumsLlum6(); }
void OnLlumsLlum7() { controlesRender().OnLlumsLlum7(); }
void OnShaderFlat() { controlesRender().OnShaderFlat(); }
void OnShaderGouraud() { controlesRender().OnShaderGouraud(); }
void OnShaderPhong() { controlesRender().OnShaderPhong(); }
void OnShaderLoadFiles() { controlesRender().OnShaderLoadFiles(); }
void OnShaderPBinaryWrite() { controlesRender().OnShaderPBinaryWrite(); }
void OnShaderPBinaryRead() { controlesRender().OnShaderPBinaryRead(); }

/* ------------------------------------------------------------------------- */
/*                            FI MENUS ImGui                                 */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/*                           CONTROL DEL TECLAT                              */
/* ------------------------------------------------------------------------- */

// OnKeyDown: Funció de tractament de teclat (funció que es crida quan es prem una tecla)
//   PARÀMETRES:
//    - key: Codi del caracter seleccionat
//    - scancode: Nombre de vegades que s'ha apretat la tecla (acceleració)
//    - action: Acció de la tecla: GLFW_PRESS (si s'ha apretat), GLFW_REPEAT, si s'ha repetit pressió i GL_RELEASE, si es deixa d'apretar.
//    - mods: Variable que identifica si la tecla s'ha pulsat directa (mods=0), juntament amb la tecla Shift (mods=1) o la tecla Ctrl (mods=2).
void OnKeyDown(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	const double incr = 0.025f;
	double modul = 0;
	GLdouble vdir[3] = { 0, 0, 0 };

	// EntornVGI.ImGui: Test si events de mouse han sigut filtrats per ImGui (io.WantCaptureMouse)
	// (1) ALWAYS forward mouse data to ImGui! This is automatic with default backends. With your own backend:
	ImGuiIO& io = ImGui::GetIO();
	// F4 es global: el panel se puede recuperar desde PLAY, EDIT, pausa o VR,
	// incluso cuando los paneles de depuracion estan cerrados.
	if (key == GLFW_KEY_F4 && action == GLFW_PRESS && mods == 0 &&
		!io.WantTextInput)
	{
		mostrarPanelRed = !mostrarPanelRed;
		sincronizarCursorFPS(window);
		return;
	}
	if (key == GLFW_KEY_E && action == GLFW_RELEASE && cargaLanzamientoActiva)
	{
		if (entradaFPSActiva() && !io.WantTextInput && !io.WantCaptureKeyboard &&
			!io.WantCaptureMouse && !gestorOpenXR.SesionEnMarcha())
			lanzarObjetoCargado();
		else
			cancelarCargaLanzamiento();
		return;
	}

	// F6: alternar entre PLAY y EDIT.
	if (key == GLFW_KEY_F6 &&
		action == GLFW_PRESS &&
		mods == 0 &&
		!io.WantTextInput &&
		estadoActual != ESTADO_MENU_PRINCIPAL)
	{
		if (redJuego.EnRed() && redJuego.EsCliente())
		{
			editorEscenas.InformarEstado("Solo el host puede editar la escena compartida.", true);
			return;
		}
		editorEscenas.Atajo(key, action, mods, false);
		sincronizarCursorFPS(window);
		return;
	}

	if (editorEscenas.Activo())
	{
		editorEscenas.Atajo(key, action, mods, io.WantTextInput);
		sincronizarCursorFPS(window);
		return;
	}

	// F: encender/apagar linterna solo durante el juego.
	if (key == GLFW_KEY_F &&
		action == GLFW_PRESS &&
		estadoActual == ESTADO_JUGANDO &&
		entradaFPSActiva() &&
		!io.WantTextInput &&
		!io.WantCaptureKeyboard)
	{
		linternaMovilActiva = !linternaMovilActiva;

		std::cout << "Linterna: "
				<< (linternaMovilActiva ? "ON" : "OFF")
				<< std::endl;
		return;
	}

	if (modoJuegoFPS())
	{
	// ESC: pausar y reanudar.
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		if (estadoActual == ESTADO_JUGANDO)
		{
			estadoActual = ESTADO_PAUSA;
			cancelarCargaLanzamiento();
		}
		else if (estadoActual == ESTADO_PAUSA)
		{
			estadoActual = ESTADO_JUGANDO;
			fpsTienePosicionAnterior = false;
			mouseDeltaX = mouseDeltaY = 0.0;
		}

		fpsPausado = (estadoActual != ESTADO_JUGANDO);
		sincronizarCursorFPS(window);
		return;
	}

	// Aquí continúa el resto del código FPS.
	if (key == GLFW_KEY_M && action == GLFW_PRESS &&
		(mods & GLFW_MOD_CONTROL) && (mods & GLFW_MOD_SHIFT))
	{
		fpsCapturaSolicitada = !fpsCapturaSolicitada;
		sincronizarCursorFPS(window);
		return;
	}
	if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
	{
		mostrarPanelesDebug = !mostrarPanelesDebug;
		if (mostrarPanelesDebug) show_EntornVGI_window = true;
		sincronizarCursorFPS(window);
		return;
	}
	// Mantener E carga el lanzamiento; soltarla lanza el objeto sujeto.
	if (key == GLFW_KEY_E && action == GLFW_PRESS)
	{
		const bool gateJuego = glb::PuedeInteractuar(fpsPausado, ventanaConFoco, io.WantTextInput);
		const bool gateInterfaz = !mostrarPanelesDebug && !io.WantCaptureKeyboard &&
			!io.WantCaptureMouse && !gestorOpenXR.SesionEnMarcha();
		if (diagnosticoAgarreActivo())
		{
			const ObjetivoInteraccion& objetivo = objetivoInteraccionActual;
			uint32_t idObjetivo = 0;
			if (objetivo.accion == AccionInteraccion::Recoger && objetivo.indiceDinamico >= 0 &&
				objetivo.indiceDinamico < static_cast<int>(objetosEscena.size()))
				idObjetivo = netjuego::HashId(objetosEscena[objetivo.indiceDinamico].id);
			glm::vec3 origenRayo(0.0f), direccionRayo(0.0f);
			origenYDireccionDeLaMirada(origenRayo, direccionRayo);
			float distanciaRayo = std::numeric_limits<float>::infinity();
			const fisica::IdCuerpo cuerpoRayo = mundoFisico.CuerpoAlFinalDelRayo(
				origenRayo, direccionRayo,
				std::max(fisica::EscenaFisica::AlcanceDeAgarre, glb::RADIO_INTERACCION), &distanciaRayo);
			const int indiceCuerpoDinamico = cuerpoRayo == fisica::CuerpoInvalido
				? -1 : escenaFisica->ObjetoDinamicoDeCuerpo(cuerpoRayo);
			const int indiceCuerpoAnimado = cuerpoRayo == fisica::CuerpoInvalido
				? -1 : escenaFisica->ObjetoAnimadoDeCuerpo(cuerpoRayo);
			std::string idCuerpoRayo = "none";
			if (indiceCuerpoDinamico >= 0 && indiceCuerpoDinamico < static_cast<int>(objetosEscena.size()))
				idCuerpoRayo = objetosEscena[indiceCuerpoDinamico].id;
			const char* rol = redJuego.EsHost() ? "host" : redJuego.EsCliente() ? "client" : "offline";
			std::ostringstream registro;
			registro << "[grab] input key=E role=" << rol
				<< " game=" << (estadoActual == ESTADO_JUGANDO)
				<< " allowed=" << (gateJuego && gateInterfaz)
				<< " paused=" << fpsPausado << " focused=" << ventanaConFoco
				<< " text=" << io.WantTextInput << " captureKeyboard=" << io.WantCaptureKeyboard
				<< " captureMouse=" << io.WantCaptureMouse
				<< " debugPanels=" << mostrarPanelesDebug
				<< " xr=" << gestorOpenXR.SesionEnMarcha()
				<< " holding=" << jugadorTieneAgarre()
				<< " action=" << static_cast<int>(objetivo.accion)
				<< " dynamicIndex=" << objetivo.indiceDinamico
				<< " targetId=" << idObjetivo
				<< " rayBody=" << cuerpoRayo << " rayDynamicIndex=" << indiceCuerpoDinamico
				<< " rayAnimatedIndex=" << indiceCuerpoAnimado << " rayObjectId=" << idCuerpoRayo
				<< " rayDistance=" << distanciaRayo
				<< " rayOrigin=(" << origenRayo.x << ',' << origenRayo.y << ',' << origenRayo.z
				<< ") rayDirection=(" << direccionRayo.x << ',' << direccionRayo.y << ',' << direccionRayo.z << ')';
			emitirDiagnosticoAgarre(registro.str());
		}
		if (gateJuego && gateInterfaz)
		{
			if (jugadorTieneAgarre()) iniciarCargaLanzamiento();
			else interactuar();
		}
		return;
	}
	// F3: activar/desactivar OpenXR de forma EXPLÍCITA (independiente de F2).
	if (key == GLFW_KEY_F3 && action == GLFW_PRESS)
	{
		alternarOpenXR();
		return;
	}
	// F2: alternar la vista estereoscópica de prueba (dos ojos en pantalla partida).
	if (key == GLFW_KEY_F2 && action == GLFW_PRESS)
	{
		modoEstereoPrueba = !modoEstereoPrueba;
		std::cout << "Vista estereoscopica de prueba: " << (modoEstereoPrueba ? "ON" : "OFF") << std::endl;
		return;
	}
	if (fpsPausado || !ventanaConFoco) return;
	if (key == GLFW_KEY_W || key == GLFW_KEY_A || key == GLFW_KEY_S || key == GLFW_KEY_D) return;
	if (key == GLFW_KEY_PRINT_SCREEN && mods == 0 && action == GLFW_PRESS)
		statusB = !statusB;
	return;
}

// (2) ONLY forward mouse data to your underlying app/game.
	if (!io.WantCaptureKeyboard) { //<Tractament mouse de l'aplicació>}
		// EntornVGI: Si tecla pulsada és ESCAPE, tancar finestres i aplicació.
		if (mods == 0 && key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		{
			if (editorEscenas.HayCambiosSinGuardar()) editorEscenas.SolicitarConfirmacionCierre();
			else glfwSetWindowShouldClose(window, GL_TRUE);
		}
		else if (mods == 0 && key == GLFW_KEY_PRINT_SCREEN && action == GLFW_PRESS) statusB = !statusB;
		else if ((mods == 1) && (action == GLFW_PRESS)) Teclat_Shift(key, window);	// Shorcuts Shift Key
		else if ((mods == 2) && (action == GLFW_PRESS)) Teclat_Ctrl(key);	// Shortcuts Ctrl Key
		else if ((objecte == C_BEZIER || objecte == C_BSPLINE || objecte == C_LEMNISCATA || objecte == C_HERMITTE
			|| objecte == C_CATMULL_ROM) && (action == GLFW_PRESS)) Teclat_PasCorbes(key, action);
		else if (camera == CAM_NAVEGA) Teclat_Navega(key, action);
		else if ((sw_grid) && ((grid.x) || (grid.y) || (grid.z))) Teclat_Grid(key, action);
		else if (((key == GLFW_KEY_G) && (action == GLFW_PRESS)) && ((grid.x) || (grid.y) || (grid.z))) sw_grid = !sw_grid;
		else if ((key == GLFW_KEY_O) && (action == GLFW_PRESS)) sw_color = true; // Activació color objecte
		else if ((key == GLFW_KEY_F) && (action == GLFW_PRESS)) sw_color = false; // Activació color objecte
		else if (pan) Teclat_Pan(key, action);
		else if (transf)
			{	if (rota) Teclat_TransRota(key, action);
					else if (trasl) Teclat_TransTraslada(key, action);
							else if (escal) Teclat_TransEscala(key, action);
			}
		else if (!sw_color) Teclat_ColorFons(key, action);
		else Teclat_ColorObjecte(key, action);
	}
	// Crida a OnPaint() per redibuixar l'escena
		//OnPaint(window);

/*
	if (key == GLFW_KEY_E && action == GLFW_PRESS)
			activate_airship();

	int state = glfwGetKey(window, GLFW_KEY_E);
	if (state == GLFW_PRESS) activate_airship();
*/
}

// Teclat_Shift: Shortcuts per Pop Ups Fitxer, Finestra, Vista, Projecció i Objecte
void Teclat_Shift(int key, GLFWwindow* window)
{
	//const char* nomfitxer;
	bool testA = false;
	int error = 0;

	//	nfdchar_t* nomFitxer = NULL;
	//	nfdresult_t result; // = NFD_OpenDialog(NULL, NULL, &nomFitxer);

	CColor color_Mar = { 0.0,0.0,0.0,1.0 };

	switch (key)
	{
	// ----------- POP UP Fitxers
	// Tecla Obrir Fractal
	case GLFW_KEY_F1:
		OnArxiuObrirFractal();
/*
		nomFitxer = NULL;
		// Entorn VGI: Obrir diàleg de lectura de fitxer (fitxers (*.MNT)
		result = NFD_OpenDialog(NULL, NULL, &nomFitxer);

		if (result == NFD_OKAY) {
			puts("Fitxer Fractal Success!");
			puts(nomFitxer);

			objecte = O_FRACTAL;
			// Entorn VGI: TO DO -> Llegir fitxer fractal (nomFitxer) i inicialitzar alçades

			free(nomFitxer);
			}
*/
		break;

	// Tecla Obrir Fitxer OBJ
	case GLFW_KEY_F2:
		OnArxiuObrirFitxerObj();
/*
		// Entorn VGI: Obrir diàleg de lectura de fitxer (fitxers (*.OBJ)
		nomFitxer = NULL;
		result = NFD_OpenDialog(NULL, NULL, &nomFitxer);

		if (result == NFD_OKAY) {
			puts("Success!");
			puts(nomFitxer);

			objecte = OBJOBJ;	textura = true;		tFlag_invert_Y = false;
			//char* nomfitx = nomFitxer;
			if (ObOBJ == NULL) ObOBJ = ::new COBJModel;
			else { // Si instància ja s'ha utilitzat en un objecte OBJ
				ObOBJ->netejaVAOList_OBJ();		// Netejar VAO, EBO i VBO
				ObOBJ->netejaTextures_OBJ();	// Netejar buffers de textures
				}

			//int error = ObOBJ->LoadModel(nomfitx);			// Carregar objecte OBJ amb textura com a varis VAO's
			int error = ObOBJ->LoadModel(nomFitxer);			// Carregar objecte OBJ amb textura com a varis VAO's

			//	Pas de paràmetres textura al shader
			if (shader_programID) glUniform1i(glcache::UniformLocation(shader_programID, "textur"), textura);
			if (shader_programID) glUniform1i(glcache::UniformLocation(shader_programID, "flag_invert_y"), tFlag_invert_Y);
			free(nomFitxer);
			}
	*/
		break;

	// ----------- POP UP Finestra
	// Tecla Obrir nova finestra
	case GLFW_KEY_W:

		break;

// ----------- POP UP Camera

	// 		// Tecla Camera Esferica
	case GLFW_KEY_L:
		OnCameraEsferica();
		oCamera = shortCut_Camera();	// Actualitzar opció de menú CAMERA
		/*
		if ((projeccio != ORTO) && (projeccio != CAP)) camera = CAM_ESFERICA;
		// Activació de zoom, mobil
		mobil = true;	zzoom = true;
		*/
		break;


	// Tecla Mobil?
	case GLFW_KEY_M:
		OnVistaMobil();
		/*
		if ((projeccio != ORTO) && (projeccio != CAP)) mobil = !mobil;
		// Desactivació de Transformacions Geomètriques via mouse
		//		si Visualització Interactiva activada.
		if (mobil) {	transX = false;	transY = false; transZ = false;
					}
		*/
		break;

	// Tecla Zoom
	case GLFW_KEY_Q:
		OnVistaZoom();
		/*if ((projeccio != ORTO) && (projeccio != CAP)) zzoom = !zzoom;
		// Desactivació de Transformacions Geomètriques via mouse
		//		si Zoom activat.
		if (zzoom) {
			zzoomO = false;  transX = false;	transY = false;	transZ = false;
			}
		*/
		break;

	// Tecla Zoom Orto
	case GLFW_KEY_F3:
		OnVistaZoomOrto();
		/*
		if (projeccio == ORTO || projeccio==AXONOM) zzoomO = !zzoomO;
		// Desactivació de Transformacions Geomètriques via mouse
		//		si Zoom Orto activat.
		if (zzoomO) {
			zzoom = false;  transX = false;		transY = false;		transZ = false;
			}
		*/
		break;

	// Tecla Satèl.lit?
	case GLFW_KEY_S:
		OnVistaSatelit();
/*
		if ((projeccio != CAP && projeccio != ORTO)) satelit = !satelit;
		if (satelit) mobil = true;
		testA = anima;				// Testejar si hi ha alguna animació activa apart de Satèlit.
*/
		break;

	// Tecla Camera Navega
	case GLFW_KEY_N:
		OnCameraNavega();
		oCamera = shortCut_Camera();	// Actualitzar opció de menú CAMERA
/*
		if ((projeccio != ORTO) && (projeccio != CAP)) camera = CAM_NAVEGA;
		// Desactivació de zoom, mobil, Transformacions Geomètriques via mouse i pan
		//		si navega activat
		if (camera == CAM_NAVEGA)
		{	mobil = false;	zzoom = false;
			transX = false;	transY = false;	transZ = false;
			pan = false;
			tr_cpv.x = 0.0;		tr_cpv.y = 0.0;		tr_cpv.z = 0.0;	// Inicialitzar a 0 desplaçament de pantalla
			tr_cpvF.x = 0.0;	tr_cpvF.y = 0.0;	tr_cpvF.x = 0.0; // Inicialitzar a 0 desplaçament de pantalla
		}
		else {	mobil = true;
				zzoom = true;
			}
*/
		break;

	// Tecla Camera Geode
	case GLFW_KEY_J:
		OnCameraGeode();
		oCamera = shortCut_Camera();	// Actualitzar opció de menú CAMERA
/*
		if ((projeccio != ORTO) && (projeccio != CAP)) camera = CAM_GEODE;
		// Desactivació de zoom, mobil, Transformacions Geomètriques via mouse i pan
		//		si navega activat
		if (camera == CAM_GEODE)
		{
			OPV_G.R = 0.0;		OPV_G.alfa = 0.0;		OPV_G.beta = 0.0;
			OPV.R = 0.0;		OPV.alfa = 0.0;			OPV.beta = 0.0;				// Origen PV en esfèriques
			mobil = true;		zzoom = true;	zzoomO = false;	 satelit = false;	pan = false;
			Vis_Polar = POLARZ;	llumGL[5].encesa = true;
			glFrontFace(GL_CW);
		}
*/
		break;

		// Tecla Origen (per a Pan i Navega)
	case GLFW_KEY_HOME:
		if (pan) OnVistaOrigenPan();
/*
		{	fact_pan = 1;
					tr_cpv.x = 0;	tr_cpv.y = 0;	tr_cpv.z = 0;
				}
*/
		if (camera == CAM_NAVEGA) OnCameraOrigenNavega();
/*
			{	n[0] = 0.0;		n[1] = 0.0;		n[2] = 0.0;
				opvN.x = 10.0;	opvN.y = 0.0;		opvN.z = 0.0;
				angleZ = 0.0;
			}
*/
		else if (camera == CAM_NAVEGA) OnCameraOrigenGeode();

		break;

	// Tecla Polars Eix X?
	case GLFW_KEY_X:
		OnVistaPolarsX();
		//if ((projeccio != CAP) && (camera != CAM_NAVEGA)) Vis_Polar = POLARX;
		break;

	// Tecla Polars Eix Y?
	case GLFW_KEY_Y:
		OnVistaPolarsY();
		//if ((projeccio != CAP) && (camera != CAM_NAVEGA)) Vis_Polar = POLARY;
		break;

	// Tecla Polars Eix Z?
	case GLFW_KEY_Z:
		OnVistaPolarsZ();
		//if ((projeccio != CAP) && (camera != CAM_NAVEGA)) Vis_Polar = POLARZ;
		break;

// ----------- POP UP Vista
	// 
				// Tecla Full Screen?
	case GLFW_KEY_F:
		OnFull_Screen(primary, window);
		break;

		// Tecla Eixos?
	case GLFW_KEY_F4:
		OnVistaEixos();
		//eixos = !eixos;
		break;

	// Tecla Skybox Cube
	case GLFW_KEY_K:
		SkyBoxCube = !SkyBoxCube;
		if (SkyBoxCube) OnVistaSkyBox();
/*
		{	Vis_Polar = POLARY;
			// Càrrega VAO Skybox Cube
			if (skC_VAOID.vaoId == 0) skC_VAOID = loadCubeSkybox_VAO();

			if (!cubemapTexture)
				{	// load Skybox textures
					// -------------
					std::vector<std::string> faces =
						{	"./assets/textures/skybox/right.jpg",
							"./assets/textures/skybox/right.jpg",
							"./assets/textures/skybox/left.jpg",
							"./assets/textures/skybox/top.jpg",
							"./assets/textures/skybox/bottom.jpg",
							"./assets/textures/skybox/front.jpg",
							"./assets/textures/skybox/back.jpg"
						};
					cubemapTexture = loadCubemap(faces);
				}
		}
*/
		break;

	// Tecla Pan?
	case GLFW_KEY_G:
		OnVistaPan();
/*
		if ((projeccio != ORTO) && (projeccio != CAP)) pan = !pan;
		// Desactivació de Transformacions Geomètriques via mouse i navega si pan activat
		if (pan) {	mobil = true;		zzoom = true;
					transX = false;		transY = false;	transZ = false;
				}
*/
		break;

// ----------- POP UP Projecció
	// Tecla Axonomètrica
	case GLFW_KEY_A:
		OnProjeccioAxonometrica();
		oProjeccio = shortCut_Projeccio();	// Actualitzar opció de menú PROJECCIO
/*
		if (projeccio != AXONOM) {
			projeccio = AXONOM;
			mobil = true;			zzoom = true;
		}
*/
		break;

	// Tecla Ortogràfica
	case GLFW_KEY_O:
		OnProjeccioOrtografica();
		oProjeccio = shortCut_Projeccio();	// Actualitzar opció de menú PROJECCIO
/*
		if (projeccio != ORTO) {
			projeccio = ORTO;
			mobil = false;			zzoom = false;
			}
*/
		break;

	// Tecla Perspectiva
	case GLFW_KEY_P:
		OnProjeccioPerspectiva();
		oProjeccio = shortCut_Projeccio();	// Actualitzar opció de menú PROJECCIO
/*
		if (projeccio != PERSPECT) {
			projeccio = PERSPECT;
			mobil = true;			zzoom = true;
			}
*/
		break;

// ----------- POP UP Objecte
	// Tecla CAP
	case GLFW_KEY_B:
		if (objecte != CAP) OnObjecteCap();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		if (objecte != CAP) {
			objecte = CAP;
			netejaVAOList();							// Neteja Llista VAO.
			}
*/
		break;

	// Tecla Cub
	case GLFW_KEY_C:
		if (objecte != CUB)	OnObjecteCub();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		objecte = CUB;
		//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

		//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)
		netejaVAOList();											// Neteja Llista VAO.

		// Posar color objecte (col_obj) al vector de colors del VAO.
		SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);
		Set_VAOList(GLUT_CUBE, loadglutSolidCube_EBO(1.0));		// Genera EBO de cub mida 1 i el guarda a la posició GLUT_CUBE.
*/
		break;

	// Tecla Cub RGB
	case GLFW_KEY_D:
		if (objecte != CUB_RGB)	OnObjecteCubRGB();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		objecte = CUB_RGB;
		//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

		//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)
		netejaVAOList();						// Neteja Llista VAO.

		Set_VAOList(GLUT_CUBE_RGB, loadglutSolidCubeRGB_EBO(1.0));	// Genera EBO de cub mida 1 i el guarda a la posició GLUT_CUBE_RGB.
*/
		break;

	// Tecla Esfera
	case GLFW_KEY_E:
		if (objecte != ESFERA)	OnObjecteEsfera();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		objecte = ESFERA;
		//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

		//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)
		netejaVAOList();						// Neteja Llista VAO.

		// Posar color objecte (col_obj) al vector de colors del VAO.
		SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

		Set_VAOList(GLU_SPHERE, loadgluSphere_EBO(1.0, 30, 30));
*/
		break;

	// Tecla Tetera
	case GLFW_KEY_T:
		if (objecte != TETERA)	OnObjecteTetera();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		objecte = TETERA;
		//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

		//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)
		netejaVAOList();						// Neteja Llista VAO.

		// Posar color objecte (col_obj) al vector de colors del VAO.
		SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

		//if (Get_VAOId(GLUT_TEAPOT) != 0) deleteVAOList(GLUT_TEAPOT);
		Set_VAOList(GLUT_TEAPOT, loadglutSolidTeapot_VAO()); //Genera VAO tetera mida 1 i el guarda a la posició GLUT_TEAPOT.
*/
		break;

	// Tecla Arc
	case GLFW_KEY_R:
		if (objecte != ARC)	OnObjecteArc();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		objecte = ARC;
		//  Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)
		//	Canviar l'escala per a centrar la vista (Ortogràfica)

		color_Mar.r = 0.5;	color_Mar.g = 0.4; color_Mar.b = 0.9; color_Mar.a = 1.0;
		//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

		//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)

		// Càrrega dels VAO's per a construir objecte ARC
		netejaVAOList();						// Neteja Llista VAO.

		// Posar color objecte (col_obj) al vector de colors del VAO.
		SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

		//if (Get_VAOId(GLUT_CUBE) != 0) deleteVAOList(GLUT_CUBE);
		Set_VAOList(GLUT_CUBE, loadglutSolidCube_EBO(1.0));		// Càrrega Cub de costat 1 com a EBO a la posició GLUT_CUBE.

		//if (Get_VAOId(GLU_SPHERE) != 0) deleteVAOList(GLU_SPHERE);
		Set_VAOList(GLU_SPHERE, loadgluSphere_EBO(0.5, 20, 20));	// Càrrega Esfera a la posició GLU_SPHERE.

		//if (Get_VAOId(GLUT_TEAPOT) != 0) deleteVAOList(GLUT_TEAPOT);
		Set_VAOList(GLUT_TEAPOT, loadglutSolidTeapot_VAO());		// Carrega Tetera a la posició GLUT_TEAPOT.

		//if (Get_VAOId(MAR_FRACTAL_VAO) != 0) deleteVAOList(MAR_FRACTAL_VAO);
		Set_VAOList(MAR_FRACTAL_VAO, loadSea_VAO(color_Mar));		// Carrega Mar a la posició MAR_FRACTAL_VAO.
*/
		break;

	// Tecla Tie (Star Wars)
	case GLFW_KEY_I:
		if (objecte != TIE)	OnObjecteTie();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		objecte = TIE;		textura = true;
			//	---- Entorn VGI: ATENCIÓ!!. Canviar l'escala per a centrar la vista (Ortogràfica)

			//  ---- Entorn VGI: ATENCIÓ!!. Modificar R per centrar la Vista a la mida de l'objecte (Perspectiva)

		// Càrrega dels VAO's per a construir objecte TIE
		netejaVAOList();						// Neteja Llista VAO.

		// Posar color objecte (col_obj) al vector de colors del VAO.
		SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

		//if (Get_VAOId(GLU_CYLINDER) != 0) deleteVAOList(GLU_CYLINDER);
		Set_VAOList(GLUT_CYLINDER, loadgluCylinder_EBO(5.0f, 5.0f, 0.5f, 6, 1));// Càrrega cilindre com a VAO.

		//if (Get_VAOId(GLU_DISK) != 0)deleteVAOList(GLU_DISK);
		Set_VAOList(GLU_DISK, loadgluDisk_EBO(0.0f, 5.0f, 6, 1));	// Càrrega disc com a VAO

		//if (Get_VAOId(GLU_SPHERE) != 0)deleteVAOList(GLU_SPHERE);
		Set_VAOList(GLU_SPHERE, loadgluSphere_EBO(10.0f, 80, 80));	// Càrrega disc com a VAO

		//if (Get_VAOId(GLUT_USER1) != 0)deleteVAOList(GLUT_USER1);
		Set_VAOList(GLUT_USER1, loadgluCylinder_EBO(5.0f, 5.0f, 2.0f, 6, 1)); // Càrrega cilindre com a VAO

		//if (Get_VAOId(GLUT_CUBE) != 0)deleteVAOList(GLUT_CUBE);
		Set_VAOList(GLUT_CUBE, loadglutSolidCube_EBO(1.0));			// Càrrega cub com a EBO

		//if (Get_VAOId(GLUT_TORUS) != 0)deleteVAOList(GLUT_TORUS);
		Set_VAOList(GLUT_TORUS, loadglutSolidTorus_EBO(1.0, 5.0, 20, 20));

		//if (Get_VAOId(GLUT_USER2) != 0)deleteVAOList(GLUT_USER2);
		Set_VAOList(GLUT_USER2, loadgluCylinder_EBO(1.0f, 0.5f, 5.0f, 60, 1)); // Càrrega cilindre com a VAO

		//if (Get_VAOId(GLUT_USER3) != 0)deleteVAOList(GLUT_USER3);
		Set_VAOList(GLUT_USER3, loadgluCylinder_EBO(0.35f, 0.35f, 5.0f, 80, 1)); // Càrrega cilindre com a VAO

		//if (Get_VAOId(GLUT_USER4) != 0)deleteVAOList(GLUT_USER4);
		Set_VAOList(GLUT_USER4, loadgluCylinder_EBO(4.0f, 2.0f, 10.25f, 40, 1)); // Càrrega cilindre com a VAO

		//if (Get_VAOId(GLUT_USER5) != 0) deleteVAOList(GLUT_USER5);
		Set_VAOList(GLUT_USER5, loadgluCylinder_EBO(1.5f, 4.5f, 2.0f, 8, 1)); // Càrrega cilindre com a VAO

		//if (Get_VAOId(GLUT_USER6) != 0) deleteVAOList(GLUT_USER6);
		Set_VAOList(GLUT_USER6, loadgluDisk_EBO(0.0f, 1.5f, 8, 1)); // Càrrega disk com a VAO
*/
		break;

	// Tecla Corbes Bezier
	case GLFW_KEY_F5:
		if (objecte != C_BEZIER)	OnObjecteCorbaBezier();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		nomFitxer = NULL;
		// Entorn VGI: Obrir diàleg de lectura de fitxer (fitxers (*.MNT)
		result = NFD_OpenDialog(NULL, NULL, &nomFitxer);

		if (result == NFD_OKAY) {
			puts("Bezier File Success!");
			puts(nomFitxer);

			nom = "";			objecte = C_BEZIER;
			npts_T = llegir_ptsC(nomFitxer);
			free(nomFitxer);

			// Càrrega dels VAO's per a construir la corba Bezier
			netejaVAOList();						// Neteja Llista VAO.

			// Posar color objecte (col_obj) al vector de colors del VAO.
			SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

			// Definir Esfera EBO per a indicar punts de control de la corba
			Set_VAOList(GLU_SPHERE, loadgluSphere_EBO(5.0, 20, 20));	// Genera esfera i la guarda a la posició GLUT_CUBE.

			// Definir Corba Bezier com a VAO
				//Set_VAOList(CRV_BEZIER, load_Bezier_Curve_VAO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
			Set_VAOList(CRV_BEZIER, load_Bezier_Curve_EBO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
			}
*/
		break;

	// Tecla Corbes B-Spline
	case GLFW_KEY_F6:
		if (objecte != C_BSPLINE)	OnObjecteCorbaBSpline();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		nomFitxer = NULL;
		// Entorn VGI: Obrir diàleg de lectura de fitxer (fitxers (*.MNT)
		result = NFD_OpenDialog(NULL, NULL, &nomFitxer);

		if (result == NFD_OKAY) {
			puts("B-Spline File Success!");
			puts(nomFitxer);

			nom = "";			objecte = C_BSPLINE;
			npts_T = llegir_ptsC(nomFitxer);
			free(nomFitxer);

			// Càrrega dels VAO's per a construir la corba BSpline
			netejaVAOList();						// Neteja Llista VAO.

			// Posar color objecte (col_obj) al vector de colors del VAO.
			SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

			// Definir Esfera EBO per a indicar punts de control de la corba
			Set_VAOList(GLU_SPHERE, loadgluSphere_EBO(5.0, 20, 20));	// Guarda (vaoId, vboId, nVertexs) a la posició GLUT_CUBE.

			// Definr Corba BSpline com a VAO
				//Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
			Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
			}
*/
		break;

	// Tecla Corbes Lemniscata
	case GLFW_KEY_F7:
		if (objecte != C_LEMNISCATA)	OnObjecteCorbaLemniscata();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		if (objecte != C_LEMNISCATA) {
			objecte = C_LEMNISCATA;
			// Càrrega dels VAO's per a construir la corba Bezier
			netejaVAOList();						// Neteja Llista VAO.

			// Posar color objecte (col_obj) al vector de colors del VAO.
			SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

			// Definr Corba Lemniscata 3D com a VAO
				//Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_VAO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.
			Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_EBO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.
			}
*/
		break;

	// Tecla Corbes Hermitte
	case GLFW_KEY_F8:
		if (objecte != C_HERMITTE)	OnObjecteCorbaHermitte();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
		break;

	// Tecla Corbes Catmull-Rom
	case GLFW_KEY_F9:
		if (objecte != C_CATMULL_ROM)	OnObjecteCorbaCatmullRom();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
		break;

	// Tecla Punts de Control?
	case GLFW_KEY_F10:
		OnObjectePuntsControl();
		break;

	// Tecla F11: alternar pantalla completa
	case GLFW_KEY_F11:
		OnFull_Screen(primary, window);
		break;

	// Tecla Triedre de Darboux?
	case GLFW_KEY_F12:
		OnCorbesTriedreDarboux();
		break;

	// Tecla Matriu Primitives
	case GLFW_KEY_H:
		if (objecte != MATRIUP)	OnObjecteMatriuPrimitives();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
		//objecte = MATRIUP;
		break;

	// Tecla Matriu Primitives VAO
	case GLFW_KEY_V:
		if (objecte != MATRIUP_VAO)	OnObjecteMatriuPrimitivesVAO();
		oObjecte = shortCut_Objecte();	// Actualitzar opció de menú OBJECTE
/*
		if (objecte != MATRIUP_VAO) {
			objecte = MATRIUP_VAO;

			// Càrrega dels VAO's per a construir objecte ARC
			netejaVAOList();						// Neteja Llista VAO.

			// Posar color objecte (col_obj) al vector de colors del VAO.
			SetColor4d(col_obj.r, col_obj.g, col_obj.b, col_obj.a);

			//if (Get_VAOId(GLUT_CUBE) != 0) deleteVAOList(GLUT_CUBE);
			Set_VAOList(GLUT_CUBE, loadglutSolidCube_EBO(1.0f));

			//if (Get_VAOId(GLUT_TORUS) != 0)deleteVAOList(GLUT_TORUS);
			Set_VAOList(GLUT_TORUS, loadglutSolidTorus_EBO(2.0, 3.0, 20, 20));

			//if (Get_VAOId(GLUT_SPHERE) != 0) deleteVAOList(GLU_SPHERE);
			Set_VAOList(GLU_SPHERE, loadgluSphere_EBO(1.0, 20, 20));
		}
*/
		break;

	default:
		break;
	}
}


// Teclat_Ctrl: Shortcuts per Pop Ups Transforma, Iluminació, llums, Shaders
void Teclat_Ctrl(int key)
{
	std::string nomVert, nomFrag;	// Nom de fitxer.

	switch (key)
	{
// ----------- POP UP TRANSFORMA
	// Tecla Transforma --> Traslació?
	case GLFW_KEY_T:
		OnTransformaTraslacio();
		break;

	// Tecla Transforma --> Rotació?
	case GLFW_KEY_R:
		OnTransformaRotacio();
		break;

	// Tecla Transforma --> Escalat?
	case GLFW_KEY_S:
		OnTransformaEscalat();
		break;

	// Tecla Escape (per a Transforma --> Origen Traslació, Transforma --> Origen Rotació i Transforma --> Origen Escalat)
	case GLFW_KEY_O:
		if (trasl)
		{	OnTransformaOrigenTraslacio();
		}
		else if (rota) {	OnTransformaOrigenRotacio();
						}
		if (escal) {	OnTransformaOrigenEscalat();
					}
		break;

	// Tecla Transforma --> Mobil Eix X? (opció booleana).
	case GLFW_KEY_X:
		OnTransformaMobilX();
		break;

	// Tecla Transforma --> Mobil Eix Y? (opció booleana).
	case GLFW_KEY_Y:
		OnTransformaMobilY();
		break;

	// Tecla Transforma --> Mobil Eix Z? (opció booleana).
	case GLFW_KEY_Z:
		OnTransformaMobilZ();
		break;

		// ----------- POP UP OCULTACIONS
	// Tecla Ocultacions --> Front faces? (opció booleana).
	case GLFW_KEY_D:
		OnOcultacionsFrontFaces();
		//front_faces = !front_faces;
		break;

	// Tecla Ocultacions --> Test Visibilitat? (back face culling)
	case GLFW_KEY_C:
		OnOcultacionsTestvis();
		//test_vis = !test_vis;
		break;

	// Tecla Ocultacions --> Z-Buffer? (opció booleana).
	case GLFW_KEY_B:
		OnOcultacionsZBuffer();
		//oculta = !oculta;
		break;

		// Tecla Ocultacions --> Back-lines? (opció booleana).
		//case GLFW_KEY_B:
		//	back_line = !back_line;
		//	break;

	// ----------- POP UP ILUMINACIÓ
	// Tecla Llum Fixe? (opció booleana).
	case GLFW_KEY_F:
		OnIluminacioLlumfixe();
		//ifixe = !ifixe;
		break;

	// Tecla Iluminació --> Punts
	case GLFW_KEY_F1:
		OnIluminacioPunts();
		// Actualitza el menú Iluminació segons el canvi.
		oIlumina = shortCut_Iluminacio();
		break;

	// Tecla Iluminació --> Filferros
	case GLFW_KEY_F2:
		OnIluminacioFilferros();
		// Actualitza el menú Iluminació segons el canvi.
		oIlumina = shortCut_Iluminacio();
		break;

	// Tecla Iluminació --> Plana
	case GLFW_KEY_F3:
		OnIluminacioPlana();
		// Actualitza el menú Iluminació segons el canvi.
		oIlumina = shortCut_Iluminacio();
		break;

	// Tecla Iluminació --> Suau
	case GLFW_KEY_F4:
		OnIluminacioSuau();
		// Actualitza el menú Iluminació segons el canvi.
		oIlumina = shortCut_Iluminacio();
		break;

	// Tecla Iluminació --> Reflexió Material --> Emissió?
	case GLFW_KEY_F6:
		OnMaterialEmissio();
		break;

	// Tecla Iluminació --> Reflexió Material -> Ambient?
	case GLFW_KEY_F7:
		OnMaterialAmbient();
		break;

	// Tecla Iluminació --> Reflexió Material -> Difusa?
	case GLFW_KEY_F8:
		OnMaterialDifusa();
		break;

	// Tecla Iluminació --> Reflexió Material -> Especular?
	case GLFW_KEY_F9:
		OnMaterialEspecular();
		break;

	// Tecla Iluminació --> Reflexió Material -> Especular?
	case GLFW_KEY_F10:
		OnMaterialReflmaterial();
		break;

	// Tecla Iluminació --> Textura?.
	case GLFW_KEY_I:
		OnIluminacioTextures();
		break;

	// Tecla Iluminació --> Fitxer Textura?
	case GLFW_KEY_J:
		OnIluminacioTexturaFitxerimatge();
		break;

	case GLFW_KEY_K:
		OnIluminacioTexturaFlagInvertY();
		break;
// ----------- POP UP Llums
	// Tecla Llums --> Llum Ambient?.
	case GLFW_KEY_A:
		llum_ambient = !llum_ambient;
		OnLlumsLlumAmbient();
		break;

	// Tecla Llums --> Llum #0? (+Z)
	case GLFW_KEY_0:
		llumGL[0].encesa = !llumGL[0].encesa;
		OnLlumsLlum0();
		break;

	// Tecla Llums --> Llum #1? (+X)
	case GLFW_KEY_1:
		llumGL[1].encesa = !llumGL[1].encesa;
		OnLlumsLlum1();
		break;

	// Tecla Llums --> Llum #2? (+Y)
	case GLFW_KEY_2:
		llumGL[2].encesa = !llumGL[2].encesa;
		OnLlumsLlum2();
		break;

	// Tecla Llums --> Llum #3? (Z=Y=X)
	case GLFW_KEY_3:
		llumGL[3].encesa = !llumGL[3].encesa;
		OnLlumsLlum3();
		break;

	// Tecla Llums --> Llum #4? (-Z)
	case GLFW_KEY_4:
		llumGL[4].encesa = !llumGL[4].encesa;
		OnLlumsLlum4();
		break;

	// Tecla Llums --> Llum #5?
	case GLFW_KEY_5:
		llumGL[5].encesa = !llumGL[5].encesa;
		OnLlumsLlum5();
		break;

		// Tecla Llums --> Llum #6?
	case GLFW_KEY_6:
		llumGL[6].encesa = !llumGL[6].encesa;
		OnLlumsLlum6();
		break;

	// Tecla Llums --> Llum #7?
	case GLFW_KEY_7:
		llumGL[7].encesa = !llumGL[7].encesa;
		OnLlumsLlum7();
		break;

	// Tecla Shaders --> Flat
	case GLFW_KEY_L:
		if (shader != FLAT_SHADER) {
			OnShaderFlat();
			oShader = shortCut_Shader();
		}
		break;

	// Tecla Shaders --> Gouraud
	case GLFW_KEY_G:
		if (shader != GOURAUD_SHADER) {
			OnShaderGouraud();
			oShader = shortCut_Shader();
		}
		break;

	// Tecla Shaders --> Phong
	case GLFW_KEY_P:
		if (shader != PHONG_SHADER) {
			OnShaderPhong();
			oShader = shortCut_Shader();
		}
		break;

	// Tecla Shaders --> Fitxers Shaders
	case GLFW_KEY_V:
		OnShaderLoadFiles();
		oShader = shortCut_Shader();
		break;

	default:
		break;
	}
}

// Teclat_ColorObjecte: Teclat pels canvis de color de l'objecte per teclat.
void Teclat_ColorObjecte(int key, int action)
{
	const double incr = 0.025f;

	if (action == GLFW_PRESS)
	{	// FRACTAL: Canvi resolució del fractal pe tecles '+' i'-'
		if (objecte == O_FRACTAL)
		{	if (key == GLFW_KEY_KP_SUBTRACT) // Caràcter '-' (ASCII 109)
			{	pas = pas * 2;
				if (pas > 64) pas = 64;
				sw_il = true;
			}
			else if (key == GLFW_KEY_KP_ADD) // Caràcter '+' (ASCII 107)
			{	pas = pas / 2;
				if (pas < 1) pas = 1;
				sw_il = true;
			}
		}
		//	else 
		if (key == GLFW_KEY_DOWN) // Caràcter VK_DOWN
			{	if (fonsR) {	col_obj.r -= incr;
								if (col_obj.r < 0.0) col_obj.r = 0.0;
							}
				if (fonsG) {	col_obj.g -= incr;
								if (col_obj.g < 0.0) col_obj.g = 0.0;
							}
				if (fonsB) {	col_obj.b -= incr;
								if (col_obj.b < 0.0) col_obj.b = 0.0;
							}
			}
		else if (key == GLFW_KEY_UP)
			{	if (fonsR) {
				col_obj.r += incr;
				if (col_obj.r > 1.0) col_obj.r = 1.0;
			}
			if (fonsG) {	col_obj.g += incr;
							if (col_obj.g > 1.0) col_obj.g = 1.0;
						}
			if (fonsB) {	col_obj.b += incr;
							if (col_obj.b > 1.0) col_obj.b = 1.0;
						}
		}
		else if (key == GLFW_KEY_SPACE)
		{
			if ((fonsR) && (fonsG) && (fonsB)) {
				fonsG = false;
				fonsB = false;
			}
			else if ((fonsR) && (fonsG)) {
				fonsG = false;
				fonsB = true;
			}
			else if ((fonsR) && (fonsB)) {
				fonsR = false;
				fonsG = true;
			}
			else if ((fonsG) && (fonsB)) fonsR = true;
			else if (fonsR) {
				fonsR = false;
				fonsG = true;
			}
			else if (fonsG) {
				fonsG = false;
				fonsB = true;
			}
			else if (fonsB) {
				fonsR = true;
				fonsG = true;
				fonsB = false;
			}
		}
		else if (key == GLFW_KEY_O || key == 'o') sw_color = !sw_color;
		else if (key == GLFW_KEY_B || key == 'b') sw_color = !sw_color;
	}
}


// Teclat_ColorFons: Teclat pels canvis del color de fons.
void Teclat_ColorFons(int key, int action)
{
	const double incr = 0.025f;

	if (action == GLFW_PRESS)
	{	// FRACTAL: Canvi resolució del fractal pe tecles '+' i'-'
		if (objecte == O_FRACTAL)
			{	if (key == GLFW_KEY_KP_SUBTRACT) // Caràcter '-' - (ASCII:109)
					{	pas = pas * 2;
						if (pas > 64) pas = 64;
						sw_il = true;
					}
				else if (key == GLFW_KEY_KP_ADD) // Caràcter '+' - (ASCII:107)
					{	pas = pas / 2;
						if (pas < 1) pas = 1;
						sw_il = true;
					}
			}
		//	else 
		if (key == GLFW_KEY_DOWN) 
			{	if (fonsR) {	c_fons.r -= incr;
								if (c_fons.r < 0.0) c_fons.r = 0.0;
							}
				if (fonsG) {	c_fons.g -= incr;
								if (c_fons.g < 0.0) c_fons.g = 0.0;
							}
				if (fonsB) {	c_fons.b -= incr;
								if (c_fons.b < 0.0) c_fons.b = 0.0;
							}
		}
		else if (key == GLFW_KEY_UP) {
			if (fonsR) 
				{	c_fons.r += incr;
					if (c_fons.r > 1.0) c_fons.r = 1.0;
				}
			if (fonsG) {	c_fons.g += incr;
							if (c_fons.g > 1.0) c_fons.g = 1.0;
						}
			if (fonsB) {	c_fons.b += incr;
							if (c_fons.b > 1.0) c_fons.b = 1.0;
						}
			}
		else if (key == GLFW_KEY_SPACE) {
			if ((fonsR) && (fonsG) && (fonsB)) {
				fonsG = false;
				fonsB = false;
			}
			else if ((fonsR) && (fonsG)) {
				fonsG = false;
				fonsB = true;
			}
			else if ((fonsR) && (fonsB)) {
				fonsR = false;
				fonsG = true;
			}
			else if ((fonsG) && (fonsB)) fonsR = true;
			else if (fonsR) {
				fonsR = false;
				fonsG = true;
			}
			else if (fonsG) {
				fonsG = false;
				fonsB = true;
			}
			else if (fonsB) {
				fonsR = true;
				fonsG = true;
				fonsB = false;
			}
		}
		else if (key == GLFW_KEY_O || key == 'o') sw_color = !sw_color;
		else if (key == GLFW_KEY_B || key == 'b') sw_color = !sw_color;
	}
}

// Teclat_Navega: Teclat pels moviments de navegació.
void Teclat_Navega(int key, int action)
{
	GLdouble vdir[3] = { 0, 0, 0 };
	double modul = 0;

	// Entorn VGI: Controls de moviment de navegació
	vdir[0] = n[0] - opvN.x;
	vdir[1] = n[1] - opvN.y;
	vdir[2] = n[2] - opvN.z;
	modul = sqrt(vdir[0] * vdir[0] + vdir[1] * vdir[1] + vdir[2] * vdir[2]);
	vdir[0] = vdir[0] / modul;
	vdir[1] = vdir[1] / modul;
	vdir[2] = vdir[2] / modul;

	if (action == GLFW_PRESS)
	{
		switch (key)
		{	// Tecla cursor amunt
		case GLFW_KEY_UP:
				if (Vis_Polar == POLARZ) 
					{  // (X,Y,Z)
						opvN.x += fact_pan * vdir[0];
						opvN.y += fact_pan * vdir[1];
						n[0] += fact_pan * vdir[0];
					n[1] += fact_pan * vdir[1];
				}
				else if (Vis_Polar == POLARY) 
				{ //(X,Y,Z) --> (Z,X,Y)
					opvN.x += fact_pan * vdir[0];
					opvN.z += fact_pan * vdir[2];
					n[0] += fact_pan * vdir[0];
					n[2] += fact_pan * vdir[2];
			}	
				else if (Vis_Polar == POLARX) 
				{	//(X,Y,Z) --> (Y,Z,X)
					opvN.y += fact_pan * vdir[1];
					opvN.z += fact_pan * vdir[2];
					n[1] += fact_pan * vdir[1];
					n[2] += fact_pan * vdir[2];
				}
			break;

		// Tecla cursor avall
		case GLFW_KEY_DOWN:
			if (Vis_Polar == POLARZ) 
			{ // (X,Y,Z)
				opvN.x -= fact_pan * vdir[0];
				opvN.y -= fact_pan * vdir[1];
				n[0] -= fact_pan * vdir[0];
				n[1] -= fact_pan * vdir[1];
			}
			else if (Vis_Polar == POLARY) 
			{ //(X,Y,Z) --> (Z,X,Y)
				opvN.x -= fact_pan * vdir[0];
				opvN.z -= fact_pan * vdir[2];
				n[0] -= fact_pan * vdir[0];
				n[2] -= fact_pan * vdir[2];
			}
			else if (Vis_Polar == POLARX) 
			{ //(X,Y,Z) --> (Y,Z,X)
				opvN.y -= fact_pan * vdir[1];
				opvN.z -= fact_pan * vdir[2];
				n[1] -= fact_pan * vdir[1];
				n[2] -= fact_pan * vdir[2];
			}
			break;

		// Tecla cursor esquerra
		case GLFW_KEY_LEFT:
			angleZ += fact_pan;
			if (Vis_Polar == POLARZ) 
			{ // (X,Y,Z)
				n[0] = vdir[0]; // n[0] - opvN.x;
				n[1] = vdir[1]; // n[1] - opvN.y;
				n[0] = n[0] * cos(angleZ * PI / 180) - n[1] * sin(angleZ * PI / 180.0);
				n[1] = n[0] * sin(angleZ * PI / 180) + n[1] * cos(angleZ * PI / 180.0);
				n[0] = n[0] + opvN.x;
				n[1] = n[1] + opvN.y;
			}
			else if (Vis_Polar == POLARY) 
			{ //(X,Y,Z) --> (Z,X,Y)
				n[2] = vdir[2]; // n[2] - opvN.z;
				n[0] = vdir[0]; // n[0] - opvN.x;
				n[2] = n[2] * cos(angleZ * PI / 180.0) - n[0] * sin(angleZ * PI / 180.0);
				n[0] = n[2] * sin(angleZ * PI / 180.0) + n[0] * cos(angleZ * PI / 180.0);
				n[2] = n[2] + opvN.z;
				n[0] = n[0] + opvN.x;
			}
			else if (Vis_Polar == POLARX) 
			{ //(X,Y,Z) --> (Y,Z,X)
				n[1] = vdir[1]; // n[1] - opvN.y;
				n[2] = vdir[2]; // n[2] - opvN.z;
				n[1] = n[1] * cos(angleZ * PI / 180.0) - n[2] * sin(angleZ * PI / 180.0);
				n[2] = n[1] * sin(angleZ * PI / 180.0) + n[2] * cos(angleZ * PI / 180.0);
				n[1] = n[1] + opvN.y;
				n[2] = n[2] + opvN.z;
			}
			break;

		// Tecla cursor dret
		case GLFW_KEY_RIGHT:
			angleZ = 360 - fact_pan;
			if (Vis_Polar == POLARZ) 
			{ // (X,Y,Z)
				n[0] = vdir[0]; // n[0] - opvN.x;
				n[1] = vdir[1]; // n[1] - opvN.y;
				n[0] = n[0] * cos(angleZ * PI / 180.0) - n[1] * sin(angleZ * PI / 180.0);
				n[1] = n[0] * sin(angleZ * PI / 180.0) + n[1] * cos(angleZ * PI / 180.0);
				n[0] = n[0] + opvN.x;
				n[1] = n[1] + opvN.y;
			}
			else if (Vis_Polar == POLARY) 
			{ //(X,Y,Z) --> (Z,X,Y)
				n[2] = vdir[2]; // n[2] - opvN.z;
				n[0] = vdir[0]; // n[0] - opvN.x;
				n[2] = n[2] * cos(angleZ * PI
					/ 180.0) - n[0] * sin(angleZ * PI / 180.0);
				n[0] = n[2] * sin(angleZ * PI / 180.0) + n[0] * cos(angleZ * PI / 180.0);
				n[2] = n[2] + opvN.z;
				n[0] = n[0] + opvN.x;
			}
			else if (Vis_Polar == POLARX) 
			{ //(X,Y,Z) --> (Y,Z,X)
				n[1] = vdir[1]; // n[1] - opvN.y;
				n[2] = vdir[2]; // n[2] - opvN.z;
				n[1] = n[1] * cos(angleZ * PI / 180.0) - n[2] * sin(angleZ * PI / 180.0);
				n[2] = n[1] * sin(angleZ * PI / 180.0) + n[2] * cos(angleZ * PI / 180.0);
				n[1] = n[1] + opvN.y;
				n[2] = n[2] + opvN.z;
			}
			break;

		// Tecla Inicio ('7')
		case GLFW_KEY_KP_7: //GLFW_KEY_HOME:
			if (Vis_Polar == POLARZ) {	opvN.z += fact_pan;
										n[2] += fact_pan;
									}
			else if (Vis_Polar == POLARY) {	opvN.y += fact_pan;
											n[1] += fact_pan;
											}
			else if (Vis_Polar == POLARX) {	opvN.x += fact_pan;
											n[0] += fact_pan;
										}
			break;

		// Tecla Fin
		case GLFW_KEY_END:
			if (Vis_Polar == POLARZ) {	opvN.z -= fact_pan;
										n[2] -= fact_pan;
									}
			else if (Vis_Polar == POLARY) {	opvN.y -= fact_pan;
											n[1] -= fact_pan;
											}
			else if (Vis_Polar == POLARX) {	opvN.x -= fact_pan;
											n[0] -= fact_pan;
										}
			break;

			// Tecla PgUp ('9)
		case GLFW_KEY_KP_9: //GLFW_KEY_PAGE_UP:
			fact_pan /= 2;
			if (fact_pan < 0.125) fact_pan = 0.125;
			break;

			// Tecla PgDown ('3')
		case GLFW_KEY_KP_3: //GLFW_KEY_PAGE_DOWN:
			fact_pan *= 2;
			if (fact_pan > 2048) fact_pan = 2048;
			break;

		default:
			if (transf)
				{	if (rota) Teclat_TransRota(key, action);
						else if (trasl) Teclat_TransTraslada(key, action);
								else if (escal) Teclat_TransEscala(key, action);
				}
			/*else if (camera == CAM_NAVEGA) Teclat_Navega(key, action);
					else if (!sw_color) Teclat_ColorFons(key, action);
								else Teclat_ColorObjecte(key, action);*/
			break;
		}
	}
}


// Teclat_Pan: Teclat pels moviments de Pan.
void Teclat_Pan(int key, int action)
{
	if (action == GLFW_PRESS)
	{	switch (key)
		{
		// Tecla cursor amunt
		case GLFW_KEY_UP:
			tr_cpv.y -= fact_pan;
			if (tr_cpv.y < -100000) tr_cpv.y = 100000;
			break;

		// Tecla cursor avall
		case GLFW_KEY_DOWN:
			tr_cpv.y += fact_pan;
			if (tr_cpv.y > 100000) tr_cpv.y = 100000;
			break;

		// Tecla cursor esquerra
		case GLFW_KEY_LEFT:
			tr_cpv.x += fact_pan;
			if (tr_cpv.x > 100000.0) tr_cpv.x = 100000.0;
			break;

		// Tecla cursor dret
		case GLFW_KEY_RIGHT:
			tr_cpv.x -= fact_pan;
			if (tr_cpv.x < -100000.0) tr_cpv.x = 100000;
			break;

		// Tecla PgUp ('9')
		case GLFW_KEY_KP_9: //GLFW_KEY_PAGE_UP.0
			fact_pan /= 2.0;
			if (fact_pan < 0.125) fact_pan = 0.125;
			break;

		// Tecla PgDown ('3')
		case GLFW_KEY_KP_3: //GLFW_KEY_PAGE_DOWN:
			fact_pan *= 2.0;
			if (fact_pan > 2048.0) fact_pan = 2048.0;
			break;

		// Tecla Insert: Fixar el desplaçament de pantalla (pan)
		case GLFW_KEY_INSERT:
			// Acumular desplaçaments de pan (tr_cpv) en variables fixes (tr_cpvF).
			tr_cpvF.x += tr_cpv.x;		tr_cpv.x = 0.0;
			if (tr_cpvF.x > 100000.0)	tr_cpvF.x = 100000.0;
			tr_cpvF.y += tr_cpv.y;		tr_cpv.y = 0.0;
			if (tr_cpvF.y > 100000.0)	tr_cpvF.y = 100000.0;
			tr_cpvF.z += tr_cpv.z;		tr_cpv.z = 0.0;
			if (tr_cpvF.z > 100000.0)	tr_cpvF.z = 100000.0;
			break;

		// Tecla Delete: Inicialitzar el desplaçament de pantalla (pan)
		case GLFW_KEY_DELETE:
			// Inicialitzar els valors de pan tant de la variable tr_cpv com de la tr_cpvF.
			tr_cpv.x = 0.0;			tr_cpv.y = 0.0;			tr_cpv.z = 0.0;
			tr_cpvF.x = 0.0;		tr_cpvF.y = 0.0;		tr_cpvF.z = 0.0;
			break;

		default:
			if (transf)
				{	if (rota) Teclat_TransRota(key, action);
						else if (trasl) Teclat_TransTraslada(key, action);
								else if (escal) Teclat_TransEscala(key, action);
				}																				
			else if (camera == CAM_NAVEGA) Teclat_Navega(key, action);
			else if (!sw_color) Teclat_ColorFons(key, action);
			else Teclat_ColorObjecte(key, action);
			break;
		}
	}
}

// Teclat_TransEscala: Teclat pels canvis del valor d'escala per X,Y,Z.
void Teclat_TransEscala(int key, int action)
{
	if (action == GLFW_PRESS)
	{	switch (key)
		{
// Modificar vector d'Escalatge per teclat (actiu amb Escalat únicament)
		// Tecla '+' (augmentar tot l'escalat)
		case GLFW_KEY_KP_ADD:
			TG.VScal.x = TG.VScal.x * 2.0;
			if (TG.VScal.x > 8192.0) TG.VScal.x = 8192.0;
			TG.VScal.y = TG.VScal.y * 2.0;
			if (TG.VScal.y > 8192.0) TG.VScal.y = 8192.0;
			TG.VScal.z = TG.VScal.z * 2.0;
			if (TG.VScal.z > 8192.0) TG.VScal.z = 8192.0;
			break;

		// Tecla '-' (disminuir tot l'escalat)
		case GLFW_KEY_KP_SUBTRACT:
			TG.VScal.x = TG.VScal.x / 2.0;
			if (TG.VScal.x < 0.125) TG.VScal.x = 0.125;
			TG.VScal.y = TG.VScal.y / 2.0;
			if (TG.VScal.y < 0.125) TG.VScal.y = 0.125;
			TG.VScal.z = TG.VScal.z / 2.0;
			if (TG.VScal.z < 0.125) TG.VScal.z = 0.125;
			break;

		// Tecla cursor amunt ('8')
		case GLFW_KEY_UP:
			TG.VScal.x = TG.VScal.x * 2.0;
			if (TG.VScal.x > 8192.0) TG.VScal.x = 8192.0;
			break;

		// Tecla cursor avall ('2')
		case GLFW_KEY_DOWN:
			TG.VScal.x = TG.VScal.x / 2.0;
			if (TG.VScal.x < 0.25) TG.VScal.x = 0.25;
			break;

		// Tecla cursor esquerra ('4')
		case GLFW_KEY_LEFT:
			TG.VScal.y = TG.VScal.y / 2.0;
			if (TG.VScal.y < 0.125) TG.VScal.y = 0.125;
			break;

		// Tecla cursor dret ('6')
		case GLFW_KEY_RIGHT:
			TG.VScal.y = TG.VScal.y * 2.0;
			if (TG.VScal.y > 8192.0) TG.VScal.y = 8192.0;
			break;

		// Tecla HOME ('7')
		case GLFW_KEY_KP_7: //GLFW_KEY_HOME:
			TG.VScal.z = TG.VScal.z * 2.0;
			if (TG.VScal.z > 8192.0) TG.VScal.z = 8192.0;
			break;

		// Tecla END ('1')
		case GLFW_KEY_KP_1: //GLFW_KEY_END:
			TG.VScal.z = TG.VScal.z / 2.0;
			if (TG.VScal.z < 0.125) TG.VScal.z = 0.125;
			break;

		// Tecla INSERT
		case GLFW_KEY_INSERT:
			// Acumular transformacions Geomètriques (variable TG) i de pan en variables fixes (variable TGF)
			TGF.VScal.x *= TG.VScal.x;	TGF.VScal.y *= TG.VScal.y; TGF.VScal.z *= TG.VScal.z;
			if (TGF.VScal.x > 8192.0) TGF.VScal.x = 8192.0;		if (TG.VScal.x < 0.125) TG.VScal.x = 0.125;
			if (TGF.VScal.y > 8192.0) TGF.VScal.y = 8192.0;		if (TG.VScal.y < 0.125) TG.VScal.y = 0.125;
			if (TGF.VScal.z > 8192.0) TGF.VScal.z = 8192.0;		if (TG.VScal.z < 0.125) TG.VScal.z = 0.125;
			TG.VScal.x = 1.0;				TG.VScal.y = 1.0;		TG.VScal.z = 1.0;
			TGF.VRota.x += TG.VRota.x;		TGF.VRota.y += TG.VRota.y;	TGF.VRota.z += TG.VRota.z;
			if (TGF.VRota.x >= 360.0) TGF.VRota.x -= 360.0; 		if (TGF.VRota.x < 0) TGF.VRota.x += 360.0;
			if (TGF.VRota.y >= 360.0) TGF.VRota.y -= 360.0;		if (TGF.VRota.y < 0.0) TGF.VRota.y += 360.0;
			if (TGF.VRota.z >= 360.0) TGF.VRota.z -= 360.0;		if (TGF.VRota.z < 0.0) TGF.VRota.z += 360.0;
			TG.VRota.x = 0.0;				TG.VRota.y = 0.0;			TG.VRota.z = 0.0;
			TGF.VTras.x += TG.VTras.x;		TGF.VTras.y += TG.VTras.y;	TGF.VTras.z += TG.VTras.z;
			if (TGF.VTras.x < -100000.0) TGF.VTras.x = 100000.0;	if (TGF.VTras.x > 10000.0) TGF.VTras.x = 100000.0;
			if (TGF.VTras.y < -100000.0) TGF.VTras.y = 100000.0;	if (TGF.VTras.y > 10000.0) TGF.VTras.y = 100000.0;
			if (TGF.VTras.z < -100000.0) TGF.VTras.z = 100000.0;	if (TGF.VTras.z > 10000.0) TGF.VTras.z = 100000.0;
			TG.VTras.x = 0.0;				TG.VTras.y = 0.0;		TG.VTras.z = 0.0;
			break;

		// Tecla Delete: Esborrar les Transformacions Geomètriques Calculades
		case GLFW_KEY_DELETE:
			// Inicialitzar els valors de transformacions Geomètriques i de pan en variables fixes.
			TGF.VScal.x = 1.0;		TGF.VScal.y = 1.0;;		TGF.VScal.z = 1.0;
			TG.VScal.x = 1.0;		TG.VScal.y = 1.0;		TG.VScal.z = 1.0;
			TGF.VRota.x = 0.0;		TGF.VRota.y = 0.0;		TGF.VRota.z = 0.0;
			TG.VRota.x = 0.0;		TG.VRota.y = 0.0;		TG.VRota.z = 0.0;
			TGF.VTras.x = 0.0;		TGF.VTras.y = 0.0;		TGF.VTras.z = 0.0;
			TG.VTras.x = 0.0;		TG.VTras.y = 0.0;		TG.VTras.z = 0.0;
			break;

		default:
			if (pan) Teclat_Pan(key, action);
				else if (camera == CAM_NAVEGA) Teclat_Navega(key, action);
						else if (!sw_color) Teclat_ColorFons(key, action);
								else Teclat_ColorObjecte(key, action);
			break;
		}
	}
}

// Teclat_TransRota: Teclat pels canvis del valor del vector de l'angle de rotació per X,Y,Z.
void Teclat_TransRota(int key, int action)
{
	if (action == GLFW_PRESS)
	{	switch (key)
		{
		// Tecla cursor amunt ('8')
		case GLFW_KEY_UP:
			TG.VRota.x += fact_Rota;
			if (TG.VRota.x >= 360.0) TG.VRota.x -= 360.0;
			// Entorn VGI. QUATERNIONS: Definir nou angle rotació X com a quaternió (angleQ) i multiplicar-lo per qT --> qT=angleQ*qT. 
			if (rotaQ) {
/*							angle += nRepCnt * fact_Rota;
							while (angle >= 360.0f) angle -= 360.0f;
							EixAngleToQuat(1.0, 0.0, 0.0, angle, angleQ);
							QuatMul(angleQ, qT, qT);
							QuatToMatrix(qT, QMatrix);
*/
			}
			break;

		// Tecla cursor avall ('2')
		case GLFW_KEY_DOWN:
			TG.VRota.x -= fact_Rota;
			if (TG.VRota.x < 1.0) TG.VRota.x += 360.0;
			// Entorn VGI. QUATERNIONS: Definir nou angle rotació X com a quaternió (angleQ) i multiplicar-lo per qT --> qT=angleQ*qT.
			if (rotaQ) { 	// TO DO. PENDENT DE FER.

						}
			break;

		// Tecla cursor esquerra ('4')
		case GLFW_KEY_LEFT:
			TG.VRota.y -= fact_Rota;
			if (TG.VRota.y < 1.0) TG.VRota.y += 360.0;
			// Entorn VGI. QUATERNIONS: Definir nou angle rotació X com a quaternió (angleQ) i multiplicar-lo per qT --> qT=angleQ*qT.
			if (rotaQ) {	// TO DO. PENDENT DE FER.

						}
			break;

		// Tecla cursor dret ('6')
		case GLFW_KEY_RIGHT:
			TG.VRota.y += fact_Rota;
			if (TG.VRota.y >= 360.0) TG.VRota.y -= 360.0;
			// Entorn VGI. QUATERNIONS: Definir nou angle rotació X com a quaternió (angleQ) i multiplicar-lo per qT --> qT=angleQ*qT.
			if (rotaQ) {	// TO DO. PENDENT DE FER.

						}
			break;

		// Tecla HOME ('7')
		case GLFW_KEY_KP_7: //GLFW_KEY_HOME:
			TG.VRota.z += fact_Rota;
			if (TG.VRota.z >= 360.0) TG.VRota.z -= 360.0;
			// Entorn VGI. QUATERNIONS: Definir nou angle rotació X com a quaternió (angleQ) i multiplicar-lo per qT --> qT=angleQ*qT.
			if (rotaQ) {	// TO DO. PENDENT DE FER.

					}
			break;

		// Tecla END ('1')
		case GLFW_KEY_KP_1: //GLFW_KEY_END:
			TG.VRota.z -= fact_Rota;
			if (TG.VRota.z < 1.0) TG.VRota.z += 360.0;
			// Entorn VGI. QUATERNIONS: Definir nou angle rotació X com a quaternió (angleQ) i multiplicar-lo per qT --> qT=angleQ*qT.
			if (rotaQ) {	// TO DO. PENDENT DE FER.

						}
			break;

		// Tecla PgUp ('9')
		case GLFW_KEY_KP_9: //GLFW_KEY_PAGE_UP:
			fact_Rota /= 2;
			if (fact_Rota < 1.0) fact_Rota = 1.0;
			break;

		// Tecla PgDown ('3')
		case GLFW_KEY_KP_3:// GLFW_KEY_PAGE_DOWN:
			fact_Rota *= 2.0;
			if (fact_Rota > 90) fact_Rota = 90;
			break;

		// Entorn VGI. QUATERNIONS: Tecla F5 (Activar animació cossos rígids per QUATERNIONS) -> PENDENT DE FER
		case GLFW_KEY_F5:
			if (rotaQ) { // TO DO. PENDENT DE FER

					}
			break;

		// Entorn VGI. QUATERNIONS: Tecla Pausa (Activar/parar animació cossos rígids per QUATERNIONS) -> PENDENT DE FER
		case GLFW_KEY_PAUSE:
			if (rotaQ) {	// TO DO. PENDENT DE FER

						}
			break;

		// Entorn VGI. QUATERNIONS: Tecla End (Para animació i inicialitzar vector Keyframes - PENDENT DE FER
		case GLFW_KEY_END:
			if (rotaQ) {	// TO DO. PENDENT DE FER

						}
			break;

		// Entorn VGI. QUATERNIONS:  Tecla F3 (Accelerar animació cossos rígids per QUATERNIONS) - PENDENT DE FER (ADDICIONAL)
		case GLFW_KEY_F3:
			if (rotaQ) {	// TO DO. PENDENT DE FER (ADDICIONAL)

				}
			break;

		// Entorn VGI. QUATERNIONS: Tecla F4 (Desaccelerar animació cossos rígids per QUATERNIONS) - PENDENT DE FER (ADDICIONAL)
		case GLFW_KEY_F4:
			if (rotaQ) {	// TO Do. PENDENT DE FER (ADDICIONAL)

					}
			break;



		// Tecla Insert: Acumular transformacions Geomètriques (variable TG) i de pan en variables fixes (variable TGF)
		case GLFW_KEY_INSERT:
			// Entorn VGI. QUATERNIONS: Fixar els Quaternions en els keyframes (animació cossos rígids per QUATERNIONS)
			if (rotaQ) { // TO DO. PENDENT DE FER

			}
			else {
				TGF.VScal.x *= TG.VScal.x;		TGF.VScal.y *= TG.VScal.y; TGF.VScal.z *= TG.VScal.z;
				if (TGF.VScal.x > 8192.0) TGF.VScal.x = 8192.0;			if (TG.VScal.x < 0.125) TG.VScal.x = 0.125;
				if (TGF.VScal.y > 8192.0) TGF.VScal.y = 8192.0;			if (TG.VScal.y < 0.125) TG.VScal.y = 0.125;
				if (TGF.VScal.z > 8192.0) TGF.VScal.z = 8192.0;			if (TG.VScal.z < 0.125) TG.VScal.z = 0.125;
				TG.VScal.x = 1.0;				TG.VScal.y = 1.0;			TG.VScal.z = 1.0;
				TGF.VRota.x += TG.VRota.x;		TGF.VRota.y += TG.VRota.y;	TGF.VRota.z += TG.VRota.z;
				if (TGF.VRota.x >= 360.0) TGF.VRota.x -= 360.0; 		if (TGF.VRota.x < 0.0) TGF.VRota.x += 360.0;
				if (TGF.VRota.y >= 360.0) TGF.VRota.y -= 360.0;			if (TGF.VRota.y < 0.0) TGF.VRota.y += 360.0;
				if (TGF.VRota.z >= 360.0) TGF.VRota.z -= 360.0;			if (TGF.VRota.z < 0.0) TGF.VRota.z += 360.0;
				TG.VRota.x = 0.0;				TG.VRota.y = 0.0;			TG.VRota.z = 0.0;
				TGF.VTras.x += TG.VTras.x;		TGF.VTras.y += TG.VTras.y;	TGF.VTras.z += TG.VTras.z;
				if (TGF.VTras.x < -100000.0) TGF.VTras.x = 100000.0;	if (TGF.VTras.x > 10000.0) TGF.VTras.x = 100000.0;
				if (TGF.VTras.y < -100000.0) TGF.VTras.y = 100000.0;	if (TGF.VTras.y > 10000.0) TGF.VTras.y = 100000.0;
				if (TGF.VTras.z < -100000.0) TGF.VTras.z = 100000.0;	if (TGF.VTras.z > 10000.0) TGF.VTras.z = 100000.0;
				TG.VTras.x = 0.0;				TG.VTras.y = 0.0;			TG.VTras.z = 0.0;
				}
			break;

		// Tecla Delete: Esborrar les Transformacions Geomètriques Calculades
		case GLFW_KEY_DELETE:
			// Inicialitzar els valors de transformacions Geomètriques i de pan en variables fixes.
			TGF.VScal.x = 1.0;		TGF.VScal.y = 1.0;;	TGF.VScal.z = 1.0;
			TG.VScal.x = 1.0;		TG.VScal.y = 1.0;		TG.VScal.z = 1.0;
			TGF.VRota.x = 0.0;		TGF.VRota.y = 0.0;	TGF.VRota.z = 0.0;
			TG.VRota.x = 0.0;		TG.VRota.y = 0.0;		TG.VRota.z = 0.0;
			TGF.VTras.x = 0.0;		TGF.VTras.y = 0.0;	TGF.VTras.z = 0.0;
			TG.VTras.x = 0.0;		TG.VTras.y = 0.0;		TG.VTras.z = 0.0;
			break;

		// Modificar vector d'Escalatge per teclat
		// Tecla '+' (augmentar escalat)
		case GLFW_KEY_KP_ADD:
			if (escal) {
				TG.VScal.x = TG.VScal.x * 2.0;
				if (TG.VScal.x > 8192.0) TG.VScal.x = 8192.0;
				TG.VScal.y = TG.VScal.y * 2.0;
				if (TG.VScal.y > 8192.0) TG.VScal.y = 8192.0;
				TG.VScal.z = TG.VScal.z * 2.0;
				if (TG.VScal.z > 8192.0) TG.VScal.z = 8192.0;
				}
			break;

		// Tecla '-' (disminuir escalat)
		case GLFW_KEY_KP_SUBTRACT:
			if (escal) {
				TG.VScal.x = TG.VScal.x / 2.0;
				if (TG.VScal.x < 0.125) TG.VScal.x = 0.125;
				TG.VScal.y = TG.VScal.y / 2.0;
				if (TG.VScal.y < 0.125) TG.VScal.y = 0.125;
				TG.VScal.z = TG.VScal.z / 2.0;
				if (TG.VScal.z < 0.125) TG.VScal.z = 0.125;
				}
			break;

		// Tecla Espaiador
		case GLFW_KEY_SPACE:
			rota = !rota;
			trasl = !trasl;
			break;

		default:
			if (pan) Teclat_Pan(key, action);
				else if (camera == CAM_NAVEGA) Teclat_Navega(key, action);
						else if (!sw_color) Teclat_ColorFons(key, action);
								else Teclat_ColorObjecte(key, action);
			break;
		}
	}
}


// Teclat_TransTraslada: Teclat pels canvis del valor de traslació per X,Y,Z.
void Teclat_TransTraslada(int key, int action)
{
	GLdouble vdir[3] = { 0, 0, 0 };
	double modul = 0;

	if (action == GLFW_PRESS)
	{	switch (key)
		{
		// Tecla cursor amunt ('8') - (ASCII:104)
		case GLFW_KEY_UP:
			TG.VTras.x -= fact_Tras;
			if (TG.VTras.x < -100000.0) TG.VTras.x = 100000.0;
			break;

		// Tecla cursor avall ('2') - (ASCII:98)
		case GLFW_KEY_DOWN:
			TG.VTras.x += fact_Tras;
			if (TG.VTras.x > 10000.0) TG.VTras.x = 100000.0;
			break;

		// Tecla cursor esquerra ('4') - (ASCII:100)
		case GLFW_KEY_LEFT:
			TG.VTras.y -= fact_Tras;
			if (TG.VTras.y < -100000.0) TG.VTras.y = -100000.0;
			break;

		// Tecla cursor dret ('6') - (ASCII:102)
		case GLFW_KEY_RIGHT:
			TG.VTras.y += fact_Tras;
			if (TG.VTras.y > 100000.0) TG.VTras.y = 100000.0;
			break;

		// Tecla HOME ('7') - (ASCII:103)
		case GLFW_KEY_KP_7: //GLFW_KEY_HOME:
			TG.VTras.z += fact_Tras;
			if (TG.VTras.z > 100000.0) TG.VTras.z = 100000.0;
			break;

		// Tecla END ('1') - (ASCII:10#)
		case GLFW_KEY_KP_1: //GLFW_KEY_END:
			TG.VTras.z -= fact_Tras;
			if (TG.VTras.z < -100000.0) TG.VTras.z = -100000.0;
			break;

		// Tecla PgUp ('9') - (ASCII:105)
		case GLFW_KEY_KP_9: //GLFW_KEY_PAGE_UP:
			fact_Tras /= 2.0;
			if (fact_Tras < 1.0) fact_Tras = 1.0;
			break;

		// Tecla PgDown ('3') - (ASCII:99)
		case GLFW_KEY_KP_3: //GLFW_KEY_PAGE_DOWN:
			fact_Tras *= 2;
			if (fact_Tras > 100000.0) fact_Tras = 100000.0;
			break;

		// Modificar vector d'Escalatge per teclat (actiu amb Traslació)
		// Tecla '+' (augmentar escalat)
		case GLFW_KEY_KP_ADD:
			if (escal) {
				TG.VScal.x = TG.VScal.x * 2.0;
				if (TG.VScal.x > 8192.0) TG.VScal.x = 8192.0;
				TG.VScal.y = TG.VScal.y * 2.0;
				if (TG.VScal.y > 8192.0) TG.VScal.y = 8192.0;
				TG.VScal.z = TG.VScal.z * 2.0;
				if (TG.VScal.z > 8192.0) TG.VScal.z = 8192.0;
			}
			break;

		// Tecla '-' (disminuir escalat) . (ASCII:109)
		case GLFW_KEY_KP_SUBTRACT:
			if (escal) {
				TG.VScal.x = TG.VScal.x / 2.0;
				if (TG.VScal.x < 0.125) TG.VScal.x = 0.125;
				TG.VScal.y = TG.VScal.y / 2.0;
				if (TG.VScal.y < 0.125) TG.VScal.y = 0.125;
				TG.VScal.z = TG.VScal.z / 2.0;
				if (TG.VScal.z < 0.125) TG.VScal.z = 0.125;
			}
			break;

		// Tecla INSERT
		case GLFW_KEY_INSERT:
			// Acumular transformacions Geomètriques (variable TG) i de pan en variables fixes (variable TGF)
			TGF.VScal.x *= TG.VScal.x;		TGF.VScal.y *= TG.VScal.y; TGF.VScal.z *= TG.VScal.z;
			if (TGF.VScal.x > 8192.0) TGF.VScal.x = 8192.0;			if (TG.VScal.x < 0.125) TG.VScal.x = 0.125;
			if (TGF.VScal.y > 8192.0) TGF.VScal.y = 8192.0;			if (TG.VScal.y < 0.125) TG.VScal.y = 0.125;
			if (TGF.VScal.z > 8192.0) TGF.VScal.z = 8192.0;			if (TG.VScal.z < 0.125) TG.VScal.z = 0.125;
			TG.VScal.x = 1.0;				TG.VScal.y = 1.0;			TG.VScal.z = 1.0;
			TGF.VRota.x += TG.VRota.x;		TGF.VRota.y += TG.VRota.y;	TGF.VRota.z += TG.VRota.z;
			if (TGF.VRota.x >= 360.0) TGF.VRota.x -= 360.0; 		if (TGF.VRota.x < 0) TGF.VRota.x += 360.0;
			if (TGF.VRota.y >= 360.0) TGF.VRota.y -= 360.0;			if (TGF.VRota.y < 0.0) TGF.VRota.y += 360.0;
			if (TGF.VRota.z >= 360.0) TGF.VRota.z -= 360.0;			if (TGF.VRota.z < 0.0) TGF.VRota.z += 360,0;
			TG.VRota.x = 0.0;				TG.VRota.y = 0.0;			TG.VRota.z = 0.0;
			TGF.VTras.x += TG.VTras.x;		TGF.VTras.y += TG.VTras.y; TGF.VTras.z += TG.VTras.z;
			if (TGF.VTras.x < -100000.0) TGF.VTras.x = 100000.0;	if (TGF.VTras.x > 10000.0) TGF.VTras.x = 100000.0;
			if (TGF.VTras.y < -100000.0) TGF.VTras.y = 100000.0;	if (TGF.VTras.y > 10000.0) TGF.VTras.y = 100000.0;
			if (TGF.VTras.z < -100000.0) TGF.VTras.z = 100000.0;	if (TGF.VTras.z > 10000.0) TGF.VTras.z = 100000.0;
			TG.VTras.x = 0.0;				TG.VTras.y = 0.0;			TG.VTras.z = 0.0;
			break;

		// Tecla Delete: Esborrar les Transformacions Geomètriques Calculades
		case GLFW_KEY_DELETE:
			// Inicialitzar els valors de transformacions Geomètriques i de pan en variables fixes.
			TGF.VScal.x = 1.0;		TGF.VScal.y = 1.0;;		TGF.VScal.z = 1.0;
			TG.VScal.x = 1.0;		TG.VScal.y = 1.0;		TG.VScal.z = 1.0;
			TGF.VRota.x = 0.0;		TGF.VRota.y = 0.0;		TGF.VRota.z = 0.0;
			TG.VRota.x = 0.0;		TG.VRota.y = 0.0;		TG.VRota.z = 0.0;
			TGF.VTras.x = 0.0;		TGF.VTras.y = 0.0;		TGF.VTras.z = 0.0;
			TG.VTras.x = 0.0;		TG.VTras.y = 0.0;		TG.VTras.z = 0.0;
			break;

		// Tecla Espaiador
		case GLFW_KEY_SPACE:
			rota = !rota;
			trasl = !trasl;
			break;

		default:
			if (pan) Teclat_Pan(key, action);
				else if (camera == CAM_NAVEGA) Teclat_Navega(key, action);
						else if (!sw_color) Teclat_ColorFons(key, action);
								else Teclat_ColorObjecte(key, action);
			break;
		}
	}
}


// Teclat_Grid: Teclat pels desplaçaments dels gridXY, gridXZ i gridYZ.
void Teclat_Grid(int key, int action)
{
	if (action == GLFW_PRESS)
	{	switch (key)
		{
		// Key Up cursor ('8') - (ASCII:104)
		case GLFW_KEY_UP:
			hgrid.x -= PAS_GRID;
			break;

		// Key Down cursor ('2') - (ASCII:98)
		case GLFW_KEY_DOWN:
			hgrid.x += PAS_GRID;
			break;

		// Key Left cursor ('4') - (ASCII:100)
		case GLFW_KEY_LEFT:
			hgrid.y -= PAS_GRID;
			break;

		// Key Right cursor ('6') - (ASCII:102)
		case GLFW_KEY_RIGHT:
			hgrid.y += PAS_GRID;
			break;

			// Key HOME ('7') - (ASCII:103)
		case GLFW_KEY_HOME:
			hgrid.z += PAS_GRID;
			break;

			// Key END ('1') - (ASCII:97)
		case GLFW_KEY_END:
			hgrid.z -= PAS_GRID;
			break;

		// Key grid ('G') - (ASCII:'G')
		case GLFW_KEY_G:
			sw_grid = !sw_grid;
			break;

/*
			// Key grid ('g')
			case 'g':
			sw_grid = !sw_grid;
			break;
*/

		default:
			if (transf)
			{	if (rota) Teclat_TransRota(key, action);
					else if (trasl) Teclat_TransTraslada(key, action);
						else if (escal) Teclat_TransEscala(key, action);
			}
			else if (pan) Teclat_Pan(key, action);
					else if (camera == CAM_NAVEGA) Teclat_Navega(key, action);
							else if (!sw_color) Teclat_ColorFons(key, action);
									else Teclat_ColorObjecte(key, action);
			break;
		}
	}
}


// Teclat_PasCorbes: Teclat per incrementar-Decrementar el pas de dibuix de les corbes (pas_CS).
void Teclat_PasCorbes(int key, int action)
{
	if (action == GLFW_PRESS)
	{	switch (key)
		{
		// Tecla '+' (incrementar pas_CS)
		case GLFW_KEY_KP_ADD:
			pas_CS = pas_CS * 2.0;
			if (pas_CS > 0.5) pas_CS = 0.5;
			if (objecte == C_BSPLINE) {
				deleteVAOList(CRV_BSPLINE);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
				Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
			}
			else if (objecte == C_BEZIER) {
				deleteVAOList(CRV_BEZIER);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_BEZIER, load_Bezier_Curve_VAO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
				Set_VAOList(CRV_BEZIER, load_Bezier_Curve_EBO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
			}
			else if (objecte == C_LEMNISCATA) {
				deleteVAOList(CRV_LEMNISCATA3D);		//Eliminar VAO anterior.
				Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_EBO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.
			}
			else if (objecte == C_HERMITTE) {
				deleteVAOList(CRV_HERMITTE);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_HERMITTE, load_Hermitte_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_HERMITTE.
				Set_VAOList(CRV_HERMITTE, load_Hermitte_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_HERMITTE.
			}
			else if (objecte == C_CATMULL_ROM) {
				deleteVAOList(CRV_CATMULL_ROM);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_CATMULL_ROM, load_CatmullRom_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_CATMULL_ROM.
				Set_VAOList(CRV_CATMULL_ROM, load_CatmullRom_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_CATMULL_ROM.
			}
			break;

		// Tecla '-' (decrementar pas_CS)
		case GLFW_KEY_KP_SUBTRACT:
			pas_CS = pas_CS / 2;
			if (pas_CS < 0.0125) pas_CS = 0.00625;
			if (objecte == C_BSPLINE) {
				deleteVAOList(CRV_BSPLINE);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
				Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
			}
			else if (objecte == C_BEZIER) {
				deleteVAOList(CRV_BEZIER);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_BEZIER, load_Bezier_Curve_VAO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
				Set_VAOList(CRV_BEZIER, load_Bezier_Curve_EBO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
			}
			else if (objecte == C_LEMNISCATA) {
				deleteVAOList(CRV_LEMNISCATA3D);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_VAO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.
				Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_EBO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.	
			}
			else if (objecte == C_HERMITTE) {
				deleteVAOList(CRV_HERMITTE);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_HERMITTE, load_Hermitte_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_HERMITTE.
				Set_VAOList(CRV_HERMITTE, load_Hermitte_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_HERMITTE.
			}
			else if (objecte == C_CATMULL_ROM) {
				deleteVAOList(CRV_CATMULL_ROM);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_CATMULL_ROM, load_CatmullRom_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_CATMULL_ROM.
				Set_VAOList(CRV_CATMULL_ROM, load_CatmullRom_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_CATMULL_ROM.
			}
			break;

		// Tecla PgUp ('9') (incrementar pas_CS)
		case GLFW_KEY_KP_9:
			pas_CS = pas_CS * 2.0;
			if (pas_CS > 0.5) pas_CS = 0.5;
			if (objecte == C_BSPLINE) {
				deleteVAOList(CRV_BSPLINE);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
				Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
			}
			else if (objecte == C_BEZIER) {
				deleteVAOList(CRV_BEZIER);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_BEZIER, load_Bezier_Curve_VAO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
				Set_VAOList(CRV_BEZIER, load_Bezier_Curve_EBO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
			}
			else if (objecte == C_LEMNISCATA) {
				deleteVAOList(CRV_LEMNISCATA3D);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_EBO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.
				Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_EBO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.
			}
			else if (objecte == C_HERMITTE) {
				deleteVAOList(CRV_HERMITTE);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_HERMITTE, load_Hermitte_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_HERMITTE.
				Set_VAOList(CRV_HERMITTE, load_Hermitte_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_HERMITTE.
			}
			else if (objecte == C_CATMULL_ROM) {
				deleteVAOList(CRV_CATMULL_ROM);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_CATMULL_ROM, load_CatmullRom_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_CATMULL_ROM.
				Set_VAOList(CRV_CATMULL_ROM, load_CatmullRom_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_CATMULL_ROM.
			}
			break;

		// Tecla PgDown ('3') (decrementar pas_CS)
		case GLFW_KEY_KP_3:
			pas_CS = pas_CS / 2;
			if (pas_CS < 0.0125) pas_CS = 0.00625;
			if (objecte == C_BSPLINE) {
				deleteVAOList(CRV_BSPLINE);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
				Set_VAOList(CRV_BSPLINE, load_BSpline_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_BSPLINE.
			}
			else if (objecte == C_BEZIER) {
				deleteVAOList(CRV_BEZIER);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_BEZIER, load_Bezier_Curve_VAO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
				Set_VAOList(CRV_BEZIER, load_Bezier_Curve_EBO(npts_T, PC_t, pas_CS, false)); // Genera corba i la guarda a la posició CRV_BEZIER.
			}
			else if (objecte == C_LEMNISCATA) {
				deleteVAOList(CRV_LEMNISCATA3D);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_VAO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.
				Set_VAOList(CRV_LEMNISCATA3D, load_Lemniscata3D_EBO(800, pas_CS * 20.0)); // Genera corba i la guarda a la posició CRV_LEMNISCATA3D.
			}
			else if (objecte == C_HERMITTE) {
				deleteVAOList(CRV_HERMITTE);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_HERMITTE, load_Hermitte_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_HERMITTE.
				Set_VAOList(CRV_HERMITTE, load_Hermitte_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_HERMITTE.
			}
			else if (objecte == C_CATMULL_ROM) {
				deleteVAOList(CRV_CATMULL_ROM);		//Eliminar VAO anterior.
				//Set_VAOList(CRV_CATMULL_ROM, load_CatmullRom_Curve_VAO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_CATMULL_ROM.
				Set_VAOList(CRV_CATMULL_ROM, load_CatmullRom_Curve_EBO(npts_T, PC_t, pas_CS)); // Genera corba i la guarda a la posició CRV_CATMULL_ROM.
			}
			break;

		default:
			if (transf)
			{	if (rota) Teclat_TransRota(key, action);
					else if (trasl) Teclat_TransTraslada(key, action);
							else if (escal) Teclat_TransEscala(key, action);
			}
			else if (pan) Teclat_Pan(key, action);
					else if (camera == CAM_NAVEGA) Teclat_Navega(key, action);
							else if (!sw_color) Teclat_ColorFons(key, action);
									else Teclat_ColorObjecte(key, action);
			break;
		}
	}
}


/* ------------------------------------------------------------------------- */
/*                           CONTROL DEL RATOLI                              */
/* ------------------------------------------------------------------------- */

// OnMouseButton: Funció que es crida quan s'apreta algun botó (esquerra o dreta) del mouse.
//      PARAMETRES: - window: Finestra activa
//					- button: Botó seleccionat (GLFW_MOUSE_BUTTON_LEFT o GLFW_MOUSE_BUTTON_RIGHT)
//					- action: Acció de la tecla: GLFW_PRESS (si s'ha apretat), GLFW_REPEAT, si s'ha repetit pressió i GL_RELEASE, si es deixa d'apretar.
void OnMouseButton(GLFWwindow* window, int button, int action, int mods)
{
// TODO: Agregue aquí su código de controlador de mensajes o llame al valor predeterminado
// Get the cursor position when the mouse key has been pressed or released.
	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);

// EntornVGI.ImGui: Test si events de mouse han sigut filtrats per ImGui (io.WantCaptureMouse)
// (1) ALWAYS forward mouse data to ImGui! This is automatic with default backends. With your own backend:
	ImGuiIO& io = ImGui::GetIO();

	// (2) ONLY forward mouse data to your underlying app/game.
	if (editorEscenas.Activo()) return;
	if (modoJuegoFPS()) return;

	if (!io.WantCaptureMouse) { //<Tractament mouse de l'aplicació>}
		// OnLButtonDown
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
		{	// Entorn VGI: Detectem en quina posició s'ha apretat el botó esquerra del
			//				mouse i ho guardem a la variable m_PosEAvall i activem flag m_ButoEAvall
			m_ButoEAvall = true;
			m_PosEAvall.x = xpos;	m_PosEAvall.y = ypos;
			m_EsfeEAvall = OPV;
		}
		// OnLButtonUp: Funció que es crida quan deixem d'apretar el botó esquerra del mouse.
		else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
		{	// Entorn VGI: Desactivem flag m_ButoEAvall quan deixem d'apretar botó esquerra del mouse.
			m_ButoEAvall = false;

			// OPCIÓ VISTA-->SATÈLIT: Càlcul increment desplaçament del Punt de Vista
			if ((satelit) && (projeccio != ORTO))
			{	//m_EsfeIncEAvall.R = m_EsfeEAvall.R - OPV.R;
				m_EsfeIncEAvall.alfa = 0.01f * (OPV.alfa - m_EsfeEAvall.alfa); //if (abs(m_EsfeIncEAvall.alfa)<0.01) { if ((m_EsfeIncEAvall.alfa)>0.0) m_EsfeIncEAvall.alfa = 0.01 else m_EsfeIncEAvall.alfa=0.01}
				m_EsfeIncEAvall.beta = 0.01f * (OPV.beta - m_EsfeEAvall.beta);
				if (abs(m_EsfeIncEAvall.beta) < 0.01)
				{	if ((m_EsfeIncEAvall.beta) > 0.0) m_EsfeIncEAvall.beta = 0.01;
					else m_EsfeIncEAvall.beta = 0.01;
				}
				//if ((m_EsfeIncEAvall.R == 0.0) && (m_EsfeIncEAvall.alfa == 0.0) && (m_EsfeIncEAvall.beta == 0.0)) KillTimer(WM_TIMER);
				//else SetTimer(WM_TIMER, 10, NULL);
			}
		}
		// OnRButtonDown
		else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
		{	// Entorn VGI: Detectem en quina posició s'ha apretat el botó esquerra del
			//				mouse i ho guardem a la variable m_PosEAvall i activem flag m_ButoEAvall
			m_ButoDAvall = true;
			//m_PosDAvall = point;
			m_PosDAvall.x = xpos;	m_PosDAvall.y = ypos;
		}
		// OnLButtonUp: Funció que es crida quan deixem d'apretar el botó esquerra del mouse.
		else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
		{	// Entorn VGI: Desactivem flag m_ButoEAvall quan deixem d'apretar botó esquerra del mouse.
			m_ButoDAvall = false;
		}
	}
}

// OnMouseMove: Funció que es crida quan es mou el mouse. La utilitzem per la 
//				  Visualització Interactiva amb les tecles del mouse apretades per 
//				  modificar els paràmetres de P.V. (R,angleh,anglev) segons els 
//				  moviments del mouse.
//      PARAMETRES: - window: Finestra activa
//					- xpos: Posició X del cursor del mouse (coord. pantalla) quan el botó s'ha apretat.
//					- ypos: Posició Y del cursor del mouse(coord.pantalla) quan el botó s'ha apretat.
void OnMouseMove(GLFWwindow* window, double xpos, double ypos)
{
// TODO: Agregue aquí su código de controlador de mensajes o llame al valor predeterminado
	double modul = 0;
	GLdouble vdir[3] = { 0, 0, 0 };
	CSize gir = { 0,0 }, girn = { 0,0 }, girT = { 0,0 }, zoomincr = { 0,0 };

	// TODO: Add your message handler code here and/or call default
	if (modoJuegoFPS())
	{
		if (!fpsCursorCapturado || fpsPausado || !ventanaConFoco) return;
		if (!fpsTienePosicionAnterior)
		{
			fpsUltimoX = xpos;
			fpsUltimoY = ypos;
			fpsTienePosicionAnterior = true;
			return;
		}
		const double deltaX = xpos - fpsUltimoX;
		const double deltaY = ypos - fpsUltimoY;
		fpsUltimoX = xpos;
		fpsUltimoY = ypos;
		mouseDeltaX += deltaX;
		mouseDeltaY += deltaY;

		if (Vis_Polar == POLARZ)
		{
			const double angulo = -deltaX * 0.12 * PI / 180.0;
			const double dx = n[0] - opvN.x;
			const double dy = n[1] - opvN.y;
			n[0] = opvN.x + dx * cos(angulo) - dy * sin(angulo);
			n[1] = opvN.y + dx * sin(angulo) + dy * cos(angulo);
			inclinarJugadorFPS(deltaY);
		}
		return;
	}

	if (m_ButoEAvall && mobil && projeccio != CAP)
	{	// Entorn VGI: Determinació dels angles (en graus) segons l'increment
		//				horitzontal i vertical de la posició del mouse.
		gir.cx = m_PosEAvall.x - xpos;		gir.cy = m_PosEAvall.y - ypos;
		m_PosEAvall.x = xpos;				m_PosEAvall.y = ypos;
		if (camera == CAM_ESFERICA)
		{	// Càmera Esfèrica
			OPV.beta = OPV.beta - gir.cx / 2.0;
			OPV.alfa = OPV.alfa + gir.cy / 2.0;

			// Entorn VGI: Control per evitar el creixement desmesurat dels angles.
			while (OPV.alfa >= 360)		OPV.alfa = OPV.alfa - 360.0;
			while (OPV.alfa < 0)		OPV.alfa = OPV.alfa + 360.0;
			while (OPV.beta >= 360)		OPV.beta = OPV.beta - 360.0;
			while (OPV.beta < 0)		OPV.beta = OPV.beta + 360.0;
		}
		else { // Càmera Geode
			OPV_G.beta = OPV_G.beta + gir.cx / 2.0;
			OPV_G.alfa = OPV_G.alfa + gir.cy / 2.0;
			// Entorn VGI: Control per evitar el creixement desmesurat dels angles
			while (OPV_G.alfa >= 360.0f)	OPV_G.alfa = OPV_G.alfa - 360.0;
			while (OPV_G.alfa < 0.0f)		OPV_G.alfa = OPV_G.alfa + 360.0;
			while (OPV_G.beta >= 360.f)		OPV_G.beta = OPV_G.beta - 360.0;
			while (OPV_G.beta < 0.0f)		OPV_G.beta = OPV_G.beta + 360.0;
		}
		// Crida a OnPaint() per redibuixar l'escena
		//OnPaint(window);
	}
	else if (m_ButoEAvall && (camera == CAM_NAVEGA) && (projeccio != CAP && projeccio != ORTO)) // Opció Navegació
	{	// Entorn VGI: Canviar orientació en opció de Navegació
		girn.cx = m_PosEAvall.x - xpos;		girn.cy = m_PosEAvall.y - ypos;
		angleZ = girn.cx / 2.0;
		// Entorn VGI: Control per evitar el creixement desmesurat dels angles.
		while (angleZ >= 360, 0) angleZ = angleZ - 360;
		while (angleZ < 0.0)	angleZ = angleZ + 360;

		// Entorn VGI: Segons orientació dels eixos Polars (Vis_Polar)
		if (Vis_Polar == POLARZ) 
		{ // (X,Y,Z)
			n[0] = n[0] - opvN.x;
			n[1] = n[1] - opvN.y;
			n[0] = n[0] * cos(angleZ * PI / 180.0) - n[1] * sin(angleZ * PI / 180.0);
			n[1] = n[0] * sin(angleZ * PI / 180.0) + n[1] * cos(angleZ * PI / 180.0);
			n[0] = n[0] + opvN.x;
			n[1] = n[1] + opvN.y;
		}
		else if (Vis_Polar == POLARY) 
		{ //(X,Y,Z) --> (Z,X,Y)
			n[2] = n[2] - opvN.z;
			n[0] = n[0] - opvN.x;
			n[2] = n[2] * cos(angleZ * PI / 180.0) - n[0] * sin(angleZ * PI / 180.0);
			n[0] = n[2] * sin(angleZ * PI / 180.0) + n[0] * cos(angleZ * PI / 180.0);
			n[2] = n[2] + opvN.z;
			n[0] = n[0] + opvN.x;
		}
		else if (Vis_Polar == POLARX) 
		{ //(X,Y,Z) --> (Y,Z,X)
			n[1] = n[1] - opvN.y;
			n[2] = n[2] - opvN.z;
			n[1] = n[1] * cos(angleZ * PI / 180.0) - n[2] * sin(angleZ * PI / 180.0);
			n[2] = n[1] * sin(angleZ * PI / 180.0) + n[2] * cos(angleZ * PI / 180.0);
			n[1] = n[1] + opvN.y;
			n[2] = n[2] + opvN.z;
		}

		m_PosEAvall.x = xpos;		m_PosEAvall.y = ypos;
		// Crida a OnPaint() per redibuixar l'escena
		//OnPaint(window);
	}

// Entorn VGI: Transformació Geomètrica interactiva pels eixos X,Y boto esquerra del mouse.
	else {
		bool transE = transX || transY;
		if (m_ButoEAvall && transE && transf)
		{	// Calcular increment
			girT.cx = m_PosEAvall.x - xpos;		girT.cy = m_PosEAvall.y - ypos;
			if (transX)
			{	long int incrT = girT.cx;
				if (trasl)
				{	TG.VTras.x += incrT * fact_Tras;
					if (TG.VTras.x < -100000.0) TG.VTras.x = 100000.0;
					if (TG.VTras.x > 100000.0) TG.VTras.x = 100000.0;
				}
				else if (rota)
				{	TG.VRota.x += incrT * fact_Rota;
					while (TG.VRota.x >= 360.0) TG.VRota.x -= 360.0;
					while (TG.VRota.x < 0.0) TG.VRota.x += 360.0;
				}
				else if (escal)
				{	if (incrT < 0) incrT = -1 / incrT;
					TG.VScal.x = TG.VScal.x * incrT;
					if (TG.VScal.x < 0.25) TG.VScal.x = 0.25;
					if (TG.VScal.x > 8192.0) TG.VScal.x = 8192.0;
				}
			}
			if (transY)
			{	long int incrT = girT.cy;
				if (trasl)
				{	TG.VTras.y += incrT * fact_Tras;
					if (TG.VTras.y < -100000) TG.VTras.y = 100000;
					if (TG.VTras.y > 100000) TG.VTras.y = 100000;
				}
				else if (rota)
				{	TG.VRota.y += incrT * fact_Rota;
					while (TG.VRota.y >= 360.0) TG.VRota.y -= 360.0;
					while (TG.VRota.y < 0.0) TG.VRota.y += 360.0;
				}
				else if (escal)
				{	if (incrT <= 0.0) 
					{	if (incrT >= -2) incrT = -2;
						incrT = 1 / Log2(-incrT);
					}
					else incrT = Log2(incrT);
					TG.VScal.y = TG.VScal.y * incrT;
					if (TG.VScal.y < 0.25) TG.VScal.y = 0.25;
					if (TG.VScal.y > 8192.0) TG.VScal.y = 8192.0;
				}
			}
			m_PosEAvall.x = xpos;	m_PosEAvall.y = ypos;
			// Crida a OnPaint() per redibuixar l'escena
			//InvalidateRect(NULL, false);
			//OnPaint(windows);
		}
	}

// Entorn VGI: Determinació del desplaçament del pan segons l'increment
//				vertical de la posició del mouse (tecla dreta apretada).
	if (m_ButoDAvall && pan && (projeccio != CAP && projeccio != ORTO))
	{
		//CSize zoomincr = m_PosDAvall - point;
		zoomincr.cx = m_PosDAvall.x - xpos;		zoomincr.cy = m_PosDAvall.y - ypos;
		long int incrx = zoomincr.cx;
		long int incry = zoomincr.cy;

		// Desplaçament pan vertical
		tr_cpv.y -= incry * fact_pan;
		if (tr_cpv.y > 100000.0) tr_cpv.y = 100000.0;
		else if (tr_cpv.y < -100000.0) tr_cpv.y = -100000.0;

		// Desplaçament pan horitzontal
		tr_cpv.x += incrx * fact_pan;
		if (tr_cpv.x > 100000.0) tr_cpv.x = 100000.0;
		else if (tr_cpv.x < -100000.0) tr_cpv.x = -100000.0;

		//m_PosDAvall = point;
		m_PosDAvall.x = xpos;	m_PosDAvall.y = ypos;
		// Crida a OnPaint() per redibuixar l'escena
		//OnPaint(window);
	}
// Determinació del paràmetre R segons l'increment
//   vertical de la posició del mouse (tecla dreta apretada)
	//else if (m_ButoDAvall && zzoom && (projeccio!=CAP && projeccio!=ORTO))
	else if (m_ButoDAvall && zzoom && (projeccio != CAP))
	{	//CSize zoomincr = m_PosDAvall - point;
		zoomincr.cx = m_PosDAvall.x - xpos;		zoomincr.cy = m_PosDAvall.y - ypos;
		long int incr = zoomincr.cy / 1.0;

		if (camera == CAM_ESFERICA) {	// Càmera Esfèrica
			OPV.R = OPV.R + incr;
			//if (OPV.R < 0.25) OPV.R = 0.25;
			if (OPV.R < p_near) OPV.R = p_near;
			if (OPV.R > p_far) OPV.R = p_far;
		}
		else { // Càmera Geode
			OPV_G.R = OPV_G.R + incr;
			if (OPV_G.R < 0.0) OPV_G.R = 0.0;
		}

		//m_PosDAvall = point;
		m_PosDAvall.x = xpos;				m_PosDAvall.y = ypos;
		// Crida a OnPaint() per redibuixar l'escena
		//OnPaint(window);
	}
	else if (m_ButoDAvall && (camera == CAM_NAVEGA) && (projeccio != CAP && projeccio != ORTO))
	{	// Avançar en opció de Navegació
		if ((m_PosDAvall.x != xpos) && (m_PosDAvall.y != ypos))
		{
			//CSize zoomincr = m_PosDAvall - point;
			zoomincr.cx = m_PosDAvall.x - xpos;		zoomincr.cy = m_PosDAvall.y - ypos;
			double incr = zoomincr.cy / 2.0;
			//	long int incr=zoomincr.cy/2.0;  // Causa assertion en "afx.inl" lin 169
			vdir[0] = n[0] - opvN.x;
			vdir[1] = n[1] - opvN.y;
			vdir[2] = n[2] - opvN.z;
			modul = sqrt(vdir[0] * vdir[0] + vdir[1] * vdir[1] + vdir[2] * vdir[2]);
			vdir[0] = vdir[0] / modul;
			vdir[1] = vdir[1] / modul;
			vdir[2] = vdir[2] / modul;

			// Entorn VGI: Segons orientació dels eixos Polars (Vis_Polar)
			if (Vis_Polar == POLARZ) 
			{ // (X,Y,Z)
				opvN.x += incr * vdir[0];
				opvN.y += incr * vdir[1];
				n[0] += incr * vdir[0];
				n[1] += incr * vdir[1];
			}
			else if (Vis_Polar == POLARY) 
			{ //(X,Y,Z) --> (Z,X,Y)
				opvN.z += incr * vdir[2];
				opvN.x += incr * vdir[0];
				n[2] += incr * vdir[2];
				n[0] += incr * vdir[0];
			}
			else if (Vis_Polar == POLARX) 
			{ //(X,Y,Z) --> (Y,Z,X)
				opvN.y += incr * vdir[1];
				opvN.z += incr * vdir[2];
				n[1] += incr * vdir[1];
				n[2] += incr * vdir[2];
			}

			//m_PosDAvall = point;
			m_PosDAvall.x = xpos;				m_PosDAvall.y = ypos;
			// Crida a OnPaint() per redibuixar l'escena
			//OnPaint(window);
		}
	}

// Entorn VGI: Transformació Geomètrica interactiva per l'eix Z amb boto dret del mouse.
	else if (m_ButoDAvall && transZ && transf)
	{	// Calcular increment
		girT.cx = m_PosDAvall.x - xpos;		girT.cy = m_PosDAvall.y - ypos;
		long int incrT = girT.cy;
		if (trasl)
		{	TG.VTras.z += incrT * fact_Tras;
			if (TG.VTras.z < -100000.0) TG.VTras.z = 100000.0;
			if (TG.VTras.z > 100000.0) TG.VTras.z = 100000.0;
		}
		else if (rota)
		{	incrT = girT.cx;
			TG.VRota.z += incrT * fact_Rota;
			while (TG.VRota.z >= 360.0) TG.VRota.z -= 360.0;
			while (TG.VRota.z < 0.0) TG.VRota.z += 360.0;
		}
		else if (escal)
		{	if (incrT <= 0) {
			if (incrT >= -2.0) incrT = -2.0;
			incrT = 1.0 / Log2(-incrT);
		}
		else incrT = Log2(incrT);
		TG.VScal.z = TG.VScal.z * incrT;
		if (TG.VScal.z < 0.25) TG.VScal.z = 0.25;
		if (TG.VScal.z > 8192.0) TG.VScal.z = 8192.0;
		}
		//m_PosDAvall = point;
		m_PosDAvall.x = xpos;	m_PosDAvall.y = ypos;
		// Crida a OnPaint() per redibuixar l'escena
		//OnPaint(window);
	}
}

// OnMouseWheel: Funció que es crida quan es mou el rodet del mouse. La utilitzem per la 
//				  Visualització Interactiva per modificar el paràmetre R de P.V. (R,angleh,anglev) 
//				  segons el moviment del rodet del mouse.
//      PARAMETRES: -  (xoffset,yoffset): Estructura (x,y) que dóna la posició del mouse 
//							 (coord. pantalla) quan el botó s'ha apretat.
void OnMouseWheel(GLFWwindow* window, double xoffset, double yoffset)
{
// TODO: Agregue aquí su código de controlador de mensajes o llame al valor predeterminado
	if (editorEscenas.Activo())
	{	// la rueda es de la camara solo sobre el viewport; sobre un panel ImGui desplaza el panel
		editorEscenas.RuedaRaton(static_cast<float>(yoffset));
		return;
	}
	double modul = 0;
	GLdouble vdir[3] = { 0, 0, 0 };

// EntornVGI.ImGui: Test si events de mouse han sigut filtrats per ImGui (io.WantCaptureMouse)
// (1) ALWAYS forward mouse data to ImGui! This is automatic with default backends. With your own backend:
	ImGuiIO& io = ImGui::GetIO();

// (2) ONLY forward mouse data to your underlying app/game.
	if (editorEscenas.Activo() || modoJuegoFPS()) return;
	if (!io.WantCaptureMouse) { // <Tractament mouse de l'aplicació>}
		// Funció de zoom quan està activada la funció pan o les T. Geomètriques
		if ((zzoom || zzoomO) || (transX) || (transY) || (transZ))
		{	if (camera == CAM_ESFERICA) 
			{	// Càmera Esfèrica
				OPV.R = OPV.R + yoffset / 4.0;
				if (OPV.R < 0.25) OPV.R = 0.25;
				//InvalidateRect(NULL, false);
			}
			else if (camera == CAM_GEODE)
			{	// Càmera Geode
				OPV_G.R = OPV_G.R + yoffset / 4.0;
				if (OPV_G.R < 0.0) OPV_G.R = 0.0;
				//InvalidateRect(NULL, false);
			}
		}
		else if (camera == CAM_NAVEGA && !io.WantCaptureMouse)
		{	vdir[0] = n[0] - opvN.x;
			vdir[1] = n[1] - opvN.y;
			vdir[2] = n[2] - opvN.z;
			modul = sqrt(vdir[0] * vdir[0] + vdir[1] * vdir[1] + vdir[2] * vdir[2]);
			vdir[0] = vdir[0] / modul;
			vdir[1] = vdir[1] / modul;
			vdir[2] = vdir[2] / modul;

			// Entorn VGI: Segons orientació dels eixos Polars (Vis_Polar)
			if (Vis_Polar == POLARZ) 
			{ // (X,Y,Z)
				opvN.x += (yoffset / 4.0) * vdir[0];
				opvN.y += (yoffset / 4.0) * vdir[1];
				n[0] += (yoffset / 4.0) * vdir[0];
				n[1] += (yoffset / 4.0) * vdir[1];
			}
			else if (Vis_Polar == POLARY) 
			{ //(X,Y,Z) --> (Z,X,Y)
				opvN.z += (yoffset / 4.0) * vdir[2];
				opvN.x += (yoffset / 4.0) * vdir[0];
				n[2] += (yoffset / 4.0) * vdir[2];
				n[0] += (yoffset / 4.0) * vdir[0];
			}
			else if (Vis_Polar == POLARX) 
			{ //(X,Y,Z) --> (Y,Z,X)
				opvN.y += (yoffset / 4.0) * vdir[1];
				opvN.z += (yoffset / 4.0) * vdir[2];
				n[1] += (yoffset / 4.0) * vdir[1];
				n[2] += (yoffset / 4.0) * vdir[2];
			}
		}
	}
}


/* ------------------------------------------------------------------------- */
void OnTimer()
{
// TODO: Agregue aquí su código de controlador de mensajes o llame al valor predeterminado
	if (anima) {
		// Codi de tractament de l'animació quan transcorren els ms. del crono.

		// Crida a OnPaint() per redibuixar l'escena
		//InvalidateRect(NULL, false);
	}
	else if (satelit) {	// OPCIÓ SATÈLIT: Increment OPV segons moviments mouse.
		if (camera == CAM_ESFERICA) 
		{	//OPV.R = OPV.R + m_EsfeIncEAvall.R;
			OPV.alfa = OPV.alfa + m_EsfeIncEAvall.alfa;
			while (OPV.alfa > 360.0) OPV.alfa = OPV.alfa - 360.0;
			while (OPV.alfa < 0.0) OPV.alfa = OPV.alfa + 360.0;
			OPV.beta = OPV.beta + m_EsfeIncEAvall.beta;
			while (OPV.beta > 360.0) OPV.beta = OPV.beta - 360.0;
			while (OPV.beta < 0.0) OPV.beta = OPV.beta + 360.0;
		}	
		else if (camera == CAM_GEODE) 
		{	//OPV.R = OPV.R + m_EsfeIncEAvall.R;
			OPV_G.alfa = OPV_G.alfa + m_EsfeIncEAvall.alfa * 0.5;
			while (OPV_G.alfa > 360.0) OPV_G.alfa = OPV_G.alfa - 360.0;
			while (OPV_G.alfa < 0) OPV_G.alfa = OPV_G.alfa + 360.0;
			OPV_G.beta = OPV_G.beta + m_EsfeIncEAvall.beta * 0.5;
			while (OPV_G.beta > 360.0) OPV_G.beta = OPV.beta - 360.0;
			while (OPV_G.beta < 0.0) OPV_G.beta = OPV_G.beta + 360.0;
			}

		// Crida a OnPaint() per redibuixar l'escena
		//OnPaint();
		}
// EntornVGI. QUATERNIONS: Animació Objecte Rígid per Quaternions
	else if (animaQ)
	{	// CAL FER: Animació sense fi a partir qI i qF amb interpolació lineal o esfèrica.

		// Crida a OnPaint() per redibuixar l'escena
		//InvalidateRect(NULL, false);
	}
// EntornVGI. ROBOT: Animació Robot Articulat
	else if (objecte == ROBOT && animaR)
	{	// CAL FER: Animació sense fi a partir de dues o tres posicios clau del robot.posicions

		// Crida a OnPaint() per redibuixar l'escena
		//InvalidateRect(NULL, false);
	}
}

// ---------------- Entorn VGI: Funcions locals a main.cpp

// Log2: Càlcul del log base 2 de num
int Log2(int num)
{
	int tlog;

	if (num >= 8192) tlog = 13;
	else if (num >= 4096) tlog = 12;
	else if (num >= 2048) tlog = 11;
	else if (num >= 1024) tlog = 10;
	else if (num >= 512) tlog = 9;
	else if (num >= 256) tlog = 8;
	else if (num >= 128) tlog = 7;
	else if (num >= 64) tlog = 6;
	else if (num >= 32) tlog = 5;
	else if (num >= 16) tlog = 4;
	else if (num >= 8) tlog = 3;
	else if (num >= 4) tlog = 2;
	else if (num >= 2) tlog = 1;
	else tlog = 0;

	return tlog;
}


// -------------------- FUNCIONS CORBES SPLINE i BEZIER

// llegir_ptsC: Llegir punts de control de corba (spline o Bezier) d'un fitxer .crv. 
//				Retorna el nombre de punts llegits en el fitxer.
//int llegir_pts(CString nomf)
int llegir_ptsC(const char* nomf)
{
	int i, j;
	FILE* fd;

// Inicialitzar vector punts de control de la corba spline
	for (i = 0; i < MAX_PATCH_CORBA; i = i++)
	{	PC_t[i].x = 0.0;
		PC_t[i].y = 0.0;
		PC_t[i].z = 0.0;
	}

//	ifstream f("altinicials.dat",ios::in);
//    f>>i; f>>j;
	if ((fd = fopen(nomf, "rt")) == NULL)
	{	//LPCWSTR texte1 = reinterpret_cast<LPCWSTR> ("ERROR:");
		//LPCWSTR texte2 = reinterpret_cast<LPCWSTR> ("File .crv was not opened");
		//MessageBox(texte1, texte2, MB_OK);
		fprintf(stderr, "ERROR: File .crv was not opened");
		return false;
	}
	fscanf(fd, "%d \n", &i);
	if (i == 0) return false;
	else {	for (j = 0; j < i; j = j++)
			{	//fscanf(fd, "%f", &corbaSpline[j].x);
				//fscanf(fd, "%f", &corbaSpline[j].y);
				//fscanf(fd, "%f \n", &corbaSpline[j].z);
				fscanf(fd, "%lf %lf %lf \n", &PC_t[j].x, &PC_t[j].y, &PC_t[j].z);
			}
		}
	fclose(fd);

	return i;
}


// Entorn VGI. OnFull_Screen: Funció per a pantalla completa
void OnFull_Screen(GLFWmonitor* monitor, GLFWwindow* window)
{
	static int windowX = 0;
	static int windowY = 0;
	static int windowWidth = 1280;
	static int windowHeight = 720;

	fullscreen = !fullscreen;

	if (fullscreen)
	{
		glfwGetWindowPos(window, &windowX, &windowY);
		glfwGetWindowSize(window, &windowWidth, &windowHeight);
		const GLFWvidmode* videoMode = glfwGetVideoMode(monitor);
		glfwSetWindowMonitor(window, monitor, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate);
	}
	else
	{
		glfwSetWindowMonitor(window, nullptr, windowX, windowY, windowWidth, windowHeight, GLFW_DONT_CARE);
	}
}

// -------------------- TRACTAMENT ERRORS
// error_callback: Displaia error que es pugui produir
void error_callback(int code, const char* description)
{
	//const char* descripcio;
	//int codi = glfwGetError(&descripcio);

 //   display_error_message(code, description);
	fprintf(stderr, "Codi Error: %i", code);
	fprintf(stderr, "Descripcio: %s\n", description);
}


void APIENTRY glDebugOutput(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
	const GLchar* message, const void* userParam)
{
	if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return; // ignore these non-significant error codes

	fprintf(stderr, "---------------\n");
	fprintf(stderr, "Debug message ( %3i %s \n", id, message);

	switch (source)
	{
	case GL_DEBUG_SOURCE_API:             fprintf(stderr, "Source: API \n"); break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   fprintf(stderr, "Source: Window System \n"); break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER: fprintf(stderr, "Source: Shader Compiler \n"); break;
	case GL_DEBUG_SOURCE_THIRD_PARTY:     fprintf(stderr, "Source: Third Party \n"); break;
	case GL_DEBUG_SOURCE_APPLICATION:     fprintf(stderr, "Source: Application \n"); break;
	case GL_DEBUG_SOURCE_OTHER:           fprintf(stderr, "Source: Other \n"); break;
	} //std::cout << std::endl;

	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR:               fprintf(stderr, "Type: Error\n"); break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: fprintf(stderr, "Type: Deprecated Behaviour\n"); break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  fprintf(stderr, "Type: Undefined Behaviour\n"); break;
	case GL_DEBUG_TYPE_PORTABILITY:         fprintf(stderr, "Type: Portability\n"); break;
	case GL_DEBUG_TYPE_PERFORMANCE:         fprintf(stderr, "Type: Performance\n"); break;
	case GL_DEBUG_TYPE_MARKER:              fprintf(stderr, "Type: Marker\n"); break;
	case GL_DEBUG_TYPE_PUSH_GROUP:          fprintf(stderr, "Type: Push Group\n"); break;
	case GL_DEBUG_TYPE_POP_GROUP:           fprintf(stderr, "Type: Pop Group\n"); break;
	case GL_DEBUG_TYPE_OTHER:               fprintf(stderr, "Type: Other\n"); break;
	} //std::cout << std::endl;

	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH:         fprintf(stderr, "Severity: high\n"); break;
	case GL_DEBUG_SEVERITY_MEDIUM:       fprintf(stderr, "Severity: medium\n"); break;
	case GL_DEBUG_SEVERITY_LOW:          fprintf(stderr, "Severity: low\n"); break;
	case GL_DEBUG_SEVERITY_NOTIFICATION: fprintf(stderr, "Severity: notification\n"); break;
	} //std::cout << std::endl;
	//std::cout << std::endl;
	fprintf(stderr, "\n");
}

// main(): Funció principal del programa. Inicialitza la finestra, els callbacks i el bucle principal d'operació
int main(int argc, char** argv)
{
	SetWorkingDirectoryToAssetRoot(argc > 0 ? argv[0] : nullptr);
	audioManager.init();
	{	// Red: --host [--port N] | --connect HOST[:PUERTO] [--name N]. Sin argumentos: OFFLINE.
		std::string errorRed;
		if (!netjuego::ParsearArgumentos(argc, argv, configRedPendiente, errorRed))
		{
			std::cerr << "Argumentos de red ignorados: " << errorRed << std::endl;
			configRedPendiente = netjuego::ConfigRed{};
		}
		else if (configRedPendiente.rol == net::Role::Client)
		{
			std::string errorGuardado;
			if (!netjuego::GuardarUltimoEndpoint({ configRedPendiente.host, configRedPendiente.puerto }, errorGuardado))
				std::cerr << "Red: no se guardo la ultima direccion: " << errorGuardado << std::endl;
		}
	}

// Entorn VGI. Timer: Variables
	float time = elapsedTime;
	float now;
	float delta;

// glfw: initialize and configure
// ------------------------------
	if (!glfwInit())
	{	fprintf(stderr, "Failed to initialize GLFW\n");
		getchar();
		return -1;
	}

	primary = glfwGetPrimaryMonitor();
	if (primary == nullptr)
	{
		fprintf(stderr, "Failed to get the primary monitor for fullscreen mode\n");
		glfwTerminate();
		return -1;
	}

	mode = glfwGetVideoMode(primary);
	if (mode == nullptr)
	{
		fprintf(stderr, "Failed to get the primary monitor video mode\n");
		glfwTerminate();
		return -1;
	}

#ifdef __APPLE__
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_FALSE);
#endif
#ifndef NDEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

// Create the game in a regular window (80% of the monitor); F11 toggles fullscreen.
	const int startW = (int)(mode->width * 0.8);
	const int startH = (int)(mode->height * 0.8);
	fullscreen = false;
#ifndef __APPLE__
	// OpenXR/OpenGL suele exigir 4.3+ en Windows. Un contexto core 3.3 puede quedarse en 3.3 en algunos
	// controladores, asi que se pide primero 4.6 (core) y, si no existe, se vuelve al 3.3 de siempre.
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	window = glfwCreateWindow(startW, startH, "EscapeTheUAB", NULL, NULL);
	if (!window)
	{
		fprintf(stderr, "OpenGL 4.6 no disponible; se usa OpenGL 3.3 (sin VR si el runtime pide mas).\n");
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		window = glfwCreateWindow(startW, startH, "EscapeTheUAB", NULL, NULL);
	}
#else
	window = glfwCreateWindow(startW, startH, "EscapeTheUAB", NULL, NULL);
#endif
	if (!window)
	{	fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 4.6 compatible. Try the 2.1 version of the tutorials.\n");
		getchar();
		glfwTerminate();
		return -1;
	}
	{	int mx = 0, my = 0;
		glfwGetMonitorPos(primary, &mx, &my);
		glfwSetWindowPos(window, mx + (mode->width - startW) / 2, my + (mode->height - startH) / 2);
	}

// Make the window's context current
	glfwMakeContextCurrent(window);

// Llegir resolució actual de pantalla
	glfwGetWindowSize(window, &width_old, &height_old);
	glfwGetFramebufferSize(window, &w, &h);

// Initialize GLEW
	glewExperimental = GL_TRUE; // Needed for core profile
	if (glewInit() != GLEW_OK) {
		glGetError();	// Esborrar codi error generat per bug a llibreria GLEW
		fprintf(stderr, "Failed to initialize GLEW\n");
		getchar();
		glfwTerminate();
		return -1;
		}

	const bool nfdAvailable = NFD_InitCompat();
	if (!nfdAvailable)
		std::fprintf(stderr, "Failed to initialize Native File Dialog: %s\n", NFD_GetError());

	GLint major = 0, minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	ventanaPrincipal = window;
	versionGLMayor = major;
	versionGLMenor = minor;
// ------------ - Entorn VGI : Enable OpenGL debug context if context allows for DEBUG CONTEXT (GL4.6)
	if (GLEW_VERSION_4_6)
	{	GLint flags; glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
		if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
		{	glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // makes sure errors are displayed synchronously
			glDebugMessageCallback(glDebugOutput, nullptr);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
		}
	}

// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

// Initialize Application control varibles
	InitGL();
	cargarTexturaManoInteraccion();

	char phonePath[] = "./assets/models/Phone.obj";
	phoneOBJ = new COBJModel();
	if (phoneOBJ->LoadModel(phonePath) != 0)
	{
		std::cerr << "No se pudo cargar Phone.obj. Comprueba la ruta y el MTL.\n";
		delete phoneOBJ;
		phoneOBJ = nullptr;
	}

// ------------- Entorn VGI: Callbacks
	glfwSetWindowSizeCallback(window, OnSize);										// - Windows Size in screen Coordinates
	glfwSetFramebufferSizeCallback(window, OnFramebufferSize);							// - Medidas del framebuffer en píxeles
	glfwSetWindowFocusCallback(window, OnWindowFocus);
	glfwSetWindowCloseCallback(window, OnWindowCloseRequest);
	editorEscenas.FijarCargadorModelos(cargarModeloParaEditor);
	editorEscenas.FijarCargadorTexturas(texturaParaEditor);
	editorEscenas.FijarCargadorPuertas([](const std::filesystem::path& ruta, const SceneObjetoAnimado& datos,
		editor::ContextoEscena contexto, std::string& error)
	{
		std::error_code ec;
		const auto absoluta = std::filesystem::absolute(ruta, ec).lexically_normal();
		if (ec || !std::filesystem::is_regular_file(absoluta, ec)) { error = "El GLB seleccionado no existe."; return false; }
		const std::string clave = absoluta.string();
		const bool modeloNuevo = !contexto.escenaGlb.modelos.contains(clave);
		if (!glb::AnadirObjeto(contexto.escenaGlb, datos.id, clave, datos.posicion, datos.giroZGrados, error)) return false;
		glb::ObjetoAnimado* instancia = glb::BuscarObjeto(contexto.escenaGlb, datos.id);
		if (instancia == nullptr) { error = "No se pudo crear la instancia GLB."; return false; }
		instancia->escala = datos.escala;
		if (!datos.animacion.empty() && !glb::SeleccionarAnimacion(*instancia, datos.animacion, error)) return false;
		if (modeloNuevo) glb::SubirModeloAGPU(contexto.escenaGlb.modelos.at(clave));
		return true;
	});
	glfwSetMouseButtonCallback(window, (GLFWmousebuttonfun)OnMouseButton);			// - Directly redirect GLFW mouse button events
	glfwSetCursorPosCallback(window, (GLFWcursorposfun)OnMouseMove);				// - Directly redirect GLFW mouse position events
	glfwSetScrollCallback(window, (GLFWscrollfun)OnMouseWheel);						// - Directly redirect GLFW mouse wheel events
	glfwSetKeyCallback(window, (GLFWkeyfun)OnKeyDown);								// - Directly redirect GLFW key events
	glfwSetErrorCallback(error_callback);											// Error callback
	glfwSetWindowRefreshCallback(window, (GLFWwindowrefreshfun)OnPaint);			// - Callback to refresh the screen

// Entorn VGI; Timer: Lectura temps
	float previous = glfwGetTime();

// Entorn VGI.ImGui: Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

// Entorn VGI.ImGui: Setup Dear ImGui style
	ImGui::StyleColorsLight();

	io.FontGlobalScale = 1.0f;

// Entorn VGI.ImGui: Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __APPLE__
	ImGui_ImplOpenGL3_Init("#version 150");
#else
	ImGui_ImplOpenGL3_Init("#version 130");
#endif

	// Arrancar directamente con la habitación y la configuración de juego.
	aplicarPresetJuego();
	std::error_code raizActivaError;
	std::cerr << "Directorio de recursos activo: " << std::filesystem::current_path(raizActivaError).string() << std::endl;
	if (!cargarYAplicarEscena("assets/scenes/aula_informatica_calibrada.json") || !crearJugadorFisico())
	{
		std::cerr << "Inicio cancelado: no se inicia PLAY sin escena, suelo y spawn Bullet validados." << std::endl;
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		glfwDestroyWindow(window);
		glfwTerminate();
		return EXIT_FAILURE;
	}
	// Red solo si se pidio por linea de comandos (--host / --connect); por defecto sigue OFFLINE.
	if (configRedPendiente.rol != net::Role::Offline) iniciarRed(configRedPendiente);
	glfwSetWindowTitle(window, "EscapeTheUAB");
	glfwGetFramebufferSize(window, &w, &h);
	OnFramebufferSize(window, w, h);

// =========================================================================
// CONFIGURACIÓ GRÁFICS PS1
// =========================================================================
	const int PS1_WIDTH = 960;
	const int PS1_HEIGHT = 540;

	unsigned int ps1FBO, ps1ColorTex, ps1RBO;
	glGenFramebuffers(1, &ps1FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, ps1FBO);

	// Crear textura de color
	glGenTextures(1, &ps1ColorTex);
	glBindTexture(GL_TEXTURE_2D, ps1ColorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, PS1_WIDTH, PS1_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	// CLAVE PARA PS1: GL_NEAREST evita el suavizado de los píxeles
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ps1ColorTex, 0);

	// Crear Renderbuffer para profundidad (Depth)
	glGenRenderbuffers(1, &ps1RBO);
	glBindRenderbuffer(GL_RENDERBUFFER, ps1RBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, PS1_WIDTH, PS1_HEIGHT);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, ps1RBO);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cerr << "Error en el FBO de PS1\n";
	glBindFramebuffer(GL_FRAMEBUFFER, 0); // Volver al framebuffer por defecto
// =========================================================================
// CONFIGURACIÓ GRÁFICS PS1
// =========================================================================

// =========================================================================
// BUCLE PRINCIPAL Y RENDER PS1
// =========================================================================
// Loop until the user closes the window

	texturaFondoMenu = SOIL_load_OGL_texture(
		"assets/textures/fonde-menu.png",
    	SOIL_LOAD_AUTO,
    	SOIL_CREATE_NEW_ID,
    	0
	);

	if (texturaFondoMenu == 0)
	{
    	std::cerr << "Error cargando fondo_menu.png: " << SOIL_last_result() << "\n";
	}
	else
	{
    	// Opcional: GL_NEAREST mantiene los píxeles nítidos (estilo PS1) al estirarse
    	glBindTexture(GL_TEXTURE_2D, texturaFondoMenu);
   		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
 	   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	while (!glfwWindowShouldClose(window))
	{
		// Entorn VGI. Timer: common part, do this only once
		now = glfwGetTime();
		delta = now - previous;
		previous = now;
		renderMetrics::AddFrame(delta * 1000.0);
		renderMetrics::BeginRender();

		// Entorn VGI. Timer: for each timer do this
		time -= delta;
		if ((time <= 0.0) && (satelit || anima)) OnTimer();

		// Poll for and process events
		glfwPollEvents();

		// 2. Sincronizar la pausa interna del motor (libera/captura el ratón automáticamente)
		fpsPausado = (estadoActual != ESTADO_JUGANDO);

		editorEscenas.Actualizar(window, static_cast<float>(delta));
		sincronizarCursorFPS(window);
		actualizarCargaLanzamiento(window, static_cast<float>(delta));

		// 3. Movimiento, física y VR (solo cuando estamos jugando)
		if (estadoActual == ESTADO_JUGANDO)
		{
			const bool ctrlPulsado = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
				glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
			const float deltaPostura = (editorEscenas.Activo() || (modoJuegoFPS() && !entradaFPSActiva()))
				? 0.0f : fps::LimitarDelta(static_cast<float>(delta));
			mundoFisico.ActualizarJugadorAgachado(modoJuegoFPS() && ctrlPulsado, deltaPostura);

			// Movimiento WASD
			mundoFisico.FijarVelocidadJugador(glm::vec3(0.0f));
			{
				fps::EntradaMovimiento entrada;
				entrada.activo = !editorEscenas.Activo() && entradaFPSActiva() && mundoFisico.HayJugador();
				entrada.quiereMoverse = entrada.activo && hayTeclaDeMovimiento(window);
				entrada.sprintPedido = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
					glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
				entrada.enSuelo = mundoFisico.JugadorEnSuelo();
				entrada.fraccionAgachado = mundoFisico.FraccionAgachado();
				const float velocidad = movimientoJugador.Actualizar(static_cast<float>(delta), entrada);
				if (entrada.activo)
					moverJugadorFPS(window, velocidad);
				barraEstamina.Actualizar(static_cast<float>(delta), movimientoJugador.FraccionEstamina(), movimientoJugador.Sprintando());
			}

			actualizarMovimientoMovil(window, static_cast<float>(delta));

			const bool animacionPausada = netjuego::PausarMundoJuego(
				estadoActual == ESTADO_JUGANDO, editorEscenas.Activo(), fpsPausado,
				ventanaConFoco, gestorOpenXR.SesionEnMarcha(), redJuego.EsHost());
			actualizarPuertaDemo(animacionPausada ? 0.0f : static_cast<float>(delta));
			const auto inicioAnimacion = std::chrono::steady_clock::now();
			glb::ActualizarObjetos(objetosAnimados, animacionPausada ? 0.0f : static_cast<float>(delta));
			if (renderMetrics::enabled) renderMetrics::animacionMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicioAnimacion).count();
			gestorOpenXR.ProcesarEventos();

			if (gestorOpenXR.SesionEnMarcha() && entradaFPSActiva())
			{
				if (gestorOpenXR.GatilloRecienPulsado())
				{
			if (jugadorTieneAgarre()) iniciarCargaLanzamiento();
					else interactuar();
				}
				if (gestorOpenXR.GatilloRecienSoltado() && cargaLanzamientoActiva)
					lanzarObjetoCargado();

				if (gestorOpenXR.GatilloIzquierdoRecienPulsado())
					linternaMovilActiva = !linternaMovilActiva;
			}

			escenaFisica->SincronizarCinematicos(objetosAnimados);
			actualizarAgarreDelJugador(!animacionPausada && !mostrarPanelesDebug && modoJuegoFPS() &&
				(!redJuego.EsHost() || entradaFPSActiva()));
			mundoFisico.FijarPausa(animacionPausada);
			const auto inicioFisica = std::chrono::steady_clock::now();
			mundoFisico.Actualizar(static_cast<float>(delta));
			if (renderMetrics::enabled) renderMetrics::fisicaMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicioFisica).count();
			sincronizarJugadorYCamara();
			actualizarBalanceoVisual(static_cast<float>(delta));
		}

		// Red: los mensajes se aplican aqui (hilo de juego). Offline: no hace nada.
		actualizarRed(glfwGetTime(), estadoActual == ESTADO_JUGANDO);

		// Una sola consulta de Bullet por fotograma; el HUD y E consumen el mismo objetivo.
		actualizarHiriti(estadoActual == ESTADO_JUGANDO &&
			entradaFPSActiva() && !gestorOpenXR.SesionEnMarcha(),
			estadoActual == ESTADO_JUGANDO && !editorEscenas.Activo() && !redJuego.EsCliente(),
			static_cast<float>(delta));
		actualizarAudioAmbiente(estadoActual == ESTADO_JUGANDO && !editorEscenas.Activo(),
			static_cast<float>(delta));
		actualizarObjetivoInteraccion(estadoActual == ESTADO_JUGANDO);

		// La linterna se ancla durante el dibujo, una vez calculada la ViewMatrix actual.
		if (estadoActual != ESTADO_JUGANDO) llumGL[3].encesa = false;
		const auto inicioLuces = std::chrono::steady_clock::now();
		actualizarLucesDeEscena();
		if (renderMetrics::enabled) renderMetrics::lucesMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicioLuces).count();

		draw_Menu_ImGui();
		if (editorEscenas.ConsumeReconstruirFisicaRequest())
		{
			std::string errorEditor;
			if (!reconstruirFisicaTrasEdicion(errorEditor))
			{
				std::cerr << "No se pudo actualizar Bullet tras cambiar instancias GLB: " << errorEditor << '\n';
				editorEscenas.InformarEstado("Physics rebuild failed: " + errorEditor, true);
			}
		}
		if (editorEscenas.ConsumeGuardarRequest())
		{
			std::string errorGuardado;
			if (serializadorEscena.Guardar(objetosEscena, datosAnimadosEscena, errorGuardado))
			{
				editorEscenas.MarcarGuardado();
				editorEscenas.InformarEstado("Saved: " + serializadorEscena.Ruta().filename().string());
				std::cout << "Escena guardada en " << serializadorEscena.Ruta() << '\n';
				if (!serializadorEscena.RutaFuente().empty())
					std::cout << "Copia fuente actualizada en " << serializadorEscena.RutaFuente() << '\n';
				if (editorEscenas.ConsumeCerrarTrasGuardar()) glfwSetWindowShouldClose(window, GLFW_TRUE);
			}
			else
			{
				editorEscenas.InformarEstado("Save failed: " + errorGuardado, true);
				std::cerr << "No se guardó la escena: " << errorGuardado << '\n';
			}
		}
		if (editorEscenas.ConsumeCerrarConfirmado()) glfwSetWindowShouldClose(window, GLFW_TRUE);
		if (editorEscenas.ConsumeEntradaPlay())
		{
			std::string errorEditor;
			if (!reconstruirFisicaTrasEdicion(errorEditor))
			{
				std::cerr << "No se pudo reconstruir Bullet al salir del editor: " << errorEditor << '\n';
				editorEscenas.Activar(true);
			}
			else
			{
				mundoFisico.FijarVelocidadJugador(glm::vec3(0.0f));
				if (redJuego.Activa() && redJuego.EsHost())
				{
					std::vector<netjuego::EstadoObjetoEscena> estados;
					if (!capturarEstadoEscenaRed(estados) || !redJuego.FijarEstadoEscena(estados, true))
						editorEscenas.InformarEstado("La sesión solo replica objetos existentes; no se sincronizaron altas, bajas o IDs no válidos.", true);
				}
			}
			// Sin saltos: se descartan el ultimo cursor, los deltas del raton y la referencia del balanceo.
			fpsTienePosicionAnterior = false;
			mouseDeltaX = mouseDeltaY = 0.0;
			hayPiesFotogramaAnterior = false;
			sincronizarCursorFPS(window);
		}

		// Con VR en marcha, la escena se dibuja en el visor (y se refleja en la ventana). Sin VR, o si el visor
		// no está mostrando imagen, se dibuja la vista de escritorio con normalidad.
		{
			static bool vrEnMarchaAntes = false;
			const bool vrEnMarcha = gestorOpenXR.SesionEnMarcha();
			if (vrEnMarcha != vrEnMarchaAntes)
			{	// xrWaitFrame ya marca el ritmo del visor: la sincronía vertical del monitor lo frenaría
				glfwSwapInterval(vrEnMarcha ? 0 : 1);
				vrEnMarchaAntes = vrEnMarcha;
			}

			const bool vrDibujado = !editorEscenas.Activo() && vrEnMarcha && dibujarEscenaVR();
			if (!vrDibujado)
			{
				// Guardamos el tamaño real de la ventana antes de cambiar w y h para el FBO PS1
				int winW = 0, winH = 0;
				glfwGetFramebufferSize(window, &winW, &winH);

				// EDIT: resolución nativa.
				// PLAY: resolución PS1 (320x240).
				const bool modoEditor = editorEscenas.Activo();

				glBindFramebuffer(
					GL_FRAMEBUFFER,
					modoEditor ? 0 : ps1FBO
				);

				w = modoEditor ? winW : PS1_WIDTH;
				h = modoEditor ? winH : PS1_HEIGHT;

				glViewport(0, 0, w, h);
				OnPaint(window);

				// Solo aplicar el escalado pixelado durante PLAY.
				if (!modoEditor)
				{
					glBindFramebuffer(GL_READ_FRAMEBUFFER, ps1FBO);
					glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

					glBlitFramebuffer(
						0, 0, PS1_WIDTH, PS1_HEIGHT,
						0, 0, winW, winH,
						GL_COLOR_BUFFER_BIT,
						GL_NEAREST
					);
				}

				// Restaurar framebuffer y dimensiones reales.
				glBindFramebuffer(GL_FRAMEBUFFER, 0);

				w = winW;
				h = winH;

				glViewport(0, 0, winW, winH);

			}

			// Renderizado final de ImGui e intercambio de buffers
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			glfwSwapBuffers(window);
		}
	}
// =========================================================================
// BUCLE PRINCIPAL Y RENDER PS1
// =========================================================================

	// Una salida por el boton de Windows tambien devuelve el cursor al sistema.
	fijarCapturaCursor(window, false);
	if (texturaManoInteraccion != 0)
	{
		glDeleteTextures(1, &texturaManoInteraccion);
		texturaManoInteraccion = 0;
	}

// =========================================================================
// RESET RECURSOS PS1
// =========================================================================
	glDeleteFramebuffers(1, &ps1FBO);
	if (texturaFondoMenu != 0)
{
    glDeleteTextures(1, &texturaFondoMenu);
    texturaFondoMenu = 0;
}
	glDeleteTextures(1, &ps1ColorTex);
	glDeleteRenderbuffers(1, &ps1RBO);
// =========================================================================
// RESET RECURSOS PS1
// =========================================================================

// Entorn VGI.ImGui: Cleanup ImGui
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	redJuego.Detener();           // cierra la red (si la hay) antes que la ventana
	gestorOpenXR.Desactivar();   // sesión e instancia OpenXR (si las hay) antes de cerrar la ventana
	NFD_QuitCompat();

	for (auto& entrada : modelosHabitacion)
		entrada.second->netejaTextures_OBJ();

	modelosHabitacion.clear();
	objetosEscena.clear();
	liberarObjetosAnimados();
	mundoFisico.EliminarJugador();
	escenaFisica.reset();                       // quita las colisiones de la habitacion antes de cerrar
	fisica::LiberarDibujoColisionadores();
	netjuego::LiberarDibujoAvatares();

	if (phoneOBJ != nullptr)
	{
		phoneOBJ->netejaVAOList_OBJ();
		phoneOBJ->netejaTextures_OBJ();
		delete phoneOBJ;
		phoneOBJ = nullptr;
	}

	if (Get_VAOId(CUBE_SKYBOX) != 0) deleteVAOList(CUBE_SKYBOX);
	netejaVAOList();
	if (eixos_Id != 0) glDeleteVertexArrays(1, &eixos_Id);
	if (rayoVao != 0) glDeleteVertexArrays(1, &rayoVao);
	if (rayoVbo != 0) glDeleteBuffers(1, &rayoVbo);
	for (int textureIndex = 0; textureIndex < NUM_MAX_TEXTURES; ++textureIndex)
	{
		if (texturesID[textureIndex] != 0)
		{
			glDeleteTextures(1, &texturesID[textureIndex]);
			texturesID[textureIndex] = 0;
		}
	}
	if (cubemapTexture != 0)
	{
		glDeleteTextures(1, &cubemapTexture);
		cubemapTexture = 0;
	}

	// Delete GPU programs while the OpenGL context that created them is current.
	glcache::InvalidateAll(); oclusoresPhongInvalidos = true;
	if (consultasGpu[0] != 0) glDeleteQueries(3, consultasGpu);
	if (shaderLighting.getProgramID() != -1) shaderLighting.DeleteProgram();
	if (shaderSkyBox.getProgramID() != -1) shaderSkyBox.DeleteProgram();
	if (shaderEixos.getProgramID() != -1) shaderEixos.DeleteProgram();
	if (shader_SkyBoxC.getProgramID() != -1) shader_SkyBoxC.DeleteProgram();

	glfwDestroyWindow(window);

// Terminating GLFW: Destroy the windows, monitors and cursor objects
	glfwTerminate();

	return 0;
}
