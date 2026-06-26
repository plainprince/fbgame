#include <audio.hpp>
#include <alsa/asoundlib.h>
#include <fstream>
#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include <algorithm>

static const int CHUNK_FRAMES = 512;

AudioManager::AudioManager() {}
AudioManager::~AudioManager() { shutdown(); }

bool AudioManager::init() {
    running = true;
    mixerThread = std::thread(&AudioManager::mixerFunc, this);
    return true;
}

void AudioManager::shutdown() {
    running = false;
    cv.notify_all();
    if (mixerThread.joinable()) mixerThread.join();
}

AudioManager::AudioClip AudioManager::loadWav(const std::string& path) {
    AudioClip clip;
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "WAV not found: " << path << "\n";
        return clip;
    }

    char buf[4];
    auto read32 = [&]() -> uint32_t {
        uint32_t v = 0;
        file.read(buf, 4);
        if (file.gcount() == 4)
            v = (uint8_t)buf[0] | ((uint8_t)buf[1] << 8) | ((uint8_t)buf[2] << 16) | ((uint8_t)buf[3] << 24);
        return v;
    };
    auto read16 = [&]() -> uint16_t {
        uint16_t v = 0;
        file.read(buf, 2);
        if (file.gcount() == 2)
            v = (uint8_t)buf[0] | ((uint8_t)buf[1] << 8);
        return v;
    };

    char riff[4];
    file.read(riff, 4);
    if (std::memcmp(riff, "RIFF", 4) != 0) return clip;
    read32();
    char wave[4];
    file.read(wave, 4);
    if (std::memcmp(wave, "WAVE", 4) != 0) return clip;

    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    std::vector<uint8_t> raw;

    while (file) {
        char chunkId[4];
        file.read(chunkId, 4);
        uint32_t chunkSize = read32();
        if (!file) break;

        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            read16();
            channels = read16();
            sampleRate = read32();
            read32(); read16();
            bitsPerSample = read16();
            if (chunkSize > 16)
                file.seekg(chunkSize - 16, std::ios::cur);
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            raw.resize(chunkSize);
            file.read((char*)raw.data(), chunkSize);
            break;
        } else {
            file.seekg(chunkSize, std::ios::cur);
        }
    }

    if (raw.empty() || channels == 0 || sampleRate == 0) return clip;

    clip.rate = sampleRate;

    size_t sampleCount = raw.size() / (bitsPerSample / 8);
    std::vector<short> interleaved(sampleCount);

    if (bitsPerSample == 16) {
        std::memcpy(interleaved.data(), raw.data(), raw.size());
    } else if (bitsPerSample == 8) {
        for (size_t i = 0; i < sampleCount; i++)
            interleaved[i] = ((int)raw[i] - 128) << 8;
    } else {
        return clip;
    }

    if (channels == 1) {
        clip.samples = std::move(interleaved);
    } else {
        size_t monoCount = sampleCount / channels;
        clip.samples.resize(monoCount);
        for (size_t i = 0; i < monoCount; i++) {
            int sum = 0;
            for (int ch = 0; ch < channels; ch++)
                sum += interleaved[i * channels + ch];
            clip.samples[i] = (short)(sum / channels);
        }
    }

    return clip;
}

void AudioManager::mixerFunc() {
    snd_pcm_t* handle = nullptr;
    int currentRate = 0;
    short mixBuf[CHUNK_FRAMES];

    auto openAlsa = [&](int rate) -> bool {
        if (handle) {
            snd_pcm_drain(handle);
            snd_pcm_close(handle);
            handle = nullptr;
            currentRate = 0;
        }
        int err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
        if (err < 0) {
            std::cerr << "ALSA open error: " << snd_strerror(err) << "\n";
            return false;
        }
        err = snd_pcm_set_params(handle, SND_PCM_FORMAT_S16_LE,
                                 SND_PCM_ACCESS_RW_INTERLEAVED,
                                 1, rate, 1, 50000);
        if (err < 0) {
            std::cerr << "ALSA set params error: " << snd_strerror(err) << "\n";
            snd_pcm_close(handle);
            handle = nullptr;
            return false;
        }
        currentRate = rate;
        return true;
    };

    while (running) {
        std::unique_lock lock(mtx);

        while (!sfxQueue.empty()) {
            std::string path = sfxQueue.front();
            sfxQueue.pop();
            lock.unlock();
            auto clip = std::make_shared<AudioClip>(loadWav(path));
            if (!clip->samples.empty() && (handle == nullptr || clip->rate != currentRate))
                openAlsa(clip->rate);
            lock.lock();
            if (!clip->samples.empty()) {
                auto v = std::make_unique<Voice>();
                v->clip = clip;
                voices.push_back(std::move(v));
            }
        }

        while (!musicQueue.empty()) {
            std::string path = musicQueue.front();
            musicQueue.pop();
            for (auto& v : voices)
                if (v->loop) v->stop = true;
            lock.unlock();
            auto clip = std::make_shared<AudioClip>(loadWav(path));
            if (!clip->samples.empty() && (handle == nullptr || clip->rate != currentRate))
                openAlsa(clip->rate);
            lock.lock();
            if (!clip->samples.empty()) {
                auto v = std::make_unique<Voice>();
                v->clip = clip;
                v->loop = true;
                voices.push_back(std::move(v));
            }
        }

        if (paused) {
            lock.unlock();
            if (handle) snd_pcm_pause(handle, 1);
            while (paused && running)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (handle) snd_pcm_pause(handle, 0);
            lock.lock();
        }

        bool anyActive = false;
        std::memset(mixBuf, 0, sizeof(mixBuf));
        float vol = volume_;

        for (auto& v : voices) {
            if (v->stop) continue;
            anyActive = true;
            size_t remaining = v->clip->samples.size() - v->framePos;
            int n = std::min((int)remaining, CHUNK_FRAMES);
            for (int i = 0; i < n; i++) {
                int s = mixBuf[i] + (int)(v->clip->samples[v->framePos + i] * vol);
                if (s > 32767) s = 32767;
                if (s < -32768) s = -32768;
                mixBuf[i] = (short)s;
            }
            v->framePos += n;
            if (v->framePos >= v->clip->samples.size()) {
                if (v->loop) v->framePos = 0;
                else v->stop = true;
            }
        }

        voices.erase(std::remove_if(voices.begin(), voices.end(),
            [](auto& v) { return v->stop; }), voices.end());

        lock.unlock();

        if (anyActive && !paused && handle) {
            snd_pcm_sframes_t written = snd_pcm_writei(handle, mixBuf, CHUNK_FRAMES);
            if (written == -EPIPE) {
                snd_pcm_prepare(handle);
                snd_pcm_writei(handle, mixBuf, CHUNK_FRAMES);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    if (handle) {
        snd_pcm_drain(handle);
        snd_pcm_close(handle);
    }
}


void AudioManager::playSfx(const std::string& path) {
    if (!running) return;
    {
        std::lock_guard lock(mtx);
        sfxQueue.push(path);
    }
    cv.notify_one();
}

void AudioManager::playMusic(const std::string& path) {
    if (!running) return;
    {
        std::lock_guard lock(mtx);
        musicQueue.push(path);
    }
    cv.notify_one();
}

void AudioManager::stopMusic() {
    std::lock_guard lock(mtx);
    for (auto& v : voices)
        if (v->loop) v->stop = true;
}

void AudioManager::stopAll() {
    std::lock_guard lock(mtx);
    for (auto& v : voices)
        v->stop = true;
}

void AudioManager::pauseAll() {
    paused = true;
}

void AudioManager::resumeAll() {
    paused = false;
}

void AudioManager::setVolume(float vol) {
    volume_ = std::max(0.0f, std::min(1.0f, vol));
}

float AudioManager::getVolume() const {
    return volume_;
}
