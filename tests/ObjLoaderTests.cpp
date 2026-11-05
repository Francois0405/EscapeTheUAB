#include "framework.h"
#include "objLoader.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace
{
    void escribir(const fs::path& ruta, const std::string& contenido)
    {
        std::ofstream archivo(ruta, std::ios::binary);
        archivo << contenido;
    }
}

int main()
{
    if (glfwInit() != GLFW_TRUE)
    {
        std::cout << "SKIP: GLFW no puede inicializarse sin un entorno grafico.\n";
        return 77;
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(64, 64, "ObjLoaderTests", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        std::cout << "SKIP: no se pudo crear un contexto OpenGL oculto.\n";
        return 77;
    }
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        std::cout << "SKIP: GLEW no pudo inicializarse en el contexto de prueba.\n";
        return 77;
    }
    glGetError(); // Consume el GL_INVALID_ENUM que GLEW puede generar en core profile.

    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path temp = fs::temp_directory_path() / ("EscapeTheUAB-ObjLoaderTests-" + std::to_string(unique));
    fs::create_directories(temp);
    escribir(temp / "plain.mtl",
        "newmtl Plain\nKa 0.1 0.1 0.1\nKd 0.8 0.7 0.6\nKs 0.0 0.0 0.0\nNs 8\n");
    escribir(temp / "plain.obj",
        "mtllib plain.mtl\n"
        "usemtl Plain\n"
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1 2 3\n");
    escribir(temp / "bad-index.obj",
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1 2 4\n");

    int failures = 0;
    {
        COBJModel plain;
        std::string plainPath = (temp / "plain.obj").string();
        if (plain.LoadModel(plainPath.data()) != 0)
        {
            std::cerr << "FALLO: OBJ con material sin map_Kd.\n";
            ++failures;
        }
        plain.netejaTextures_OBJ();
        plain.netejaVAOList_OBJ();
    }
    {
        COBJModel malformed;
        std::string malformedPath = (temp / "bad-index.obj").string();
        if (malformed.LoadModel(malformedPath.data()) == 0)
        {
            std::cerr << "FALLO: el indice de cara fuera de rango debe rechazarse.\n";
            ++failures;
        }
        malformed.netejaTextures_OBJ();
        malformed.netejaVAOList_OBJ();
    }
    {
        COBJModel phone;
        std::string phonePath = (fs::path(ESCAPETHEUAB_SOURCE_DIR) /
            "EntornVGI/assets/models/Phone.obj").string();
        if (phone.LoadModel(phonePath.data()) != 0)
        {
            std::cerr << "FALLO: no se pudo cargar Phone.obj con su MTL y map_Kd relativo.\n";
            ++failures;
        }
        phone.netejaTextures_OBJ();
        phone.netejaVAOList_OBJ();
    }
    {
        COBJModel missing;
        std::string missingPath = (temp / "missing.obj").string();
        if (missing.LoadModel(missingPath.data()) == 0)
        {
            std::cerr << "FALLO: un OBJ inexistente no debe cargarse.\n";
            ++failures;
        }
    }

    std::error_code cleanupError;
    fs::remove_all(temp, cleanupError);
    if (cleanupError)
    {
        std::cerr << "No se pudo limpiar el directorio temporal: " << cleanupError.message() << '\n';
        ++failures;
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    if (failures == 0) std::cout << "ObjLoaderTests: OK\n";
    return failures == 0 ? 0 : 1;
}
