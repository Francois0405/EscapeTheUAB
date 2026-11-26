#include "src/app/AppIncludes.h"
#include "src/app/state/SceneState.h"
#include "src/app/state/RenderState.h"
#include "src/app/state/PlayerState.h"
#include "src/app/state/GameplayState.h"
#include "src/app/state/NetworkState.h"
#include "src/app/state/VrState.h"
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
#include "src/app/render/SceneLighting.h"
#include "src/app/ui/UiFrame.h"
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
		fpsPaused = (currentGameState != GAME_STATE_PLAYING);

		sceneEditor.Update(window, static_cast<float>(delta));
		syncFpsCursor(window);
		updateThrowChargeInput(window, static_cast<float>(delta));

		// 3. Movimiento, física y VR (solo cuando estamos jugando)
		if (currentGameState == GAME_STATE_PLAYING)
		{
			const bool ctrlPulsado = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
				glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
			const float deltaPostura = (sceneEditor.IsEnabled() || (fpsModeActive() && !fpsInputActive()))
				? 0.0f : fps::ClampDelta(static_cast<float>(delta));
			physicsWorld.UpdatePlayerCrouch(fpsModeActive() && ctrlPulsado, deltaPostura);

			// Movimiento WASD
			physicsWorld.SetPlayerVelocity(glm::vec3(0.0f));
			{
				fps::MovementInput entrada;
				entrada.activo = !sceneEditor.IsEnabled() && fpsInputActive() && physicsWorld.HasPlayerBody();
				entrada.wantsToMove = entrada.activo && movementKeyPressed(window);
				entrada.sprintRequested = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
					glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
				entrada.enSuelo = physicsWorld.IsPlayerGrounded();
				entrada.crouchRatio = physicsWorld.CrouchFraction();
				const float speed = playerMovement.Update(static_cast<float>(delta), entrada);
				if (entrada.activo)
					moveFirstPersonPlayer(window, speed);
				staminaBar.Update(static_cast<float>(delta), playerMovement.StaminaFraction(), playerMovement.IsSprinting());
			}

			updatePhoneMotion(window, static_cast<float>(delta));

			const bool animacionPausada = netgame::ShouldPauseGameWorld(
				currentGameState == GAME_STATE_PLAYING, sceneEditor.IsEnabled(), fpsPaused,
				windowFocused, openXrManager.IsSessionRunning(), networkGame.IsHosting());
			updateDemoDoor(animacionPausada ? 0.0f : static_cast<float>(delta));
			const auto inicioAnimacion = std::chrono::steady_clock::now();
			glb::UpdateAnimatedObjects(animatedObjects, animacionPausada ? 0.0f : static_cast<float>(delta));
			if (renderMetrics::enabled) renderMetrics::animacionMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicioAnimacion).count();
			openXrManager.ProcessEvents();

			if (openXrManager.IsSessionRunning() && fpsInputActive())
			{
				if (openXrManager.GatilloRecienPulsado())
				{
			if (playerHoldsObject()) beginThrowCharge();
					else performInteraction();
				}
				if (openXrManager.TriggerJustReleased() && throwChargeActive)
					throwHeldObject();

				if (openXrManager.LeftTriggerJustPressed())
					phoneFlashlightEnabled = !phoneFlashlightEnabled;
			}

			physicsScene->SyncKinematics(animatedObjects);
			updateHeldObject(!animacionPausada && !showDebugPanels && fpsModeActive() &&
				(!networkGame.IsHosting() || fpsInputActive()));
			physicsWorld.SetPaused(animacionPausada);
			const auto inicioFisica = std::chrono::steady_clock::now();
			physicsWorld.Update(static_cast<float>(delta));
			if (renderMetrics::enabled) renderMetrics::fisicaMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicioFisica).count();
			syncPlayerWithCamera();
			updateCameraBobbing(static_cast<float>(delta));
		}

		// Red: los mensajes se aplican aqui (hilo de juego). Offline: no hace nada.
		updateNetwork(glfwGetTime(), currentGameState == GAME_STATE_PLAYING);

		// Una sola consulta de Bullet por fotograma; el HUD y E consumen el mismo objetivo.
		updateHiritiBehaviour(currentGameState == GAME_STATE_PLAYING &&
			fpsInputActive() && !openXrManager.IsSessionRunning(),
			currentGameState == GAME_STATE_PLAYING && !sceneEditor.IsEnabled() && !networkGame.IsClient(),
			static_cast<float>(delta));
		updateAmbientSound(currentGameState == GAME_STATE_PLAYING && !sceneEditor.IsEnabled(),
			static_cast<float>(delta));
		updateCurrentInteractionTarget(currentGameState == GAME_STATE_PLAYING);

		// La linterna se ancla durante el dibujo, una vez calculada la ViewMatrix actual.
		if (currentGameState != GAME_STATE_PLAYING) llumGL[3].encesa = false;
		const auto inicioLuces = std::chrono::steady_clock::now();
		updateSceneLights();
		if (renderMetrics::enabled) renderMetrics::lucesMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicioLuces).count();

		draw_Menu_ImGui();
		if (sceneEditor.ConsumeRebuildPhysicsRequest())
		{
			std::string errorEditor;
			if (!rebuildPhysicsAfterEditing(errorEditor))
			{
				std::cerr << "No se pudo actualizar Bullet tras cambiar instancias GLB: " << errorEditor << '\n';
				sceneEditor.ReportStatus("Physics rebuild failed: " + errorEditor, true);
			}
		}
		if (sceneEditor.ConsumeSaveRequest())
		{
			std::string errorGuardado;
			if (sceneSerializer.SaveScene(sceneObjects, sceneAnimatedData, errorGuardado))
			{
				sceneEditor.MarkSaved();
				sceneEditor.ReportStatus("Saved: " + sceneSerializer.Ruta().filename().string());
				std::cout << "Escena guardada en " << sceneSerializer.Ruta() << '\n';
				if (!sceneSerializer.RutaFuente().empty())
					std::cout << "Copia fuente actualizada en " << sceneSerializer.RutaFuente() << '\n';
				if (sceneEditor.ConsumeCloseAfterSave()) glfwSetWindowShouldClose(window, GLFW_TRUE);
			}
			else
			{
				sceneEditor.ReportStatus("Save failed: " + errorGuardado, true);
				std::cerr << "No se guardó la escena: " << errorGuardado << '\n';
			}
		}
		if (sceneEditor.ConsumeCloseConfirmed()) glfwSetWindowShouldClose(window, GLFW_TRUE);
		if (sceneEditor.ConsumePlayEntry())
		{
			std::string errorEditor;
			if (!rebuildPhysicsAfterEditing(errorEditor))
			{
				std::cerr << "No se pudo reconstruir Bullet al salir del editor: " << errorEditor << '\n';
				sceneEditor.SetActive(true);
			}
			else
			{
				physicsWorld.SetPlayerVelocity(glm::vec3(0.0f));
				if (networkGame.IsActive() && networkGame.IsHosting())
				{
					std::vector<netgame::SceneObjectState> estados;
					if (!captureNetworkSceneState(estados) || !networkGame.SetSceneState(estados, true))
						sceneEditor.ReportStatus("La sesión solo replica objetos existentes; no se sincronizaron altas, bajas o IDs no válidos.", true);
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
			const bool vrEnMarcha = openXrManager.IsSessionRunning();
			if (vrEnMarcha != vrEnMarchaAntes)
			{	// xrWaitFrame ya marca el ritmo del visor: la sincronía vertical del monitor lo frenaría
				glfwSwapInterval(vrEnMarcha ? 0 : 1);
				vrEnMarchaAntes = vrEnMarcha;
			}

			const bool vrDibujado = !sceneEditor.IsEnabled() && vrEnMarcha && drawVrScene();
			if (!vrDibujado)
			{
				// Guardamos el tamaño real de la ventana antes de cambiar w y h para el FBO PS1
				int winW = 0, winH = 0;
				glfwGetFramebufferSize(window, &winW, &winH);

				// EDIT: resolución nativa.
				// PLAY: resolución PS1 (320x240).
				const bool modoEditor = sceneEditor.IsEnabled();

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
