#pragma once
#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <functional>

class SaveManager {
public:
    SaveManager();
    ~SaveManager();

    void init(const std::string& basePath);
    void shutdown();

    std::string read(const std::string& name);
    void writeAsync(const std::string& name, const std::string& data);
    void flush();

private:
    struct WriteOp {
        std::string name;
        std::string data;
    };

    void workerThread();

    std::string base;
    std::unordered_map<std::string, std::string> cache;
    std::queue<WriteOp> writeQueue;
    mutable std::mutex cacheMtx;
    std::mutex queueMtx;
    std::condition_variable cv;
    std::thread worker;
    std::atomic<bool> running{false};
};
