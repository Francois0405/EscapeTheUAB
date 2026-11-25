// OpenXRManager.cpp
// Ver OpenXRManager.h para el alcance (Etapas A, B y C).
#include "src/vr/OpenXRManager.h"

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
	const char* NombreDelEstado(OpenXrState estado)
	{
		switch (estado)
		{
		case OpenXrState::Desactivado:             return "Desactivado";
		case OpenXrState::NoCompilado:             return "No compilado";
		case OpenXrState::SinRuntime:              return "Sin runtime OpenXR";
		case OpenXrState::SinVisor:                return "Sin visor";
		case OpenXrState::FaltaExtensionOpenGL:    return "Falta XR_KHR_opengl_enable";
		case OpenXrState::OpenGLInsuficiente:      return "OpenGL insuficiente";
		case OpenXrState::InstanciaYSistemaListos: return "Visor detectado (sin sesion en esta plataforma)";
		case OpenXrState::SesionCreada:            return "Sesion creada";
		case OpenXrState::Error:                   return "Error";
		}
		return "?";
	}

	GamePlayMode ElegirModoDeJuego(bool vrSolicitado, OpenXrState estado)
	{
		return (vrSolicitado && estado == OpenXrState::SesionCreada) ? GamePlayMode::VR : GamePlayMode::Pantalla;
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
	struct OpenXrManager::Impl
	{
		XrInstance instancia = XR_NULL_HANDLE;
		XrSystemId sistema = XR_NULL_SYSTEM_ID;
		XrSession sesion = XR_NULL_HANDLE;
		XrSpace espacio = XR_NULL_HANDLE;
		ReferenceSpaceKind tipoEspacio = ReferenceSpaceKind::Ninguno;

		XrSessionState estadoSesion = XR_SESSION_STATE_UNKNOWN;
		bool sesionIniciada = false;                    // entre xrBeginSession y xrEndSession

		XrEnvironmentBlendMode mezcla = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
		std::vector<CadenaOjo> ojos;
		std::vector<XrView> vistas;

		bool fotogramaAbierto = false;                  // xrBeginFrame hecho, falta xrEndFrame
		bool vistasValidas = false;                     // xrLocateViews dio poses utilizables este fotograma
		XrTime tiempoPrediccion = 0;

		XrActionSet actionSet = XR_NULL_HANDLE;
		XrAction accionAgarrar = XR_NULL_HANDLE;
		XrPath rutaManos[2] = { XR_NULL_PATH, XR_NULL_PATH }; // 0: izq, 1: der

		bool gatilloAnterior = false;
		bool gatilloActual = false;
		bool gatilloRecienPulsado = false;
		bool gatilloRecienSoltado = false;
		bool gatilloIzqAnterior = false;
		bool gatilloIzqRecienPulsado = false;

		XrAction accionApuntar = XR_NULL_HANDLE;
		XrSpace espacioApuntar[2] = { XR_NULL_HANDLE, XR_NULL_HANDLE };
		XrPose poseMando[2];
		bool poseMandoValida[2] = { false, false };
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
		OpenXrState ComprobarExtensionOpenGL(std::string& mensaje, std::vector<std::string>& lista)
		{
			uint32_t cuantas = 0;
			XrResult r = xrEnumerateInstanceExtensionProperties(nullptr, 0, &cuantas, nullptr);
			if (XR_FAILED(r))
			{
				mensaje = EsFaltaDeRuntime(r) ? "No hay un runtime OpenXR activo (abre Meta Horizon Link / Quest Link, SteamVR u otro)."
					: Texto("No se pudieron enumerar las extensiones de OpenXR", r);
				return EsFaltaDeRuntime(r) ? OpenXrState::SinRuntime : OpenXrState::Error;
			}
			std::vector<XrExtensionProperties> extensiones(cuantas, { XR_TYPE_EXTENSION_PROPERTIES });
			r = xrEnumerateInstanceExtensionProperties(nullptr, cuantas, &cuantas, extensiones.data());
			if (XR_FAILED(r)) { mensaje = Texto("Fallo al leer las extensiones de OpenXR", r); return OpenXrState::Error; }

			bool hayOpenGL = false;
			for (const XrExtensionProperties& e : extensiones)
			{
				lista.push_back(e.extensionName);
				if (std::strcmp(e.extensionName, XR_KHR_OPENGL_ENABLE_EXTENSION_NAME) == 0) hayOpenGL = true;
			}
			if (hayOpenGL) return OpenXrState::Desactivado;   // "sin problema"
			mensaje = "El runtime OpenXR no ofrece XR_KHR_opengl_enable: no puede dibujar con OpenGL.";
			return OpenXrState::FaltaExtensionOpenGL;
		}

		// Etapa A, paso 2: crear la instancia (la "conexión" con el runtime).
		OpenXrState CrearInstancia(XrInstance& instancia, std::string& mensaje)
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
			if (XR_SUCCEEDED(r)) return OpenXrState::Desactivado;
			instancia = XR_NULL_HANDLE;
			mensaje = EsFaltaDeRuntime(r) ? "No hay un runtime OpenXR activo (abre Meta Horizon Link / Quest Link, SteamVR u otro)."
				: Texto("No se pudo crear la instancia de OpenXR", r);
			return EsFaltaDeRuntime(r) ? OpenXrState::SinRuntime : OpenXrState::Error;
		}

		// Etapa A, paso 3: buscar el visor.
		OpenXrState BuscarVisor(XrInstance instancia, XrSystemId& sistema, std::string& mensaje)
		{
			XrSystemGetInfo info = { XR_TYPE_SYSTEM_GET_INFO };
			info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
			const XrResult r = xrGetSystem(instancia, &info, &sistema);
			if (XR_SUCCEEDED(r)) return OpenXrState::Desactivado;
			if (r == XR_ERROR_FORM_FACTOR_UNAVAILABLE)
			{
				mensaje = "Hay runtime OpenXR pero no hay ningun visor conectado (enciende el casco y activa Meta Horizon Link / Quest Link).";
				return OpenXrState::SinVisor;
			}
			mensaje = Texto("No se pudo consultar el visor", r);
			return OpenXrState::Error;
		}

		// Etapa A, paso 4: OpenXR EXIGE llamar a esta función antes de crear la sesión, y la versión de
		// OpenGL de nuestra ventana debe ser al menos la que pide el runtime.
		OpenXrState ComprobarVersionOpenGL(XrInstance instancia, XrSystemId sistema, int mayor, int menor, std::string& mensaje,
			std::string& diagnostico)
		{
			PFN_xrGetOpenGLGraphicsRequirementsKHR pedirRequisitos = nullptr;
			XrResult r = xrGetInstanceProcAddr(instancia, "xrGetOpenGLGraphicsRequirementsKHR",
				reinterpret_cast<PFN_xrVoidFunction*>(&pedirRequisitos));
			if (XR_FAILED(r) || pedirRequisitos == nullptr)
			{
				mensaje = Texto("No se encontro xrGetOpenGLGraphicsRequirementsKHR", r);
				return OpenXrState::FaltaExtensionOpenGL;
			}
			XrGraphicsRequirementsOpenGLKHR requisitos = { XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR };
			r = pedirRequisitos(instancia, sistema, &requisitos);
			if (XR_FAILED(r)) { mensaje = Texto("No se pudieron leer los requisitos graficos", r); return OpenXrState::Error; }

			diagnostico += "OpenGL del runtime: minimo " + VersionTexto(requisitos.minApiVersionSupported) + ", maximo probado " +
				VersionTexto(requisitos.maxApiVersionSupported) + "; ventana " + std::to_string(mayor) + "." + std::to_string(menor) + "\n";
			if (XR_MAKE_VERSION(mayor, menor, 0) < requisitos.minApiVersionSupported)
			{
				mensaje = "El runtime necesita OpenGL " + std::to_string(XR_VERSION_MAJOR(requisitos.minApiVersionSupported)) + "." +
					std::to_string(XR_VERSION_MINOR(requisitos.minApiVersionSupported)) + " o superior y la ventana tiene " +
					std::to_string(mayor) + "." + std::to_string(menor) + ".";
				return OpenXrState::OpenGLInsuficiente;
			}
			return OpenXrState::Desactivado;
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
		OpenXrState CrearSesion(XrInstance instancia, XrSystemId sistema, XrSession& sesion, std::string& mensaje)
		{
			const HDC dispositivo = wglGetCurrentDC();      // los de la ventana ACTIVA ahora mismo
			const HGLRC contexto = wglGetCurrentContext();
			if (dispositivo == nullptr || contexto == nullptr)
			{
				mensaje = "No hay un contexto OpenGL (WGL) activo: la sesion VR necesita la ventana ya creada.";
				return OpenXrState::Error;
			}
			XrGraphicsBindingOpenGLWin32KHR enlace = { XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR };
			enlace.hDC = dispositivo;
			enlace.hGLRC = contexto;

			XrSessionCreateInfo info = { XR_TYPE_SESSION_CREATE_INFO };
			info.next = &enlace;    // aqui se une OpenGL a la sesion
			info.systemId = sistema;
			const XrResult r = xrCreateSession(instancia, &info, &sesion);
			if (XR_FAILED(r)) { sesion = XR_NULL_HANDLE; mensaje = Texto("No se pudo crear la sesion OpenXR con OpenGL", r); return OpenXrState::Error; }
			return OpenXrState::SesionCreada;
		}

		// Etapa C, paso 1: espacio de referencia. STAGE (suelo) si el runtime lo ofrece; si no, LOCAL.
		OpenXrState CrearEspacio(XrSession sesion, XrSpace& espacio, ReferenceSpaceKind& tipo, std::string& mensaje, std::string& d)
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
				if (XR_SUCCEEDED(r)) tipo = ReferenceSpaceKind::Suelo;
			}
			if (XR_FAILED(r) && hayLocal)
			{
				info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
				r = xrCreateReferenceSpace(sesion, &info, &espacio);
				if (XR_SUCCEEDED(r)) tipo = ReferenceSpaceKind::Local;
			}
			if (XR_FAILED(r)) { espacio = XR_NULL_HANDLE; mensaje = Texto("No se pudo crear un espacio de referencia (STAGE ni LOCAL)", r); return OpenXrState::Error; }
			d += std::string("Espacio elegido: ") + (tipo == ReferenceSpaceKind::Suelo ? "STAGE (origen en el suelo)" : "LOCAL (origen en la cabeza)") + "\n";
			return OpenXrState::SesionCreada;
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

		// Etapa D: crear el ActionSet, la acción "agarrar" y vincularla al gatillo frontal de los mandos.
		// Etapa D: crear el ActionSet, las acciones "agarrar" y "apuntar_mando" y vincularlas.
		bool CrearAcciones(XrInstance instancia, XrSession sesion, XrActionSet& actionSet,
			XrAction& accionAgarrar, XrAction& accionApuntar, XrPath rutaManos[2],
			XrSpace espacioApuntar[2], std::string& mensaje, std::string& d)
		{
			XrActionSetCreateInfo setInfo = { XR_TYPE_ACTION_SET_CREATE_INFO };
			std::strncpy(setInfo.actionSetName, "acciones_juego", XR_MAX_ACTION_SET_NAME_SIZE - 1);
			std::strncpy(setInfo.localizedActionSetName, "Acciones del Juego", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE - 1);
			setInfo.priority = 0;
			XrResult r = xrCreateActionSet(instancia, &setInfo, &actionSet);
			if (XR_FAILED(r)) { mensaje = Texto("No se pudo crear el ActionSet de OpenXR", r); return false; }

			xrStringToPath(instancia, "/user/hand/left", &rutaManos[0]);
			xrStringToPath(instancia, "/user/hand/right", &rutaManos[1]);

			// 1. Acción booleana de agarrar
			XrActionCreateInfo actionInfo = { XR_TYPE_ACTION_CREATE_INFO };
			actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
			std::strncpy(actionInfo.actionName, "agarrar", XR_MAX_ACTION_NAME_SIZE - 1);
			std::strncpy(actionInfo.localizedActionName, "Agarrar", XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
			actionInfo.countSubactionPaths = 2;
			actionInfo.subactionPaths = rutaManos;
			r = xrCreateAction(actionSet, &actionInfo, &accionAgarrar);
			if (XR_FAILED(r)) { mensaje = Texto("No se pudo crear la accion agarrar", r); return false; }

			// 2. Acción de pose para apuntar con los mandos
			XrActionCreateInfo poseInfo = { XR_TYPE_ACTION_CREATE_INFO };
			poseInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
			std::strncpy(poseInfo.actionName, "apuntar_mando", XR_MAX_ACTION_NAME_SIZE - 1);
			std::strncpy(poseInfo.localizedActionName, "Apuntar Mando", XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
			poseInfo.countSubactionPaths = 2;
			poseInfo.subactionPaths = rutaManos;
			r = xrCreateAction(actionSet, &poseInfo, &accionApuntar);
			if (XR_FAILED(r)) { mensaje = Texto("No se pudo crear la accion apuntar_mando", r); return false; }

			// Rutas físicas
			XrPath triggerIzq = XR_NULL_PATH, triggerDer = XR_NULL_PATH;
			XrPath selectIzq = XR_NULL_PATH, selectDer = XR_NULL_PATH;
			XrPath aimIzq = XR_NULL_PATH, aimDer = XR_NULL_PATH;
			xrStringToPath(instancia, "/user/hand/left/input/trigger/value", &triggerIzq);
			xrStringToPath(instancia, "/user/hand/right/input/trigger/value", &triggerDer);
			xrStringToPath(instancia, "/user/hand/left/input/select/click", &selectIzq);
			xrStringToPath(instancia, "/user/hand/right/input/select/click", &selectDer);
			xrStringToPath(instancia, "/user/hand/left/input/aim/pose", &aimIzq);
			xrStringToPath(instancia, "/user/hand/right/input/aim/pose", &aimDer);

			// 3. Perfil Oculus Touch (Meta Quest / Rift)
			XrPath perfilOculus = XR_NULL_PATH;
			xrStringToPath(instancia, "/interaction_profiles/oculus/touch_controller", &perfilOculus);
			XrActionSuggestedBinding bindingsOculus[] = {
				{ accionAgarrar, triggerIzq },
				{ accionAgarrar, triggerDer },
				{ accionApuntar, aimIzq },
				{ accionApuntar, aimDer }
			};
			XrInteractionProfileSuggestedBinding sugOculus = { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
			sugOculus.interactionProfile = perfilOculus;
			sugOculus.suggestedBindings = bindingsOculus;
			sugOculus.countSuggestedBindings = 4;
			xrSuggestInteractionProfileBindings(instancia, &sugOculus);

			// 4. Perfil genérico Khronos Simple Controller (respaldo)
			XrPath perfilSimple = XR_NULL_PATH;
			xrStringToPath(instancia, "/interaction_profiles/khr/simple_controller", &perfilSimple);
			XrActionSuggestedBinding bindingsSimple[] = {
				{ accionAgarrar, selectIzq },
				{ accionAgarrar, selectDer },
				{ accionApuntar, aimIzq },
				{ accionApuntar, aimDer }
			};
			XrInteractionProfileSuggestedBinding sugSimple = { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
			sugSimple.interactionProfile = perfilSimple;
			sugSimple.suggestedBindings = bindingsSimple;
			sugSimple.countSuggestedBindings = 4;
			xrSuggestInteractionProfileBindings(instancia, &sugSimple);

			// 5. Adjuntar el ActionSet a la sesión (SIEMPRE DESPUÉS de crear todas las acciones y bindings)
			XrSessionActionSetsAttachInfo attachInfo = { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
			attachInfo.countActionSets = 1;
			attachInfo.actionSets = &actionSet;
			r = xrAttachSessionActionSets(sesion, &attachInfo);
			if (XR_FAILED(r)) { mensaje = Texto("No se pudieron adjuntar las acciones a la sesion", r); return false; }

			// 6. Crear espacios de acción para rastrear la pose de ambos mandos
			for (int i = 0; i < 2; ++i)
			{
				XrActionSpaceCreateInfo spaceInfo = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
				spaceInfo.action = accionApuntar;
				spaceInfo.subactionPath = rutaManos[i];
				spaceInfo.poseInActionSpace.orientation.w = 1.0f;
				r = xrCreateActionSpace(sesion, &spaceInfo, &espacioApuntar[i]);
				if (XR_FAILED(r)) { mensaje = Texto("No se pudo crear XrActionSpace del mando", r); return false; }
			}

			d += "Acciones de mandos listas (gatillo frontal y apuntado vinculados).\n";
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

	OpenXrManager::OpenXrManager() : impl(std::make_unique<Impl>()) {}
	OpenXrManager::~OpenXrManager() { Desactivar(); }
	bool OpenXrManager::EstaCompilado() { return true; }
	bool OpenXrManager::TieneRecursos() const { return impl->instancia != XR_NULL_HANDLE || impl->sesion != XR_NULL_HANDLE; }
	bool OpenXrManager::SesionEnMarcha() const { return impl->sesion != XR_NULL_HANDLE && impl->sesionIniciada; }
	bool OpenXrManager::Enfocada() const { return SesionEnMarcha() && impl->estadoSesion == XR_SESSION_STATE_FOCUSED; }
	const char* OpenXrManager::NombreEstadoSesion() const { return impl->sesion != XR_NULL_HANDLE ? NombreSesion(impl->estadoSesion) : "sin sesion"; }
	ReferenceSpaceKind OpenXrManager::Espacio() const { return impl->tipoEspacio; }
	int OpenXrManager::NumeroDeOjos() const { return static_cast<int>(impl->ojos.size()); }
	bool OpenXrManager::GatilloRecienPulsado() const { return impl->gatilloRecienPulsado; }
	bool OpenXrManager::GatilloRecienSoltado() const { return impl->gatilloRecienSoltado; }
	bool OpenXrManager::GatilloPulsado() const { return impl->gatilloActual; }
	bool OpenXrManager::GatilloIzquierdoRecienPulsado() const { return impl->gatilloIzqRecienPulsado; }

	void OpenXrManager::Desactivar()
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
		if (impl->espacioApuntar[0] != XR_NULL_HANDLE) xrDestroySpace(impl->espacioApuntar[0]);
		if (impl->espacioApuntar[1] != XR_NULL_HANDLE) xrDestroySpace(impl->espacioApuntar[1]);
		if (impl->accionApuntar != XR_NULL_HANDLE) xrDestroyAction(impl->accionApuntar);
		if (impl->accionAgarrar != XR_NULL_HANDLE) xrDestroyAction(impl->accionAgarrar);
		if (impl->actionSet != XR_NULL_HANDLE) xrDestroyActionSet(impl->actionSet);
		if (impl->espacio != XR_NULL_HANDLE) xrDestroySpace(impl->espacio);
		if (impl->sesion != XR_NULL_HANDLE)
		{
			if (impl->sesionIniciada) xrEndSession(impl->sesion);
			xrDestroySession(impl->sesion);
		}
		if (impl->instancia != XR_NULL_HANDLE) xrDestroyInstance(impl->instancia);
		impl->gatilloIzqAnterior = false;
		impl->gatilloIzqRecienPulsado = false;
		impl->espacioApuntar[0] = XR_NULL_HANDLE;
		impl->espacioApuntar[1] = XR_NULL_HANDLE;
		impl->accionApuntar = XR_NULL_HANDLE;
		impl->poseMandoValida[0] = false;
		impl->poseMandoValida[1] = false;
		impl->gatilloRecienSoltado = false;
		impl->accionAgarrar = XR_NULL_HANDLE;
		impl->actionSet = XR_NULL_HANDLE;
		impl->rutaManos[0] = XR_NULL_PATH;
		impl->rutaManos[1] = XR_NULL_PATH;
		impl->gatilloAnterior = false;
		impl->gatilloActual = false;
		impl->gatilloRecienPulsado = false;
		impl->espacio = XR_NULL_HANDLE;
		impl->sesion = XR_NULL_HANDLE;
		impl->instancia = XR_NULL_HANDLE;
		impl->sistema = XR_NULL_SYSTEM_ID;
		impl->tipoEspacio = ReferenceSpaceKind::Ninguno;
		impl->estadoSesion = XR_SESSION_STATE_UNKNOWN;
		impl->sesionIniciada = false;
		impl->vistasValidas = false;
		if (estado != OpenXrState::Desactivado && estado != OpenXrState::NoCompilado) { estado = OpenXrState::Desactivado; mensaje = "OpenXR desactivado."; }
	}

	OpenXrState OpenXrManager::Activar(GLFWwindow* ventana, int glMajorVersion, int glMinorVersion)
	{
		(void)ventana;   // solo se usa en Windows (ya hay un contexto activo); se conserva para otras plataformas
		if (TieneRecursos()) return estado;   // ya activo: no se crea nada dos veces
		std::string aviso;
		std::string informe;
		std::vector<std::string> extensiones;

		// Cada paso devuelve "Desactivado" si todo fue bien; si no, el motivo. Al primer fallo se limpia todo.
		OpenXrState resultado = ComprobarExtensionOpenGL(aviso, extensiones);
		if (resultado == OpenXrState::Desactivado) resultado = CrearInstancia(impl->instancia, aviso);
		if (resultado == OpenXrState::Desactivado) resultado = BuscarVisor(impl->instancia, impl->sistema, aviso);
		if (resultado == OpenXrState::Desactivado)
		{
			DescribirRuntimeYVisor(impl->instancia, impl->sistema, informe);
			resultado = ComprobarVersionOpenGL(impl->instancia, impl->sistema, glMajorVersion, glMinorVersion, aviso, informe);
		}
		if (resultado == OpenXrState::Desactivado)
		{
#if defined(_WIN32)
			resultado = CrearSesion(impl->instancia, impl->sistema, impl->sesion, aviso);
			if (resultado == OpenXrState::SesionCreada) resultado = CrearEspacio(impl->sesion, impl->espacio, impl->tipoEspacio, aviso, informe);
			int64_t formato = 0;
			if (resultado == OpenXrState::SesionCreada && !ElegirFormato(impl->sesion, formato, aviso, informe)) resultado = OpenXrState::Error;
			if (resultado == OpenXrState::SesionCreada &&
				!CrearCadenas(impl->instancia, impl->sistema, impl->sesion, formato, impl->ojos, impl->vistas, impl->mezcla, aviso, informe))
				resultado = OpenXrState::Error;
			if (resultado == OpenXrState::SesionCreada &&
				!CrearAcciones(impl->instancia, impl->sesion, impl->actionSet, impl->accionAgarrar,
					impl->accionApuntar, impl->rutaManos, impl->espacioApuntar, aviso, informe))
				resultado = OpenXrState::Error;
			if (resultado == OpenXrState::SesionCreada)
				aviso = "Sesion OpenXR lista. Esperando a que el runtime la ponga en READY (ponte el casco).";
#else
			resultado = OpenXrState::InstanciaYSistemaListos;
			aviso = "Visor detectado. En esta plataforma no se crea sesion VR (solo implementado en Windows con OpenGL).";
#endif
		}

		const bool exito = resultado == OpenXrState::SesionCreada || resultado == OpenXrState::InstanciaYSistemaListos;
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

	void OpenXrManager::ProcesarEventos()
	{
		impl->gatilloIzqRecienPulsado = false;
		impl->gatilloRecienPulsado = false;
		impl->gatilloRecienSoltado = false;
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

		if (Enfocada() && impl->actionSet != XR_NULL_HANDLE)
		{
			XrActiveActionSet conjuntoActivo = { impl->actionSet, XR_NULL_PATH };
			XrActionsSyncInfo syncInfo = { XR_TYPE_ACTIONS_SYNC_INFO };
			syncInfo.countActiveActionSets = 1;
			syncInfo.activeActionSets = &conjuntoActivo;

			if (XR_SUCCEEDED(xrSyncActions(impl->sesion, &syncInfo)))
			{
				XrActionStateGetInfo getInfoIzq = { XR_TYPE_ACTION_STATE_GET_INFO };
				getInfoIzq.action = impl->accionAgarrar;
				getInfoIzq.subactionPath = impl->rutaManos[0]; // Mano izquierda (0)

				XrActionStateBoolean estadoBoolIzq = { XR_TYPE_ACTION_STATE_BOOLEAN };
				bool pulsadoIzquierda = false;
				if (XR_SUCCEEDED(xrGetActionStateBoolean(impl->sesion, &getInfoIzq, &estadoBoolIzq)))
				{
					if (estadoBoolIzq.isActive && estadoBoolIzq.currentState == XR_TRUE)
						pulsadoIzquierda = true;
				}
				impl->gatilloIzqRecienPulsado = (pulsadoIzquierda && !impl->gatilloIzqAnterior);
				impl->gatilloIzqAnterior = pulsadoIzquierda;

				XrActionStateGetInfo getInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
				getInfo.action = impl->accionAgarrar;
				getInfo.subactionPath = impl->rutaManos[1]; // SOLO mano derecha (1)

				XrActionStateBoolean estadoBool = { XR_TYPE_ACTION_STATE_BOOLEAN };
				bool pulsadoDerecha = false;
				if (XR_SUCCEEDED(xrGetActionStateBoolean(impl->sesion, &getInfo, &estadoBool)))
				{
					if (estadoBool.isActive && estadoBool.currentState == XR_TRUE)
						pulsadoDerecha = true;
				}

				impl->gatilloActual = pulsadoDerecha;
				impl->gatilloRecienPulsado = (impl->gatilloActual && !impl->gatilloAnterior);
				impl->gatilloRecienSoltado = (!impl->gatilloActual && impl->gatilloAnterior);
				impl->gatilloAnterior = impl->gatilloActual;
			}
		}
		else
		{
			impl->gatilloActual = false;
			impl->gatilloAnterior = false;
			impl->gatilloIzqAnterior = false;
		}
	}

	FrameResult OpenXrManager::EmpezarFotograma()
	{
		if (!SesionEnMarcha() || impl->fotogramaAbierto) return FrameResult::NoActivo;

		XrFrameState estadoFotograma = { XR_TYPE_FRAME_STATE };
		XrFrameWaitInfo espera = { XR_TYPE_FRAME_WAIT_INFO };
		XrResult r = xrWaitFrame(impl->sesion, &espera, &estadoFotograma);
		if (XR_FAILED(r)) { mensaje = Texto("xrWaitFrame fallo", r); return FrameResult::NoActivo; }
		XrFrameBeginInfo inicio = { XR_TYPE_FRAME_BEGIN_INFO };
		r = xrBeginFrame(impl->sesion, &inicio);
		if (XR_FAILED(r)) { mensaje = Texto("xrBeginFrame fallo", r); return FrameResult::NoActivo; }
		impl->fotogramaAbierto = true;
		impl->tiempoPrediccion = estadoFotograma.predictedDisplayTime;
		impl->vistasValidas = false;
		for (CadenaOjo& o : impl->ojos) o.dibujada = false;

		if (!estadoFotograma.shouldRender) return FrameResult::SinRender;

		XrViewLocateInfo localizar = { XR_TYPE_VIEW_LOCATE_INFO };
		localizar.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
		localizar.displayTime = estadoFotograma.predictedDisplayTime;
		localizar.space = impl->espacio;
		XrViewState estadoVistas = { XR_TYPE_VIEW_STATE };
		uint32_t cuantas = 0;
		r = xrLocateViews(impl->sesion, &localizar, &estadoVistas, static_cast<uint32_t>(impl->vistas.size()), &cuantas, impl->vistas.data());
		for (int i = 0; i < 2; ++i)
		{
			impl->poseMandoValida[i] = false;
			if (impl->espacioApuntar[i] == XR_NULL_HANDLE) continue;

			XrSpaceLocation loc = { XR_TYPE_SPACE_LOCATION };
			if (XR_SUCCEEDED(xrLocateSpace(impl->espacioApuntar[i], impl->espacio, estadoFotograma.predictedDisplayTime, &loc)))
			{
				const XrSpaceLocationFlags flagsReq = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
				if ((loc.locationFlags & flagsReq) == flagsReq)
				{
					impl->poseMando[i].posicion = glm::vec3(loc.pose.position.x, loc.pose.position.y, loc.pose.position.z);
					impl->poseMando[i].orientacion = glm::quat(loc.pose.orientation.w, loc.pose.orientation.x, loc.pose.orientation.y, loc.pose.orientation.z);
					impl->poseMandoValida[i] = true;
				}
			}
		}
		const XrViewStateFlags necesarias = XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;
		if (XR_FAILED(r) || (estadoVistas.viewStateFlags & necesarias) != necesarias || cuantas != impl->vistas.size())
			return FrameResult::SinRender;   // sin seguimiento valido este fotograma: se envia vacio
		impl->vistasValidas = true;
		return FrameResult::Renderizar;
	}

	bool OpenXrManager::AdquirirOjo(int indice, VrEye& ojo)
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

	void OpenXrManager::LiberarOjo(int indice)
	{
		if (indice < 0 || indice >= NumeroDeOjos()) return;
		CadenaOjo& o = impl->ojos[indice];
		if (!o.adquirida) return;
		XrSwapchainImageReleaseInfo liberar = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
		xrReleaseSwapchainImage(o.cadena, &liberar);
		o.adquirida = false;
	}

	void OpenXrManager::TerminarFotograma()
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

	bool OpenXrManager::PoseApuntadoMando(int mano, XrPose& poseOut) const
	{
		if (mano < 0 || mano > 1 || !impl->poseMandoValida[mano]) return false;
		poseOut = impl->poseMando[mano];
		return true;
	}

#else   // ---- Sin OpenXR en esta compilación: la clase existe, pero no hace nada ----

	struct OpenXrManager::Impl {};

	OpenXrManager::OpenXrManager() : impl(std::make_unique<Impl>()) {}
	OpenXrManager::~OpenXrManager() = default;
	bool OpenXrManager::EstaCompilado() { return false; }
	bool OpenXrManager::TieneRecursos() const { return false; }
	void OpenXrManager::ProcesarEventos() {}
	void OpenXrManager::Desactivar() { if (estado != OpenXrState::NoCompilado) { estado = OpenXrState::Desactivado; mensaje = "OpenXR no activado."; } }
	bool OpenXrManager::SesionEnMarcha() const { return false; }
	bool OpenXrManager::Enfocada() const { return false; }
	const char* OpenXrManager::NombreEstadoSesion() const { return "sin sesion"; }
	ReferenceSpaceKind OpenXrManager::Espacio() const { return ReferenceSpaceKind::Ninguno; }
	int OpenXrManager::NumeroDeOjos() const { return 0; }
	FrameResult OpenXrManager::EmpezarFotograma() { return FrameResult::NoActivo; }
	bool OpenXrManager::AdquirirOjo(int, VrEye&) { return false; }
	void OpenXrManager::LiberarOjo(int) {}
	void OpenXrManager::TerminarFotograma() {}
	bool OpenXrManager::GatilloRecienPulsado() const { return false; }
	bool OpenXrManager::GatilloRecienSoltado() const { return false; }
	bool OpenXrManager::GatilloPulsado() const { return false; }
	bool OpenXrManager::PoseApuntadoMando(int, XrPose&) const { return false; }
	bool OpenXrManager::GatilloIzquierdoRecienPulsado() const { return false; }

	OpenXrState OpenXrManager::Activar(GLFWwindow*, int, int)
	{
		estado = OpenXrState::NoCompilado;
		mensaje = "Esta compilacion no incluye OpenXR (macOS, o falta la biblioteca). El juego sigue en modo pantalla.";
		return estado;
	}

#endif
}
