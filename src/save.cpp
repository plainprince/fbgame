#include <save.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>

SaveManager::SaveManager() {}
SaveManager::~SaveManager() { shutdown(); }

void SaveManager::init(const std::string& basePath) {
    base = basePath;
    if (!base.empty() && base.back() != '/') base += '/';
    std::filesystem::create_directories(base);
    running = true;
    worker = std::thread(&SaveManager::workerThread, this);
}

void SaveManager::shutdown() {
    running = false;
    cv.notify_one();
    if (worker.joinable()) worker.join();
    flush();
}

std::string SaveManager::read(const std::string& name) {
    {
        std::lock_guard<std::mutex> lk(cacheMtx);
        auto it = cache.find(name);
        if (it != cache.end()) return it->second;
    }

    std::ifstream file(base + name + ".txt");
    if (!file.is_open()) return "";

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    {
        std::lock_guard<std::mutex> lk(cacheMtx);
        cache[name] = content;
    }
    return content;
}

void SaveManager::writeAsync(const std::string& name, const std::string& data) {
    {
        std::lock_guard<std::mutex> lk(cacheMtx);
        cache[name] = data;
    }
    {
        std::lock_guard<std::mutex> lk(queueMtx);
        writeQueue.push({name, data});
    }
    cv.notify_one();
}

void SaveManager::flush() {
    while (true) {
        WriteOp op;
        {
            std::lock_guard<std::mutex> lk(queueMtx);
            if (writeQueue.empty()) return;
            op = writeQueue.front();
            writeQueue.pop();
        }
        std::string fp = base + op.name + ".txt";
        std::ofstream file(fp);
        if (!file.is_open()) {
            size_t pos = fp.rfind('/');
            if (pos != std::string::npos) {
                std::filesystem::create_directories(fp.substr(0, pos));
                file.open(fp);
            }
        }
        if (file.is_open())
            file << op.data;
    }
}

void SaveManager::workerThread() {
    std::unique_lock<std::mutex> lk(queueMtx);
    while (running) {
        cv.wait(lk, [this] { return !running || !writeQueue.empty(); });
        while (!writeQueue.empty()) {
            WriteOp op = writeQueue.front();
            writeQueue.pop();
            lk.unlock();
            {
                std::string fp = base + op.name + ".txt";
                std::ofstream file(fp);
                if (!file.is_open()) {
                    size_t pos = fp.rfind('/');
                    if (pos != std::string::npos) {
                        std::filesystem::create_directories(fp.substr(0, pos));
                        file.open(fp);
                    }
                }
                if (file.is_open()) file << op.data;
            }
            lk.lock();
        }
    }
}
