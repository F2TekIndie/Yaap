#include "core/AsyncTaskReaper.hpp"

#include <utility>

namespace yaap {

AsyncTaskReaper::AsyncTaskReaper()
    : m_reaperThread([this] { run(); })
{
}

AsyncTaskReaper::~AsyncTaskReaper()
{
    {
        const std::scoped_lock lock{m_mutex};
        m_stopping = true;
    }
    m_condition.notify_one();
    if (m_reaperThread.joinable()) {
        m_reaperThread.join();
    }
}

void AsyncTaskReaper::retire(std::jthread task)
{
    if (!task.joinable()) {
        return;
    }

    task.request_stop();
    {
        const std::scoped_lock lock{m_mutex};
        m_tasks.push_back(std::move(task));
    }
    m_condition.notify_one();
}

void AsyncTaskReaper::run()
{
    while (true) {
        std::jthread task;
        {
            std::unique_lock lock{m_mutex};
            m_condition.wait(lock, [this] { return m_stopping || !m_tasks.empty(); });
            if (m_tasks.empty()) {
                if (m_stopping) {
                    return;
                }
                continue;
            }
            task = std::move(m_tasks.front());
            m_tasks.pop_front();
        }

        if (task.joinable()) {
            task.join();
        }
    }
}

} // namespace yaap
