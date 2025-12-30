#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#if defined(__linux__)
    #include <sys/eventfs.h>
    #include <unistd.h>
    #include <sys/select.h>
    #include <cstdint>
#endif


namespace AtomicCScompact
{

class wakethread
{
private:
    int fd_{-1};
    bool useEFD_{false};
    std::atomic<int> counter_{0};
    std::mutex mu_;
    std::condition_variable cv_;
public:
    wakethread(/* args */) noexcept
    {
#if defined(__linux__)
        fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        useEFD_ = (fd_ >= 0);
#else
        fd_ = -1;
        useEFD_ = false;
#endif

    }
    ~wakethread()
    {
#if defined(__linux__)
        if (fd_ >= 0)
        {
            close(fd_);
        }
#endif
        counter_.fetch_add(1, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lk(mu_);
            cv_.notify_one();
        }   
    }
    
    bool WaitForMs(int ms)
    {
#if defined(__linux__)
        if (useEFD_)
        {
            fd_set s;
            FD_ZERO(&s);
            FD_SET(fd_, &s);
            struct timeval
            {
                tv.tv_sec = ms / 1000;
                tv.tv_usec = (ms % 1000)*1000;
            };
            int r = select(fd_ + 1, &s, nullptr, nullptr, &tv);
            if (r > 0 && FD_ISSET(fd_, &s))
            {
                uint64_t v;
                read(fd_, &v, sizeof(v));
                void(v);
                return true;
            }
        }
#endif
        //fallback windows and other sys
        std::unique_lock<std::mutex> lk(mu_);
        if (counter_.load(std::memory_order_relaxed) > 0)
        {
            counter_.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }
        auto dur = std::chrono::milliseconds(ms);
        if (cv_.wait_for(lk, dur) == std::cv_status::no_timeout)
        {
            counter_.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }
        return false;
    }
    bool KernelBacked() const noexcept
    {
        return useEFD_;
    }
};


}