#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "AudioManager.h"
#include <iostream>
#include <filesystem>

AudioManager::AudioManager() {}

AudioManager::~AudioManager() {
    shutdown();
}

bool AudioManager::init() {
    m_engine = new ma_engine();
    ma_result result = ma_engine_init(NULL, m_engine);
    
    if (result != MA_SUCCESS) {
        std::cerr << "[AudioManager] Error al iniciar motor de audio. El juego continuara sin sonido.\n";
        delete m_engine;
        m_engine = nullptr;
        return false; // No cerramos el juego si no hay tarjeta de sonido
    }

    m_initialized = true;
    return true;
}

void AudioManager::shutdown() {
    if (!m_initialized) return;

    // Limpiar todos los sonidos cargados
    for (auto& pair : m_sounds) {
        ma_sound_uninit(pair.second);
        delete pair.second;
    }
    m_sounds.clear();

    ma_engine_uninit(m_engine);
    delete m_engine;
    m_engine = nullptr;
    m_initialized = false;
}

void AudioManager::playOneShot(const std::string& filepath, float volume) {
    if (!m_initialized || !std::filesystem::exists(filepath)) return;

    // ma_engine_play_sound dispara el audio en un hilo secundario sin bloquear el render
    ma_engine_play_sound(m_engine, filepath.c_str(), NULL);
}

bool AudioManager::loadSound(const std::string& id, const std::string& filepath, bool loop, bool spatial) {
    if (!m_initialized) return false;

    // Criterio de aceptacion: compila y corre sin archivos de audio; fallos no cierran el juego
    if (!std::filesystem::exists(filepath)) {
        std::cerr << "[AudioManager] Archivo no encontrado: " << filepath << "\n";
        return false;
    }

    ma_sound* sound = new ma_sound();
    ma_uint32 flags = spatial ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;

    ma_result result = ma_sound_init_from_file(m_engine, filepath.c_str(), flags, NULL, NULL, sound);
    if (result != MA_SUCCESS) {
        delete sound;
        return false;
    }

    ma_sound_set_looping(sound, loop ? MA_TRUE : MA_FALSE);
    m_sounds[id] = sound;
    return true;
}

void AudioManager::playSound(const std::string& id) {
    if (!m_initialized || m_sounds.find(id) == m_sounds.end()) return;
    
    // Reinicia el sonido al principio para evitar que se corte o no suene si ya se reprodujo antes
    ma_sound_seek_to_pcm_frame(m_sounds[id], 0);
    ma_sound_start(m_sounds[id]);
}

void AudioManager::stopSound(const std::string& id) {
    if (!m_initialized || m_sounds.find(id) == m_sounds.end()) return;
    ma_sound_stop(m_sounds[id]);
}

void AudioManager::updateListener(float posX, float posY, float posZ, float dirX, float dirY, float dirZ) {
    if (!m_initialized) return;
    ma_engine_listener_set_position(m_engine, 0, posX, posY, posZ);
    ma_engine_listener_set_direction(m_engine, 0, dirX, dirY, dirZ);
}

void AudioManager::setMasterVolume(float volume) {
    if (!m_initialized) return;
    ma_engine_set_volume(m_engine, volume);
}