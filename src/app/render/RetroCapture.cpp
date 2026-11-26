#include "src/app/AppIncludes.h"
#include "src/app/state/SceneState.h"
#include "src/app/state/RenderState.h"
#include "src/app/state/PlayerState.h"
#include "src/app/state/GameplayState.h"
#include "src/app/state/VrState.h"
#include "src/app/adapters/HirityAdapters.h"
#include "src/app/adapters/PlayerAdapters.h"
#include "src/app/render/RetroCapture.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace
{
	constexpr int kWarmup = 120;    // fotogramas sin medir tras cambiar de fase
	constexpr int kMeasured = 300;

	struct Phase { const char* name; retro::Preset preset; bool grab; bool stereo; float fog; };
	constexpr Phase kPhases[] = {
		{ "original", retro::Preset::Original, false, false, 0.0f },
		{ "cinematic", retro::Preset::Cinematic, false, false, 0.0f },
		{ "psx", retro::Preset::PsxClassic, false, false, 0.0f },
		{ "cinematic_fog", retro::Preset::Cinematic, false, false, 0.04f },
		{ "held_charge_subtitle", retro::Preset::Cinematic, true, false, 0.0f },
		{ "f2_stereo", retro::Preset::Cinematic, false, true, 0.0f },
		{ "original_again", retro::Preset::Original, false, false, 0.0f },
	};
	constexpr int kPhaseCount = static_cast<int>(sizeof(kPhases) / sizeof(kPhases[0]));

	int phase = 0;
	int frame = 0;
	int grabbed = -1;
	double lastTime = 0.0;
	double frameSum = 0.0;
	double gpuSum = 0.0;
	std::ofstream csv;

	const std::filesystem::path& captureDirectory()
	{
		static const std::filesystem::path carpeta = []
		{
			const char* valor = std::getenv("ENTORNVGI_RETRO_CAPTURE");
			return std::filesystem::path(valor ? valor : "");
		}();
		return carpeta;
	}

	// Coloca un objeto dinamico (no Hiriti) en la mano y lo agarra.
	void grabSomething()
	{
		glm::vec3 origen(0.0f), direccion(0.0f);
		gazeOriginAndDirection(origen, direccion);
		const glm::vec3 destino = physics::PhysicsScene::GrabDestination(origen, direccion);
		for (std::size_t i = 0; i < sceneObjects.size(); ++i)
		{
			const SceneObject& objeto = sceneObjects[i];
			if (objeto.id == "hiriti" || !objeto.fisica.presente ||
				objeto.fisica.tipo != physics::BodyType::Dynamic) continue;
			if (!physicsScene->TeleportDynamicObject(static_cast<int>(i), destino)) continue;
			grabbed = physicsScene->TryGrab(origen, glm::normalize(destino - origen));
			if (grabbed >= 0) return;
		}
	}
}

bool retroCaptureEnabled() { return !captureDirectory().empty(); }

void retroCaptureBeforeUi(GLFWwindow* window)
{
	if (!retroCaptureEnabled() || phase >= kPhaseCount) return;
	const Phase& actual = kPhases[phase];
	if (frame == 0)
	{
		if (phase == 0)
		{
			std::error_code error;
			std::filesystem::create_directories(captureDirectory(), error);
			csv.open(captureDirectory() / "metricas.csv", std::ios::trunc);
			csv << "fase,ancho_ventana,alto_ventana,ancho_interno,alto_interno,frametime_ms,fps,gpu_escena_ms,gl_error,agarrado,renderer\n";
			audioManager.setMasterVolume(0.0f);
			glfwSwapInterval(0);   // con vsync todos los presets medirian lo mismo
			renderMetrics::enabled = true;
			currentGameState = GAME_STATE_PLAYING;
		}
		retro::ApplyPreset(retroSettings, actual.preset);
		retroSettings.fogDensity = actual.fog;
		retroSettings.heldViewModel = actual.grab;
		stereoTestMode = actual.stereo;
		if (actual.grab) grabSomething();
		else if (physicsScene->HasGrab()) physicsScene->ReleaseGrab();
	}
	if (actual.grab)
	{
		// Anillo a ~70 % y un dialogo real sonando (en silencio) en el momento de la captura.
		throwChargeActive = physicsScene->HasGrab();
		throwChargeTime = 0.875f;
		if (frame == kWarmup + kMeasured - 3) playHiritiSoundCue("assets/audio/hirity/story/agarrame.ogg", 1.0f);
	}
	else throwChargeActive = false;
}

void retroCaptureAfterUi(GLFWwindow* window)
{
	if (!retroCaptureEnabled() || phase >= kPhaseCount) return;
	const double ahora = glfwGetTime();
	if (frame > kWarmup)
	{
		frameSum += (ahora - lastTime) * 1000.0;
		gpuSum += lastGpuMs;
	}
	lastTime = ahora;
	if (++frame <= kWarmup + kMeasured) return;

	int ancho = 0, alto = 0;
	glfwGetFramebufferSize(window, &ancho, &alto);
	const Phase& actual = kPhases[phase];
	const retro::Size interna = retro::InternalSize(retroSettings,
		retro::IsActive(retroSettings, false, stereoTestMode, false), ancho, alto);
	const double media = frameSum / kMeasured;
	const std::string png = (captureDirectory() / (std::string(actual.name) + ".png")).string();
	SOIL_save_screenshot(png.c_str(), SOIL_SAVE_TYPE_PNG, 0, 0, ancho, alto);
	csv << actual.name << ',' << ancho << ',' << alto << ',' << interna.width << ',' << interna.height << ','
		<< media << ',' << (media > 0.0 ? 1000.0 / media : 0.0) << ',' << gpuSum / kMeasured << ','
		<< glGetError() << ',' << (physicsScene->HasGrab() ? sceneObjects[physicsScene->GrabbedObject()].id : "")
		<< ',' << reinterpret_cast<const char*>(glGetString(GL_RENDERER)) << '\n' << std::flush;

	frame = 0;
	frameSum = gpuSum = 0.0;
	if (++phase >= kPhaseCount) glfwSetWindowShouldClose(window, GLFW_TRUE);
}
