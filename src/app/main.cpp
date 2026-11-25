//******** PRACTICA VISUALITZACIÓ GRÀFICA INTERACTIVA (Escola Enginyeria - UAB)
//******** Entorn bàsic VS2026 MONOFINESTRA amb OpenGL 4.6, interfície GLFW 3.4, ImGui i llibreries GLM
//******** Ferran Poveda, Marc Vivet, Carme Julià, Débora Gil, Enric Martí (Setembre 2026)
// main.cpp : Este archivo contiene la función "main". La ejecución del programa comienza y termina ahí.
//    Versió 0.5:	- Interficie ImGui
//					- Per a dialeg de cerca de fitxers, s'utilitza la llibreria NativeFileDialog
//

// Entorn VGI.ImGui: Includes llibreria ImGui

#include "src/app/AppState.h"
#include "src/app/DemoDoor.h"
#include "src/app/WindowCallbacks.h"
#include "src/app/WorkingDirectory.h"
#include "src/app/adapters/GraphicsAdapters.h"
#include "src/app/adapters/HirityAdapters.h"
#include "src/app/adapters/InteractionAdapters.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/app/adapters/RenderAdapters.h"
#include "src/app/adapters/SceneAdapters.h"
#include "src/app/adapters/SessionAdapters.h"
#include "src/app/input/CursorCapture.h"
#include "src/app/input/KeyboardInput.h"
#include "src/app/input/MouseInput.h"
#include "src/legacy_vgi/VgiTimer.h"
#include "src/rendering/MaterialTextures.h"
#include "src/rendering/SceneLighting.h"
#include "src/ui/InteractionHudTexture.h"
#include "src/ui/UiFrame.h"

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
