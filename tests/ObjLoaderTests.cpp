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

    int failures = 0;

    // Comprueba el contrato vectorial GLSL: un bvec4 no admite una escritura escalar.
    auto compilar = [](GLenum tipo, const char* fuente) -> GLuint
    {
        const GLuint shader = glCreateShader(tipo);
        glShaderSource(shader, 1, &fuente, nullptr);
        glCompileShader(shader);
        GLint correcto = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &correcto);
        if (!correcto) { glDeleteShader(shader); return 0; }
        return shader;
    };
    const char* vertexSource = "#version 330 core\nuniform bvec4 sw_intensity;\nvoid main(){ gl_Position=vec4(sw_intensity.x?0.1:0.2,sw_intensity.y?0.1:0.2,sw_intensity.z?0.1:0.2,sw_intensity.w?1.0:0.5); }\n";
    const char* fragmentSource = "#version 330 core\nout vec4 color;\nvoid main(){ color=vec4(1.0); }\n";
    const GLuint vertexShader = compilar(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragmentShader = compilar(GL_FRAGMENT_SHADER, fragmentSource);
    GLuint program = 0;
    if (vertexShader != 0 && fragmentShader != 0)
    {
        program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);
        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked)
        {
            glUseProgram(program);
            const GLint location = glGetUniformLocation(program, "sw_intensity");
            glUniform1i(location, GL_TRUE);
            const GLenum errorEscalar = glGetError();
            glUniform4i(location, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            const GLenum errorVector = glGetError();
            if (errorEscalar != GL_INVALID_OPERATION || errorVector != GL_NO_ERROR)
            {
                std::cerr << "FALLO: bvec4 exige glUniform4i; errores " << errorEscalar << ", " << errorVector << ".\n";
                ++failures;
            }
        }
        else { std::cerr << "FALLO: no se pudo enlazar la prueba de tipo bvec4.\n"; ++failures; }
        glUseProgram(0);
        glDeleteProgram(program);
    }
    else { std::cerr << "FALLO: no se compilaron shaders de prueba bvec4.\n"; ++failures; }
    if (vertexShader) glDeleteShader(vertexShader);
    if (fragmentShader) glDeleteShader(fragmentShader);

    {
        auto leerShader = [](const fs::path& ruta)
        {
            std::ifstream archivo(ruta, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(archivo), std::istreambuf_iterator<char>());
        };
        const fs::path shaders = fs::path(ESCAPETHEUAB_SOURCE_DIR) / "EntornVGI/shaders";
        const std::string vs = leerShader(shaders / "phong_shdrML.vert");
        const std::string fsSource = leerShader(shaders / "phong_shdrML.frag");
        const char* vsPtr = vs.c_str();
        const char* fsPtr = fsSource.c_str();
        const GLuint vsId = compilar(GL_VERTEX_SHADER, vsPtr);
        const GLuint fsId = compilar(GL_FRAGMENT_SHADER, fsPtr);
        GLuint phong = 0;
        if (vsId != 0 && fsId != 0)
        {
            phong = glCreateProgram();
            glAttachShader(phong, vsId);
            glAttachShader(phong, fsId);
            glLinkProgram(phong);
            GLint linked = GL_FALSE;
            glGetProgramiv(phong, GL_LINK_STATUS, &linked);
            const GLint intensityLocation = glGetUniformLocation(phong, "sw_intensity");
            const GLint emissionLocation = glGetUniformLocation(phong, "material.emission");
            const GLint tileLocation = glGetUniformLocation(phong, "uWorldTileSize");
            if (!linked || intensityLocation < 0 || emissionLocation < 0 || tileLocation < 0)
            {
                std::cerr << "FALLO: GLSL Phong no enlaza o pierde uniforms de material/tiling.\n";
                ++failures;
            }
            else
            {
                glUseProgram(phong);
                glUniform4i(intensityLocation, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                const GLenum intensidadError = glGetError();
                // La llamada anterior del cargador usaba 3 componentes para este vec4.
                glUniform3f(emissionLocation, 0.2f, 0.3f, 0.4f);
                const GLenum emisionError3 = glGetError();
                glUniform4f(emissionLocation, 0.2f, 0.3f, 0.4f, 1.0f);
                const GLenum emisionError4 = glGetError();
                if (intensidadError != GL_NO_ERROR || emisionError3 != GL_INVALID_OPERATION || emisionError4 != GL_NO_ERROR)
                {
                    std::cerr << "FALLO: contrato uniforms Phong: bvec4=" << intensidadError <<
                        ", vec4 con 3 comp=" << emisionError3 << ", vec4 completo=" << emisionError4 << ".\n";
                    ++failures;
                }
            }
        }
        else { std::cerr << "FALLO: GLSL Phong no compila en el contexto OpenGL disponible.\n"; ++failures; }
        glUseProgram(0);
        if (phong) glDeleteProgram(phong);
        if (vsId) glDeleteShader(vsId);
        if (fsId) glDeleteShader(fsId);
    }

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
        const fs::path assets = fs::path(ESCAPETHEUAB_SOURCE_DIR) / "EntornVGI/assets/models/escape_pack";
        const std::vector<fs::path> modelos = {
            assets / "arquitectura/suelo_base.obj",
            assets / "arquitectura/pared.obj",
            assets / "arquitectura/techo.obj",
            assets / "arquitectura/suelo_tecnico.obj",
            assets / "arquitectura/pared_modular.obj",
            assets / "arquitectura/techo_modular.obj",
            assets / "arquitectura/rejilla_sector.obj",
            assets / "arquitectura/suelo_rejilla_3d.obj",
            assets / "scifi/Prop_Light_Wide.obj",
            assets / "scifi/Prop_Computer.obj",
            assets / "scifi/Prop_Crate3.obj",
            assets / "lab/counter_counter/counter_counter.obj",
            assets / "lab/cabinet_cabinet_two_shelves/cabinet_cabinet_two_shelves.obj",
            assets / "lab/machine_microscope/machine_microscope.obj"
        };
        std::ifstream sueloSimple(assets / "arquitectura/suelo_base.obj");
        int triangulosSuelo = 0;
        for (std::string linea; std::getline(sueloSimple, linea); )
            if (linea.rfind("f ", 0) == 0) ++triangulosSuelo;
        if (triangulosSuelo != 2)
        {
            std::cerr << "FALLO: suelo_base.obj debe ser una superficie de dos triangulos.\n";
            ++failures;
        }
        for (const fs::path& modelPath : modelos)
        {
            COBJModel modelo;
            std::string ruta = modelPath.string();
            if (modelo.LoadModel(ruta.data()) != 0)
            {
                std::cerr << "FALLO: OBJ/MTL del laboratorio no carga: " << modelPath << '\n';
                ++failures;
            }
            modelo.netejaTextures_OBJ();
            modelo.netejaVAOList_OBJ();
        }
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
