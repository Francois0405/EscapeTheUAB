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
		fpsPaused = (currentGameState != ESTADO_JUGANDO);

		sceneEditor.Actualizar(window, static_cast<float>(delta));
		syncFpsCursor(window);
		updateThrowChargeInput(window, static_cast<float>(delta));

		// 3. Movimiento, física y VR (solo cuando estamos jugando)
		if (currentGameState == ESTADO_JUGANDO)
		{
			const bool ctrlPulsado = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
				glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
			const float deltaPostura = (sceneEditor.Activo() || (fpsModeActive() && !fpsInputActive()))
				? 0.0f : fps::LimitarDelta(static_cast<float>(delta));
			physicsWorld.ActualizarJugadorAgachado(fpsModeActive() && ctrlPulsado, deltaPostura);

			// Movimiento WASD
			physicsWorld.FijarVelocidadJugador(glm::vec3(0.0f));
			{
				fps::MovementInput entrada;
				entrada.activo = !sceneEditor.Activo() && fpsInputActive() && physicsWorld.HayJugador();
				entrada.quiereMoverse = entrada.activo && movementKeyPressed(window);
				entrada.sprintPedido = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
					glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
				entrada.enSuelo = physicsWorld.JugadorEnSuelo();
				entrada.fraccionAgachado = physicsWorld.FraccionAgachado();
				const float velocidad = playerMovement.Actualizar(static_cast<float>(delta), entrada);
				if (entrada.activo)
					moveFirstPersonPlayer(window, velocidad);
				staminaBar.Actualizar(static_cast<float>(delta), playerMovement.FraccionEstamina(), playerMovement.Sprintando());
			}

			updatePhoneMotion(window, static_cast<float>(delta));

			const bool animacionPausada = netgame::PausarMundoJuego(
				currentGameState == ESTADO_JUGANDO, sceneEditor.Activo(), fpsPaused,
				windowFocused, openXrManager.SesionEnMarcha(), networkGame.EsHost());
			updateDemoDoor(animacionPausada ? 0.0f : static_cast<float>(delta));
			const auto inicioAnimacion = std::chrono::steady_clock::now();
			glb::ActualizarObjetos(animatedObjects, animacionPausada ? 0.0f : static_cast<float>(delta));
			if (renderMetrics::enabled) renderMetrics::animacionMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicioAnimacion).count();
			openXrManager.ProcesarEventos();

			if (openXrManager.SesionEnMarcha() && fpsInputActive())
			{
				if (openXrManager.GatilloRecienPulsado())
				{
			if (playerHoldsObject()) beginThrowCharge();
					else performInteraction();
				}
				if (openXrManager.GatilloRecienSoltado() && throwChargeActive)
					throwHeldObject();

				if (openXrManager.GatilloIzquierdoRecienPulsado())
					phoneFlashlightEnabled = !phoneFlashlightEnabled;
			}

			physicsScene->SincronizarCinematicos(animatedObjects);
			updateHeldObject(!animacionPausada && !showDebugPanels && fpsModeActive() &&
				(!networkGame.EsHost() || fpsInputActive()));
			physicsWorld.FijarPausa(animacionPausada);
			const auto inicioFisica = std::chrono::steady_clock::now();
			physicsWorld.Actualizar(static_cast<float>(delta));
			if (renderMetrics::enabled) renderMetrics::fisicaMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicioFisica).count();
			syncPlayerWithCamera();
			updateCameraBobbing(static_cast<float>(delta));
		}

		// Red: los mensajes se aplican aqui (hilo de juego). Offline: no hace nada.
		updateNetwork(glfwGetTime(), currentGameState == ESTADO_JUGANDO);

		// Una sola consulta de Bullet por fotograma; el HUD y E consumen el mismo objetivo.
		updateHiritiBehaviour(currentGameState == ESTADO_JUGANDO &&
			fpsInputActive() && !openXrManager.SesionEnMarcha(),
			currentGameState == ESTADO_JUGANDO && !sceneEditor.Activo() && !networkGame.EsCliente(),
			static_cast<float>(delta));
		updateAmbientSound(currentGameState == ESTADO_JUGANDO && !sceneEditor.Activo(),
			static_cast<float>(delta));
		updateCurrentInteractionTarget(currentGameState == ESTADO_JUGANDO);

		// La linterna se ancla durante el dibujo, una vez calculada la ViewMatrix actual.
		if (currentGameState != ESTADO_JUGANDO) llumGL[3].encesa = false;
		const auto inicioLuces = std::chrono::steady_clock::now();
		updateSceneLights();
		if (renderMetrics::enabled) renderMetrics::lucesMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicioLuces).count();

		draw_Menu_ImGui();
		if (sceneEditor.ConsumeReconstruirFisicaRequest())
		{
			std::string errorEditor;
			if (!rebuildPhysicsAfterEditing(errorEditor))
			{
				std::cerr << "No se pudo actualizar Bullet tras cambiar instancias GLB: " << errorEditor << '\n';
				sceneEditor.InformarEstado("Physics rebuild failed: " + errorEditor, true);
			}
		}
		if (sceneEditor.ConsumeGuardarRequest())
		{
			std::string errorGuardado;
			if (sceneSerializer.Guardar(sceneObjects, sceneAnimatedData, errorGuardado))
			{
				sceneEditor.MarcarGuardado();
				sceneEditor.InformarEstado("Saved: " + sceneSerializer.Ruta().filename().string());
				std::cout << "Escena guardada en " << sceneSerializer.Ruta() << '\n';
				if (!sceneSerializer.RutaFuente().empty())
					std::cout << "Copia fuente actualizada en " << sceneSerializer.RutaFuente() << '\n';
				if (sceneEditor.ConsumeCerrarTrasGuardar()) glfwSetWindowShouldClose(window, GLFW_TRUE);
			}
			else
			{
				sceneEditor.InformarEstado("Save failed: " + errorGuardado, true);
				std::cerr << "No se guardó la escena: " << errorGuardado << '\n';
			}
		}
		if (sceneEditor.ConsumeCerrarConfirmado()) glfwSetWindowShouldClose(window, GLFW_TRUE);
		if (sceneEditor.ConsumeEntradaPlay())
		{
			std::string errorEditor;
			if (!rebuildPhysicsAfterEditing(errorEditor))
			{
				std::cerr << "No se pudo reconstruir Bullet al salir del editor: " << errorEditor << '\n';
				sceneEditor.Activar(true);
			}
			else
			{
				physicsWorld.FijarVelocidadJugador(glm::vec3(0.0f));
				if (networkGame.Activa() && networkGame.EsHost())
				{
					std::vector<netgame::SceneObjectState> estados;
					if (!captureNetworkSceneState(estados) || !networkGame.FijarEstadoEscena(estados, true))
						sceneEditor.InformarEstado("La sesión solo replica objetos existentes; no se sincronizaron altas, bajas o IDs no válidos.", true);
				}
			}
			// Sin saltos: se descartan el ultimo cursor, los deltas del raton y la referencia del balanceo.
			fpsHasPreviousPosition = false;
			mouseDeltaX = mouseDeltaY = 0.0;
			hasPreviousFrameFeet = false;
			syncFpsCursor(window);
		}

		// Con VR en marcha, la escena se dibuja en el visor (y se refleja en la ventana). Sin VR, o si el visor
		// no está mostrando imagen, se dibuja la vista de escritorio con normalidad.
		{
			static bool vrEnMarchaAntes = false;
			const bool vrEnMarcha = openXrManager.SesionEnMarcha();
			if (vrEnMarcha != vrEnMarchaAntes)
			{	// xrWaitFrame ya marca el ritmo del visor: la sincronía vertical del monitor lo frenaría
				glfwSwapInterval(vrEnMarcha ? 0 : 1);
				vrEnMarchaAntes = vrEnMarcha;
			}

			const bool vrDibujado = !sceneEditor.Activo() && vrEnMarcha && drawVrScene();
			if (!vrDibujado)
			{
				// Guardamos el tamaño real de la ventana antes de cambiar w y h para el FBO PS1
				int winW = 0, winH = 0;
				glfwGetFramebufferSize(window, &winW, &winH);

				// EDIT: resolución nativa.
				// PLAY: resolución PS1 (320x240).
				const bool modoEditor = sceneEditor.Activo();

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
