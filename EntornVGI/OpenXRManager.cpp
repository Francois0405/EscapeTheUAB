// OpenXRManager.cpp
// Ver OpenXRManager.h para el alcance (Etapas A y B; la C no está hecha).
#include "OpenXRManager.h"

#ifdef ENTORNVGI_HAS_OPENXR

// La plataforma gráfica solo se declara donde existe el código para ella.
#if defined(_WIN32)
#define XR_USE_PLATFORM_WIN32
#define NOMINMAX                  // windows.h define min/max como macros y rompe std::min/std::max
#include <windows.h>              // HDC, HGLRC y wglGetCurrent*: openxr_platform.h los necesita
#endif
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <cstring>
#include <iostream>
#include <vector>

#endif

namespace vr
{
	const char* NombreDelEstado(EstadoOpenXR estado)
	{
		switch (estado)
		{
		case EstadoOpenXR::Desactivado:             return "Desactivado";
		case EstadoOpenXR::NoCompilado:             return "No compilado";
		case EstadoOpenXR::SinRuntime:              return "Sin runtime OpenXR";
		case EstadoOpenXR::SinVisor:                return "Sin visor";
		case EstadoOpenXR::FaltaExtensionOpenGL:    return "Falta XR_KHR_opengl_enable";
		case EstadoOpenXR::OpenGLInsuficiente:      return "OpenGL insuficiente";
		case EstadoOpenXR::InstanciaYSistemaListos: return "Visor detectado (sin sesion en esta plataforma)";
		case EstadoOpenXR::SesionCreada:            return "Sesion creada";
		case EstadoOpenXR::Error:                   return "Error";
		}
		return "?";
	}

	ModoDeJuego ElegirModoDeJuego(bool vrSolicitado, EstadoOpenXR estado)
	{
		return (vrSolicitado && estado == EstadoOpenXR::SesionCreada) ? ModoDeJuego::VR : ModoDeJuego::Pantalla;
	}

#ifdef ENTORNVGI_HAS_OPENXR

	// Los "manejadores" de OpenXR que hay que destruir. Se destruyen en orden inverso al de creación.
	struct GestorOpenXR::Impl
	{
		XrInstance instancia = XR_NULL_HANDLE;
		XrSystemId sistema = XR_NULL_SYSTEM_ID;
		XrSession sesion = XR_NULL_HANDLE;
	};

	namespace
	{
		// Los errores "no hay nada con lo que hablar" se distinguen de los fallos de verdad.
		bool EsFaltaDeRuntime(XrResult r)
		{
			return r == XR_ERROR_RUNTIME_UNAVAILABLE || r == XR_ERROR_RUNTIME_FAILURE ||
				r == XR_ERROR_FILE_ACCESS_ERROR || r == XR_ERROR_FILE_CONTENTS_INVALID;
		}

		std::string Texto(const char* queFalla, XrResult r)
		{
			return std::string(queFalla) + " (codigo OpenXR " + std::to_string(static_cast<int>(r)) + ").";
		}

		// Etapa A, paso 1: ¿el runtime ofrece OpenGL? Devuelve OK si sí; si no, el estado que corresponde.
		EstadoOpenXR ComprobarExtensionOpenGL(std::string& mensaje)
		{
			uint32_t cuantas = 0;
			XrResult r = xrEnumerateInstanceExtensionProperties(nullptr, 0, &cuantas, nullptr);
			if (XR_FAILED(r))
			{
				mensaje = EsFaltaDeRuntime(r) ? "No hay un runtime OpenXR activo (abre Meta Quest Link, SteamVR u otro)."
					: Texto("No se pudieron enumerar las extensiones de OpenXR", r);
				return EsFaltaDeRuntime(r) ? EstadoOpenXR::SinRuntime : EstadoOpenXR::Error;
			}
			std::vector<XrExtensionProperties> extensiones(cuantas, { XR_TYPE_EXTENSION_PROPERTIES });
			r = xrEnumerateInstanceExtensionProperties(nullptr, cuantas, &cuantas, extensiones.data());
			if (XR_FAILED(r)) { mensaje = Texto("Fallo al leer las extensiones de OpenXR", r); return EstadoOpenXR::Error; }

			for (const XrExtensionProperties& e : extensiones)
				if (std::strcmp(e.extensionName, XR_KHR_OPENGL_ENABLE_EXTENSION_NAME) == 0) return EstadoOpenXR::Desactivado;   // "sin problema"
			mensaje = "El runtime OpenXR no ofrece XR_KHR_opengl_enable: no puede dibujar con OpenGL.";
			return EstadoOpenXR::FaltaExtensionOpenGL;
		}

		// Etapa A, paso 2: crear la instancia (la "conexión" con el runtime).
		EstadoOpenXR CrearInstancia(XrInstance& instancia, std::string& mensaje)
		{
			const char* extensiones[] = { XR_KHR_OPENGL_ENABLE_EXTENSION_NAME };
			XrInstanceCreateInfo info = { XR_TYPE_INSTANCE_CREATE_INFO };
			std::strncpy(info.applicationInfo.applicationName, "EscapeTheUAB", XR_MAX_APPLICATION_NAME_SIZE - 1);
			std::strncpy(info.applicationInfo.engineName, "EntornVGI", XR_MAX_ENGINE_NAME_SIZE - 1);
			info.applicationInfo.applicationVersion = 1;
			info.applicationInfo.engineVersion = 1;
			info.applicationInfo.apiVersion = XR_API_VERSION_1_0;   // 1.0 basta y lo entienden todos los runtimes
			info.enabledExtensionCount = 1;
			info.enabledExtensionNames = extensiones;

			const XrResult r = xrCreateInstance(&info, &instancia);
			if (XR_SUCCEEDED(r)) return EstadoOpenXR::Desactivado;
			instancia = XR_NULL_HANDLE;
			mensaje = EsFaltaDeRuntime(r) ? "No hay un runtime OpenXR activo (abre Meta Quest Link, SteamVR u otro)."
				: Texto("No se pudo crear la instancia de OpenXR", r);
			return EsFaltaDeRuntime(r) ? EstadoOpenXR::SinRuntime : EstadoOpenXR::Error;
		}

		// Etapa A, paso 3: buscar el visor.
		EstadoOpenXR BuscarVisor(XrInstance instancia, XrSystemId& sistema, std::string& mensaje)
		{
			XrSystemGetInfo info = { XR_TYPE_SYSTEM_GET_INFO };
			info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
			const XrResult r = xrGetSystem(instancia, &info, &sistema);
			if (XR_SUCCEEDED(r)) return EstadoOpenXR::Desactivado;
			if (r == XR_ERROR_FORM_FACTOR_UNAVAILABLE)
			{
				mensaje = "Hay runtime OpenXR pero no hay ningun visor conectado (enciende el casco y activa Quest Link).";
				return EstadoOpenXR::SinVisor;
			}
			mensaje = Texto("No se pudo consultar el visor", r);
			return EstadoOpenXR::Error;
		}

		// Etapa A, paso 4: OpenXR EXIGE llamar a esta función antes de crear la sesión, y la versión de
		// OpenGL de nuestra ventana debe ser al menos la que pide el runtime.
		EstadoOpenXR ComprobarVersionOpenGL(XrInstance instancia, XrSystemId sistema, int mayor, int menor, std::string& mensaje)
		{
			PFN_xrGetOpenGLGraphicsRequirementsKHR pedirRequisitos = nullptr;
			XrResult r = xrGetInstanceProcAddr(instancia, "xrGetOpenGLGraphicsRequirementsKHR",
				reinterpret_cast<PFN_xrVoidFunction*>(&pedirRequisitos));
			if (XR_FAILED(r) || pedirRequisitos == nullptr)
			{
				mensaje = Texto("No se encontro xrGetOpenGLGraphicsRequirementsKHR", r);
				return EstadoOpenXR::FaltaExtensionOpenGL;
			}
			XrGraphicsRequirementsOpenGLKHR requisitos = { XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR };
			r = pedirRequisitos(instancia, sistema, &requisitos);
			if (XR_FAILED(r)) { mensaje = Texto("No se pudieron leer los requisitos graficos", r); return EstadoOpenXR::Error; }

			if (XR_MAKE_VERSION(mayor, menor, 0) < requisitos.minApiVersionSupported)
			{
				mensaje = "El runtime necesita OpenGL " + std::to_string(XR_VERSION_MAJOR(requisitos.minApiVersionSupported)) + "." +
					std::to_string(XR_VERSION_MINOR(requisitos.minApiVersionSupported)) + " o superior y la ventana tiene " +
					std::to_string(mayor) + "." + std::to_string(menor) + ".";
				return EstadoOpenXR::OpenGLInsuficiente;
			}
			return EstadoOpenXR::Desactivado;
		}

#if defined(_WIN32)
		// Etapa B: crear la sesión enlazada al contexto WGL que GLFW ya tiene activo.
		EstadoOpenXR CrearSesion(XrInstance instancia, XrSystemId sistema, XrSession& sesion, std::string& mensaje)
		{
			const HDC dispositivo = wglGetCurrentDC();      // los de la ventana ACTIVA ahora mismo
			const HGLRC contexto = wglGetCurrentContext();
			if (dispositivo == nullptr || contexto == nullptr)
			{
				mensaje = "No hay un contexto OpenGL (WGL) activo: la sesion VR necesita la ventana ya creada.";
				return EstadoOpenXR::Error;
			}
			XrGraphicsBindingOpenGLWin32KHR enlace = { XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR };
			enlace.hDC = dispositivo;
			enlace.hGLRC = contexto;

			XrSessionCreateInfo info = { XR_TYPE_SESSION_CREATE_INFO };
			info.next = &enlace;    // aqui se une OpenGL a la sesion
			info.systemId = sistema;
			const XrResult r = xrCreateSession(instancia, &info, &sesion);
			if (XR_FAILED(r)) { sesion = XR_NULL_HANDLE; mensaje = Texto("No se pudo crear la sesion OpenXR con OpenGL", r); return EstadoOpenXR::Error; }
			return EstadoOpenXR::SesionCreada;
		}
#endif
	}

	GestorOpenXR::GestorOpenXR() : impl(std::make_unique<Impl>()) {}
	GestorOpenXR::~GestorOpenXR() { Desactivar(); }
	bool GestorOpenXR::EstaCompilado() { return true; }
	bool GestorOpenXR::TieneRecursos() const { return impl->instancia != XR_NULL_HANDLE || impl->sesion != XR_NULL_HANDLE; }

	void GestorOpenXR::Desactivar()
	{
		// Orden inverso al de creación: primero la sesión, luego la instancia.
		if (impl->sesion != XR_NULL_HANDLE) xrDestroySession(impl->sesion);
		if (impl->instancia != XR_NULL_HANDLE) xrDestroyInstance(impl->instancia);
		impl->sesion = XR_NULL_HANDLE;
		impl->instancia = XR_NULL_HANDLE;
		impl->sistema = XR_NULL_SYSTEM_ID;
		if (estado != EstadoOpenXR::Desactivado && estado != EstadoOpenXR::NoCompilado) { estado = EstadoOpenXR::Desactivado; mensaje = "OpenXR desactivado."; }
	}

	EstadoOpenXR GestorOpenXR::Activar(GLFWwindow* ventana, int versionGLMayor, int versionGLMenor)
	{
		(void)ventana;   // solo se usa en Windows (ya hay un contexto activo); se conserva para otras plataformas
		if (TieneRecursos()) return estado;   // ya activo: no se crea nada dos veces
		std::string aviso;

		// Cada paso devuelve "Desactivado" si todo fue bien; si no, el motivo. Al primer fallo se limpia todo.
		EstadoOpenXR resultado = ComprobarExtensionOpenGL(aviso);
		if (resultado == EstadoOpenXR::Desactivado) resultado = CrearInstancia(impl->instancia, aviso);
		if (resultado == EstadoOpenXR::Desactivado) resultado = BuscarVisor(impl->instancia, impl->sistema, aviso);
		if (resultado == EstadoOpenXR::Desactivado) resultado = ComprobarVersionOpenGL(impl->instancia, impl->sistema, versionGLMayor, versionGLMenor, aviso);
		if (resultado == EstadoOpenXR::Desactivado)
		{
#if defined(_WIN32)
			resultado = CrearSesion(impl->instancia, impl->sistema, impl->sesion, aviso);
			if (resultado == EstadoOpenXR::SesionCreada) aviso = "Sesion OpenXR creada y enlazada a OpenGL (falta la Etapa C: dibujar en el visor).";
#else
			resultado = EstadoOpenXR::InstanciaYSistemaListos;
			aviso = "Visor detectado. En esta plataforma no se crea sesion VR (solo implementado en Windows con OpenGL).";
#endif
		}

		const bool exito = resultado == EstadoOpenXR::SesionCreada || resultado == EstadoOpenXR::InstanciaYSistemaListos;
		if (!exito) Desactivar();   // fallo parcial: destruir lo creado (Desactivar no toca el estado que fijamos ahora)
		estado = resultado;
		mensaje = aviso;
		std::cout << "[OpenXR] " << NombreDelEstado(estado) << ": " << mensaje << std::endl;
		return estado;
	}

	void GestorOpenXR::ProcesarEventos()
	{
		if (impl->instancia == XR_NULL_HANDLE) return;
		XrEventDataBuffer evento = { XR_TYPE_EVENT_DATA_BUFFER };
		while (xrPollEvent(impl->instancia, &evento) == XR_SUCCESS)
		{
			if (evento.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
			{
				const auto* cambio = reinterpret_cast<const XrEventDataSessionStateChanged*>(&evento);
				std::cout << "[OpenXR] estado de la sesion: " << static_cast<int>(cambio->state) << std::endl;
				// READY: aqui empezaria xrBeginSession + el bucle de fotogramas (Etapa C, pendiente).
				// Cuando el runtime pide terminar o se pierde la sesion, se cierra todo con seguridad.
				if (cambio->state == XR_SESSION_STATE_EXITING || cambio->state == XR_SESSION_STATE_LOSS_PENDING)
				{
					Desactivar();
					mensaje = "El runtime cerro la sesion VR.";
					return;
				}
			}
			else if (evento.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)
			{
				Desactivar();
				mensaje = "El runtime OpenXR se ha cerrado.";
				return;
			}
			evento = { XR_TYPE_EVENT_DATA_BUFFER };
		}
	}

#else   // ---- Sin OpenXR en esta compilación: la clase existe, pero no hace nada ----

	struct GestorOpenXR::Impl {};

	GestorOpenXR::GestorOpenXR() : impl(std::make_unique<Impl>()) {}
	GestorOpenXR::~GestorOpenXR() = default;
	bool GestorOpenXR::EstaCompilado() { return false; }
	bool GestorOpenXR::TieneRecursos() const { return false; }
	void GestorOpenXR::ProcesarEventos() {}
	void GestorOpenXR::Desactivar() { if (estado != EstadoOpenXR::NoCompilado) { estado = EstadoOpenXR::Desactivado; mensaje = "OpenXR no activado."; } }

	EstadoOpenXR GestorOpenXR::Activar(GLFWwindow*, int, int)
	{
		estado = EstadoOpenXR::NoCompilado;
		mensaje = "Esta compilacion no incluye OpenXR (macOS, o falta la biblioteca). El juego sigue en modo pantalla.";
		return estado;
	}

#endif
}
