#include "src/app/AppState.h"
#include "src/rendering/SceneLighting.h"

// Slots 4-16 pertenecen a las luces declaradas en JSON. Los slots 0-3 siguen
// reservados para las fuentes generales y la linterna móvil.
void actualizarLucesDeEscena()
{
	constexpr int primerSlot = 4;
	const bool hayLucesDeEscena = std::any_of(objetosEscena.begin(), objetosEscena.end(),
		[](const SceneObject& objeto) { return objeto.luz.presente; });
	for (int slot = primerSlot; slot < NUM_MAX_LLUMS; ++slot)
	{
		llumGL[slot].encesa = false;
		llumGL[slot].areaSource = false;
		llumGL[slot].areaHalfSizeU = llumGL[slot].areaHalfSizeV = 0.0f;
	}
	if (!hayLucesDeEscena) return;

	if (!lamparasLaboratorioActivas) return;

	int slot = primerSlot;
	std::size_t lucesSinSlot = 0;
	for (const SceneObject& objeto : objetosEscena)
	{
		if (!objeto.luz.presente || !objeto.luz.encendida) continue;
		if (slot >= NUM_MAX_LLUMS) { ++lucesSinSlot; continue; }

		auto& luz = llumGL[slot++];
		const glm::mat4 modelo = fisica::MatrizDeObjeto(objeto);
		const glm::vec3 ejeU = glm::normalize(glm::vec3(modelo[0]));
		const glm::vec3 ejeV = glm::normalize(glm::vec3(modelo[2]));
		const glm::vec3 direccion = glm::normalize(glm::mat3(modelo) * glm::vec3(0.0f, -1.0f, 0.0f));
		const glm::vec3 posicion(modelo * glm::vec4(objeto.luz.desplazamiento, 1.0f));
		luz.posicio = { posicion.x, posicion.y, posicion.z, 1.0 };
		luz.difusa = { objeto.luz.color.r * objeto.luz.intensidad,
			objeto.luz.color.g * objeto.luz.intensidad,
			objeto.luz.color.b * objeto.luz.intensidad, 1.0f };
		luz.especular = { 0.14f, 0.16f, 0.16f, 1.0f };
		luz.atenuacio.a = 0.018;
		luz.atenuacio.b = 0.08;
		luz.atenuacio.c = 1.0;
		luz.restringida = true;
		luz.spotdirection = { direccion.x, direccion.y, direccion.z, 0.0 };
		luz.spotcoscutoff = std::cos(glm::radians(68.0f));
		luz.spotInnerCosCutoff = std::cos(glm::radians(48.0f));
		luz.spotexponent = 1.0f;
		luz.areaSource = objeto.luz.areaSize.x > 0.0f && objeto.luz.areaSize.y > 0.0f;
		luz.areaAxisU = { ejeU.x, ejeU.y, ejeU.z, 0.0 };
		luz.areaAxisV = { ejeV.x, ejeV.y, ejeV.z, 0.0 };
		luz.areaHalfSizeU = objeto.luz.areaSize.x * glm::length(glm::vec3(modelo[0])) * 0.5f;
		luz.areaHalfSizeV = objeto.luz.areaSize.y * glm::length(glm::vec3(modelo[2])) * 0.5f;
		luz.encesa = true;
	}
	static std::size_t ultimoAviso = 0;
	if (lucesSinSlot != ultimoAviso)
	{
		ultimoAviso = lucesSinSlot;
		if (lucesSinSlot != 0)
			std::cerr << "Iluminacion Phong limitada a " << (NUM_MAX_LLUMS - primerSlot)
				<< " lamparas de escena; quedan " << lucesSinSlot << " sin slot.\n";
	}
}

void actualizarOclusoresPhong()
{
	const auto inicio = std::chrono::steady_clock::now();
	static std::size_t firmaAnterior = 0;
	static GLuint programaAnterior = 0;
	static GLuint programaBloqueAnterior = 0;
	static GLuint indiceBloqueOclusores = GL_INVALID_INDEX;
	static GLuint bufferOclusores = 0;
	size_t firma = 1469598103934665603ull;
	auto mezclar = [&firma](size_t valor) { firma ^= valor; firma *= 1099511628211ull; };
	for (const SceneObject& objeto : objetosEscena)
	{
		if (!objeto.fisica.presente || objeto.fisica.tipo != fisica::BodyType::Estatico) continue;
		for (int eje = 0; eje < 3; ++eje)
		{
			mezclar(std::hash<float>{}(objeto.posicion[eje])); mezclar(std::hash<float>{}(objeto.rotacion[eje]));
			mezclar(std::hash<float>{}(objeto.escala[eje])); mezclar(std::hash<float>{}(objeto.fisica.mitadTamano[eje]));
			mezclar(std::hash<float>{}(objeto.fisica.desplazamiento[eje]));
		}
		mezclar(std::hash<std::string>{}(objeto.categoria));
	}
	constexpr GLuint bindingOclusores = 0;
	constexpr int maxOclusores = 12;
	constexpr int maxOclusoresIndirectos = 24;
	struct alignas(16) OclusorGpu
	{
		GLfloat mundoALocal[16]{};
		GLfloat mitad[4]{};
	};
	struct alignas(16) BloqueOclusoresGpu
	{
		std::array<OclusorGpu, maxOclusores> directos{};
		std::array<OclusorGpu, maxOclusoresIndirectos> indirectos{};
	};
	static_assert(sizeof(OclusorGpu) == 80, "std140: mat4 + vec4 ocupan 80 bytes");
	static_assert(sizeof(BloqueOclusoresGpu) == 2880, "12+24 oclusores std140 ocupan 2880 bytes");
	// GL 3.3 garantiza 16 KiB por UBO: quedan 13504 bytes.
	// En uniforms default activos del fragment shader: 17*32 + (16 view + 17 material + 4 ambient +
	// 21 escalares/bool/int + 6 colores) = 608; sampler/matrices no usadas no cuentan; margen 416/1024.
	// El GLuint static vive con el contexto principal, que OpenGL libera al destruirse.
	if (bufferOclusores == 0)
	{
		glGenBuffers(1, &bufferOclusores);
		if (bufferOclusores != 0)
		{
			glBindBuffer(GL_UNIFORM_BUFFER, bufferOclusores);
			glBufferData(GL_UNIFORM_BUFFER, sizeof(BloqueOclusoresGpu), nullptr, GL_DYNAMIC_DRAW);
		}
	}
	if (bufferOclusores == 0)
	{
		static bool avisoBuffer = false;
		if (!avisoBuffer) { std::cerr << "No se pudo crear el UBO de oclusores Phong.\n"; avisoBuffer = true; }
		if (renderMetrics::enabled) renderMetrics::oclusoresMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicio).count();
		return;
	}
	// El binding point pertenece al contexto y se reafirma incluso si la firma de escena no cambió.
	glBindBufferBase(GL_UNIFORM_BUFFER, bindingOclusores, bufferOclusores);
	if (renderizadoOptimizado && !oclusoresPhongInvalidos && firma == firmaAnterior && programaAnterior == shader_programID)
	{
		if (renderMetrics::enabled) renderMetrics::oclusoresMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicio).count();
		return;
	}
	BloqueOclusoresGpu bloque{};
	int cantidad = 0;
	int cantidadIndirecta = 0;
	for (const SceneObject& objeto : objetosEscena)
	{
		if (!objeto.fisica.presente ||
			objeto.fisica.tipo != fisica::BodyType::Estatico) continue;
		const glm::mat4 matrizObjeto = fisica::MatrizDeObjeto(objeto);
		glm::mat4 giro(1.0f);
		giro = glm::rotate(giro, glm::radians(objeto.rotacion.z), glm::vec3(0, 0, 1));
		giro = glm::rotate(giro, glm::radians(objeto.rotacion.y), glm::vec3(0, 1, 0));
		giro = glm::rotate(giro, glm::radians(objeto.rotacion.x), glm::vec3(1, 0, 0));
		const glm::vec3 centro = glm::vec3(matrizObjeto * glm::vec4(objeto.fisica.desplazamiento, 1.0f));
		const glm::mat4 mundoCollider = glm::translate(glm::mat4(1.0f), centro) * giro;
		const glm::vec3 mitad = glm::abs(objeto.escala) * objeto.fisica.mitadTamano;
		if (cantidad < maxOclusores)
		{
			const auto indice = static_cast<std::size_t>(cantidad);
			const auto matriz = glm::inverse(mundoCollider);
			std::copy_n(glm::value_ptr(matriz), 16, bloque.directos[indice].mundoALocal);
			for (int eje = 0; eje < 3; ++eje) bloque.directos[indice].mitad[eje] = mitad[eje];
			++cantidad;
		}
		// El rebote se bloquea por arquitectura, no por mesas/sillas: así una silla bajo
		// una mesa recibe relleno local sin que la luz pueda saltar a otra habitación.
		if (objeto.categoria == "architecture" && cantidadIndirecta < maxOclusoresIndirectos)
		{
			const auto matriz = glm::inverse(mundoCollider);
			std::copy_n(glm::value_ptr(matriz), 16,
				bloque.indirectos[static_cast<std::size_t>(cantidadIndirecta)].mundoALocal);
			for (int eje = 0; eje < 3; ++eje)
				bloque.indirectos[static_cast<std::size_t>(cantidadIndirecta)].mitad[eje] = mitad[eje];
			++cantidadIndirecta;
		}
	}

	glUseProgram(shader_programID);
	if (shader_programID != 0 && (programaBloqueAnterior != shader_programID || oclusoresPhongInvalidos))
	{
		indiceBloqueOclusores = glGetUniformBlockIndex(shader_programID, "PhongOccludersBlock");
		if (indiceBloqueOclusores != GL_INVALID_INDEX)
			glUniformBlockBinding(shader_programID, indiceBloqueOclusores, bindingOclusores);
		programaBloqueAnterior = shader_programID;
	}
	glUniform1i(glcache::UniformLocation(shader_programID, "uOccluderCount"), cantidad);
	glUniform1i(glcache::UniformLocation(shader_programID, "uIndirectOccluderCount"), cantidadIndirecta);
	if ((cantidad > 0 || cantidadIndirecta > 0) && indiceBloqueOclusores != GL_INVALID_INDEX)
	{
		glBindBuffer(GL_UNIFORM_BUFFER, bufferOclusores);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(BloqueOclusoresGpu), &bloque);
	}
	firmaAnterior = firma; programaAnterior = shader_programID; oclusoresPhongInvalidos = false;
	if (renderMetrics::enabled) renderMetrics::oclusoresMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicio).count();
}
