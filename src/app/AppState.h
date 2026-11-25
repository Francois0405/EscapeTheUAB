#pragma once

// Game state shared by the application modules. Definitions live in AppState.cpp.
#include "src/app/AppIncludes.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using juego::interaction::AccionInteraccion;
using juego::interaction::ObjetivoInteraccion;

extern std::vector<SceneObject> objetosEscena;
extern std::unordered_map<std::string, std::unique_ptr<COBJModel>> modelosHabitacion;
extern editor::EditorEscenas editorEscenas;
extern editor::SerializadorEscenas serializadorEscena;
extern std::vector<SceneObjetoAnimado> datosAnimadosEscena;
extern COBJModel* phoneOBJ;
extern std::unordered_map<std::string, GLuint> texturasMaterialEscena;
extern bool modoEstereoPrueba;
extern vr::GestorOpenXR gestorOpenXR;
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
extern juego::AnimacionHiriti animacionHiriti;
extern juego::TemporizadorAudioHiriti temporizadorAudioHiriti;
extern fps::MovimientoJugador movimientoJugador;
extern fps::BalanceoCamara balanceoCamara;
extern fps::BarraEstamina barraEstamina;
extern bool mostrarPorcentajeEstamina;
extern glm::vec3 piesFotogramaAnterior;
extern bool hayPiesFotogramaAnterior;
extern std::vector<animacion::KeyframeAngulo> keyframesPuertaDemo;
extern animacion::Reproductor reproductorPuertaDemo;
extern float anguloPuertaDemo;
extern const glm::vec3 mitadTamanoPuertaDemo;
extern const glm::vec3 bisagraPuertaDemo;
extern glb::EscenaAnimada objetosAnimados;
extern fisica::MundoFisico mundoFisico;
extern std::unique_ptr<fisica::EscenaFisica> escenaFisica;
extern fisica::DemoCuboCaida demoFisica;
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
extern netjuego::RedJuego redJuego;
extern bool mostrarPanelRed;
extern std::unique_ptr<netjuego::PuertasGLB> puertasRed;
extern netjuego::ConfigRed configRedPendiente;
extern netjuego::session::SessionRuntimeState estadoSesionRed;
extern vr::PoseXR ultimaCabezaVR;
extern bool hayCabezaVR;
extern ObjetivoInteraccion objetivoInteraccionActual;
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

inline constexpr float TiempoMaximoCargaLanzamiento = juego::interaction::kMaxThrowChargeSeconds;
