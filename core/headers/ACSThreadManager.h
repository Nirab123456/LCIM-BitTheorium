#pragma once 
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include<optional>
#include <queue>
#include <thread>
#include <vector>
#include <deque>

class ThreadPool 
{
    public:
        using Task = std::function<void()>;
    private:
        struct DelayedItem
        {

        };
        struct cmp
        {

        };

        std::atomic<bool> running_{false};
        std::vector<std::thread>workers_;
        std::deque<Task> tasks_;
        std::priority_queue<DelayedItem, std::vector<DelayedItem>, cmp> delayed_;
        std::mutex mtx_;
        std::condition_variable cv_;

        void WorkerLoop();
    public:
        using Task = std::function<void()>;

        ThreadPool(size_t n = std::thread::hardware_concurrency())
        {
            if (n == 0)
            {
                n = 1;
            }
            running_.store(true);
            for (size_t i = 0; i < n; i++)
            {
                workers_.emplace_back([this]{
                    WorkerLoop();
                });
            }
        }
        ~ThreadPool(){
            stop();
        }

        void stop();
        void enqueue(Task t);
        void ShedulerAfter(std::chrono::milliseconds delay, Task t);
};