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
#include "src/rendering/RetroPostFX.h"
#include "src/app/render/RetroCapture.h"

namespace
{
	// Filtrado nearest solo en las texturas de material del entorno: las incrustadas en los OBJ
	// (carteles, pistas) y las de la interfaz conservan el filtrado original. Es estado global de la
	// textura, asi que el editor y OpenXR necesitan que se restaure antes de dibujar.
	void applyEnvironmentTextureFilter(bool retroActive)
	{
		static bool aplicado = false;
		static std::size_t cantidad = 0;
		const bool nearest = retroActive && retroSettings.nearestTextures;
		if (nearest == aplicado && (!nearest || cantidad == sceneMaterialTextures.size())) return;
		GLint anterior = 0;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &anterior);
		for (const auto& entrada : sceneMaterialTextures)
		{
			glBindTexture(GL_TEXTURE_2D, entrada.second);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, nearest ? GL_NEAREST : GL_LINEAR);
			// Los mipmaps se conservan para limitar el aliasing a distancia.
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
				nearest ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_LINEAR);
		}
		glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(anterior));
		aplicado = nearest;
		cantidad = sceneMaterialTextures.size();
	}
}

void RunMainLoop(Ps1Framebuffer& ps1, float previous, const RetroPostFX& postFx)
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
		{
			// Partida nueva (menu principal -> jugar): la cordura vuelve al 100 %. La pausa no la consume.
			static GameState estadoAnterior = currentGameState;
			if (estadoAnterior == GAME_STATE_MAIN_MENU && currentGameState == GAME_STATE_PLAYING)
			{
				sanitySystem.Reset();
				sanityEffects.Reset();
			}
			estadoAnterior = currentGameState;
			if (currentGameState == GAME_STATE_PLAYING && !sceneEditor.IsEnabled())
			{
				sanitySystem.Update(static_cast<float>(delta));
				sanityEffects.Update(static_cast<float>(delta), sanitySystem.Insanity());
			}
		}
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

		retroCaptureBeforeUi(window);
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

			// Editor, F2 y OpenXR conservan la presentacion anterior.
			const bool retroActivo = retro::IsActive(retroSettings, sceneEditor.IsEnabled(), stereoTestMode, vrEnMarcha);
			applyEnvironmentTextureFilter(retroActivo);
			const bool vrDibujado = !sceneEditor.IsEnabled() && vrEnMarcha && drawVrScene();
			if (!vrDibujado)
			{
				// Guardamos el tamaño real de la ventana antes de cambiar w y h para el FBO PS1
				int winW = 0, winH = 0;
				glfwGetFramebufferSize(window, &winW, &winH);

				// EDIT: resolución nativa.
				// PLAY: resolución interna del preset retro (sin efectos = 960x540).
				const bool modoEditor = sceneEditor.IsEnabled();
				const retro::Size interna = retro::InternalSize(retroSettings, retroActivo, winW, winH);
				if (!modoEditor)
					ps1.Resize(interna.width, interna.height);

				glBindFramebuffer(
					GL_FRAMEBUFFER,
					modoEditor ? 0 : ps1.fbo
				);

				w = modoEditor ? winW : ps1.width;
				h = modoEditor ? winH : ps1.height;

				glViewport(0, 0, w, h);
				OnPaint(window);

				// Solo escalar durante PLAY. Sin efectos (o sin shader) se usa el blit de siempre.
				if (!modoEditor)
				{
					if (retroActivo && postFx.Ready())
						postFx.Draw(ps1.color, { ps1.width, ps1.height }, winW, winH, retroSettings);
					else
					{
						glBindFramebuffer(GL_READ_FRAMEBUFFER, ps1.fbo);
						glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

						glBlitFramebuffer(
							0, 0, ps1.width, ps1.height,
							0, 0, winW, winH,
							GL_COLOR_BUFFER_BIT,
							GL_NEAREST
						);
					}
				}

				// Restaurar framebuffer y dimensiones reales.
				glBindFramebuffer(GL_FRAMEBUFFER, 0);

				w = winW;
				h = winH;

				glViewport(0, 0, winW, winH);

			}

			// Renderizado final de ImGui e intercambio de buffers
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			retroCaptureAfterUi(window);
			glfwSwapBuffers(window);
		}
	}
}
