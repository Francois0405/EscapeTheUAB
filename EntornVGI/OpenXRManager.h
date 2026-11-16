// OpenXRManager.h
// -----------------------------------------------------------------------------
// Base OpenXR (VR real) de EscapeTheUAB. Parte del trabajo de Hiroo (rama VR), reorganizado para
// que el juego normal NO dependa de OpenXR.
//
// QUÉ HACE HOY (y qué no):
//   Etapa A  Crear la instancia OpenXR, comprobar que hay runtime y visor, y que el runtime ofrece
//            OpenGL y una versión compatible.                         (todas las plataformas con OpenXR)
//   Etapa B  Crear la sesión enlazada al contexto OpenGL de la ventana y atender los eventos de estado
//            de la sesión.                                            (solo Windows, WGL)
//   Etapa C  Espacio de referencia (STAGE o LOCAL), configuración de las vistas, swapchains OpenGL con su
//            framebuffer, estados de sesión (READY/FOCUSED/STOPPING/pérdida), xrBeginSession/xrEndSession y el
//            bucle xrWaitFrame/xrBeginFrame/xrLocateViews/xrEndFrame con una capa de proyección.
//                                                                     (solo Windows; falta PROBARLA con un visor real)
//   La matemática de la vista (OpenXR -> motor, Z arriba) está aparte, en VistaVR.h, y tiene tests sin hardware.
//   No hay manos, mandos, agarre con mandos ni locomoción VR todavía: solo imagen y seguimiento de cabeza.
//
// REGLAS DE DISEÑO
//   * OpenXR solo se inicia cuando el usuario lo pide (F3 o el botón del panel). Nunca al arrancar.
//   * Si no hay runtime, visor o soporte, el juego sigue en modo pantalla; solo se explica por qué.
//   * F2 (vista estereoscópica de prueba, VistaEstereo.h) es otra cosa: no usa OpenXR.
//   * Este módulo no toca la física: la pose del visor y la del cuerpo del jugador son datos distintos.
//   * Con ENTORNVGI_HAS_OPENXR sin definir (macOS por defecto, o sin la biblioteca) la clase existe
//     pero Activar() solo informa de que no está compilado.
// -----------------------------------------------------------------------------
#pragma once

#include "VistaVR.h"

#include <memory>
#include <string>

struct GLFWwindow;

namespace vr
{
	enum class EstadoOpenXR
	{
		Desactivado,             // aún no se ha pedido (estado normal al arrancar)
		NoCompilado,             // esta compilación no incluye OpenXR
		SinRuntime,              // no hay runtime OpenXR activo (Quest Link / SteamVR / Oculus no instalado o cerrado)
		SinVisor,                // hay runtime pero ningún visor conectado
		FaltaExtensionOpenGL,    // el runtime no ofrece XR_KHR_opengl_enable
		OpenGLInsuficiente,      // la versión de OpenGL de la ventana no llega al mínimo del runtime
		InstanciaYSistemaListos, // instancia y visor correctos, pero esta plataforma no crea sesión (solo Windows)
		SesionCreada,            // sesión creada, con swapchains y espacio listos (se empieza a dibujar al llegar a READY)
		Error                    // cualquier otro fallo (ver Mensaje())
	};

	const char* NombreDelEstado(EstadoOpenXR estado);

	// Modo de dibujo del juego. Solo es VR si se ha pedido Y la sesión existe; en cualquier otro caso se
	// sigue jugando en pantalla (F2 no cuenta: es una prueba en pantalla).
	enum class ModoDeJuego { Pantalla, VR };
	ModoDeJuego ElegirModoDeJuego(bool vrSolicitado, EstadoOpenXR estado);

	// Espacio de referencia elegido: STAGE (origen en el suelo del área de juego) o LOCAL (origen en la cabeza).
	enum class EspacioReferencia { Ninguno, Suelo, Local };

	// Qué hay que hacer con el fotograma actual.
	enum class ResultadoFotograma
	{
		NoActivo,    // no hay sesión en marcha: NO llamar a TerminarFotograma()
		SinRender,   // el visor no está visible: llamar a TerminarFotograma() sin dibujar
		Renderizar   // dibujar cada ojo (AdquirirOjo/LiberarOjo) y llamar a TerminarFotograma()
	};

	// Un ojo listo para dibujar: framebuffer con la imagen del swapchain ya adjunta, pose y campo de visión.
	struct OjoVR
	{
		unsigned fbo = 0;      // framebuffer de OpenGL (con profundidad); AdquirirOjo lo deja ENLAZADO
		unsigned textura = 0;  // textura de color adquirida (para copiarla a la ventana antes de liberar)
		int ancho = 0, alto = 0;
		PoseXR pose;           // espacio de referencia de OpenXR (metros)
		CampoDeVision fov;     // radianes
	};

	class GestorOpenXR
	{
	public:
		GestorOpenXR();
		~GestorOpenXR();   // libera todo lo que siga vivo
		GestorOpenXR(const GestorOpenXR&) = delete;
		GestorOpenXR& operator=(const GestorOpenXR&) = delete;

		// True si esta compilación incluye OpenXR (ENTORNVGI_HAS_OPENXR).
		static bool EstaCompilado();

		// Activación EXPLÍCITA. Necesita el contexto OpenGL de 'ventana' activo y su versión (por ejemplo 4, 6).
		// Si algo falla no queda ningún recurso a medias (se destruye lo creado) y se devuelve el motivo.
		EstadoOpenXR Activar(GLFWwindow* ventana, int versionGLMayor, int versionGLMenor);

		// Atiende los eventos de OpenXR (inicia/termina la sesión según su estado). Llamar una vez por
		// fotograma; no hace nada si no está activo.
		void ProcesarEventos();

		// ---- Bucle de fotogramas (Etapa C). Necesita el contexto OpenGL activo. ----
		// ¿La sesión ha empezado (READY) y no ha terminado? Solo entonces se dibuja en el visor.
		bool SesionEnMarcha() const;
		bool Enfocada() const;   // estado FOCUSED: recibe entrada y es lo que ve el usuario
		const char* NombreEstadoSesion() const;
		EspacioReferencia Espacio() const;
		int NumeroDeOjos() const;

		// xrWaitFrame + xrBeginFrame + xrLocateViews. Si no devuelve NoActivo hay que cerrar con TerminarFotograma().
		ResultadoFotograma EmpezarFotograma();
		// Adquiere la imagen del ojo 'indice' y deja su framebuffer enlazado. False si falla (el fotograma se
		// enviará vacío, sin dejar nada a medias).
		bool AdquirirOjo(int indice, OjoVR& ojo);
		void LiberarOjo(int indice);
		// xrEndFrame con una capa de proyección (si los dos ojos se dibujaron) o vacío.
		void TerminarFotograma();

		// Informe de diagnóstico (runtime, visor, GPU, extensiones, formatos, swapchains) para consola y panel.
		const std::string& Diagnostico() const { return diagnostico; }

		// Destruye sesión e instancia (en ese orden). Se puede llamar siempre, también sin haber activado.
		void Desactivar();

		EstadoOpenXR Estado() const { return estado; }
		const std::string& Mensaje() const { return mensaje; }
		bool TieneRecursos() const;   // ¿queda alguna instancia o sesión de OpenXR viva?

	private:
		struct Impl;   // oculta los tipos de OpenXR: el resto del juego no incluye sus cabeceras
		std::unique_ptr<Impl> impl;
		EstadoOpenXR estado = EstadoOpenXR::Desactivado;
		std::string mensaje = "OpenXR no activado.";
		std::string diagnostico;
	};
}
