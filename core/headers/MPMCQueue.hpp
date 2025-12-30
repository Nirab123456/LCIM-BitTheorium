#pragma once 
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <new>

namespace AtomicCScompact
{
    template<typename T>
    class MPMCQueue
    {
    private:
        struct Cell
        {
            std::atomic<size_t> _seq;
            T _data;
            Cell():
                _seq(0), _data()
            {}
        };
        Cell* buffer_{nullptr};
        size_t capacity_{0};
        size_t mask_{0};
        std::atomic<size_t> head_{0};
        std::atomic<size_t> tail_{0};
    public:
        explicit MPMCQueue(size_t CapacityPow2)
        {
            assert((CapacityPow2 & (CapacityPow2 -1)) == 0);
            capacity_ = CapacityPow2;
            mask_ = capacity_ - 1;
#if defined(_MSC_VER)
            buffer_ = static_cast<Cell*>(_aligned_malloc(sizeof(Cell) * capacity_, 64));
#else
            buffer_ = static_cast<Cell*>(std::aligned_malloc(sizeof(Cell) * capacity_, 64));
#endif
            if(!buffer_) return;
            for (size_t i = 0; i < capacity_; i++)
            {
                new(&buffer_[i]) Cell();
                buffer_[i]._seq.store(i, std::memory_order_relaxed);
            }
            head_.store(0, std::memory_order_relaxed);
            tail.store(0, std::memory_order_relaxed);
        }

        ~MPMCQueue()
        {
            if (!buffer_)
            {
                return;
            }
            for (size_t i = 0; i < capacity_; i++)
            {
                buffer_[i].~Cell();
            }
#if defined(_MSC_VER)
            _aligned_free(buffer_);
#else
            std::free(buffer_);
#endif
            buffer_ = nullptr;
        }

        bool pushAC(const T &item) noexcept
        {
            size_t pos = head.load(std::memory_order_relaxed);
            while (true)
            {
                Cell* cell = &buffer_[pos & mask_];
                size_t seq = cell->_seq.load(std::memory_order_acquire);
                intptr_t dif = (intptr_t)seq - (intptr_t)pos;
                if (dif == 0)
                {
                    if (head_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    {
                        cell->_data = item;
                        cell->_seq.store(pos + 1, std::memory_order_release);
                        return true;
                    }
                    else if (dif < 0)
                    {
                        return false;
                    }
                    else
                    {
                        pos = head.load(std::memory_order_relaxed);
                    }
                }
            }
        }

        bool popAC(T &out)
        {
            size_t pos = tail_.load(std::memory_order_relaxed);
            while (true)
            {
                Cell* cell = &buffer_[pos & mask_];
                size_t seq = cell->_seq.load(std::memory_order_acquire);
                intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
                if (dif == 0)
                {
                    if (tail_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    {
                        out = cell->_data;
                        cell->_seq.store(pos + capacity_, std ::memory_order_release);
                        return true;
                    }
                }
                else if (dif < 0)
                {
                    return false;
                }
                else
                {
                    pos = tail.load(std::memory_order_relaxed);
                }
            }
        }
        size_t CapacitySixe() const noexcept
        {
            return capacity_;
        }

    };



    

    
}