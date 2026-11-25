// Game state definitions. One translation unit keeps the original construction order.
#include "src/app/AppState.h"

// ---------------------------

// Objetos de la escena cargada.
std::vector<SceneObject> objetosEscena;

std::unordered_map<std::string, std::unique_ptr<COBJModel>> modelosHabitacion;

editor::SceneEditor editorEscenas;

editor::SceneSerializer serializadorEscena;

std::vector<SceneAnimatedObject> datosAnimadosEscena;

COBJModel* phoneOBJ = nullptr;

std::unordered_map<std::string, GLuint> texturasMaterialEscena;

// F2: vista estereoscópica DE PRUEBA en pantalla (dos mitades). No es VR real: no hay OpenXR ni visor.
bool modoEstereoPrueba = false;

// OpenXR (VR real): SOLO se inicia cuando el usuario lo pide (F3 o el botón del panel de física).
// No tiene nada que ver con F2. Si no hay runtime o visor, el juego sigue en pantalla.
vr::OpenXrManager gestorOpenXR;

GLFWwindow* ventanaPrincipal = nullptr;   // la necesita OpenXR para enlazar su sesión con el contexto OpenGL

int versionGLMayor = 0, versionGLMenor = 0;

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

game::HiritiAnimator animacionHiriti;

game::HiritiAudioTimer temporizadorAudioHiriti;

// --- Movimiento FPS: sprint, estamina y balanceo visual (MovimientoFPS.h) ---
fps::PlayerMovement movimientoJugador;

fps::CameraBob balanceoCamara;

fps::StaminaBar barraEstamina;

bool mostrarPorcentajeEstamina = true;

glm::vec3 piesFotogramaAnterior(0.0f);

bool hayPiesFotogramaAnterior = false;

// --- DEMO FASE 1: puerta que se abre girando sobre su bisagra ---
// Keyframes: en 0 s la puerta está cerrada (0 grados), en 2 s abierta (90 grados).
// Para cambiar la duración o el ángulo basta con editar esta lista.
std::vector<animation::AngleKeyframe> keyframesPuertaDemo = { {0.0f, 0.0f}, {2.0f, 90.0f} };

animation::Reproductor reproductorPuertaDemo;

float anguloPuertaDemo = 0.0f;                              // ángulo actual (se recalcula cada fotograma)

extern const glm::vec3 mitadTamanoPuertaDemo(0.05f, 0.5f, 1.0f);   // puerta de 1 m de ancho, 10 cm de grosor y 2 m de alto

extern const glm::vec3 bisagraPuertaDemo(6.0f, -0.5f, -1.7f);      // punto del mundo (a ras de suelo)

// --- OBJETOS ANIMADOS GLB: las animaciones vienen de Blender (assets/animations/*.glb) ---
// No hay keyframes ni puertas escritas aquí: la lista "animated" del JSON de la habitación
// declara cada instancia (puerta, cajón...) y cargarYAplicarEscena() las crea.
// Varias instancias pueden usar el mismo .glb y se abren de forma independiente.
glb::AnimatedScene objetosAnimados;

// --- FÍSICA (Bullet, Fase 1): cubo que cae sobre un suelo estático. Ver MundoFisico.h ---
// UN solo mundo físico para toda la partida. La habitación activa y la demo ponen cuerpos
// en él y cada una quita solo los suyos. No depende de GLFW ni de la cámara (válido para VR).
fisica::PhysicsWorld mundoFisico;

std::unique_ptr<fisica::PhysicsScene> escenaFisica = std::make_unique<fisica::PhysicsScene>(mundoFisico);

fisica::FallingCubeDemo demoFisica(mundoFisico);

bool mostrarColisionadores = false;   // opción de depuración (panel "Fisica (Bullet)")

bool hiritiFollowPlayerEnabled = false;

float hiritiFollowStopDistance = 1.8f;

float hiritiFollowMaxSpeed = 2.4f;

bool objetosAnimadosListos = false;   // true cuando los modelos ya están en la GPU

GLuint texturaManoInteraccion = 0;

bool cargaLanzamientoActiva = false;

float tiempoCargaLanzamiento = 0.0f;

// ÚNICO sitio que conoce la cámara FPS: convierte "lo que ve el jugador" en una posición y una
// dirección del mundo. Todo el sistema de interacción (puertas y agarre) solo recibe esos dos
// vectores; con VR se llamaría igual, pero con los de la mano o el mando.

bool linternaMovilActiva = true;

bool lamparasLaboratorioActivas = true;

// --- Multijugador (OFFLINE por defecto: no hay hilo, ni socket, ni ENet hasta pulsar Iniciar o usar --host/--connect) ---
netgame::NetworkGame redJuego;

// El panel puede cerrarse y recuperarse con F4, sin depender de los paneles F1.
bool mostrarPanelRed = true;

std::unique_ptr<netgame::GlbDoors> puertasRed;       // adaptador de las puertas GLB (IDs de escena)

netgame::NetworkConfig configRedPendiente;                 // lo que editan el menu y la linea de comandos

netgame::session::SessionRuntimeState estadoSesionRed;  // estado propio de src/networking/session

vr::XrPose ultimaCabezaVR;                              // pose real del visor, la rellena el render VR

bool hayCabezaVR = false;

InteractionTarget objetivoInteraccionActual;

std::string nombreEscenaActiva;

SceneSpawn spawnEscenaActiva;

float intensidadIndirectaEscena = 0.16f;

float intensidadAmbientalBaseEscena = 0.025f;

float escalaLuzDirectaEscena = 1.0f;

glm::vec3 colorReboteSueloEscena{ 0.42f, 0.46f, 0.43f };

glm::vec3 colorReboteCieloEscena{ 0.24f, 0.28f, 0.34f };

bool escenaTieneGeometriaValida = false;

GLuint rayoVao = 0, rayoVbo = 0;

/* ------------------------------------------------------------------------- */
/*                            MENUS ImGui                                    */
/* ------------------------------------------------------------------------- */

// Entorn VGI: Dibuixa el menú principal ImGui que controla colors i permet activar altres menús:
//		- menú standard Imgui [show_demo_window()]
//		- menú propi de l'aplicació EntornVGI [show_EntornVGI_window()]

GLuint texturaFondoMenu = 0;

EstadoJuego estadoActual = ESTADO_MENU_PRINCIPAL;
