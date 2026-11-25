#include "src/app/AppState.h"
#include "src/app/adapters/InteractionAdapters.h"
#include "src/app/adapters/HirityAdapters.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/app/input/CursorCapture.h"
#include "src/diagnostics/GrabTrace.h"

// Contexto prestado a src/gameplay/interaction: el estado sigue siendo de main.cpp.
const game::interaction::InteractionContext& contextoInteraccion()
{
	static const game::interaction::InteractionContext contexto{
		objetosEscena, escenaFisica, mundoFisico, redJuego, objetosAnimados, puertasRed,
		objetivoInteraccionActual, cargaLanzamientoActiva, tiempoCargaLanzamiento, mostrarPanelesDebug,
		[] { return entradaFPSActiva(); },
		[] { return editorEscenas.Activo(); },
		[] { return gestorOpenXR.SesionEnMarcha(); },
		[](glm::vec3& origen, glm::vec3& direccion) { origenYDireccionDeLaMirada(origen, direccion); },
		[] { return glfwGetTime(); },
		[](const char* evento, uint32_t id, net::PeerId peer, net::PeerId propietario,
			const glm::vec3& posicion, const glm::vec3& punto, const char* detalle)
			{ trazarAgarre(evento, id, peer, propietario, posicion, punto, detalle); },
		[]
		{
			animacionHiriti.DescubrirPorInteraccion();
			if (animacionHiriti.MarcarAgarrado())
				reproducirSonidoHiriti("assets/audio/hirity/story/lohashechobien.ogg");
		},
		[](float carga) { animacionHiriti.MarcarLanzadoPorJugador(carga); }
	};
	return contexto;
}

bool jugadorTieneAgarre()
{
	return game::interaction::PlayerHasGrab(contextoInteraccion());
}

InteractionTarget buscarObjetivoInteraccion()
{
	return game::interaction::FindInteractionTarget(contextoInteraccion());
}

void actualizarObjetivoInteraccion(bool permitido)
{
	game::interaction::UpdateInteractionTarget(contextoInteraccion(), permitido);
}

void iniciarCargaLanzamiento()
{
	game::interaction::StartThrowCharge(contextoInteraccion());
}

void interactuar()
{
	game::interaction::Interact(contextoInteraccion());
}

void actualizarAgarreDelJugador(bool puedeJugar)
{
	game::interaction::UpdatePlayerGrab(contextoInteraccion(), puedeJugar);
}

void cancelarCargaLanzamiento()
{
	game::interaction::CancelThrowCharge(contextoInteraccion());
}

void lanzarObjetoCargado()
{
	game::interaction::ThrowChargedObject(contextoInteraccion());
}

void actualizarCargaLanzamiento(GLFWwindow* window, float delta)
{
	game::interaction::UpdateThrowCharge(contextoInteraccion(), delta, [window]
		{ return glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS || gestorOpenXR.GatilloPulsado(); });
}
