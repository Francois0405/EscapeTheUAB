#include "src/app/AppState.h"
#include "src/app/MainLoop.h"
#include "src/app/DemoDoor.h"
#include "src/app/adapters/HirityAdapters.h"
#include "src/app/adapters/InteractionAdapters.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/app/adapters/RenderAdapters.h"
#include "src/app/adapters/SceneAdapters.h"
#include "src/app/adapters/SessionAdapters.h"
#include "src/app/input/CursorCapture.h"
#include "src/legacy_vgi/VgiTimer.h"
#include "src/rendering/SceneLighting.h"
#include "src/ui/UiFrame.h"
#include "src/rendering/Ps1Framebuffer.h"

void RunMainLoop(const Ps1Framebuffer& ps1, float previous)
{
// Entorn VGI. Timer: Variables
	float time = elapsedTime;
	float now;
	float delta;

// Loop until the user closes the window
	while (!glfwWindowShouldClose(window))
	{
		// Entorn VGI. Timer: common part, do this only once
		now = glfwGetTime();
		delta = now - previous;
		previous = now;
		renderMetrics::AddFrame(delta * 1000.0);
		renderMetrics::BeginRender();

		// Entorn VGI. Timer: for each timer do this
		time -= delta;
		if ((time <= 0.0) && (satelit || anima)) OnTimer();

		// Poll for and process events
		glfwPollEvents();

		// 2. Sincronizar la pausa interna del motor (libera/captura el ratón automáticamente)
		fpsPausado = (estadoActual != ESTADO_JUGANDO);

		editorEscenas.Actualizar(window, static_cast<float>(delta));
		sincronizarCursorFPS(window);
		actualizarCargaLanzamiento(window, static_cast<float>(delta));

		// 3. Movimiento, física y VR (solo cuando estamos jugando)
		if (estadoActual == ESTADO_JUGANDO)
		{
			const bool ctrlPulsado = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
				glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
			const float deltaPostura = (editorEscenas.Activo() || (modoJuegoFPS() && !entradaFPSActiva()))
				? 0.0f : fps::LimitarDelta(static_cast<float>(delta));
			mundoFisico.ActualizarJugadorAgachado(modoJuegoFPS() && ctrlPulsado, deltaPostura);

			// Movimiento WASD
			mundoFisico.FijarVelocidadJugador(glm::vec3(0.0f));
			{
				fps::MovementInput entrada;
				entrada.activo = !editorEscenas.Activo() && entradaFPSActiva() && mundoFisico.HayJugador();
				entrada.quiereMoverse = entrada.activo && hayTeclaDeMovimiento(window);
				entrada.sprintPedido = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
					glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
				entrada.enSuelo = mundoFisico.JugadorEnSuelo();
				entrada.fraccionAgachado = mundoFisico.FraccionAgachado();
				const float velocidad = movimientoJugador.Actualizar(static_cast<float>(delta), entrada);
				if (entrada.activo)
					moverJugadorFPS(window, velocidad);
				barraEstamina.Actualizar(static_cast<float>(delta), movimientoJugador.FraccionEstamina(), movimientoJugador.Sprintando());
			}

			actualizarMovimientoMovil(window, static_cast<float>(delta));

			const bool animacionPausada = netgame::PausarMundoJuego(
				estadoActual == ESTADO_JUGANDO, editorEscenas.Activo(), fpsPausado,
				ventanaConFoco, gestorOpenXR.SesionEnMarcha(), redJuego.EsHost());
			actualizarPuertaDemo(animacionPausada ? 0.0f : static_cast<float>(delta));
			const auto inicioAnimacion = std::chrono::steady_clock::now();
			glb::ActualizarObjetos(objetosAnimados, animacionPausada ? 0.0f : static_cast<float>(delta));
			if (renderMetrics::enabled) renderMetrics::animacionMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicioAnimacion).count();
			gestorOpenXR.ProcesarEventos();

			if (gestorOpenXR.SesionEnMarcha() && entradaFPSActiva())
			{
				if (gestorOpenXR.GatilloRecienPulsado())
				{
			if (jugadorTieneAgarre()) iniciarCargaLanzamiento();
					else interactuar();
				}
				if (gestorOpenXR.GatilloRecienSoltado() && cargaLanzamientoActiva)
					lanzarObjetoCargado();

				if (gestorOpenXR.GatilloIzquierdoRecienPulsado())
					linternaMovilActiva = !linternaMovilActiva;
			}

			escenaFisica->SincronizarCinematicos(objetosAnimados);
			actualizarAgarreDelJugador(!animacionPausada && !mostrarPanelesDebug && modoJuegoFPS() &&
				(!redJuego.EsHost() || entradaFPSActiva()));
			mundoFisico.FijarPausa(animacionPausada);
			const auto inicioFisica = std::chrono::steady_clock::now();
			mundoFisico.Actualizar(static_cast<float>(delta));
			if (renderMetrics::enabled) renderMetrics::fisicaMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicioFisica).count();
			sincronizarJugadorYCamara();
			actualizarBalanceoVisual(static_cast<float>(delta));
		}

		// Red: los mensajes se aplican aqui (hilo de juego). Offline: no hace nada.
		actualizarRed(glfwGetTime(), estadoActual == ESTADO_JUGANDO);

		// Una sola consulta de Bullet por fotograma; el HUD y E consumen el mismo objetivo.
		actualizarHiriti(estadoActual == ESTADO_JUGANDO &&
			entradaFPSActiva() && !gestorOpenXR.SesionEnMarcha(),
			estadoActual == ESTADO_JUGANDO && !editorEscenas.Activo() && !redJuego.EsCliente(),
			static_cast<float>(delta));
		actualizarAudioAmbiente(estadoActual == ESTADO_JUGANDO && !editorEscenas.Activo(),
			static_cast<float>(delta));
		actualizarObjetivoInteraccion(estadoActual == ESTADO_JUGANDO);

		// La linterna se ancla durante el dibujo, una vez calculada la ViewMatrix actual.
		if (estadoActual != ESTADO_JUGANDO) llumGL[3].encesa = false;
		const auto inicioLuces = std::chrono::steady_clock::now();
		actualizarLucesDeEscena();
		if (renderMetrics::enabled) renderMetrics::lucesMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicioLuces).count();

		draw_Menu_ImGui();
		if (editorEscenas.ConsumeReconstruirFisicaRequest())
		{
			std::string errorEditor;
			if (!reconstruirFisicaTrasEdicion(errorEditor))
			{
				std::cerr << "No se pudo actualizar Bullet tras cambiar instancias GLB: " << errorEditor << '\n';
				editorEscenas.InformarEstado("Physics rebuild failed: " + errorEditor, true);
			}
		}
		if (editorEscenas.ConsumeGuardarRequest())
		{
			std::string errorGuardado;
			if (serializadorEscena.Guardar(objetosEscena, datosAnimadosEscena, errorGuardado))
			{
				editorEscenas.MarcarGuardado();
				editorEscenas.InformarEstado("Saved: " + serializadorEscena.Ruta().filename().string());
				std::cout << "Escena guardada en " << serializadorEscena.Ruta() << '\n';
				if (!serializadorEscena.RutaFuente().empty())
					std::cout << "Copia fuente actualizada en " << serializadorEscena.RutaFuente() << '\n';
				if (editorEscenas.ConsumeCerrarTrasGuardar()) glfwSetWindowShouldClose(window, GLFW_TRUE);
			}
			else
			{
				editorEscenas.InformarEstado("Save failed: " + errorGuardado, true);
				std::cerr << "No se guardó la escena: " << errorGuardado << '\n';
			}
		}
		if (editorEscenas.ConsumeCerrarConfirmado()) glfwSetWindowShouldClose(window, GLFW_TRUE);
		if (editorEscenas.ConsumeEntradaPlay())
		{
			std::string errorEditor;
			if (!reconstruirFisicaTrasEdicion(errorEditor))
			{
				std::cerr << "No se pudo reconstruir Bullet al salir del editor: " << errorEditor << '\n';
				editorEscenas.Activar(true);
			}
			else
			{
				mundoFisico.FijarVelocidadJugador(glm::vec3(0.0f));
				if (redJuego.Activa() && redJuego.EsHost())
				{
					std::vector<netgame::SceneObjectState> estados;
					if (!capturarEstadoEscenaRed(estados) || !redJuego.FijarEstadoEscena(estados, true))
						editorEscenas.InformarEstado("La sesión solo replica objetos existentes; no se sincronizaron altas, bajas o IDs no válidos.", true);
				}
			}
			// Sin saltos: se descartan el ultimo cursor, los deltas del raton y la referencia del balanceo.
			fpsTienePosicionAnterior = false;
			mouseDeltaX = mouseDeltaY = 0.0;
			hayPiesFotogramaAnterior = false;
			sincronizarCursorFPS(window);
		}

		// Con VR en marcha, la escena se dibuja en el visor (y se refleja en la ventana). Sin VR, o si el visor
		// no está mostrando imagen, se dibuja la vista de escritorio con normalidad.
		{
			static bool vrEnMarchaAntes = false;
			const bool vrEnMarcha = gestorOpenXR.SesionEnMarcha();
			if (vrEnMarcha != vrEnMarchaAntes)
			{	// xrWaitFrame ya marca el ritmo del visor: la sincronía vertical del monitor lo frenaría
				glfwSwapInterval(vrEnMarcha ? 0 : 1);
				vrEnMarchaAntes = vrEnMarcha;
			}

			const bool vrDibujado = !editorEscenas.Activo() && vrEnMarcha && dibujarEscenaVR();
			if (!vrDibujado)
			{
				// Guardamos el tamaño real de la ventana antes de cambiar w y h para el FBO PS1
				int winW = 0, winH = 0;
				glfwGetFramebufferSize(window, &winW, &winH);

				// EDIT: resolución nativa.
				// PLAY: resolución PS1 (320x240).
				const bool modoEditor = editorEscenas.Activo();

				glBindFramebuffer(
					GL_FRAMEBUFFER,
					modoEditor ? 0 : ps1.fbo
				);

				w = modoEditor ? winW : Ps1Framebuffer::kWidth;
				h = modoEditor ? winH : Ps1Framebuffer::kHeight;

				glViewport(0, 0, w, h);
				OnPaint(window);

				// Solo aplicar el escalado pixelado durante PLAY.
				if (!modoEditor)
				{
					glBindFramebuffer(GL_READ_FRAMEBUFFER, ps1.fbo);
					glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

					glBlitFramebuffer(
						0, 0, Ps1Framebuffer::kWidth, Ps1Framebuffer::kHeight,
						0, 0, winW, winH,
						GL_COLOR_BUFFER_BIT,
						GL_NEAREST
					);
				}

				// Restaurar framebuffer y dimensiones reales.
				glBindFramebuffer(GL_FRAMEBUFFER, 0);

				w = winW;
				h = winH;

				glViewport(0, 0, winW, winH);

			}

			// Renderizado final de ImGui e intercambio de buffers
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			glfwSwapBuffers(window);
		}
	}
}
