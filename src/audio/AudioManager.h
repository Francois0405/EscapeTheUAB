#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "src/gameplay/player/FpsMovement.h"

struct ma_engine;
struct ma_sound;

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    bool init();
    void shutdown();

    // Devuelve un identificador de la reproduccion, o 0 si no se pudo iniciar.
    unsigned int playOneShot(const std::string& filepath, float volume = 1.0f);
    // true mientras esa reproduccion siga sonando.
    bool isPlaying(unsigned int playback) const;
    // Se avisa cada vez que un one-shot empieza a sonar de verdad.
    void setPlaybackListener(std::function<void(const std::string&, unsigned int, float)> listener);

    bool loadSound(const std::string& id, const std::string& filepath, bool loop = false, bool spatial = false);
    void playSound(const std::string& id);
    void stopSound(const std::string& id);

    void updateListener(float posX, float posY, float posZ, float dirX, float dirY, float dirZ);
    
    void setMasterVolume(float volume);

    void playFootstep(fps::StepState estado);

private:
    ma_engine* m_engine = nullptr;
    bool m_initialized = false;

    float m_masterVolume = 1.0f;
    float m_sfxVolume = 1.0f;
    float m_ambientVolume = 1.0f;
    
    std::unordered_map<std::string, ma_sound*> m_sounds;
    struct OneShot { ma_sound* sound; unsigned int id; };
    std::vector<OneShot> m_oneShotSounds;
    unsigned int m_lastPlayback = 0;
    std::function<void(const std::string&, unsigned int, float)> m_playbackListener;
};