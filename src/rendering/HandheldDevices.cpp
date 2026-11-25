#include "src/app/AppState.h"
#include "src/rendering/HandheldDevices.h"
#include "src/app/adapters/InteractionAdapters.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/app/input/CursorCapture.h"

glm::mat4 matrizLocalMovilActual()
{
	const float idle = intensidadIdle;
	const float bob = pesoCaminata * intensidadWalking;
	glm::mat4 matriz(1.0f);
	matriz = glm::translate(matriz, movilPosicion + glm::vec3(
		std::sin(faseCaminata) * bob * 0.45f,
		std::sin(tiempoFPS * 1.4f) * idle + std::abs(std::sin(faseCaminata)) * bob, 0.0f));
	matriz = glm::rotate(matriz, glm::radians(movilRotacion.x) + std::sin(tiempoFPS * 1.4f) * idle + mouseSwayY, glm::vec3(1, 0, 0));
	matriz = glm::rotate(matriz, glm::radians(movilRotacion.y) - std::sin(faseCaminata) * bob * 0.8f - mouseSwayX, glm::vec3(0, 1, 0));
	matriz = glm::rotate(matriz, glm::radians(movilRotacion.z) + std::cos(faseCaminata) * bob * 0.4f, glm::vec3(0, 0, 1));
	return glm::scale(matriz, glm::vec3(movilEscala));
}

void actualizarLinternaMovil(const glm::mat4& matrizMovilMundo)
{
	constexpr int idx = 3;
	auto& luz = llumGL[idx];
	if (!linternaMovilActiva || !mostrarMovil || !modoJuegoFPS() || editorEscenas.Activo())
	{ luz.encesa = false; return; }

	// Phone.obj coloca el conjunto cámara/flash trasero en el saliente superior de la cara +Y.
	const glm::vec3 ledLocal(0.614f, 0.068f, -1.412f);
	const glm::vec3 origen = glm::vec3(matrizMovilMundo * glm::vec4(ledLocal, 1.0f));
	const glm::vec3 frenteTelefono = glm::normalize(glm::vec3(matrizMovilMundo * glm::vec4(0, -1, 0, 0)));
	// También sigue la orientación del ojo actual cuando el renderizado usa OpenXR.
	glm::vec3 mirada = glm::vec3(glm::inverse(ViewMatrix) * glm::vec4(0, 0, -1, 0));
	if (glm::length(mirada) < 0.0001f) { luz.encesa = false; return; }
	mirada = glm::normalize(mirada);
	const glm::vec3 direccion = glm::normalize(glm::mix(frenteTelefono, mirada, 0.68f));

	luz.encesa = true;
	luz.posicio = { origen.x, origen.y, origen.z, 1.0 };
	luz.difusa = { 0.88f, 0.90f, 0.84f, 1.0f };
	luz.especular = { 1.0f, 1.0f, 1.0f, 1.0f };
	luz.atenuacio.a = 0.02; luz.atenuacio.b = 0.08; luz.atenuacio.c = 1.0;
	luz.restringida = true;
	luz.spotdirection = { direccion.x, direccion.y, direccion.z, 0.0 };
	luz.spotcoscutoff = std::cos(glm::radians(48.0f));
	luz.spotInnerCosCutoff = std::cos(glm::radians(28.0f));
	luz.spotexponent = 1.15f;
	luz.areaSource = false;
}

bool matrizMandoIzquierdoMundo(glm::mat4& matrizMundo)
{
	if (!gestorOpenXR.SesionEnMarcha()) return false;

	vr::XrPose poseMandoIzq;
	if (!gestorOpenXR.PoseApuntadoMando(0, poseMandoIzq)) return false;

	const glm::vec3 pies = mundoFisico.HayJugador() ? mundoFisico.PosicionPiesJugador() : posicionPiesDesdeCamara();
	const glm::vec3 frenteFPS(static_cast<float>(n[0] - opvN.x), static_cast<float>(n[1] - opvN.y), static_cast<float>(n[2] - opvN.z));
	const vr::Anchor anclaje = vr::AnclajeDelJugador(pies, frenteFPS,
		gestorOpenXR.Espacio() == vr::ReferenceSpaceKind::Suelo, mundoFisico.AlturaOjosJugador());

	const glm::mat4 vistaMando = vr::VistaDeOjoVR(poseMandoIzq, anclaje);
	matrizMundo = glm::inverse(vistaMando);
	return true;
}

void dibujarRayoMandoVR()
{
	if (!gestorOpenXR.SesionEnMarcha() || eixos_programID == 0) return;

	vr::XrPose poseMando;
	if (!gestorOpenXR.PoseApuntadoMando(1, poseMando)) return; // Solo mando derecho (1)

	glm::vec3 origen, direccion;
	origenYDireccionDeLaMirada(origen, direccion);
	direccion = glm::normalize(direccion);

	// Longitud del láser (mismo alcance que el agarre/interacción)
	const float alcance = std::max(fisica::PhysicsScene::AlcanceDeAgarre, glb::RADIO_INTERACCION);
	const glm::vec3 destino = origen + direccion * alcance;

	// Color RGBA: verde si estás agarrando o apuntando a algo interactuable; cian por defecto
	const InteractionTarget objetivo = buscarObjetivoInteraccion();
	const bool resaltado = escenaFisica->HayAgarre() || objetivo.accion != InteractionAction::Ninguna;
	const float r = resaltado ? 0.1f : 0.0f;
	const float g = resaltado ? 1.0f : 0.85f;
	const float b = resaltado ? 0.2f : 1.0f;
	const float a = 1.0f;

	// 7 floats por vértice: pos (vec3) + color (vec4)
	const float vertices[] = {
		origen.x,  origen.y,  origen.z,  r, g, b, a,
		destino.x, destino.y, destino.z, r, g, b, a
	};

	if (rayoVao == 0)
	{
		glGenVertexArrays(1, &rayoVao);
		glGenBuffers(1, &rayoVbo);
		glBindVertexArray(rayoVao);
		glBindBuffer(GL_ARRAY_BUFFER, rayoVbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		// location = 0 -> vec3 posición
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);

		// location = 1 -> vec4 color
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
	}
	else
	{
		glBindVertexArray(rayoVao);
		glBindBuffer(GL_ARRAY_BUFFER, rayoVbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
	}

	glUseProgram(eixos_programID);
	glUniformMatrix4fv(glcache::UniformLocation(eixos_programID, "projectionMatrix"), 1, GL_FALSE, &ProjectionMatrix[0][0]);
	glUniformMatrix4fv(glcache::UniformLocation(eixos_programID, "viewMatrix"), 1, GL_FALSE, &ViewMatrix[0][0]);

	glLineWidth(2.5f);
	renderMetrics::DrawArrays(GL_LINES, 0, 2);
	glLineWidth(1.0f);

	glBindVertexArray(0);
	glUseProgram(shader_programID);
}
