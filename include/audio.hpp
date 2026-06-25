#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    bool init();
    void shutdown();

    void playSfx(const std::string& path);
    void playMusic(const std::string& path);
    void stopMusic();
    void stopAll();
    void pauseAll();
    void resumeAll();
    void setVolume(float vol);
    float getVolume() const;

private:
    struct AudioClip {
        std::vector<short> samples;
        int rate = 44100;
    };

    struct Voice {
        std::shared_ptr<const AudioClip> clip;
        size_t framePos = 0;
        bool loop = false;
        bool stop = false;
    };

    std::mutex mtx;
    std::condition_variable cv;
    std::queue<std::string> sfxQueue;
    std::queue<std::string> musicQueue;
    std::vector<std::unique_ptr<Voice>> voices;

    std::thread mixerThread;
    std::atomic<bool> running{false};
    std::atomic<bool> paused{false};
    std::atomic<float> volume_{1.0f};

    AudioClip loadWav(const std::string& path);
    void mixerFunc();
};
