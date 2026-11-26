// Identificacion de reproducciones, cursor, parada y fin de AudioManager con un motor sin
// dispositivo: el tiempo avanza a mano y no suena nada.
#include "src/audio/AudioManager.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

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
	const std::string audio = ENTORNVGI_SOURCE_ASSETS_DIR "/audio/hirity/";
	AudioManager manager;
	Check(manager.playOneShot(audio + "ambient/cough.ogg") == 0, "without engine there is no playback");
	if (!manager.init(true))
	{
		std::cerr << "FAIL: engine without device\n";
		return 1;
	}

	std::vector<unsigned int> avisos;
	manager.setPlaybackListener([&](const std::string&, unsigned int id, float) { avisos.push_back(id); });
	Check(manager.playOneShot(audio + "no_existe.ogg") == 0, "missing file returns 0");
	Check(manager.playOneShot(audio + "ambient/cough.ogg", -1.0f) == 0, "invalid volume returns 0");
	Check(avisos.empty(), "failed playbacks are not announced");

	const unsigned int tos = manager.playOneShot(audio + "ambient/cough.ogg");     // 2.4 s
	const unsigned int pasos = manager.playOneShot(audio + "ambient/pasosfuertes.ogg");   // 6.2 s
	Check(tos != 0 && pasos != 0 && tos != pasos, "each playback has its own id");
	Check(avisos.size() == 2 && avisos[0] == tos && avisos[1] == pasos, "listener receives each start");
	Check(manager.isPlaying(tos) && manager.isPlaying(pasos), "simultaneous playbacks");

	manager.advanceWithoutDevice(48000);   // 1 s
	Check(std::abs(manager.playbackCursorSeconds(tos) - 1.0f) < 0.1f, "cursor follows real playback");

	manager.advanceWithoutDevice(2 * 48000);   // 3 s: la tos ha terminado
	Check(!manager.isPlaying(tos) && manager.isPlaying(pasos), "finished sound stops reporting");

	manager.stopOneShot(pasos);
	Check(!manager.isPlaying(pasos), "interrupted sound stops reporting");

	manager.playOneShot(audio + "ambient/cough.ogg");   // libera las reproducciones terminadas
	Check(!manager.isPlaying(tos) && manager.playbackCursorSeconds(tos) == 0.0f, "released ids stay safe");
	Check(!manager.isPlaying(12345), "unknown id");

	manager.shutdown();
	if (failures == 0) std::cout << "AudioPlaybackTests OK\n";
	return failures == 0 ? 0 : 1;
}
