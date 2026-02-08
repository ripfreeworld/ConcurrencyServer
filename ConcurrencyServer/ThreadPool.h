#pragma once
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void Enqueue(std::function<void()> task);
    void Stop();

private:
    void WorkerLoop();

    std::vector<std::thread> _threads;
    std::mutex _mtx;
    std::condition_variable _cv;
    std::queue<std::function<void()>> _q;
    std::atomic<bool> _stopping{ false };
};
