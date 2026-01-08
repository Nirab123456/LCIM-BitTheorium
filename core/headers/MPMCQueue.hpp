#pragma once
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <chrono>
#include <vector>
#include <AllocNW.hpp>
#include <PackedCell.hpp>

namespace AtomicCScompact
{
    template<typename T>
    class MPMCQueue
    {
    public:
        using CallBack = void(*)(size_t current, size_t capacity, void* user);
    private:
        CallBack HighWaterCB_{nullptr};
        void* CBUser_{nullptr};
        std::atomic<size_t> Head_{0};
        std::atomic<size_t> Tail_{0};
        struct Cell_
        {
            std::atomic<size_t> Seq_;
            T Data_;
            Cell_():
                Seq_(0),Data_()
            {}
        };
        Cell_* Buffer_{nullptr};
        size_t Capacity_{0};
        size_t Mask_{0};
        void CheckHighWater_(size_t head_pos) noexcept
        {
            if (!HighWaterCB_)
            {
                return;
            }
            size_t h = head_pos;
            size_t t = Tail_.load(std::memory_order_relaxed);
            size_t occ = (h >= t) ? (h - t) : (h + Capacity_ - t);
            if (occ * 10 >= Capacity_ * 8)
            {
                HighWaterCB_(occ, Capacity_, CBUser_);
            }
        }
    
        /* data */
    public:
        explicit MPMCQueue(size_t capacity_pow2, CallBack high_water_cb = nullptr, void* cb_user = nullptr) :
            HighWaterCB_(high_water_cb), CBUser_(cb_user)
        {
            assert((capacity_pow2 & (capacity_pow2 - 1)) == 0);
                Capacity_ = capacity_pow2;
                Mask_ = Capacity_ - 1;
                Buffer_ = static_cast<Cell_*>(AllocNW::AlignedAllocP(ATOMIC_THRESHOLD, ))
        }
        ~MPMCQueue();
    };
    
}