#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t threadCount) {
    if (threadCount == 0) threadCount = 1;
    _threads.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i) {
        _threads.emplace_back([this] { WorkerLoop(); });
    }
}

ThreadPool::~ThreadPool() {
    Stop();
}

void ThreadPool::Enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_stopping.load()) return;
        _q.push(std::move(task));
    }
    // Wake one worker to process the new task.
    _cv.notify_one();
}

void ThreadPool::Stop() {
    bool expected = false;
    if (!_stopping.compare_exchange_strong(expected, true)) return;

    _cv.notify_all();
    for (auto& t : _threads) {
        if (t.joinable()) t.join();
    }
}

void ThreadPool::WorkerLoop() {
    while (!_stopping.load()) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(_mtx);
            // Sleep until there is work or a stop request.
            _cv.wait(lock, [this] { return _stopping.load() || !_q.empty(); });
            if (_stopping.load() && _q.empty()) return;
            task = std::move(_q.front());
            _q.pop();
        }
        task();
    }
}
