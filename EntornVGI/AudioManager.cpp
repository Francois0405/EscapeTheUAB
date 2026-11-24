#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis_compat.h>
#undef STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_INCLUDE_STB_VORBIS_H
#define MINIAUDIO_IMPLEMENTATION
#include <vector>
#include <cstdlib>
#include <cmath>
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
    ma_engine_set_volume(m_engine, m_masterVolume);

    loadSound("paso_0", "assets/audio/paso_1.wav", false, false);
    loadSound("paso_1", "assets/audio/paso_2.wav", false, false);
    loadSound("paso_2", "assets/audio/paso_3.wav", false, false);

    return true;
}

void AudioManager::shutdown() {
    if (!m_initialized) return;

    for (ma_sound* sound : m_oneShotSounds) {
        ma_sound_uninit(sound);
        delete sound;
    }
    m_oneShotSounds.clear();

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
    if (!m_initialized) {
        std::cerr << "[AudioManager] No se puede reproducir sin inicializar el motor: " << filepath << "\n";
        return;
    }
    if (!std::isfinite(volume) || volume < 0.0f) {
        std::cerr << "[AudioManager] Volumen no valido para " << filepath << ".\n";
        return;
    }
    if (!std::filesystem::exists(filepath)) {
        std::cerr << "[AudioManager] Archivo no encontrado: " << filepath << "\n";
        return;
    }

    for (auto sound = m_oneShotSounds.begin(); sound != m_oneShotSounds.end();) {
        if (ma_sound_at_end(*sound)) {
            ma_sound_uninit(*sound);
            delete *sound;
            sound = m_oneShotSounds.erase(sound);
        } else {
            ++sound;
        }
    }

    ma_sound* sound = new ma_sound();
    const ma_result initResult = ma_sound_init_from_file(m_engine, filepath.c_str(),
        MA_SOUND_FLAG_NO_SPATIALIZATION, NULL, NULL, sound);
    if (initResult != MA_SUCCESS) {
        delete sound;
        std::cerr << "[AudioManager] Error al cargar " << filepath << " (codigo " << initResult << ").\n";
        return;
    }

    ma_sound_set_volume(sound, volume);
    const ma_result startResult = ma_sound_start(sound);
    if (startResult != MA_SUCCESS) {
        ma_sound_uninit(sound);
        delete sound;
        std::cerr << "[AudioManager] Error al reproducir " << filepath << " (codigo " << startResult << ").\n";
        return;
    }
    m_oneShotSounds.push_back(sound);
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
    m_masterVolume = std::clamp(volume, 0.0f, 1.0f);
    if (m_initialized) ma_engine_set_volume(m_engine, m_masterVolume);
}

void AudioManager::playFootstep(fps::EstadoPaso estado) {
    if (!m_initialized || estado == fps::EstadoPaso::Reposo) return;

    std::vector<std::string> disponibles;
    for (const auto& id : {"paso_0", "paso_1", "paso_2"}) {
        if (m_sounds.find(id) != m_sounds.end()) {
            disponibles.push_back(id);
        }
    }
    
    if (disponibles.empty()) return;

    // Elegir uno distinto al del paso anterior
    static int ultimoIndice = -1;
    int indice = std::rand() % static_cast<int>(disponibles.size());
    if (disponibles.size() > 1 && indice == ultimoIndice) {
        indice = (indice + 1) % static_cast<int>(disponibles.size());
    }
    ultimoIndice = indice;

    // Ajustar volumen y pitch según si camina, corre o va agachado
    float volumen = 0.55f;
    float pitch   = 1.0f;

    switch (estado) {
        case fps::EstadoPaso::Sprint:
            volumen = 0.90f;
            pitch   = 1.15f;
            break;
        case fps::EstadoPaso::Agachado:
            volumen = 0.25f;
            pitch   = 0.85f;
            break;
        case fps::EstadoPaso::Caminando:
        default:
            volumen = 0.55f;
            pitch   = 1.0f;
            break;
    }

    // Variación orgánica sutil (+/- 5% en pitch)
    float variacion = 0.95f + static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX / 0.10f));
    pitch *= variacion;

    ma_sound* snd = m_sounds[disponibles[indice]];
    ma_sound_set_volume(snd, volumen * m_masterVolume * m_sfxVolume);
    ma_sound_set_pitch(snd, pitch);
    ma_sound_seek_to_pcm_frame(snd, 0);
    ma_sound_start(snd);
}