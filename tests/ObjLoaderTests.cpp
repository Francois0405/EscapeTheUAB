#include "framework.h"
#include "objLoader.h"
#include <SOIL2.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    void escribir(const fs::path& ruta, const std::string& contenido)
    {
        std::ofstream archivo(ruta, std::ios::binary);
        archivo << contenido;
    }

bool validarGeometriaCuadroBotanico(const fs::path& ruta)
{
	std::ifstream archivo(ruta);
	if (!archivo) return false;

	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> uv;
	std::vector<glm::vec3> normales;
	std::set<std::array<int, 3>> triangulos;
	int cantidadTriangulos = 0;
	bool mtllibOriginal = false;
	bool materialOriginal = false;
	std::string linea;
	while (std::getline(archivo, linea))
	{
		std::istringstream entrada(linea);
		std::string tipo;
		if (!(entrada >> tipo) || tipo[0] == '#') continue;
		if (tipo == "v")
		{
			glm::vec3 v{};
			if (!(entrada >> v.x >> v.y >> v.z)) return false;
			vertices.push_back(v);
		}
		else if (tipo == "vt")
		{
			glm::vec2 t{};
			if (!(entrada >> t.x >> t.y)) return false;
			uv.push_back(t);
		}
		else if (tipo == "vn")
		{
			glm::vec3 n{};
			if (!(entrada >> n.x >> n.y >> n.z)) return false;
			normales.push_back(n);
		}
		else if (tipo == "mtllib") mtllibOriginal = linea.find("clue_botanical_frame.mtl") != std::string::npos;
		else if (tipo == "usemtl") materialOriginal = linea.find("BotanicalPlate") != std::string::npos;
		else if (tipo == "f")
		{
			std::array<int, 3> vi{}, ti{}, ni{};
			for (int i = 0; i < 3; ++i)
			{
				std::string token;
				if (!(entrada >> token)) return false;
				std::replace(token.begin(), token.end(), '/', ' ');
				std::istringstream indices(token);
				if (!(indices >> vi[i] >> ti[i] >> ni[i])) return false;
				--vi[i]; --ti[i]; --ni[i];
			}
			std::string extra;
			if (entrada >> extra) return false; // Este activo debe triangularse explícitamente.
			for (int i = 0; i < 3; ++i)
				if (vi[i] < 0 || vi[i] >= static_cast<int>(vertices.size()) ||
					ti[i] < 0 || ti[i] >= static_cast<int>(uv.size()) ||
					ni[i] < 0 || ni[i] >= static_cast<int>(normales.size())) return false;

			const glm::vec3 geometrica = glm::normalize(glm::cross(
				vertices[vi[1]] - vertices[vi[0]], vertices[vi[2]] - vertices[vi[0]]));
			for (int i = 0; i < 3; ++i)
				if (glm::dot(geometrica, glm::normalize(normales[ni[i]])) < 0.999f) return false;

			auto ordenada = vi;
			std::sort(ordenada.begin(), ordenada.end());
			if (!triangulos.insert(ordenada).second) return false;
			++cantidadTriangulos;
		}
	}
	if (vertices.size() != 8 || uv.size() != 4 || normales.size() != 6 ||
		cantidadTriangulos != 12 || !mtllibOriginal || !materialOriginal) return false;

	const std::array<glm::vec2, 4> uvOriginal = {
		glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1)
	};
	for (std::size_t i = 0; i < uvOriginal.size(); ++i)
		if (glm::length(uv[i] - uvOriginal[i]) > 1.0e-6f) return false;

	float minimoY = vertices.front().y, maximoY = vertices.front().y;
	for (const glm::vec3& v : vertices)
	{
		minimoY = std::min(minimoY, v.y);
		maximoY = std::max(maximoY, v.y);
	}
	return std::abs((maximoY - minimoY) - 0.02f) < 1.0e-6f;
}

bool renderizarPreviewCuadro(GLuint programa, const fs::path& rutaModelo)
{
	GLuint fbo = 0, color = 0, depth = 0;
	constexpr int ancho = 512, alto = 256, mitadAncho = ancho / 2;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glGenTextures(1, &color);
	glBindTexture(GL_TEXTURE_2D, color);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ancho, alto, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
	glGenRenderbuffers(1, &depth);
	glBindRenderbuffer(GL_RENDERBUFFER, depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, ancho, alto);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteRenderbuffers(1, &depth);
		glDeleteTextures(1, &color);
		glDeleteFramebuffers(1, &fbo);
		return false;
	}

	glUseProgram(programa);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDisable(GL_CULL_FACE);
	auto loc = [programa](const char* nombre) { return glGetUniformLocation(programa, nombre); };
	glUniform1i(loc("fixedLight"), GL_FALSE);
	glUniform4i(loc("sw_intensity"), GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
	glUniform4f(loc("LightModelAmbient"), 0.80f, 0.80f, 0.80f, 1.0f);
	glUniform1i(loc("uLightingDebugMode"), 0);
	glUniform1f(loc("uIndirectIntensity"), 0.0f);
	glUniform1f(loc("uIndirectAmbientBase"), 0.0f);
	glUniform1f(loc("uDirtStrength"), 0.0f);
	glUniform1f(loc("uTextureEmission"), 0.0f);
	glUniform1i(loc("uMobileSurface"), GL_FALSE);
	glUniform1i(loc("modulate"), GL_TRUE);
	const glm::mat4 projection = glm::ortho(-0.75f, 0.75f, -0.55f, 0.55f, 0.1f, 10.0f);
	const glm::mat4 model(1.0f);
	glUniformMatrix4fv(loc("projectionMatrix"), 1, GL_FALSE, glm::value_ptr(projection));
	glUniformMatrix4fv(loc("modelMatrix"), 1, GL_FALSE, glm::value_ptr(model));

	auto frame = std::make_unique<COBJModel>();
	std::string modelPath = rutaModelo.string();
	bool correcto = frame->LoadModel(modelPath.data()) == 0;
	std::vector<unsigned char> pixels(static_cast<std::size_t>(ancho) * alto * 4);
	std::size_t frontPixels = 0, backPixels = 0;
	GLenum error = GL_NO_ERROR;
	if (correcto)
	{
		glViewport(0, 0, ancho, alto);
		glClearColor(0.015f, 0.020f, 0.025f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		const glm::mat4 frontView = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -2.0f)) *
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1, 0, 0));
		glViewport(0, 0, mitadAncho, alto);
		glUniformMatrix4fv(loc("viewMatrix"), 1, GL_FALSE, glm::value_ptr(frontView));
		frame->draw_TriVAO_OBJ(programa);

		const glm::mat4 backView = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -2.0f)) *
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1, 0, 0)) *
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0, 0, 1));
		glViewport(mitadAncho, 0, mitadAncho, alto);
		glUniformMatrix4fv(loc("viewMatrix"), 1, GL_FALSE, glm::value_ptr(backView));
		frame->draw_TriVAO_OBJ(programa);

		glReadPixels(0, 0, ancho, alto, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		for (int y = 0; y < alto; ++y)
			for (int x = 0; x < ancho; ++x)
			{
				const std::size_t p = (static_cast<std::size_t>(y) * ancho + x) * 4;
				if (pixels[p] > 20 || pixels[p + 1] > 20 || pixels[p + 2] > 20)
					(x < mitadAncho ? frontPixels : backPixels)++;
			}
		error = glGetError();
		correcto = error == GL_NO_ERROR && frontPixels > 10000 && backPixels > 10000;
		if (const char* previewPath = std::getenv("ESCAPETHEUAB_BOTANICAL_PREVIEW"))
		{
			std::ofstream output(previewPath, std::ios::binary);
			output << "P6\n" << ancho << ' ' << alto << "\n255\n";
			for (int y = alto - 1; y >= 0; --y)
				for (int x = 0; x < ancho; ++x)
				{
					const std::size_t p = (static_cast<std::size_t>(y) * ancho + x) * 4;
					output.write(reinterpret_cast<const char*>(pixels.data() + p), 3);
				}
			correcto = correcto && static_cast<bool>(output);
		}
	}
	frame->netejaTextures_OBJ();
	frame->netejaVAOList_OBJ();
	glDisable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteRenderbuffers(1, &depth);
	glDeleteTextures(1, &color);
	glDeleteFramebuffers(1, &fbo);
	if (!correcto)
		std::cerr << "FALLO: render del cuadro por ambas caras, pixeles front/back="
			<< frontPixels << '/' << backPixels << ", GL=" << error << ".\n";
	return correcto;
}
}

int main()
{
    if (NUM_MAX_LLUMS != 17)
    {
        std::cerr << "FALLO: 13 luminarias y cuatro slots reservados requieren 17 slots de motor.\n";
        return 1;
    }

    const fs::path cuadroBotanico = fs::path(ESCAPETHEUAB_SOURCE_DIR) /
        "assets/models/escape_pack/lab/clue_botanical_frame.obj";
    if (!validarGeometriaCuadroBotanico(cuadroBotanico))
    {
        std::cerr << "FALLO: geometria del cuadro botanico debe estar cerrada, sin caras repetidas y con UV/normales coherentes.\n";
        return 1;
    }

    {
        Material alfombra{};
        alfombra.fDiffuse[0] = 0.9433962f;
        alfombra.fDiffuse[1] = 0.367176f;
        alfombra.fDiffuse[2] = 0.3426486f;
        if (!AplicarFallbackAmbientalDesdeDifuso(alfombra) ||
            std::abs(alfombra.fAmbient[0] - 0.1698113f) > 1.0e-5f ||
            std::abs(alfombra.fAmbient[1] - 0.0660917f) > 1.0e-5f ||
            std::abs(alfombra.fAmbient[2] - 0.0616767f) > 1.0e-5f)
        {
            std::cerr << "FALLO: Ka=0 debe derivar un ambiente tenue que conserva el color Kd.\n";
            return 1;
        }

        Material metal{};
        metal.fAmbient[0] = 0.18f;
        metal.fAmbient[1] = 0.12f;
        metal.fAmbient[2] = 0.08f;
        metal.fDiffuse[0] = 0.37f;
        metal.fDiffuse[1] = 0.47f;
        metal.fDiffuse[2] = 0.47f;
        if (AplicarFallbackAmbientalDesdeDifuso(metal) || metal.fAmbient[0] != 0.18f ||
            metal.fAmbient[1] != 0.12f || metal.fAmbient[2] != 0.08f)
        {
            std::cerr << "FALLO: el fallback no debe reemplazar un Ka especificado.\n";
            return 1;
        }
    }

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
        if (!correcto)
        {
            GLint longitud = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &longitud);
            std::string log(static_cast<std::size_t>(std::max(longitud, 1)), '\0');
            glGetShaderInfoLog(shader, longitud, nullptr, log.data());
            std::cerr << "GLSL compile error: " << log.c_str() << '\n';
            glDeleteShader(shader);
            return 0;
        }
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
        const fs::path shaders = fs::path(ESCAPETHEUAB_SOURCE_DIR) / "assets/shaders";
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
            const GLint dirtStrengthLocation = glGetUniformLocation(phong, "uDirtStrength");
            const GLint dirtScaleLocation = glGetUniformLocation(phong, "uDirtScale");
            const GLint dirtSeedLocation = glGetUniformLocation(phong, "uDirtSeed");
            const GLint textureEmissionLocation = glGetUniformLocation(phong, "uTextureEmission");
            const GLint mobileSurfaceLocation = glGetUniformLocation(phong, "uMobileSurface");
            const GLint lightingDebugLocation = glGetUniformLocation(phong, "uLightingDebugMode");
            const int ultimoSlotLuz = NUM_MAX_LLUMS - 1;
            const std::string prefijoUltimaLuz = "LightSource[" + std::to_string(ultimoSlotLuz) + "].";
            const GLint lastLightLocation = glGetUniformLocation(phong, (prefijoUltimaLuz + "sw_light").c_str());
            constexpr int maxOclusores = 12;
            constexpr int maxOclusoresIndirectos = 24;
            struct alignas(16) OclusorGpuTest
            {
                GLfloat mundoALocal[16]{};
                GLfloat mitad[4]{};
            };
            struct alignas(16) BloqueOclusoresGpuTest
            {
                std::array<OclusorGpuTest, maxOclusores> directos{};
                std::array<OclusorGpuTest, maxOclusoresIndirectos> indirectos{};
            };
            static_assert(sizeof(OclusorGpuTest) == 80, "mat4 + vec4 std140");
            static_assert(sizeof(BloqueOclusoresGpuTest) == 2880, "12 OBB directos + 24 indirectos");
            constexpr GLuint bindingOclusores = 0;
            constexpr GLint tamanoBloqueEsperado = sizeof(BloqueOclusoresGpuTest);
            const GLuint bloqueOclusores = glGetUniformBlockIndex(phong, "PhongOccludersBlock");
            GLint tamanoBloqueOclusores = 0;
            if (bloqueOclusores != GL_INVALID_INDEX)
                glGetActiveUniformBlockiv(phong, bloqueOclusores, GL_UNIFORM_BLOCK_DATA_SIZE, &tamanoBloqueOclusores);
            if (!linked || intensityLocation < 0 || emissionLocation < 0 || tileLocation < 0 ||
                dirtStrengthLocation < 0 || dirtScaleLocation < 0 || dirtSeedLocation < 0 ||
                textureEmissionLocation < 0 || mobileSurfaceLocation < 0 || lightingDebugLocation < 0 ||
                lastLightLocation < 0 || bloqueOclusores == GL_INVALID_INDEX ||
                tamanoBloqueOclusores != tamanoBloqueEsperado)
            {
                std::cerr << "FALLO: GLSL Phong no enlaza el slot 16 o el bloque std140 de 2880 bytes (real="
                    << tamanoBloqueOclusores << ").\n";
                ++failures;
            }
            else
            {
                glUseProgram(phong);
                glUniformBlockBinding(phong, bloqueOclusores, bindingOclusores);
                GLuint oclusoresBuffer = 0;
                BloqueOclusoresGpuTest oclusoresData{};
                glGenBuffers(1, &oclusoresBuffer);
                glBindBuffer(GL_UNIFORM_BUFFER, oclusoresBuffer);
                glBufferData(GL_UNIFORM_BUFFER, tamanoBloqueEsperado, &oclusoresData, GL_DYNAMIC_DRAW);
                glBindBufferBase(GL_UNIFORM_BUFFER, bindingOclusores, oclusoresBuffer);
                auto subirOclusorDirectoTest = [&](const glm::mat4& mundoALocal, const glm::vec3& mitad)
                {
                    auto& oclusor = oclusoresData.directos[0];
                    std::copy_n(glm::value_ptr(mundoALocal), 16, oclusor.mundoALocal);
                    oclusor.mitad[0] = mitad.x; oclusor.mitad[1] = mitad.y; oclusor.mitad[2] = mitad.z;
                    glBindBuffer(GL_UNIFORM_BUFFER, oclusoresBuffer);
                    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(oclusoresData), &oclusoresData);
                    glBindBufferBase(GL_UNIFORM_BUFFER, bindingOclusores, oclusoresBuffer);
                };
                auto subirOclusorIndirectoTest = [&](const glm::mat4& mundoALocal, const glm::vec3& mitad)
                {
                    auto& oclusor = oclusoresData.indirectos[0];
                    std::copy_n(glm::value_ptr(mundoALocal), 16, oclusor.mundoALocal);
                    oclusor.mitad[0] = mitad.x; oclusor.mitad[1] = mitad.y; oclusor.mitad[2] = mitad.z;
                    glBindBuffer(GL_UNIFORM_BUFFER, oclusoresBuffer);
                    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(oclusoresData), &oclusoresData);
                    glBindBufferBase(GL_UNIFORM_BUFFER, bindingOclusores, oclusoresBuffer);
                };
                glUniform4i(intensityLocation, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                const GLenum intensidadError = glGetError();
                // La llamada anterior del cargador usaba 3 componentes para este vec4.
                glUniform3f(emissionLocation, 0.2f, 0.3f, 0.4f);
                const GLenum emisionError3 = glGetError();
                glUniform4f(emissionLocation, 0.2f, 0.3f, 0.4f, 1.0f);
                const GLenum emisionError4 = glGetError();
                glUniform1f(dirtStrengthLocation, 0.2f);
                glUniform1f(dirtScaleLocation, 0.42f);
                glUniform1f(dirtSeedLocation, 17.0f);
                glUniform1f(textureEmissionLocation, 0.18f);
                glUniform1i(mobileSurfaceLocation, GL_TRUE);
                glUniform1i(lightingDebugLocation, 0);
                const GLenum materialNuevoError = glGetError();
                if (intensidadError != GL_NO_ERROR || emisionError3 != GL_INVALID_OPERATION || emisionError4 != GL_NO_ERROR || materialNuevoError != GL_NO_ERROR)
                {
                    std::cerr << "FALLO: contrato uniforms Phong: bvec4=" << intensidadError <<
                        ", vec4 con 3 comp=" << emisionError3 << ", vec4 completo=" << emisionError4 <<
                        ", nuevos materiales=" << materialNuevoError << ".\n";
                    ++failures;
                }

                // Mini escena GPU: dos receptores bajo la misma luz, con una caja OBB
                // opaca solo entre la fuente y el receptor derecho.
                GLuint framebuffer = 0, colorTexture = 0, vao = 0, vbo = 0;
                glGenFramebuffers(1, &framebuffer);
                glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
                glGenTextures(1, &colorTexture);
                glBindTexture(GL_TEXTURE_2D, colorTexture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

                const float quad[] = {
                    -0.18f,-0.18f,0,  1,1,1,1,  0,0,1,  0,0,
                     0.18f,-0.18f,0,  1,1,1,1,  0,0,1,  1,0,
                     0.18f, 0.18f,0,  1,1,1,1,  0,0,1,  1,1,
                    -0.18f,-0.18f,0,  1,1,1,1,  0,0,1,  0,0,
                     0.18f, 0.18f,0,  1,1,1,1,  0,0,1,  1,1,
                    -0.18f, 0.18f,0,  1,1,1,1,  0,0,1,  0,1
                };
                glGenVertexArrays(1, &vao);
                glBindVertexArray(vao);
                glGenBuffers(1, &vbo);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
                const GLsizei stride = 12 * static_cast<GLsizei>(sizeof(float));
                glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
                glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
                glEnableVertexAttribArray(2); glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(7 * sizeof(float)));
                glEnableVertexAttribArray(3); glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(10 * sizeof(float)));

                auto loc = [phong](const char* nombre) { return glGetUniformLocation(phong, nombre); };
                const glm::mat4 identidad(1.0f);
                glUniformMatrix4fv(loc("viewMatrix"), 1, GL_FALSE, glm::value_ptr(identidad));
                glUniformMatrix4fv(loc("projectionMatrix"), 1, GL_FALSE, glm::value_ptr(identidad));
                glUniform1i(loc("fixedLight"), GL_TRUE);
                glUniform1i(loc("sw_material"), GL_TRUE);
                glUniform4i(loc("sw_intensity"), GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);
                glUniform1i(loc("textur"), GL_FALSE);
                glUniform1i(loc("modulate"), GL_TRUE);
                glUniform1i(loc("uMaterialEmission"), GL_FALSE);
                glUniform4f(loc("material.diffuse"), 1, 1, 1, 1);
                glUniform4f(loc("material.ambient"), 0, 0, 0, 1);
                glUniform4f(loc("material.emission"), 0, 0, 0, 1);
                glUniform4f(loc("material.specular"), 0, 0, 0, 1);
                glUniform1f(loc("material.shininess"), 16.0f);
                glUniform4f(loc("LightModelAmbient"), 0, 0, 0, 1);
                glUniform1f(loc("uLampEmissionScale"), 1.0f);
                glUniform1f(loc("uDirtStrength"), 0.0f);
                glUniform1f(loc("uTextureEmission"), 0.0f);
                glUniform1i(loc("uMobileSurface"), GL_FALSE);
                glUniform1f(loc("uIndirectIntensity"), 0.0f);
                glUniform1f(loc("uIndirectAmbientBase"), 0.0f);
                glUniform1f(loc("uSceneDirectScale"), 1.0f);
                glUniform3f(loc("uIndirectFloorColor"), 0.42f, 0.46f, 0.43f);
                glUniform3f(loc("uIndirectSkyColor"), 0.24f, 0.28f, 0.34f);
                glUniform1i(loc("uIndirectOccluderCount"), 0);

                // El ultimo slot es la ultima luminaria de escena admitida; debe
                // estar activo en el shader y aportar luz directa al receptor.
                glViewport(0, 0, 64, 32);
                glUniform1i(loc("uOccluderCount"), 0);
                glUniform1i(lastLightLocation, GL_TRUE);
                glUniform4f(loc((prefijoUltimaLuz + "position").c_str()), 0, 0, 2, 1);
                glUniform4f(loc((prefijoUltimaLuz + "diffuse").c_str()), 1, 1, 1, 1);
                glUniform4f(loc((prefijoUltimaLuz + "specular").c_str()), 0, 0, 0, 1);
                glUniform3f(loc((prefijoUltimaLuz + "attenuation").c_str()), 0, 0, 1);
                glUniform1i(loc((prefijoUltimaLuz + "restricted").c_str()), GL_FALSE);
                glUniform1i(loc((prefijoUltimaLuz + "areaSource").c_str()), GL_FALSE);
                glClear(GL_COLOR_BUFFER_BIT);
                glUniformMatrix4fv(loc("modelMatrix"), 1, GL_FALSE, glm::value_ptr(identidad));
                glDrawArrays(GL_TRIANGLES, 0, 6);
                unsigned char slotFinalIluminado[4] = {};
                glReadPixels(32, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, slotFinalIluminado);
                const GLenum slotFinalError = glGetError();
                if (slotFinalError != GL_NO_ERROR || slotFinalIluminado[0] < 80)
                {
                    std::cerr << "FALLO: la luz del slot " << ultimoSlotLuz << " no ilumina el receptor: rojo="
                        << static_cast<int>(slotFinalIluminado[0]) << ", GL=" << slotFinalError << ".\n";
                    ++failures;
                }
                glUniform1i(lastLightLocation, GL_FALSE);

                glUniform1i(loc("uOccluderCount"), 1);
                // La pared esta girada 90 grados: su eje fino local pasa a ser el normal mundial.
                const glm::mat4 paredMundo = glm::translate(glm::mat4(1.0f), glm::vec3(0.35f, 0.0f, 1.0f))
                    * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 0, 1));
                const glm::mat4 mundoALocal = glm::inverse(paredMundo);
                subirOclusorDirectoTest(mundoALocal, glm::vec3(1.0f, 0.18f, 1.2f));
                glUniform1i(loc("LightSource[4].sw_light"), GL_TRUE);
                glUniform4f(loc("LightSource[4].position"), 0, 0, 2, 1);
                glUniform4f(loc("LightSource[4].diffuse"), 1, 1, 1, 1);
                glUniform4f(loc("LightSource[4].specular"), 0, 0, 0, 1);
                glUniform3f(loc("LightSource[4].attenuation"), 0, 0, 1);
                glUniform1i(loc("LightSource[4].restricted"), GL_FALSE);
                glUniform1i(loc("LightSource[4].areaSource"), GL_FALSE);
                glUniform3f(loc("LightSource[4].areaAxisU"), 1, 0, 0);
                glUniform3f(loc("LightSource[4].areaAxisV"), 0, 1, 0);
                glUniform2f(loc("LightSource[4].areaHalfSize"), 1.2f, 0.19f);

                glDisable(GL_BLEND); glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST);
                glViewport(0, 0, 64, 32);
                glClearColor(0, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT);
                glBindVertexArray(vao);
                for (float x : { -0.7f, 0.7f })
                {
                    const glm::mat4 modelo = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0, 0));
                    glUniformMatrix4fv(loc("modelMatrix"), 1, GL_FALSE, glm::value_ptr(modelo));
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
                unsigned char visible[4] = {}, oculta[4] = {};
                glReadPixels(10, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, visible);
                glReadPixels(54, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, oculta);
                const GLenum renderError = glGetError();
                if (renderError != GL_NO_ERROR || visible[0] < 80 || oculta[0] > visible[0] / 5)
                {
                    std::cerr << "FALLO: la prueba GPU de oclusion no separa superficie visible/oculta: visible="
                        << static_cast<int>(visible[0]) << ", oculta=" << static_cast<int>(oculta[0])
                        << ", GL=" << renderError << ".\n";
                    ++failures;
                }

                // Un Ka=0 de un MTL recibe el fallback proporcional a Kd:
                // con luz directa apagada, el modo ambiental debe conservar el rojo.
                glUniform4i(loc("sw_intensity"), GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
                glUniform1i(loc("LightSource[4].sw_light"), GL_FALSE);
                glUniform1i(loc("uLightingDebugMode"), 1);
                glUniform4f(loc("LightModelAmbient"), 0.26f, 0.27f, 0.30f, 1.0f);
                glUniform4f(loc("material.diffuse"), 0.9433962f, 0.367176f, 0.3426486f, 1.0f);
                glUniform4f(loc("material.ambient"), 0.18f * 0.9433962f,
                    0.18f * 0.367176f, 0.18f * 0.3426486f, 1.0f);
                glUniform1f(loc("uIndirectIntensity"), 0.0f);
                glUniform1f(loc("uIndirectAmbientBase"), 0.035f);
                glClear(GL_COLOR_BUFFER_BIT);
                glUniformMatrix4fv(loc("modelMatrix"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
                glDrawArrays(GL_TRIANGLES, 0, 6);
                unsigned char ambienteRojo[4] = {};
                glReadPixels(32, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, ambienteRojo);
                const GLenum ambienteError = glGetError();
                if (ambienteError != GL_NO_ERROR || ambienteRojo[0] < 8 ||
                    ambienteRojo[0] <= ambienteRojo[1] * 2)
                {
                    std::cerr << "FALLO: Ka=0 no conserva una respuesta ambiental rojiza: RGB="
                        << static_cast<int>(ambienteRojo[0]) << ',' << static_cast<int>(ambienteRojo[1]) << ','
                        << static_cast<int>(ambienteRojo[2]) << ", GL=" << ambienteError << ".\n";
                    ++failures;
                }
                glUniform1i(loc("uLightingDebugMode"), 5);
                glClear(GL_COLOR_BUFFER_BIT);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                unsigned char albedoRojo[4] = {};
                glReadPixels(32, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, albedoRojo);
                if (glGetError() != GL_NO_ERROR || albedoRojo[0] < 220 || albedoRojo[1] < 75)
                {
                    std::cerr << "FALLO: el modo Albedo no muestra el Kd original: RGB="
                        << static_cast<int>(albedoRojo[0]) << ',' << static_cast<int>(albedoRojo[1]) << ','
                        << static_cast<int>(albedoRojo[2]) << ".\n";
                    ++failures;
                }
                glUniform1i(loc("uLightingDebugMode"), 0);
                glUniform1f(loc("uIndirectAmbientBase"), 0.035f);
                glClear(GL_COLOR_BUFFER_BIT);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                unsigned char habitacionSinLuces[4] = {};
                glReadPixels(32, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, habitacionSinLuces);
                if (glGetError() != GL_NO_ERROR || habitacionSinLuces[0] == 0 || habitacionSinLuces[0] > 24)
                {
                    std::cerr << "FALLO: el residuo ambiental sin luminarias debe seguir oscuro: rojo="
                        << static_cast<int>(habitacionSinLuces[0]) << ".\n";
                    ++failures;
                }
                glUniform1f(loc("uIndirectAmbientBase"), 0.0f);
                glUniform1i(loc("LightSource[4].sw_light"), GL_TRUE);

                // Aísla la contribución local indirecta: sin pared, ambos receptores
                // reciben relleno; al añadir el OBB arquitectónico, el oculto queda negro.
                glUniform4i(loc("sw_intensity"), GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                glUniform1f(loc("uIndirectIntensity"), 0.35f);
                glUniform1f(loc("uSceneDirectScale"), 0.0f);
                glUniform3f(loc("LightSource[4].attenuation"), 0.0f, 0.08f, 1.0f);
                glUniform1i(loc("uIndirectOccluderCount"), 0);
                glClear(GL_COLOR_BUFFER_BIT);
                for (float x : { -0.7f, 0.7f })
                {
                    const glm::mat4 modelo = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0, 0));
                    glUniformMatrix4fv(loc("modelMatrix"), 1, GL_FALSE, glm::value_ptr(modelo));
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
                unsigned char reboteVisible[4] = {}, reboteSinPared[4] = {};
                glReadPixels(10, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, reboteVisible);
                glReadPixels(54, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, reboteSinPared);
                glUniform1i(loc("uIndirectOccluderCount"), 1);
                subirOclusorIndirectoTest(mundoALocal, glm::vec3(1.0f, 0.18f, 1.2f));
                glClear(GL_COLOR_BUFFER_BIT);
                for (float x : { -0.7f, 0.7f })
                {
                    const glm::mat4 modelo = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0, 0));
                    glUniformMatrix4fv(loc("modelMatrix"), 1, GL_FALSE, glm::value_ptr(modelo));
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
                unsigned char reboteOculto[4] = {};
                glReadPixels(54, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, reboteOculto);
                glUniform1i(loc("uIndirectOccluderCount"), 0);
                glUniform1i(loc("LightSource[4].sw_light"), GL_FALSE);
                glClear(GL_COLOR_BUFFER_BIT);
                const glm::mat4 receptorDerecho = glm::translate(glm::mat4(1.0f), glm::vec3(0.7f, 0, 0));
                glUniformMatrix4fv(loc("modelMatrix"), 1, GL_FALSE, glm::value_ptr(receptorDerecho));
                glDrawArrays(GL_TRIANGLES, 0, 6);
                unsigned char reboteApagado[4] = {};
                glReadPixels(54, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, reboteApagado);
                glUniform1i(loc("LightSource[4].sw_light"), GL_TRUE);
                glUniform1f(loc("uSceneDirectScale"), 1.0f);
                const GLenum reboteError = glGetError();
                if (reboteError != GL_NO_ERROR || reboteVisible[0] < 20 ||
                    reboteSinPared[0] < reboteVisible[0] / 2 || reboteOculto[0] > reboteVisible[0] / 5 ||
                    reboteApagado[0] > 1)
                {
                    std::cerr << "FALLO: el rebote local no ilumina sin obstáculos y bloquea la pared: visible="
                        << static_cast<int>(reboteVisible[0]) << ", libre=" << static_cast<int>(reboteSinPared[0])
                        << ", oculto=" << static_cast<int>(reboteOculto[0]) << ", apagado="
                        << static_cast<int>(reboteApagado[0]) << ", GL=" << reboteError << ".\n";
                    ++failures;
                }
                glUniform1f(loc("uIndirectIntensity"), 0.0f);
                glUniform1i(loc("uIndirectOccluderCount"), 0);
                glUniform4i(loc("sw_intensity"), GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);

                // El ajuste de intensidad de la escena no debe afectar slots 0-3 y,
                // con el relleno apagado, escala la luz directa de techo hasta cero.
                glUniform1f(loc("uSceneDirectScale"), 0.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                const glm::mat4 receptorVisible = glm::translate(glm::mat4(1.0f), glm::vec3(-0.7f, 0, 0));
                glUniformMatrix4fv(loc("modelMatrix"), 1, GL_FALSE, glm::value_ptr(receptorVisible));
                glDrawArrays(GL_TRIANGLES, 0, 6);
                unsigned char escenaApagada[4] = {};
                glReadPixels(10, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, escenaApagada);
                const GLenum escalaError = glGetError();
                if (escalaError != GL_NO_ERROR || escenaApagada[0] > 1)
                {
                    std::cerr << "FALLO: uSceneDirectScale=0 no apaga la luz de escena: valor="
                        << static_cast<int>(escenaApagada[0]) << ", GL=" << escalaError << ".\n";
                    ++failures;
                }
				glUniform1i(loc("LightSource[3].sw_light"), GL_TRUE);
				glUniform4f(loc("LightSource[3].position"), 0, 0, 2, 1);
				glUniform4f(loc("LightSource[3].diffuse"), 1, 1, 1, 1);
				glUniform4f(loc("LightSource[3].specular"), 0, 0, 0, 1);
				glUniform3f(loc("LightSource[3].attenuation"), 0, 0, 1);
				glUniform1i(loc("LightSource[3].restricted"), GL_FALSE);
				glUniform1i(loc("LightSource[3].areaSource"), GL_FALSE);
				glClear(GL_COLOR_BUFFER_BIT);
				glDrawArrays(GL_TRIANGLES, 0, 6);
				unsigned char linternaConEscalaCero[4] = {};
				glReadPixels(10, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, linternaConEscalaCero);
				glUniform1i(loc("LightSource[3].sw_light"), GL_FALSE);
				const GLenum linternaError = glGetError();
				if (linternaError != GL_NO_ERROR || linternaConEscalaCero[0] < 80)
				{
					std::cerr << "FALLO: directScale de escena afecta a la linterna del slot 3: valor="
						<< static_cast<int>(linternaConEscalaCero[0]) << ", GL=" << linternaError << ".\n";
					++failures;
				}
                glUniform1f(loc("uSceneDirectScale"), 1.0f);

				// La emisión del difusor de la luminaria debe seguir el mismo interruptor
				// que la fuente de luz: con escala cero no queda superficie encendida.
				glUniform4i(loc("sw_intensity"), GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
				glUniform1i(loc("uMaterialEmission"), GL_TRUE);
				glUniform4f(loc("material.emission"), 0.78f, 0.90f, 1.0f, 1.0f);
				glUniform1i(loc("uOccluderCount"), 0);
				glUniform1f(loc("uLampEmissionScale"), 1.0f);
				glClear(GL_COLOR_BUFFER_BIT);
				const glm::mat4 modeloCentro(1.0f);
				glUniformMatrix4fv(loc("modelMatrix"), 1, GL_FALSE, glm::value_ptr(modeloCentro));
				glDrawArrays(GL_TRIANGLES, 0, 6);
				unsigned char emisionEncendida[4] = {};
				glReadPixels(32, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, emisionEncendida);
				glUniform1f(loc("uLampEmissionScale"), 0.0f);
				glClear(GL_COLOR_BUFFER_BIT);
				glDrawArrays(GL_TRIANGLES, 0, 6);
				unsigned char emisionApagada[4] = {};
				glReadPixels(32, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, emisionApagada);
				const GLenum emisionError = glGetError();
				if (emisionError != GL_NO_ERROR || emisionEncendida[0] < 150 || emisionApagada[0] > 1)
				{
					std::cerr << "FALLO: emision MTL no sigue el estado: encendida="
						<< static_cast<int>(emisionEncendida[0]) << ", apagada="
						<< static_cast<int>(emisionApagada[0]) << ", GL=" << emisionError << ".\n";
					++failures;
				}

				if (!renderizarPreviewCuadro(phong, cuadroBotanico)) ++failures;
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glBindBufferBase(GL_UNIFORM_BUFFER, bindingOclusores, 0);
                glDeleteBuffers(1, &oclusoresBuffer);
                glDeleteBuffers(1, &vbo); glDeleteVertexArrays(1, &vao);
                glDeleteTextures(1, &colorTexture); glDeleteFramebuffers(1, &framebuffer);
            }
        }
        else { std::cerr << "FALLO: GLSL Phong no compila en el contexto OpenGL disponible.\n"; ++failures; }
        glUseProgram(0);
        if (phong) glDeleteProgram(phong);
        if (vsId) glDeleteShader(vsId);
        if (fsId) glDeleteShader(fsId);
    }

    {
        const fs::path arquitectura = fs::path(ESCAPETHEUAB_SOURCE_DIR) /
            "assets/models/escape_pack/arquitectura";
        for (const char* nombre : { "industrial_wall_lab.png", "industrial_wall_maintenance.png" })
        {
            int ancho = 0, alto = 0, canales = 0;
            const fs::path ruta = arquitectura / nombre;
            unsigned char* pixeles = SOIL_load_image(ruta.string().c_str(), &ancho, &alto, &canales, SOIL_LOAD_RGBA);
            if (pixeles == nullptr || ancho != 512 || alto != 512)
            {
                std::cerr << "FALLO: SOIL no carga textura modular 512x512: " << ruta << '\n';
                ++failures;
            }
            if (pixeles != nullptr) SOIL_free_image_data(pixeles);
        }
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
            "assets/models/Phone.obj").string();
        if (phone.LoadModel(phonePath.data()) != 0)
        {
            std::cerr << "FALLO: no se pudo cargar Phone.obj con su MTL y map_Kd relativo.\n";
            ++failures;
        }
        phone.netejaTextures_OBJ();
        phone.netejaVAOList_OBJ();
    }
    {
        const fs::path assets = fs::path(ESCAPETHEUAB_SOURCE_DIR) / "assets/models/escape_pack";
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
            assets / "lab/machine_microscope/machine_microscope.obj",
            assets / "lab/bottle_test_tube_rack/bottle_test_tube_rack.obj",
            assets / "kenney/cardboardBoxClosed.obj",
            assets / "scifi/Prop_Cable_1.obj",
            assets / "scifi/Prop_Cable_3.obj",
            assets / "scifi/Prop_Vent_Wide.obj",
            assets / "lab/counter_counter_sink/counter_counter_sink.obj",
            assets / "lab/cabinet_cabinet/cabinet_cabinet.obj",
            assets / "lab/machine_hot_plate/machine_hot_plate.obj",
		    assets / "lab/clue_botanical_frame.obj",
            assets / "../Marc/armariLab.obj",
            assets / "../Marc/monstruoVentana.obj",
            assets / "../Marc/windowCreepy.obj"
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
