#pragma once 
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <chrono>
#include <functional>
#include <type_traits>
#include <thread>

#if defined(__linux__)
#include <sys/eventfd.h>
#include <unistd.h>
#endif

#include "AtomicCSCompact.h"

namespace AtomicCScompact
{
    struct ThreadDesc
    {
        uint8_t operation;
        uint8_t st;
        uint8_t rel;
        uint8_t flags;
        uint32_t idx;
        uint32_t count;
        uint32_t value;
        uintptr_t Vptr;
        ThreadDesc() noexcept :
            operation(0), st(0), rel(0), flags(0), idx(0), count(0), value(0), Vptr(0)
        {}
    };

    using DescHandle = uint64_t;

    void* AlgAllocPort(size_t alignment, size_t size);
    void AlgFreePort(void* p) noexcept;

    class RespawnThread
    {
    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    
    public:
        RespawnThread();
        ~RespawnThread();
        RespawnThread(const RespawnThread&) = delete;
        RespawnThread& operator = (const RespawnThread&) = delete;

        void notifyOne() noexcept;
        void waitFrMs(int ms) noexcept;
        bool valid() const noexcept;
         
    };


    template<typename T>
    class MPMCQueue
    {
    private:
        struct cell;
        cell* buffer_;
        size_t capicity;
        size_t mask;
        alignas(64) std::atomic<size_t> head_;
        alignas(64) std::atomic<size_t> tail_;
    };
    
    template<typename PackedArr>
    class RingWorker
    {
    public:
        using Arr = PackedArr;
        using value_t = typename Arr::value_t;
        using strl_t = typename Arr::strl_t;

        RingWorker(
            Arr& array, MPMCQueue<DescHandle>& queue,
            unsigned maxBatch = 4096                                                                                                                                                                         
        );
        ~RingWorker();
        RingWorker(const RingWorker&) = delete;
        RingWorker& operator = (const RingWorker&) = delete;

        void stop();

        void notify() noexcept;
    private:
        void runLoop_();
        Arr& arr_;
        MPMCQueue<DescHandle>& queue_;
        std::unique_ptr<RespawnThread> waker_;
        std::thread WrkrThread_;
        std::atomic<bool>running_;
        unsigned maxBatch_;

    };


    
}