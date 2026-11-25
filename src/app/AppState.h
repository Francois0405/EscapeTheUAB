#pragma once

// Game state shared by the application modules. Definitions live in AppState.cpp.
#include "src/app/AppIncludes.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using game::interaction::InteractionAction;
using game::interaction::InteractionTarget;

extern std::vector<SceneObject> objetosEscena;
extern std::unordered_map<std::string, std::unique_ptr<COBJModel>> modelosHabitacion;
extern editor::SceneEditor editorEscenas;
extern editor::SceneSerializer serializadorEscena;
extern std::vector<SceneAnimatedObject> datosAnimadosEscena;
extern COBJModel* phoneOBJ;
extern std::unordered_map<std::string, GLuint> texturasMaterialEscena;
extern bool modoEstereoPrueba;
extern vr::OpenXrManager gestorOpenXR;
extern GLFWwindow* ventanaPrincipal;
extern int versionGLMayor, versionGLMenor;
extern bool fpsPausado;
extern bool fpsCapturaSolicitada;
extern bool fpsCursorCapturado;
extern bool fpsTienePosicionAnterior;
extern bool ventanaConFoco;
extern bool mostrarPanelesDebug;
extern bool mostrarContadorFPS;
extern bool renderizadoOptimizado;
extern bool oclusoresPhongInvalidos;
extern GLuint consultasGpu[3];
extern bool consultaGpuPendiente[3];
extern int consultaGpuActual;
extern double ultimoGpuMs;
extern bool consultaGpuActiva;
extern int modoDiagnosticoIluminacion;
extern double fpsUltimoX;
extern double fpsUltimoY;
extern bool mostrarMovil;
extern glm::vec3 movilPosicion;
extern glm::vec3 movilRotacion;
extern float movilEscala;
extern float intensidadIdle;
extern float intensidadWalking;
extern float intensidadMouse;
extern float pesoCaminata;
extern float faseCaminata;
extern float mouseSwayX;
extern float mouseSwayY;
extern double mouseDeltaX;
extern double mouseDeltaY;
extern float tiempoFPS;
extern float deltaFPS;
extern AudioManager audioManager;
extern float volumenGeneralPorcentaje;
extern game::HiritiAnimator animacionHiriti;
extern game::HiritiAudioTimer temporizadorAudioHiriti;
extern fps::PlayerMovement movimientoJugador;
extern fps::CameraBob balanceoCamara;
extern fps::StaminaBar barraEstamina;
extern bool mostrarPorcentajeEstamina;
extern glm::vec3 piesFotogramaAnterior;
extern bool hayPiesFotogramaAnterior;
extern std::vector<animation::AngleKeyframe> keyframesPuertaDemo;
extern animation::Reproductor reproductorPuertaDemo;
extern float anguloPuertaDemo;
extern const glm::vec3 mitadTamanoPuertaDemo;
extern const glm::vec3 bisagraPuertaDemo;
extern glb::AnimatedScene objetosAnimados;
extern fisica::PhysicsWorld mundoFisico;
extern std::unique_ptr<fisica::PhysicsScene> escenaFisica;
extern fisica::FallingCubeDemo demoFisica;
extern bool mostrarColisionadores;
extern bool hiritiFollowPlayerEnabled;
extern float hiritiFollowStopDistance;
extern float hiritiFollowMaxSpeed;
extern bool objetosAnimadosListos;
extern GLuint texturaManoInteraccion;
extern bool cargaLanzamientoActiva;
extern float tiempoCargaLanzamiento;
extern bool linternaMovilActiva;
extern bool lamparasLaboratorioActivas;
extern netgame::NetworkGame redJuego;
extern bool mostrarPanelRed;
extern std::unique_ptr<netgame::GlbDoors> puertasRed;
extern netgame::NetworkConfig configRedPendiente;
extern netgame::session::SessionRuntimeState estadoSesionRed;
extern vr::XrPose ultimaCabezaVR;
extern bool hayCabezaVR;
extern InteractionTarget objetivoInteraccionActual;
extern std::string nombreEscenaActiva;
extern SceneSpawn spawnEscenaActiva;
extern float intensidadIndirectaEscena;
extern float intensidadAmbientalBaseEscena;
extern float escalaLuzDirectaEscena;
extern glm::vec3 colorReboteSueloEscena;
extern glm::vec3 colorReboteCieloEscena;
extern bool escenaTieneGeometriaValida;
extern GLuint rayoVao, rayoVbo;
extern GLuint texturaFondoMenu;
extern EstadoJuego estadoActual;

inline constexpr float TiempoMaximoCargaLanzamiento = game::interaction::kMaxThrowChargeSeconds;
