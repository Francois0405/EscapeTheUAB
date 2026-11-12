#pragma once
#include <string>
#include <unordered_map>

struct ma_engine;
struct ma_sound;

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    bool init();
    void shutdown();

    void playOneShot(const std::string& filepath, float volume = 1.0f);

    bool loadSound(const std::string& id, const std::string& filepath, bool loop = false, bool spatial = false);
    void playSound(const std::string& id);
    void stopSound(const std::string& id);

    void updateListener(float posX, float posY, float posZ, float dirX, float dirY, float dirZ);
    
    void setMasterVolume(float volume);

private:
    ma_engine* m_engine = nullptr;
    bool m_initialized = false;
    std::unordered_map<std::string, ma_sound*> m_sounds;
};