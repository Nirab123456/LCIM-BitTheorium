#include <ACSThreadManager.h>


void ThreadPool::stop()
{
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false))
    {
        std::unique_lock<std::mutex>lk(mtx_);
        cv_.notify_all();
    }
    for (auto &t : workers_)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
}

void ThreadPool::enqueue(Task t)
{
    {
        std::unique_lock<std::mutex> lk(mtx_);
        tasks_.push_back(std::move(t));
    }
    cv_.notify_one();
}

void ThreadPool::ShedulerAfter(std::chrono::milliseconds delay, Task t)
{
}