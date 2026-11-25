// Game state definitions. One translation unit keeps the original construction order.
#include "src/app/AppState.h"

// ---------------------------

// Objetos de la escena cargada.
std::vector<SceneObject> sceneObjects;

std::unordered_map<std::string, std::unique_ptr<COBJModel>> roomModels;

editor::SceneEditor sceneEditor;

editor::SceneSerializer sceneSerializer;

std::vector<SceneAnimatedObject> sceneAnimatedData;

COBJModel* phoneOBJ = nullptr;

std::unordered_map<std::string, GLuint> sceneMaterialTextures;

// F2: vista estereoscópica DE PRUEBA en pantalla (dos mitades). No es VR real: no hay OpenXR ni visor.
bool stereoTestMode = false;

// OpenXR (VR real): SOLO se inicia cuando el usuario lo pide (F3 o el botón del panel de física).
// No tiene nada que ver con F2. Si no hay runtime o visor, el juego sigue en pantalla.
vr::OpenXrManager openXrManager;

GLFWwindow* mainWindow = nullptr;   // la necesita OpenXR para enlazar su sesión con el contexto OpenGL

int glMajorVersion = 0, glMinorVersion = 0;

bool fpsPaused = false;

bool fpsCaptureRequested = true;

bool fpsCursorCaptured = false;

bool fpsHasPreviousPosition = false;

bool windowFocused = true;

bool showDebugPanels = false;

bool showFpsCounter = false;

bool optimizedRendering = true;

bool phongOccludersDirty = true;

GLuint gpuQueries[3]{};

bool gpuQueryPending[3]{};

int currentGpuQuery = 0;

double lastGpuMs = 0.0;

bool gpuQueryActive = false;

int lightingDiagnosticMode = 0;

double fpsLastX = 0.0;

double fpsLastY = 0.0;

bool showPhone = true;

glm::vec3 phonePosition(-0.30f, -0.28f, -0.65f);

glm::vec3 phoneRotation(90.0f, 0.0f, -12.0f);

float phoneScale = 0.09f;

float idleSwayIntensity = 0.002f;

float walkSwayIntensity = 0.018f;

float mouseSwayIntensity = 0.0008f;

float walkWeight = 0.0f;

float walkPhase = 0.0f;

float mouseSwayX = 0.0f;

float mouseSwayY = 0.0f;

double mouseDeltaX = 0.0;

double mouseDeltaY = 0.0;

float fpsTime = 0.0f;

float deltaFPS = 0.0f;

AudioManager audioManager;

float masterVolumePercent = 100.0f;

game::HiritiAnimator hiritiAnimator;

game::HiritiAudioTimer hiritiAudioTimer;

// --- Movimiento FPS: sprint, estamina y balanceo visual (MovimientoFPS.h) ---
fps::PlayerMovement playerMovement;

fps::CameraBob cameraBob;

fps::StaminaBar staminaBar;

bool showStaminaPercent = true;

glm::vec3 previousFrameFeet(0.0f);

bool hasPreviousFrameFeet = false;

// --- DEMO FASE 1: puerta que se abre girando sobre su bisagra ---
// Keyframes: en 0 s la puerta está cerrada (0 grados), en 2 s abierta (90 grados).
// Para cambiar la duración o el ángulo basta con editar esta lista.
std::vector<animation::AngleKeyframe> demoDoorKeyframes = { {0.0f, 0.0f}, {2.0f, 90.0f} };

animation::Reproductor demoDoorPlayback;

float demoDoorAngle = 0.0f;                              // ángulo actual (se recalcula cada fotograma)

extern const glm::vec3 demoDoorHalfSize(0.05f, 0.5f, 1.0f);   // puerta de 1 m de ancho, 10 cm de grosor y 2 m de alto

extern const glm::vec3 demoDoorHinge(6.0f, -0.5f, -1.7f);      // punto del mundo (a ras de suelo)

// --- OBJETOS ANIMADOS GLB: las animaciones vienen de Blender (assets/animations/*.glb) ---
// No hay keyframes ni puertas escritas aquí: la lista "animated" del JSON de la habitación
// declara cada instancia (puerta, cajón...) y cargarYAplicarEscena() las crea.
// Varias instancias pueden usar el mismo .glb y se abren de forma independiente.
glb::AnimatedScene animatedObjects;

// --- FÍSICA (Bullet, Fase 1): cubo que cae sobre un suelo estático. Ver MundoFisico.h ---
// UN solo mundo físico para toda la partida. La habitación activa y la demo ponen cuerpos
// en él y cada una quita solo los suyos. No depende de GLFW ni de la cámara (válido para VR).
fisica::PhysicsWorld physicsWorld;

std::unique_ptr<fisica::PhysicsScene> physicsScene = std::make_unique<fisica::PhysicsScene>(physicsWorld);

fisica::FallingCubeDemo fallingCubeDemo(physicsWorld);

bool showColliders = false;   // opción de depuración (panel "Fisica (Bullet)")

bool hiritiFollowPlayerEnabled = false;

float hiritiFollowStopDistance = 1.8f;

float hiritiFollowMaxSpeed = 2.4f;

bool animatedObjectsReady = false;   // true cuando los modelos ya están en la GPU

GLuint interactionHandTexture = 0;

bool throwChargeActive = false;

float throwChargeTime = 0.0f;

// ÚNICO sitio que conoce la cámara FPS: convierte "lo que ve el jugador" en una posición y una
// dirección del mundo. Todo el sistema de interacción (puertas y agarre) solo recibe esos dos
// vectores; con VR se llamaría igual, pero con los de la mano o el mando.

bool phoneFlashlightEnabled = true;

bool labLampsEnabled = true;

// --- Multijugador (OFFLINE por defecto: no hay hilo, ni socket, ni ENet hasta pulsar Iniciar o usar --host/--connect) ---
netgame::NetworkGame networkGame;

// El panel puede cerrarse y recuperarse con F4, sin depender de los paneles F1.
bool showNetworkPanel = true;

std::unique_ptr<netgame::GlbDoors> networkDoors;       // adaptador de las puertas GLB (IDs de escena)

netgame::NetworkConfig pendingNetworkConfig;                 // lo que editan el menu y la linea de comandos

netgame::session::SessionRuntimeState networkSessionState;  // estado propio de src/networking/session

vr::XrPose lastVrHead;                              // pose real del visor, la rellena el render VR

bool hasVrHead = false;

InteractionTarget currentInteractionTarget;

std::string activeSceneName;

SceneSpawn activeSceneSpawn;

float sceneIndirectIntensity = 0.16f;

float sceneBaseAmbientIntensity = 0.025f;

float sceneDirectLightScale = 1.0f;

glm::vec3 sceneGroundBounceColor{ 0.42f, 0.46f, 0.43f };

glm::vec3 sceneSkyBounceColor{ 0.24f, 0.28f, 0.34f };

bool sceneHasValidGeometry = false;

GLuint rayVao = 0, rayVbo = 0;

/* ------------------------------------------------------------------------- */
/*                            MENUS ImGui                                    */
/* ------------------------------------------------------------------------- */

// Entorn VGI: Dibuixa el menú principal ImGui que controla colors i permet activar altres menús:
//		- menú standard Imgui [show_demo_window()]
//		- menú propi de l'aplicació EntornVGI [show_EntornVGI_window()]

GLuint menuBackgroundTexture = 0;

EstadoJuego currentGameState = ESTADO_MENU_PRINCIPAL;
