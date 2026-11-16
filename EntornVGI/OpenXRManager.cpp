// OpenXRManager.cpp
// Ver OpenXRManager.h para el alcance (Etapas A, B y C).
#include "OpenXRManager.h"

#ifdef ENTORNVGI_HAS_OPENXR

// La plataforma gráfica solo se declara donde existe el código para ella.
#if defined(_WIN32)
#define XR_USE_PLATFORM_WIN32
#define NOMINMAX                  // windows.h define min/max como macros y rompe std::min/std::max
#include <windows.h>              // HDC, HGLRC y wglGetCurrent*: openxr_platform.h los necesita
#endif
#include <GL/glew.h>              // framebuffers de los swapchains (glew debe ir antes que cualquier otro GL)
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
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

	namespace
	{
		// Un ojo = un swapchain con sus imágenes de OpenGL, un framebuffer y un búfer de profundidad.
		struct CadenaOjo
		{
			XrSwapchain cadena = XR_NULL_HANDLE;
			int ancho = 0, alto = 0;
			std::vector<XrSwapchainImageOpenGLKHR> imagenes;
			GLuint fbo = 0, profundidad = 0;
			bool adquirida = false;      // hay una imagen adquirida sin liberar
			bool dibujada = false;       // este fotograma se dibujó completo
			bool framebufferComprobado = false;
		};
	}

	// Los "manejadores" de OpenXR que hay que destruir. Se destruyen en orden inverso al de creación.
	struct GestorOpenXR::Impl
	{
		XrInstance instancia = XR_NULL_HANDLE;
		XrSystemId sistema = XR_NULL_SYSTEM_ID;
		XrSession sesion = XR_NULL_HANDLE;
		XrSpace espacio = XR_NULL_HANDLE;
		EspacioReferencia tipoEspacio = EspacioReferencia::Ninguno;

		XrSessionState estadoSesion = XR_SESSION_STATE_UNKNOWN;
		bool sesionIniciada = false;                    // entre xrBeginSession y xrEndSession

		XrEnvironmentBlendMode mezcla = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
		std::vector<CadenaOjo> ojos;
		std::vector<XrView> vistas;

		bool fotogramaAbierto = false;                  // xrBeginFrame hecho, falta xrEndFrame
		bool vistasValidas = false;                     // xrLocateViews dio poses utilizables este fotograma
		XrTime tiempoPrediccion = 0;
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

		const char* NombreSesion(XrSessionState s)
		{
			switch (s)
			{
			case XR_SESSION_STATE_IDLE:         return "IDLE";
			case XR_SESSION_STATE_READY:        return "READY";
			case XR_SESSION_STATE_SYNCHRONIZED: return "SYNCHRONIZED";
			case XR_SESSION_STATE_VISIBLE:      return "VISIBLE";
			case XR_SESSION_STATE_FOCUSED:      return "FOCUSED";
			case XR_SESSION_STATE_STOPPING:     return "STOPPING";
			case XR_SESSION_STATE_LOSS_PENDING: return "LOSS_PENDING";
			case XR_SESSION_STATE_EXITING:      return "EXITING";
			default:                            return "UNKNOWN";
			}
		}

		std::string VersionTexto(XrVersion v)
		{
			return std::to_string(XR_VERSION_MAJOR(v)) + "." + std::to_string(XR_VERSION_MINOR(v)) + "." + std::to_string(XR_VERSION_PATCH(v));
		}

		// Etapa A, paso 1: ¿el runtime ofrece OpenGL? Devuelve OK si sí; si no, el estado que corresponde.
		// Además deja en 'lista' los nombres de todas las extensiones (para el diagnóstico).
		EstadoOpenXR ComprobarExtensionOpenGL(std::string& mensaje, std::vector<std::string>& lista)
		{
			uint32_t cuantas = 0;
			XrResult r = xrEnumerateInstanceExtensionProperties(nullptr, 0, &cuantas, nullptr);
			if (XR_FAILED(r))
			{
				mensaje = EsFaltaDeRuntime(r) ? "No hay un runtime OpenXR activo (abre Meta Horizon Link / Quest Link, SteamVR u otro)."
					: Texto("No se pudieron enumerar las extensiones de OpenXR", r);
				return EsFaltaDeRuntime(r) ? EstadoOpenXR::SinRuntime : EstadoOpenXR::Error;
			}
			std::vector<XrExtensionProperties> extensiones(cuantas, { XR_TYPE_EXTENSION_PROPERTIES });
			r = xrEnumerateInstanceExtensionProperties(nullptr, cuantas, &cuantas, extensiones.data());
			if (XR_FAILED(r)) { mensaje = Texto("Fallo al leer las extensiones de OpenXR", r); return EstadoOpenXR::Error; }

			bool hayOpenGL = false;
			for (const XrExtensionProperties& e : extensiones)
			{
				lista.push_back(e.extensionName);
				if (std::strcmp(e.extensionName, XR_KHR_OPENGL_ENABLE_EXTENSION_NAME) == 0) hayOpenGL = true;
			}
			if (hayOpenGL) return EstadoOpenXR::Desactivado;   // "sin problema"
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
			mensaje = EsFaltaDeRuntime(r) ? "No hay un runtime OpenXR activo (abre Meta Horizon Link / Quest Link, SteamVR u otro)."
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
				mensaje = "Hay runtime OpenXR pero no hay ningun visor conectado (enciende el casco y activa Meta Horizon Link / Quest Link).";
				return EstadoOpenXR::SinVisor;
			}
			mensaje = Texto("No se pudo consultar el visor", r);
			return EstadoOpenXR::Error;
		}

		// Etapa A, paso 4: OpenXR EXIGE llamar a esta función antes de crear la sesión, y la versión de
		// OpenGL de nuestra ventana debe ser al menos la que pide el runtime.
		EstadoOpenXR ComprobarVersionOpenGL(XrInstance instancia, XrSystemId sistema, int mayor, int menor, std::string& mensaje,
			std::string& diagnostico)
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

			diagnostico += "OpenGL del runtime: minimo " + VersionTexto(requisitos.minApiVersionSupported) + ", maximo probado " +
				VersionTexto(requisitos.maxApiVersionSupported) + "; ventana " + std::to_string(mayor) + "." + std::to_string(menor) + "\n";
			if (XR_MAKE_VERSION(mayor, menor, 0) < requisitos.minApiVersionSupported)
			{
				mensaje = "El runtime necesita OpenGL " + std::to_string(XR_VERSION_MAJOR(requisitos.minApiVersionSupported)) + "." +
					std::to_string(XR_VERSION_MINOR(requisitos.minApiVersionSupported)) + " o superior y la ventana tiene " +
					std::to_string(mayor) + "." + std::to_string(menor) + ".";
				return EstadoOpenXR::OpenGLInsuficiente;
			}
			return EstadoOpenXR::Desactivado;
		}

		// Datos del runtime y del visor para el informe de diagnóstico.
		void DescribirRuntimeYVisor(XrInstance instancia, XrSystemId sistema, std::string& d)
		{
			XrInstanceProperties prop = { XR_TYPE_INSTANCE_PROPERTIES };
			if (XR_SUCCEEDED(xrGetInstanceProperties(instancia, &prop)))
				d += std::string("Runtime: ") + prop.runtimeName + " " + VersionTexto(prop.runtimeVersion) + "\n";
			XrSystemProperties sis = { XR_TYPE_SYSTEM_PROPERTIES };
			if (XR_SUCCEEDED(xrGetSystemProperties(instancia, sistema, &sis)))
			{
				d += std::string("Visor: ") + sis.systemName + " (vendorId " + std::to_string(sis.vendorId) + ")\n";
				d += std::string("Seguimiento: orientacion=") + (sis.trackingProperties.orientationTracking ? "si" : "no") +
					", posicion=" + (sis.trackingProperties.positionTracking ? "si" : "no") + "\n";
				d += "Limite de swapchain del runtime: " + std::to_string(sis.graphicsProperties.maxSwapchainImageWidth) + "x" +
					std::to_string(sis.graphicsProperties.maxSwapchainImageHeight) + ", capas " + std::to_string(sis.graphicsProperties.maxLayerCount) + "\n";
			}
			const GLubyte* gpu = glGetString(GL_RENDERER);
			const GLubyte* fab = glGetString(GL_VENDOR);
			const GLubyte* ver = glGetString(GL_VERSION);
			d += std::string("GPU: ") + (gpu ? reinterpret_cast<const char*>(gpu) : "?") + " / " + (fab ? reinterpret_cast<const char*>(fab) : "?") +
				", OpenGL " + (ver ? reinterpret_cast<const char*>(ver) : "?") + "\n";
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

		// Etapa C, paso 1: espacio de referencia. STAGE (suelo) si el runtime lo ofrece; si no, LOCAL.
		EstadoOpenXR CrearEspacio(XrSession sesion, XrSpace& espacio, EspacioReferencia& tipo, std::string& mensaje, std::string& d)
		{
			uint32_t cuantos = 0;
			xrEnumerateReferenceSpaces(sesion, 0, &cuantos, nullptr);
			std::vector<XrReferenceSpaceType> tipos(cuantos);
			if (cuantos > 0) xrEnumerateReferenceSpaces(sesion, cuantos, &cuantos, tipos.data());
			const bool hayStage = std::find(tipos.begin(), tipos.end(), XR_REFERENCE_SPACE_TYPE_STAGE) != tipos.end();
			const bool hayLocal = std::find(tipos.begin(), tipos.end(), XR_REFERENCE_SPACE_TYPE_LOCAL) != tipos.end();
			d += std::string("Espacios de referencia: LOCAL=") + (hayLocal ? "si" : "no") + ", STAGE=" + (hayStage ? "si" : "no") + "\n";

			XrReferenceSpaceCreateInfo info = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
			info.poseInReferenceSpace.orientation.w = 1.0f;   // identidad
			XrResult r = XR_ERROR_REFERENCE_SPACE_UNSUPPORTED;
			if (hayStage)
			{
				info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
				r = xrCreateReferenceSpace(sesion, &info, &espacio);
				if (XR_SUCCEEDED(r)) tipo = EspacioReferencia::Suelo;
			}
			if (XR_FAILED(r) && hayLocal)
			{
				info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
				r = xrCreateReferenceSpace(sesion, &info, &espacio);
				if (XR_SUCCEEDED(r)) tipo = EspacioReferencia::Local;
			}
			if (XR_FAILED(r)) { espacio = XR_NULL_HANDLE; mensaje = Texto("No se pudo crear un espacio de referencia (STAGE ni LOCAL)", r); return EstadoOpenXR::Error; }
			d += std::string("Espacio elegido: ") + (tipo == EspacioReferencia::Suelo ? "STAGE (origen en el suelo)" : "LOCAL (origen en la cabeza)") + "\n";
			return EstadoOpenXR::SesionCreada;
		}

		// Etapa C, paso 2: formato de color del swapchain. Se elige entre los que ofrece el runtime; nada fijo.
		// Preferencia: sRGB (es lo que espera el compositor para una imagen ya "de pantalla"), luego RGBA8.
		bool ElegirFormato(XrSession sesion, int64_t& formato, std::string& mensaje, std::string& d)
		{
			uint32_t cuantos = 0;
			xrEnumerateSwapchainFormats(sesion, 0, &cuantos, nullptr);
			std::vector<int64_t> formatos(cuantos);
			if (cuantos > 0) xrEnumerateSwapchainFormats(sesion, cuantos, &cuantos, formatos.data());
			if (formatos.empty()) { mensaje = "El runtime no ofrece ningun formato de swapchain."; return false; }

			d += "Formatos de swapchain:";
			for (int64_t f : formatos) { std::ostringstream s; s << " 0x" << std::hex << f; d += s.str(); }
			d += "\n";

			const int64_t preferidos[] = { GL_SRGB8_ALPHA8, GL_RGBA8, GL_RGB10_A2, GL_RGBA16F };
			formato = formatos[0];   // si no hay ninguno conocido, el preferido por el runtime (primero de la lista)
			for (int64_t p : preferidos)
				if (std::find(formatos.begin(), formatos.end(), p) != formatos.end()) { formato = p; break; }
			{ std::ostringstream s; s << "Formato elegido: 0x" << std::hex << formato << "\n"; d += s.str(); }
			return true;
		}

		// Etapa C, paso 3: un swapchain por ojo con el tamaño que RECOMIENDA el runtime, más su framebuffer.
		bool CrearCadenas(XrInstance instancia, XrSystemId sistema, XrSession sesion, int64_t formato, std::vector<CadenaOjo>& ojos,
			std::vector<XrView>& vistas, XrEnvironmentBlendMode& mezcla, std::string& mensaje, std::string& d)
		{
			uint32_t cuantas = 0;
			XrResult r = xrEnumerateViewConfigurationViews(instancia, sistema, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &cuantas, nullptr);
			if (XR_FAILED(r) || cuantas == 0) { mensaje = Texto("El visor no ofrece la configuracion estereo PRIMARY_STEREO", r); return false; }
			std::vector<XrViewConfigurationView> config(cuantas, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
			r = xrEnumerateViewConfigurationViews(instancia, sistema, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, cuantas, &cuantas, config.data());
			if (XR_FAILED(r)) { mensaje = Texto("No se pudo leer la configuracion de las vistas", r); return false; }

			// Modo de mezcla con el entorno: el primero que ofrece el runtime (OPAQUE en un casco VR).
			uint32_t nMezclas = 0;
			xrEnumerateEnvironmentBlendModes(instancia, sistema, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &nMezclas, nullptr);
			std::vector<XrEnvironmentBlendMode> mezclas(nMezclas);
			if (nMezclas > 0 && XR_SUCCEEDED(xrEnumerateEnvironmentBlendModes(instancia, sistema, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
				nMezclas, &nMezclas, mezclas.data())))
				mezcla = mezclas[0];

			d += "Vistas: " + std::to_string(cuantas) + ", modo de mezcla " + std::to_string(static_cast<int>(mezcla)) + "\n";
			ojos.assign(cuantas, CadenaOjo());
			vistas.assign(cuantas, XrView{ XR_TYPE_VIEW });

			for (uint32_t i = 0; i < cuantas; ++i)
			{
				CadenaOjo& o = ojos[i];
				o.ancho = static_cast<int>(config[i].recommendedImageRectWidth);
				o.alto = static_cast<int>(config[i].recommendedImageRectHeight);

				XrSwapchainCreateInfo info = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
				info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
				info.format = formato;
				info.sampleCount = 1;   // sin multimuestreo en el swapchain (el framebuffer propio no lo usa)
				info.width = static_cast<uint32_t>(o.ancho);
				info.height = static_cast<uint32_t>(o.alto);
				info.faceCount = 1;
				info.arraySize = 1;
				info.mipCount = 1;
				r = xrCreateSwapchain(sesion, &info, &o.cadena);
				if (XR_FAILED(r)) { o.cadena = XR_NULL_HANDLE; mensaje = Texto("No se pudo crear el swapchain del ojo", r); return false; }

				uint32_t nImagenes = 0;
				xrEnumerateSwapchainImages(o.cadena, 0, &nImagenes, nullptr);
				o.imagenes.assign(nImagenes, XrSwapchainImageOpenGLKHR{ XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
				r = xrEnumerateSwapchainImages(o.cadena, nImagenes, &nImagenes,
					reinterpret_cast<XrSwapchainImageBaseHeader*>(o.imagenes.data()));
				if (XR_FAILED(r)) { mensaje = Texto("No se pudieron leer las imagenes del swapchain", r); return false; }

				// Framebuffer propio: el color cambia cada fotograma (imagen adquirida); la profundidad es fija.
				glGenFramebuffers(1, &o.fbo);
				glGenRenderbuffers(1, &o.profundidad);
				glBindRenderbuffer(GL_RENDERBUFFER, o.profundidad);
				glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, o.ancho, o.alto);
				glBindRenderbuffer(GL_RENDERBUFFER, 0);
				glBindFramebuffer(GL_FRAMEBUFFER, o.fbo);
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, o.profundidad);
				glBindFramebuffer(GL_FRAMEBUFFER, 0);

				d += "Ojo " + std::to_string(i) + ": " + std::to_string(o.ancho) + "x" + std::to_string(o.alto) + " (recomendado; maximo " +
					std::to_string(config[i].maxImageRectWidth) + "x" + std::to_string(config[i].maxImageRectHeight) + "), " +
					std::to_string(nImagenes) + " imagenes, muestras recomendadas " + std::to_string(config[i].recommendedSwapchainSampleCount) + "\n";
			}
			return true;
		}
#endif

		void DestruirCadenas(std::vector<CadenaOjo>& ojos)
		{
			for (CadenaOjo& o : ojos)
			{
				if (o.adquirida && o.cadena != XR_NULL_HANDLE)
				{
					XrSwapchainImageReleaseInfo info = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
					xrReleaseSwapchainImage(o.cadena, &info);
				}
				if (o.cadena != XR_NULL_HANDLE) xrDestroySwapchain(o.cadena);
				// Los objetos de OpenGL solo existen si se llegaron a crear (y entonces hay contexto activo).
				if (o.fbo != 0) glDeleteFramebuffers(1, &o.fbo);
				if (o.profundidad != 0) glDeleteRenderbuffers(1, &o.profundidad);
			}
			ojos.clear();
		}
	}

	GestorOpenXR::GestorOpenXR() : impl(std::make_unique<Impl>()) {}
	GestorOpenXR::~GestorOpenXR() { Desactivar(); }
	bool GestorOpenXR::EstaCompilado() { return true; }
	bool GestorOpenXR::TieneRecursos() const { return impl->instancia != XR_NULL_HANDLE || impl->sesion != XR_NULL_HANDLE; }
	bool GestorOpenXR::SesionEnMarcha() const { return impl->sesion != XR_NULL_HANDLE && impl->sesionIniciada; }
	bool GestorOpenXR::Enfocada() const { return SesionEnMarcha() && impl->estadoSesion == XR_SESSION_STATE_FOCUSED; }
	const char* GestorOpenXR::NombreEstadoSesion() const { return impl->sesion != XR_NULL_HANDLE ? NombreSesion(impl->estadoSesion) : "sin sesion"; }
	EspacioReferencia GestorOpenXR::Espacio() const { return impl->tipoEspacio; }
	int GestorOpenXR::NumeroDeOjos() const { return static_cast<int>(impl->ojos.size()); }

	void GestorOpenXR::Desactivar()
	{
		// Orden inverso al de creación: swapchains, espacio, sesión (se termina antes si estaba en marcha), instancia.
		if (impl->fotogramaAbierto && impl->sesion != XR_NULL_HANDLE)
		{
			XrFrameEndInfo fin = { XR_TYPE_FRAME_END_INFO };   // cerrar el fotograma abierto para no dejar la sesión a medias
			fin.displayTime = impl->tiempoPrediccion;
			fin.environmentBlendMode = impl->mezcla;
			xrEndFrame(impl->sesion, &fin);
		}
		impl->fotogramaAbierto = false;
		DestruirCadenas(impl->ojos);
		impl->vistas.clear();
		if (impl->espacio != XR_NULL_HANDLE) xrDestroySpace(impl->espacio);
		if (impl->sesion != XR_NULL_HANDLE)
		{
			if (impl->sesionIniciada) xrEndSession(impl->sesion);
			xrDestroySession(impl->sesion);
		}
		if (impl->instancia != XR_NULL_HANDLE) xrDestroyInstance(impl->instancia);
		impl->espacio = XR_NULL_HANDLE;
		impl->sesion = XR_NULL_HANDLE;
		impl->instancia = XR_NULL_HANDLE;
		impl->sistema = XR_NULL_SYSTEM_ID;
		impl->tipoEspacio = EspacioReferencia::Ninguno;
		impl->estadoSesion = XR_SESSION_STATE_UNKNOWN;
		impl->sesionIniciada = false;
		impl->vistasValidas = false;
		if (estado != EstadoOpenXR::Desactivado && estado != EstadoOpenXR::NoCompilado) { estado = EstadoOpenXR::Desactivado; mensaje = "OpenXR desactivado."; }
	}

	EstadoOpenXR GestorOpenXR::Activar(GLFWwindow* ventana, int versionGLMayor, int versionGLMenor)
	{
		(void)ventana;   // solo se usa en Windows (ya hay un contexto activo); se conserva para otras plataformas
		if (TieneRecursos()) return estado;   // ya activo: no se crea nada dos veces
		std::string aviso;
		std::string informe;
		std::vector<std::string> extensiones;

		// Cada paso devuelve "Desactivado" si todo fue bien; si no, el motivo. Al primer fallo se limpia todo.
		EstadoOpenXR resultado = ComprobarExtensionOpenGL(aviso, extensiones);
		if (resultado == EstadoOpenXR::Desactivado) resultado = CrearInstancia(impl->instancia, aviso);
		if (resultado == EstadoOpenXR::Desactivado) resultado = BuscarVisor(impl->instancia, impl->sistema, aviso);
		if (resultado == EstadoOpenXR::Desactivado)
		{
			DescribirRuntimeYVisor(impl->instancia, impl->sistema, informe);
			resultado = ComprobarVersionOpenGL(impl->instancia, impl->sistema, versionGLMayor, versionGLMenor, aviso, informe);
		}
		if (resultado == EstadoOpenXR::Desactivado)
		{
#if defined(_WIN32)
			resultado = CrearSesion(impl->instancia, impl->sistema, impl->sesion, aviso);
			if (resultado == EstadoOpenXR::SesionCreada) resultado = CrearEspacio(impl->sesion, impl->espacio, impl->tipoEspacio, aviso, informe);
			int64_t formato = 0;
			if (resultado == EstadoOpenXR::SesionCreada && !ElegirFormato(impl->sesion, formato, aviso, informe)) resultado = EstadoOpenXR::Error;
			if (resultado == EstadoOpenXR::SesionCreada &&
				!CrearCadenas(impl->instancia, impl->sistema, impl->sesion, formato, impl->ojos, impl->vistas, impl->mezcla, aviso, informe))
				resultado = EstadoOpenXR::Error;
			if (resultado == EstadoOpenXR::SesionCreada)
				aviso = "Sesion OpenXR lista. Esperando a que el runtime la ponga en READY (ponte el casco).";
#else
			resultado = EstadoOpenXR::InstanciaYSistemaListos;
			aviso = "Visor detectado. En esta plataforma no se crea sesion VR (solo implementado en Windows con OpenGL).";
#endif
		}

		const bool exito = resultado == EstadoOpenXR::SesionCreada || resultado == EstadoOpenXR::InstanciaYSistemaListos;
		if (!exito) Desactivar();   // fallo parcial: destruir lo creado (Desactivar no toca el estado que fijamos ahora)
		estado = resultado;
		mensaje = aviso;

		// Informe de diagnóstico completo (también en caso de fallo: lo que se averiguó hasta entonces).
		std::string completo = "Estado: " + std::string(NombreDelEstado(estado)) + " - " + aviso + "\n" + informe;
		if (!extensiones.empty())
		{
			completo += "Extensiones del runtime (" + std::to_string(extensiones.size()) + "):";
			for (const std::string& e : extensiones) completo += " " + e;
			completo += "\n";
		}
		diagnostico = completo;
		std::cout << "[OpenXR] " << NombreDelEstado(estado) << ": " << mensaje << std::endl;
		std::cout << "[OpenXR] Diagnostico:\n" << diagnostico << std::flush;
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
				impl->estadoSesion = cambio->state;
				std::cout << "[OpenXR] estado de la sesion: " << NombreSesion(cambio->state) << std::endl;
				switch (cambio->state)
				{
				case XR_SESSION_STATE_READY:
					if (!impl->sesionIniciada)
					{
						XrSessionBeginInfo inicio = { XR_TYPE_SESSION_BEGIN_INFO };
						inicio.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
						const XrResult r = xrBeginSession(impl->sesion, &inicio);
						if (XR_SUCCEEDED(r)) { impl->sesionIniciada = true; mensaje = "Sesion VR en marcha."; }
						else { mensaje = Texto("No se pudo iniciar la sesion VR (xrBeginSession)", r); std::cout << "[OpenXR] " << mensaje << std::endl; }
					}
					break;
				case XR_SESSION_STATE_FOCUSED:
					mensaje = "VR activa y enfocada.";
					break;
				case XR_SESSION_STATE_STOPPING:
					// El runtime pide parar: se termina la sesión (sigue viva; puede volver a READY).
					if (impl->fotogramaAbierto) TerminarFotograma();
					if (impl->sesionIniciada) { xrEndSession(impl->sesion); impl->sesionIniciada = false; }
					mensaje = "Sesion VR detenida por el runtime (STOPPING).";
					break;
				case XR_SESSION_STATE_EXITING:
				case XR_SESSION_STATE_LOSS_PENDING:
					// Cuando el runtime pide terminar o se pierde la sesión, se cierra todo con seguridad.
					Desactivar();
					mensaje = cambio->state == XR_SESSION_STATE_EXITING ? "El runtime cerro la sesion VR." : "Se perdio la sesion VR (el runtime o el casco se han desconectado).";
					return;
				default:
					break;
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

	ResultadoFotograma GestorOpenXR::EmpezarFotograma()
	{
		if (!SesionEnMarcha() || impl->fotogramaAbierto) return ResultadoFotograma::NoActivo;

		XrFrameState estadoFotograma = { XR_TYPE_FRAME_STATE };
		XrFrameWaitInfo espera = { XR_TYPE_FRAME_WAIT_INFO };
		XrResult r = xrWaitFrame(impl->sesion, &espera, &estadoFotograma);
		if (XR_FAILED(r)) { mensaje = Texto("xrWaitFrame fallo", r); return ResultadoFotograma::NoActivo; }
		XrFrameBeginInfo inicio = { XR_TYPE_FRAME_BEGIN_INFO };
		r = xrBeginFrame(impl->sesion, &inicio);
		if (XR_FAILED(r)) { mensaje = Texto("xrBeginFrame fallo", r); return ResultadoFotograma::NoActivo; }
		impl->fotogramaAbierto = true;
		impl->tiempoPrediccion = estadoFotograma.predictedDisplayTime;
		impl->vistasValidas = false;
		for (CadenaOjo& o : impl->ojos) o.dibujada = false;

		if (!estadoFotograma.shouldRender) return ResultadoFotograma::SinRender;

		XrViewLocateInfo localizar = { XR_TYPE_VIEW_LOCATE_INFO };
		localizar.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
		localizar.displayTime = estadoFotograma.predictedDisplayTime;
		localizar.space = impl->espacio;
		XrViewState estadoVistas = { XR_TYPE_VIEW_STATE };
		uint32_t cuantas = 0;
		r = xrLocateViews(impl->sesion, &localizar, &estadoVistas, static_cast<uint32_t>(impl->vistas.size()), &cuantas, impl->vistas.data());
		const XrViewStateFlags necesarias = XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;
		if (XR_FAILED(r) || (estadoVistas.viewStateFlags & necesarias) != necesarias || cuantas != impl->vistas.size())
			return ResultadoFotograma::SinRender;   // sin seguimiento valido este fotograma: se envia vacio
		impl->vistasValidas = true;
		return ResultadoFotograma::Renderizar;
	}

	bool GestorOpenXR::AdquirirOjo(int indice, OjoVR& ojo)
	{
		if (!impl->fotogramaAbierto || !impl->vistasValidas || indice < 0 || indice >= NumeroDeOjos()) return false;
		CadenaOjo& o = impl->ojos[indice];
		if (o.adquirida) return false;

		uint32_t posicion = 0;
		XrSwapchainImageAcquireInfo adquirir = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
		if (XR_FAILED(xrAcquireSwapchainImage(o.cadena, &adquirir, &posicion))) return false;
		XrSwapchainImageWaitInfo esperar = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
		esperar.timeout = XR_INFINITE_DURATION;
		const XrResult r = xrWaitSwapchainImage(o.cadena, &esperar);
		if (XR_FAILED(r) || posicion >= o.imagenes.size())
		{
			XrSwapchainImageReleaseInfo liberar = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
			xrReleaseSwapchainImage(o.cadena, &liberar);   // la imagen se adquirió: hay que liberarla igualmente
			return false;
		}
		o.adquirida = true;

		const GLuint textura = o.imagenes[posicion].image;
		glBindFramebuffer(GL_FRAMEBUFFER, o.fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textura, 0);
		if (!o.framebufferComprobado)
		{
			o.framebufferComprobado = true;
			const GLenum estadoFb = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (estadoFb != GL_FRAMEBUFFER_COMPLETE)
			{
				std::cout << "[OpenXR] Framebuffer del ojo " << indice << " incompleto (0x" << std::hex << estadoFb << std::dec << ")." << std::endl;
				mensaje = "El framebuffer del ojo no es completo con el formato del swapchain.";
			}
		}
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			LiberarOjo(indice);
			return false;
		}

		const XrView& v = impl->vistas[indice];
		ojo.fbo = o.fbo;
		ojo.textura = textura;
		ojo.ancho = o.ancho;
		ojo.alto = o.alto;
		ojo.pose.posicion = glm::vec3(v.pose.position.x, v.pose.position.y, v.pose.position.z);
		ojo.pose.orientacion = glm::quat(v.pose.orientation.w, v.pose.orientation.x, v.pose.orientation.y, v.pose.orientation.z);
		ojo.fov = { v.fov.angleLeft, v.fov.angleRight, v.fov.angleUp, v.fov.angleDown };
		o.dibujada = true;
		return true;
	}

	void GestorOpenXR::LiberarOjo(int indice)
	{
		if (indice < 0 || indice >= NumeroDeOjos()) return;
		CadenaOjo& o = impl->ojos[indice];
		if (!o.adquirida) return;
		XrSwapchainImageReleaseInfo liberar = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
		xrReleaseSwapchainImage(o.cadena, &liberar);
		o.adquirida = false;
	}

	void GestorOpenXR::TerminarFotograma()
	{
		if (!impl->fotogramaAbierto) return;
		bool todosDibujados = impl->vistasValidas && !impl->ojos.empty();
		for (int i = 0; i < NumeroDeOjos(); ++i)
		{
			LiberarOjo(i);   // por si el llamador olvidó liberar alguno
			todosDibujados = todosDibujados && impl->ojos[i].dibujada;
		}

		std::vector<XrCompositionLayerProjectionView> capas(impl->ojos.size(), { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW });
		XrCompositionLayerProjection proyeccion = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
		const XrCompositionLayerBaseHeader* lista[1] = { reinterpret_cast<const XrCompositionLayerBaseHeader*>(&proyeccion) };
		if (todosDibujados)
		{
			for (size_t i = 0; i < capas.size(); ++i)
			{
				capas[i].pose = impl->vistas[i].pose;
				capas[i].fov = impl->vistas[i].fov;
				capas[i].subImage.swapchain = impl->ojos[i].cadena;
				capas[i].subImage.imageRect.offset = { 0, 0 };
				capas[i].subImage.imageRect.extent = { impl->ojos[i].ancho, impl->ojos[i].alto };
				capas[i].subImage.imageArrayIndex = 0;
			}
			proyeccion.space = impl->espacio;
			proyeccion.viewCount = static_cast<uint32_t>(capas.size());
			proyeccion.views = capas.data();
		}

		XrFrameEndInfo fin = { XR_TYPE_FRAME_END_INFO };
		fin.displayTime = impl->tiempoPrediccion;
		fin.environmentBlendMode = impl->mezcla;
		fin.layerCount = todosDibujados ? 1u : 0u;
		fin.layers = todosDibujados ? lista : nullptr;
		const XrResult r = xrEndFrame(impl->sesion, &fin);
		impl->fotogramaAbierto = false;
		impl->vistasValidas = false;
		if (XR_FAILED(r)) mensaje = Texto("xrEndFrame fallo", r);
	}

#else   // ---- Sin OpenXR en esta compilación: la clase existe, pero no hace nada ----

	struct GestorOpenXR::Impl {};

	GestorOpenXR::GestorOpenXR() : impl(std::make_unique<Impl>()) {}
	GestorOpenXR::~GestorOpenXR() = default;
	bool GestorOpenXR::EstaCompilado() { return false; }
	bool GestorOpenXR::TieneRecursos() const { return false; }
	void GestorOpenXR::ProcesarEventos() {}
	void GestorOpenXR::Desactivar() { if (estado != EstadoOpenXR::NoCompilado) { estado = EstadoOpenXR::Desactivado; mensaje = "OpenXR no activado."; } }
	bool GestorOpenXR::SesionEnMarcha() const { return false; }
	bool GestorOpenXR::Enfocada() const { return false; }
	const char* GestorOpenXR::NombreEstadoSesion() const { return "sin sesion"; }
	EspacioReferencia GestorOpenXR::Espacio() const { return EspacioReferencia::Ninguno; }
	int GestorOpenXR::NumeroDeOjos() const { return 0; }
	ResultadoFotograma GestorOpenXR::EmpezarFotograma() { return ResultadoFotograma::NoActivo; }
	bool GestorOpenXR::AdquirirOjo(int, OjoVR&) { return false; }
	void GestorOpenXR::LiberarOjo(int) {}
	void GestorOpenXR::TerminarFotograma() {}

	EstadoOpenXR GestorOpenXR::Activar(GLFWwindow*, int, int)
	{
		estado = EstadoOpenXR::NoCompilado;
		mensaje = "Esta compilacion no incluye OpenXR (macOS, o falta la biblioteca). El juego sigue en modo pantalla.";
		return estado;
	}

#endif
}
