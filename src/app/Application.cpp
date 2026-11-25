#include "src/app/AppState.h"
#include "src/app/Application.h"
#include "src/app/WindowCallbacks.h"
#include "src/app/adapters/GraphicsAdapters.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/app/adapters/RenderAdapters.h"
#include "src/app/adapters/SceneAdapters.h"
#include "src/app/adapters/SessionAdapters.h"
#include "src/app/input/KeyboardInput.h"
#include "src/app/input/MouseInput.h"
#include "src/rendering/MaterialTextures.h"
#include "src/ui/InteractionHudTexture.h"

void ParseNetworkArguments(int argc, char** argv)
{
	{	// Red: --host [--port N] | --connect HOST[:PUERTO] [--name N]. Sin argumentos: OFFLINE.
		std::string errorRed;
		if (!netgame::ParseArguments(argc, argv, pendingNetworkConfig, errorRed))
		{
			std::cerr << "Argumentos de red ignorados: " << errorRed << std::endl;
			pendingNetworkConfig = netgame::NetworkConfig{};
		}
		else if (pendingNetworkConfig.rol == net::Role::Client)
		{
			std::string errorGuardado;
			if (!netgame::SaveLastEndpoint({ pendingNetworkConfig.host, pendingNetworkConfig.puerto }, errorGuardado))
				std::cerr << "Red: no se guardo la ultima direccion: " << errorGuardado << std::endl;
		}
	}
}

bool CreateMainWindow()
{
// glfw: initialize and configure
// ------------------------------
	if (!glfwInit())
	{	fprintf(stderr, "Failed to initialize GLFW\n");
		getchar();
		return false;
	}

	primary = glfwGetPrimaryMonitor();
	if (primary == nullptr)
	{
		fprintf(stderr, "Failed to get the primary monitor for fullscreen mode\n");
		glfwTerminate();
		return false;
	}

	mode = glfwGetVideoMode(primary);
	if (mode == nullptr)
	{
		fprintf(stderr, "Failed to get the primary monitor video mode\n");
		glfwTerminate();
		return false;
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
		return false;
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
		return false;
		}

	const bool nfdAvailable = NFD_InitCompat();
	if (!nfdAvailable)
		std::fprintf(stderr, "Failed to initialize Native File Dialog: %s\n", NFD_GetError());

	GLint major = 0, minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	mainWindow = window;
	glMajorVersion = major;
	glMinorVersion = minor;
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

	return true;
}

void InitializeGraphicsResources()
{
// Initialize Application control varibles
	InitGL();
	loadInteractionHandTexture();

	char phonePath[] = "./assets/models/Phone.obj";
	phoneOBJ = new COBJModel();
	if (phoneOBJ->LoadModel(phonePath) != 0)
	{
		std::cerr << "No se pudo cargar Phone.obj. Comprueba la ruta y el MTL.\n";
		delete phoneOBJ;
		phoneOBJ = nullptr;
	}
}

void RegisterWindowCallbacks()
{
// ------------- Entorn VGI: Callbacks
	glfwSetWindowSizeCallback(window, OnSize);										// - Windows Size in screen Coordinates
	glfwSetFramebufferSizeCallback(window, OnFramebufferSize);							// - Medidas del framebuffer en píxeles
	glfwSetWindowFocusCallback(window, OnWindowFocus);
	glfwSetWindowCloseCallback(window, OnWindowCloseRequest);
	sceneEditor.SetModelLoader(loadEditorModel);
	sceneEditor.SetTextureLoader(editorTexture);
	sceneEditor.SetDoorLoader([](const std::filesystem::path& ruta, const SceneAnimatedObject& datos,
		editor::EditorSceneContext contexto, std::string& error)
	{
		std::error_code ec;
		const auto absoluta = std::filesystem::absolute(ruta, ec).lexically_normal();
		if (ec || !std::filesystem::is_regular_file(absoluta, ec)) { error = "El GLB seleccionado no existe."; return false; }
		const std::string clave = absoluta.string();
		const bool modeloNuevo = !contexto.escenaGlb.modelos.contains(clave);
		if (!glb::AddAnimatedObject(contexto.escenaGlb, datos.id, clave, datos.posicion, datos.giroZGrados, error)) return false;
		glb::AnimatedObject* instancia = glb::FindAnimatedObject(contexto.escenaGlb, datos.id);
		if (instancia == nullptr) { error = "No se pudo crear la instancia GLB."; return false; }
		instancia->escala = datos.escala;
		if (!datos.animation.empty() && !glb::SelectAnimation(*instancia, datos.animation, error)) return false;
		if (modeloNuevo) glb::UploadModelToGpu(contexto.escenaGlb.modelos.at(clave));
		return true;
	});
	glfwSetMouseButtonCallback(window, (GLFWmousebuttonfun)OnMouseButton);			// - Directly redirect GLFW mouse button events
	glfwSetCursorPosCallback(window, (GLFWcursorposfun)OnMouseMove);				// - Directly redirect GLFW mouse position events
	glfwSetScrollCallback(window, (GLFWscrollfun)OnMouseWheel);						// - Directly redirect GLFW mouse wheel events
	glfwSetKeyCallback(window, (GLFWkeyfun)OnKeyDown);								// - Directly redirect GLFW key events
	glfwSetErrorCallback(error_callback);											// Error callback
	glfwSetWindowRefreshCallback(window, (GLFWwindowrefreshfun)OnPaint);			// - Callback to refresh the screen
}

void InitializeImGui()
{
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
}

bool StartGame()
{
	// Arrancar directamente con la habitación y la configuración de juego.
	applyStartupPreset();
	std::error_code raizActivaError;
	std::cerr << "Directorio de recursos activo: " << std::filesystem::current_path(raizActivaError).string() << std::endl;
	if (!loadAndApplyScene("assets/scenes/aula_informatica_calibrada.json") || !spawnPhysicsPlayer())
	{
		std::cerr << "Inicio cancelado: no se inicia PLAY sin escena, suelo y spawn Bullet validados." << std::endl;
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		glfwDestroyWindow(window);
		glfwTerminate();
		return false;
	}
	// Red solo si se pidio por linea de comandos (--host / --connect); por defecto sigue OFFLINE.
	if (pendingNetworkConfig.rol != net::Role::Offline) startNetwork(pendingNetworkConfig);
	glfwSetWindowTitle(window, "EscapeTheUAB");
	glfwGetFramebufferSize(window, &w, &h);
	OnFramebufferSize(window, w, h);

	return true;
}

void LoadMenuBackground()
{
	menuBackgroundTexture = SOIL_load_OGL_texture(
		"assets/textures/fonde-menu.png",
    	SOIL_LOAD_AUTO,
    	SOIL_CREATE_NEW_ID,
    	0
	);

	if (menuBackgroundTexture == 0)
	{
    	std::cerr << "Error cargando fondo_menu.png: " << SOIL_last_result() << "\n";
	}
	else
	{
    	// Opcional: GL_NEAREST mantiene los píxeles nítidos (estilo PS1) al estirarse
    	glBindTexture(GL_TEXTURE_2D, menuBackgroundTexture);
   		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
 	   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
}
