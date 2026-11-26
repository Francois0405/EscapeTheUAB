// Presets, persistencia, dithering de referencia y subtitulos (sin ventana, audio ni OpenGL).
#include "src/rendering/RetroSettings.h"
#include "src/ui/Subtitles.h"

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
}

int main()
{
	retro::Settings s;
	s.subtitles = false;
	retro::ApplyPreset(s, retro::Preset::PsxClassic);
	Check(s.bitsPerChannel == 4 && s.internalHeight == 360, "PSX preset values");
	Check(!s.subtitles, "preset keeps accessibility choices");
	retro::ApplyPreset(s, retro::Preset::Original);
	Check(!s.nearestTextures, "Original keeps linear textures");
	const retro::Size original = retro::InternalSize(s, 2560, 1080);
	Check(original.width == 960 && original.height == 540, "Original keeps 960x540");

	retro::ApplyPreset(s, retro::Preset::Cinematic);
	Check(retro::InternalSize(s, 1920, 1080).width == 960, "16:9 internal width");
	Check(retro::InternalSize(s, 2560, 1080).width == 1280, "width follows window aspect");
	Check(retro::InternalSize(s, 0, 0).height == 540, "minimised window has a safe size");

	s.ditherStrength = 0.5f;
	s.preset = retro::Preset::Custom;
	const retro::Settings leido = retro::Parse(retro::Serialize(s));
	Check(leido.preset == retro::Preset::Custom && leido.ditherStrength == 0.5f && !leido.subtitles, "round trip");

	const retro::Settings roto = retro::Parse("preset=99\nbitsPerChannel=0\ninternalHeight=-5\ncontrast=nan\ngrain=7\nbasura\n=3\n");
	Check(roto.preset == retro::Preset::Cinematic && roto.bitsPerChannel == 2 && roto.internalHeight == retro::kMinHeight,
		"invalid integers become safe");
	Check(roto.contrast == retro::Settings{}.contrast && roto.grain == 0.03f, "invalid floats become safe");

	std::set<int> cuatro, ocho;
	for (int y = 0; y < 8; ++y)
		for (int x = 0; x < 8; ++x)
		{
			if (x < 4 && y < 4) cuatro.insert(retro::Bayer4Value(x, y));
			ocho.insert(retro::Bayer8Value(x, y));
			for (int bits = 2; bits <= 8; ++bits)
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

	subtitles::Manager manager;
	std::string error;
	Check(manager.LoadFile(ESCAPETHEUAB_SOURCE_DIR "/assets/subtitles/hirity.json", error), "catalogue loads");
	Check(manager.LineCount() == 13, "one line per Hirity recording");
	Check(!manager.LoadJson("{ roto", error) && manager.LineCount() == 13, "invalid JSON keeps catalogue");

	std::set<unsigned int> sonando = { 1, 2 };
	const auto suena = [&](unsigned int id) { return sonando.count(id) != 0; };
	manager.OnPlay("assets\\audio\\hirity\\story\\agarrame.ogg", 1);
	manager.OnPlay("assets/audio/hirity/ambient/cough.ogg", 2);
	manager.OnPlay("assets/audio/desconocido.ogg", 3);
	const subtitles::Line* actual = manager.Current(suena, true);
	Check(actual && actual->id == "story.grab_me", "dialogue outranks ambient caption");
	sonando.erase(1);
	actual = manager.Current(suena, true);
	Check(actual && actual->id == "ambient.cough", "caption shows once dialogue ends");
	Check(manager.Current(suena, false) == nullptr, "captions can be disabled");
	sonando.clear();
	Check(manager.Current(suena, true) == nullptr, "nothing remains after playback ends");
	sonando = { 4 };
	manager.OnPlay("assets/audio/hirity/story/holahirity.ogg", 4);
	manager.Clear();
	Check(manager.Current(suena, true) == nullptr, "clear drops active lines");

	if (failures == 0) std::cout << "RetroTests OK\n";
	return failures == 0 ? 0 : 1;
}
