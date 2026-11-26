// Presets, persistencia, dithering de referencia, viewmodel y subtitulos
// (sin ventana, audio ni OpenGL).
#include "src/audio/SubtitleManager.h"
#include "src/rendering/HeldViewModel.h"
#include "src/rendering/RetroSettings.h"

#include <cmath>
#include <iostream>
#include <set>
#include <string>

namespace
{
	int failures = 0;

	void Check(bool condition, const char* what)
	{
		if (condition) return;
		std::cerr << "FAIL: " << what << '\n';
		++failures;
	}

	bool Near(float a, float b, float tolerance = 1.0e-4f) { return std::abs(a - b) <= tolerance; }

	void TestSettings()
	{
		retro::Settings s;
		Check(s.preset == retro::Preset::PsxClassic && !s.modified && s.internalHeight == 360 &&
			s.bitsPerChannel == 4 && s.dither == retro::DitherMode::Chromatic && s.pattern == retro::Pattern::Bayer8 &&
			Near(s.ditherStrength, 1.0f) && Near(s.saturation, 0.90f) && Near(s.contrast, 1.12f) &&
			s.exposureEV == 0.0f && s.grain == 0.0f && Near(s.vignette, 0.10f) && s.grading == 0.0f &&
			Near(s.fogDensity, 0.040f) && s.nearestTextures && !s.heldViewModel, "official defaults");
		retro::Settings oficial;
		retro::ApplyPreset(oficial, retro::Preset::PsxClassic);
		Check(retro::Serialize(oficial) == retro::Serialize(retro::Settings{}), "PSX Classic is the official look");

		// Las alternativas de depuracion conservan todos sus valores anteriores.
		for (const retro::Preset alternativa : { retro::Preset::Cinematic, retro::Preset::Original })
		{
			retro::Settings a;
			retro::ApplyPreset(a, alternativa);
			Check(a.preset == alternativa && a.internalHeight == 540 && a.bitsPerChannel == 5 &&
				a.dither == retro::DitherMode::Chromatic && a.pattern == retro::Pattern::Bayer4 &&
				Near(a.ditherStrength, 0.85f) && Near(a.saturation, 0.92f) && Near(a.contrast, 1.08f) &&
				a.exposureEV == 0.0f && a.grain == 0.0f && a.vignette == 0.0f && a.grading == 0.0f &&
				a.fogDensity == 0.0f && a.nearestTextures == (alternativa == retro::Preset::Cinematic),
				"Cinematic and Original unchanged");
		}

		s.subtitles = false;
		s.bitsPerChannel = 7;
		s.modified = true;
		retro::ApplyPreset(s, s.preset);
		Check(s.bitsPerChannel == 4 && !s.modified && Near(s.vignette, 0.10f), "restore preset");
		Check(!s.subtitles, "preset keeps accessibility choices");

		retro::ApplyPreset(s, retro::Preset::Original);
		Check(!s.nearestTextures, "Original keeps linear textures");
		Check(!retro::IsActive(s, false, false, false), "Original is never active");
		retro::ApplyPreset(s, retro::Preset::Cinematic);
		Check(retro::IsActive(s, false, false, false), "active in desktop play");
		Check(!retro::IsActive(s, true, false, false) && !retro::IsActive(s, false, true, false) &&
			!retro::IsActive(s, false, false, true), "editor, F2 and OpenXR keep the old path");

		const retro::Size inactivo = retro::InternalSize(s, false, 2560, 1080);
		Check(inactivo.width == 960 && inactivo.height == 540, "inactive keeps 960x540");
		Check(retro::InternalSize(s, true, 1920, 1080).width == 960, "16:9 internal width");
		Check(retro::InternalSize(s, true, 2560, 1080).width == 1280, "width follows ultrawide aspect");
		Check(retro::InternalSize(s, true, 1600, 1200).width == 720, "width follows 4:3 aspect");
		Check(retro::InternalSize(s, true, 0, 0).height == 540, "minimised window has a safe size");

		s.ditherStrength = 1.5f;
		s.modified = true;
		const retro::Settings leido = retro::Parse(retro::Serialize(s));
		Check(leido.preset == retro::Preset::Cinematic && leido.modified && leido.ditherStrength == 1.5f &&
			!leido.subtitles, "round trip");

		// Un ini ya guardado se respeta, aunque no sea el oficial.
		const retro::Settings antiguo = retro::Parse("preset=1\nmodified=0\ninternalHeight=540\nbitsPerChannel=5\n"
			"pattern=0\nvignette=0\nfogDensity=0\n");
		Check(antiguo.preset == retro::Preset::Cinematic && antiguo.internalHeight == 540 &&
			antiguo.vignette == 0.0f && antiguo.fogDensity == 0.0f, "existing preferences are preserved");
		const retro::Settings psxAntiguo = retro::Parse("preset=2\nmodified=0\nvignette=0\nfogDensity=0\nsubtitles=0\n");
		Check(Near(psxAntiguo.vignette, 0.10f) && Near(psxAntiguo.fogDensity, 0.040f) && !psxAntiguo.subtitles,
			"untouched old PSX file adopts the official look and keeps other choices");
		const retro::Settings psxRetocado = retro::Parse("preset=2\nmodified=1\nvignette=0\nfogDensity=0\n");
		Check(psxRetocado.vignette == 0.0f && psxRetocado.fogDensity == 0.0f, "tweaked old file is preserved");
		const retro::Settings vigente = retro::Parse("version=2\npreset=2\nmodified=0\nvignette=0\n");
		Check(vigente.vignette == 0.0f, "current files are read as written");
		const retro::Settings parcial = retro::Parse("preset=1\n");
		Check(parcial.internalHeight == 540 && parcial.pattern == retro::Pattern::Bayer4 &&
			parcial.fogDensity == 0.0f, "missing keys come from the saved preset");
		Check(retro::Serialize(retro::Parse("")) == retro::Serialize(retro::Settings{}),
			"new install gets the official look");
		Check(retro::Serialize(retro::Settings{}).rfind("version=2\n", 0) == 0, "file is versioned");

		const retro::Settings roto = retro::Parse(
			"version=2\npreset=99\nbitsPerChannel=0\ninternalHeight=-5\ncontrast=nan\ngrain=7\nexposureEV=9\nbasura\n=3\n");
		Check(roto.preset == retro::Preset::PsxClassic && roto.bitsPerChannel == 3 &&
			roto.internalHeight == retro::kMinHeight, "invalid integers become safe");
		Check(roto.contrast == retro::Settings{}.contrast && roto.grain == 0.03f && roto.exposureEV == 1.0f,
			"invalid floats become safe");
	}

	void TestDithering()
	{
		std::set<int> cuatro, ocho;
		for (int y = 0; y < 8; ++y)
			for (int x = 0; x < 8; ++x)
			{
				if (x < 4 && y < 4) cuatro.insert(retro::Bayer4Value(x, y));
				ocho.insert(retro::Bayer8Value(x, y));
				for (int bits = 3; bits <= 8; ++bits)
				{
					Check(retro::Quantize(0.0f, bits, retro::Bayer8Offset(x, y)) == 0.0f, "black stays black");
					Check(retro::Quantize(1.0f, bits, retro::Bayer8Offset(x, y)) == 1.0f, "white stays white");
				}
			}
		Check(cuatro.size() == 16 && *cuatro.rbegin() == 15, "Bayer 4x4 covers 0..15");
		Check(ocho.size() == 64 && *ocho.rbegin() == 63, "Bayer 8x8 covers 0..63");

		float media = 0.0f;
		for (int y = 0; y < 4; ++y)
			for (int x = 0; x < 4; ++x) media += retro::Quantize(0.4f, 3, retro::Bayer4Offset(x, y)) / 16.0f;
		Check(std::abs(media - 0.4f) < 1.0f / 7.0f / 16.0f + 1.0e-4f, "dithering preserves mean intensity");

		Check(retro::ChannelShift(0).x == 0 && retro::ChannelShift(0).y == 0 &&
			retro::ChannelShift(1).x == 1 && retro::ChannelShift(1).y == 2 &&
			retro::ChannelShift(2).x == 2 && retro::ChannelShift(2).y == 1, "channel shifts");
		retro::Settings s;
		s.pattern = retro::Pattern::Bayer4;
		s.ditherStrength = 1.0f;
		Check(Near(retro::DitherOffset(s, 0, 0, 1), retro::Bayer4Offset(1, 2)), "chromatic uses per channel shift");
		s.dither = retro::DitherMode::Mono;
		Check(Near(retro::DitherOffset(s, 3, 1, 2), retro::Bayer4Offset(3, 1)), "mono shares one threshold");
		s.dither = retro::DitherMode::Off;
		Check(retro::DitherOffset(s, 3, 1, 2) == 0.0f, "dither off");
	}

	void TestViewModel()
	{
		const std::vector<std::uint32_t> hashes = { 11, 22, 33 };
		Check(viewmodel::SelectLocalHeld(2, false, 0, hashes) == 2, "host/offline uses the local grab");
		Check(viewmodel::SelectLocalHeld(-1, true, 22, hashes) == 1, "client uses the host assignment");
		Check(viewmodel::SelectLocalHeld(-1, true, 0, hashes) == -1, "client holding nothing");
		Check(viewmodel::SelectLocalHeld(-1, false, 22, hashes) == -1, "host ignores remote assignments");
		Check(viewmodel::SelectLocalHeld(-1, true, 99, hashes) == -1, "unknown id");

		const glm::mat4 projection = glm::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.05f, 100.0f);
		const glm::mat4 view = glm::lookAt(glm::vec3(1, 2, 3), glm::vec3(4, 2, 3), glm::vec3(0, 0, 1));
		for (const float lado : { 0.05f, 0.2f, 0.5f })
		{
			const viewmodel::Object cubo{ "frasco", {}, glm::vec3(1), glm::vec3(-lado / 2), glm::vec3(lado / 2) };
			glm::mat4 model;
			Check(viewmodel::Build(cubo, view, projection, 1.0f, model), "small object gets a viewmodel");
			const glm::vec4 clip = projection * view * model * glm::vec4(0, 0, 0, 1);
			Check(Near(clip.x / clip.w, 0.64f, 1.0e-3f) && Near(clip.y / clip.w, -0.56f, 1.0e-3f),
				"centre lands at 82% / 78%");
			float lo = 10.0f, hi = -10.0f;
			for (int i = 0; i < 8; ++i)
			{
				const glm::vec3 c((i & 1) ? lado / 2 : -lado / 2, (i & 2) ? lado / 2 : -lado / 2, (i & 4) ? lado / 2 : -lado / 2);
				const glm::vec4 p = projection * view * model * glm::vec4(c, 1.0f);
				lo = std::min(lo, p.y / p.w);
				hi = std::max(hi, p.y / p.w);
			}
			const float alto = (hi - lo) * 0.5f;   // fraccion de pantalla, con perspectiva
			Check(alto > 0.15f && alto < 0.30f, "apparent size is bounded");
		}

		glm::mat4 model;
		viewmodel::Object objeto{ "hiriti", {}, glm::vec3(1), glm::vec3(-0.1f), glm::vec3(0.1f) };
		Check(!viewmodel::Build(objeto, view, projection, 1.0f, model), "Hiriti keeps the physical view");
		objeto.id = "caja";
		objeto.scale = glm::vec3(5.0f);
		Check(!viewmodel::Build(objeto, view, projection, 1.0f, model), "large object keeps the physical view");
		objeto.scale = glm::vec3(1.0f);
		objeto.boundsMin = objeto.boundsMax = glm::vec3(0.0f);
		Check(!viewmodel::Build(objeto, view, projection, 1.0f, model), "empty bounds fall back");
		objeto.boundsMax = glm::vec3(0.1f);
		Check(!viewmodel::Build(objeto, view, glm::mat4(0.0f), 1.0f, model), "invalid projection falls back");
	}

	void TestSubtitles()
	{
		subtitles::Manager manager;
		std::string error;
		Check(manager.LoadFile(ESCAPETHEUAB_SOURCE_DIR "/assets/subtitles/hirity.json", error), "catalogue loads");
		Check(manager.LineCount() == 13, "one line per Hirity recording");
		Check(!manager.LoadFile("no/existe.json", error) && manager.LineCount() == 13, "missing file keeps catalogue");
		Check(!manager.LoadJson("{ roto", error) && manager.LineCount() == 13, "invalid JSON keeps catalogue");
		Check(!manager.LoadJson(R"({"lines":[{"audio":"a","text":"b","segments":[{"start":2,"end":1,"text":"c"}]}]})", error),
			"inverted segment rejected");

		std::set<unsigned int> sonando = { 1, 2 };
		float cursor = 0.0f;
		const auto suena = [&](unsigned int id) { return sonando.count(id) != 0; };
		const auto posicion = [&](unsigned int) { return cursor; };
		subtitles::Visible linea;
		const auto actual = [&](subtitles::Filter f = {}) { return manager.Current(suena, posicion, f, linea); };

		manager.OnPlay("assets\\audio\\hirity\\story\\agarrame.ogg", 1);
		manager.OnPlay("assets/audio/hirity/ambient/cough.ogg", 2);
		manager.OnPlay("assets/audio/desconocido.ogg", 3);
		Check(actual() && linea.speaker == "Hirity" && !linea.verified, "dialogue outranks ambient caption");
		Check(!actual({ true, false }), "unverified lines can be hidden");
		sonando.erase(1);
		Check(actual() && linea.text == "[Tos]", "caption shows once dialogue ends");
		Check(!actual({ false, true }), "captions can be disabled");
		sonando.clear();
		Check(!actual(), "nothing remains after playback ends");

		sonando = { 4 };
		manager.OnPlay("assets/audio/hirity/story/holahiritygallego.ogg", 4);
		cursor = 1.0f;
		Check(actual() && linea.text.rfind("Ola", 0) == 0, "first segment follows the cursor");
		cursor = 4.0f;
		Check(actual() && linea.text.rfind("Carallo", 0) == 0, "second segment follows the cursor");
		cursor = 9.0f;
		Check(!actual(), "outside every segment shows nothing");
		cursor = 1.0f;
		manager.Clear();
		Check(!actual(), "clear drops active lines");
		manager.OnPlay("assets/audio/hirity/story/holahirity.ogg", 0);
		Check(!actual(), "failed playback adds nothing");
	}
}

int main()
{
	TestSettings();
	TestDithering();
	TestViewModel();
	TestSubtitles();
	if (failures == 0) std::cout << "RetroTests OK\n";
	return failures == 0 ? 0 : 1;
}
