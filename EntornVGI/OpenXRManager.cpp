#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#define GLFW_EXPOSE_NATIVE_WIN32
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <iostream>
#include <vector>

#include "OpenXRManager.h"
#include <GLFW/glfw3native.h>


// Instancia global o dentro de una clase gestora
XrInstance xrInstance = XR_NULL_HANDLE;
XrSystemId xrSystemId = 0;
XrSession xrSession = XR_NULL_HANDLE;

bool InicializarOpenXRInstance() 
{
    // 1. Comprobar extensiones soportadas por el runtime activo (ej. Meta Quest Link)
    uint32_t extensionCount = 0;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr);
    std::vector<XrExtensionProperties> extensionProperties(extensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
    xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensionProperties.data());

    // Buscamos y habilitamos la extensión de OpenGL obligatoria
    std::vector<const char*> enabledExtensions;
    enabledExtensions.push_back(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);

    // 2. Crear la Instancia de OpenXR
    XrInstanceCreateInfo createInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
    strcpy(createInfo.applicationInfo.applicationName, "EscapeTheUAB_VR");
    createInfo.applicationInfo.applicationVersion = 1;
    strcpy(createInfo.applicationInfo.engineName, "EntornVGI_OpenXR");
    createInfo.applicationInfo.engineVersion = 1;
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    
    createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    createInfo.enabledExtensionNames = enabledExtensions.data();

    XrResult result = xrCreateInstance(&createInfo, &xrInstance);
    if (result != XR_SUCCESS) {
        std::cerr << "Error al crear la instancia OpenXR. Codigo: " << result << "\n";
        return false;
    }

    std::cout << "Instancia OpenXR creada correctamente!\n";
    return true;
}

bool ObtenerSistemaOpenXR() 
{
    XrSystemGetInfo systemInfo = { XR_TYPE_SYSTEM_GET_INFO };
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    
    XrResult result = xrGetSystem(xrInstance, &systemInfo, &xrSystemId);
    if (result != XR_SUCCESS) {
        std::cerr << "No se pudo encontrar un sistema VR compatible (¿Están encendidas y conectadas las Quest?).\n";
        return false;
    }

    std::cout << "Sistema VR detectado con exito (SystemId: " << xrSystemId << ")\n";
    return true;
}

bool CrearSesionOpenXR(GLFWwindow* glfwWindow) 
{
    // 1. Obtener los requerimientos gráficos de OpenGL para el sistema OpenXR
    PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGetOpenGLGraphicsRequirementsKHR = nullptr;
    xrGetInstanceProcAddr(xrInstance, "xrGetOpenGLGraphicsRequirementsKHR", 
                          reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetOpenGLGraphicsRequirementsKHR));

    XrGraphicsRequirementsOpenGLKHR graphicsRequirements = { XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR };
    pfnGetOpenGLGraphicsRequirementsKHR(xrInstance, xrSystemId, &graphicsRequirements);

    // 2. Extraer el contexto de Windows WGL desde GLFW
    HDC hdc = GetDC(glfwGetWin32Window(glfwWindow));
    HGLRC hglrc = wglGetCurrentContext();

    if (!hdc || !hglrc) {
        std::cerr << "Error: No se pudo obtener el contexto WGL de la ventana GLFW.\n";
        return false;
    }

    // 3. Preparar la estructura de bindings gráficos para Windows
    XrGraphicsBindingOpenGLWin32KHR graphicsBinding = { XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR };
    graphicsBinding.hDC = hdc;
    graphicsBinding.hGLRC = hglrc;

    // 4. Crear la Sesión OpenXR uniendo el sistema y los gráficos
    XrSessionCreateInfo sessionCreateInfo = { XR_TYPE_SESSION_CREATE_INFO };
    sessionCreateInfo.next = &graphicsBinding; // ¡Aquí vinculamos OpenGL a la sesión!
    sessionCreateInfo.systemId = xrSystemId;

    XrResult result = xrCreateSession(xrInstance, &sessionCreateInfo, &xrSession);
    if (result != XR_SUCCESS) {
        std::cerr << "Error al crear la sesion OpenXR con OpenGL. Codigo: " << result << "\n";
        return false;
    }

    std::cout << "¡Sesion OpenXR y contexto OpenGL vinculados con exito!\n";
    return true;
}