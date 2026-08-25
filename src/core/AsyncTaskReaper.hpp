#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace yaap {

class AsyncTaskReaper final {
public:
    AsyncTaskReaper();
    ~AsyncTaskReaper();

    AsyncTaskReaper(const AsyncTaskReaper&) = delete;
    AsyncTaskReaper& operator=(const AsyncTaskReaper&) = delete;
    AsyncTaskReaper(AsyncTaskReaper&&) = delete;
    AsyncTaskReaper& operator=(AsyncTaskReaper&&) = delete;

    // Requests cancellation immediately; joining happens on the reaper thread.
    void retire(std::jthread task);

private:
    void run();

    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::deque<std::jthread> m_tasks;
    bool m_stopping{};
    std::thread m_reaperThread;
};

} // namespace yaap
