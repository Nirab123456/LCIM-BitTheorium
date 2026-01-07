#pragma once 
#include "PackedCell.hpp"
#include "PackedStRel.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <chrono>
#include <cassert>

namespace AtomicCScompact
{
    template<PackedMode MODE>
    class AtomicPCArray
    {
    private:
        size_t n_;
        std::atomic<packed64_t>* data_;
        size_t OwnedSizeBytes_;

        //mem order
        std::memory_order MoLoad_      = std::memory_order_acquire;
        std::memory_order MoStore_     = std::memory_order_release;
        std::memory_order EXsuccess_ = std::memory_order_acq_rel;
        std::memory_order EXfailure_ = std::memory_order_relaxed;
        /* data */
    public:
        AtomicPCArray(/* args */) noexcept :
        n_(0), data_(nullptr), OwnedSizeBytes_(0)
        {}
        ~AtomicPCArray()
        {
            FreeAll();
        }

        size_t GetSize() const noexcept
        {
            return n_;
        }

        void InitOnNode(size_t n, int node, size_t alignment = 0)
        {
            FreeAll();
            if (n == 0)
            {
                throw std::invalid_argument("n == 0");
            }

            std::atomic<packed64_t> test{0};
            if (!test.is_lock_free())
            {
                throw std::runtime_error("<atomic> is not lock free in the Heardware context");
            }

            n_ = n;

            OwnedSizeBytes_ = sizeof(std::atomic<packed64_t>) * n_;
            void* p = nullptr;
            try
            {
                p = AllocNW::AlignedAllocONnode(alignment, n_, OwnedSizeBytes_);
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }

            data_ = reinterpret_cast<std::atomic<packed64_t>*>(p);
            for (size_t i = 0; i < n_; i++)
            {
                new (&data_[i]) std::atomic<packed64_t>(packed64_t(0));
            }
            
        }

        void InitFromExisting(std::atomic<packed64_t>* backing, size_t n)
        {
            FreeAll();

            if (!backing)
            {
                throw std::invalid_argument("backing == nullptr");
            }
            if (n == 0)
            {
                throw std::invalid_argument("n == 0");
            }
            n_ = n;
            data_ = backing;
            OwnedSizeBytes_ = 0;
        }

        void FreeAll()
        {
            if (data_)
            {
                for (size_t i = 0; i < n_; i++)
                {
                    data_[i].~atomic();
                }
                if (OwnedSizeBytes_ != 0)
                {
                    AllocNW::FreeONNode(static_cast<void*>(data_), OwnedSizeBytes_);
                }
                data_ = nullptr;
            }
            n_ = 0;
            OwnedSizeBytes_ = 0;
        }

        packed64_t Load(size_t idx)const noexcept
        {
            if (idx >= n_)
            {
                return packed64_t(0);
            }
            return data_[idx].load(MoLoad_);
        }

        void store(size_t idx, packed64_t val) noexcept
        {
            if (idx >= n_)
            {
                return;
            }
            data_[idx].store(val, MoStore_);
            std::atomic_notify_all(&data_[idx]);         
        }

        bool CompExchange(size_t idx, packed64_t &expected, packed64_t desired) noexcept
        {
            if (idx >= n_)
            {
                return false;
            }
            return data_[idx].compare_exchange_strong(expected, desired, EXsuccess_, EXfailure_);
        }

        bool ReserveForUpdate(size_t idx, packed64_t &expected_observed, packed64_t pending_packed) noexcept
        {
            return CompExchange(idx, expected_observed, pending_packed);
        }

        bool CommitUpdate(size_t idx, packed64_t expected_pending, packed64_t commited_packed) noexcept
        {
            bool ok = CompExchange(idx, expected_pending, commited_packed);
            if (ok)
            {
                std::atomic_notify_all(&data_[idx]);
            }
            return ok;
        }

        bool TryReserveFromLoad(size_t idx, packed64_t& expected_out, packed64_t pending_template) noexcept
        {
            if (idx >= n_)
            {
                return false;
            }
            packed64_t observed = data_[idx].load(MoLoad_);
            expected_out = observed;
            return data_[idx].compare_exchange_strong(observed, pending_template, EXsuccess_, EXfailure_);
            
        }

        bool WaitForChanges(size_t idx, packed64_t expected, int timeout_ms = -1) const noexcept
        {
            if (idx >= n_)
            {
                return false;
            }
            if (timeout_ms < 0)
            {
                std::atomic_wait(&data_[idx], expected);
                return true;
            }
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
            while (std::chrono::steady_clock::now() < deadline)
            {
                packed64_t cur = data_[idx].load(MoLoad_);
                if (cur != expected)
                {
                    return true;
                }
                std::atomic_wait(&data[idx], expected);
            }
            return false;
        }

        //extract val32
        val32_t LoadValue32(size_t idx) const noexcept
        {
            packed64_t p = Load(idx);
            return PackedCell64_t::extract_value32(p);
        }
        clk16_t LoadInClock16(size_t idx) const noexcept
        {
            packed64_t p = Load(idx);
            return PackedCell64_t::extract_clk16(p);
        }
        tag8_t LoadState(size_t idx) const noexcept
        {
            packed64_t p = Load(idx);
            return PackedCell64_t::extract_st_value32(p);
        }
        tag8_t LoadRelation(size_t idx) const noexcept
        {
            packed64_t p = Load(idx);
            return PackedCell64_t::extract_rel_value32(p);
        }

        //clk48
        uint64_t LoadClock48(size_t idx) const noexcept
        {
            packed64_t p = Load(idx);
            return PackedCell64_t::extract_clk48(p);
        }
        tag8_t LoadState(size_t idx) const noexcept
        {
            packed64_t p = Load(idx);
            return PackedCell64_t::extract_st_clk48(p);
        }
        tag8_t LoadRelation(size_t idx) const noexcept
        {
            packed64_t p = Load(idx);
            return PackedCell64_t::extract_rel_clk48(p);
        }

    };
    
    
}