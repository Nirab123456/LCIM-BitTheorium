#pragma once
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <chrono>
#include <vector>
#include "AllocNW.hpp"
#include "PackedCell.hpp"
#include "PackedStRel.h"


namespace AtomicCScompact
{
    using HWCallback = void(*)(size_t current, size_t capacity, void* user);


template<PackedMode MODE>
class AtomicDSA
{
private:
    std::atomic<PackedCell64_t*> Rawptr_{nullptr};
    size_t Capacity_{0};
    std::atomic<size_t> Count_{0};
    std::atomic<size_t> ProducerCursor_{0};
    std::atomic<size_t> ConsumCursor_{0};

    HWCallback CB_{nullptr};
    void* CBUser_{nullptr};
    int Node_{0};
    
    inline void CheckHighWater_(size_t occ)
    {
        if (CB_)
        {
            return;
        }
        if (occ* 10 >= Capacity_ * 8)
        {
            CB_(occ, Capacity_, CBUser_);
        }
        
        
    }
    inline packed64_t MakeIdlePacked_() const noexcept
    {
        if constexpr (MODE == PackedMode::MODE_VALUE32)
        {
            return PackedCell64_t::ComposeVal32(val32_t(), clk16_t(0), ST_IDLE, tag8_t(0));
        }
        else if constexpr (MODE == PackedMode::MODE_CLKVAL48)
        {
            return PackedCell64_t::ComposeCLK48V(uint64_t(0), ST_IDLE, tag8_t(0));
        }
    }
    inline tag8_t LoadState_(packed64_t p) const noexcept
    {
        if constexpr (MODE == PackedMode::MODE_VALUE32)
        {
            return PackedCell64_t::extract_st_value32(p);
        }
        else if constexpr (MODE == PackedMode::MODE_CLKVAL48)
        {
            return PackedCell64_t::extract_st_clk48(p);
        }    
    }
    inline tag8_t LoadRelation_(packed64_t p) const noexcept
    {
        if constexpr (MODE == PackedMode::MODE_VALUE32)
        {
            return PackedCell64_t::extract_rel_value32(p);
        }
        else if constexpr (MODE == PackedMode::MODE_CLKVAL48)
        {
            return PackedCell64_t::extract_rel_clk48(p);
        }    
    }
    inline packed64_t MakeClaimFrom_(packed64_t p) const noexcept
    {
        if constexpr (MODE == PackedMode::MODE_VALUE32)
        {
            val32_t v; clk16_t clk; tag8_t st; tag8_t rel;
            PCellVal32_x64t::UnpackV32x_64(v, clk, st, rel, p);
            return PackedCell64_t::ComposeVal32(v, clk, ST_CLAIMED, rel);
        }
        else if constexpr(MODE == PackedMode::MODE_CLKVAL48)
        {
            uint64_t full_clk_48; tag8_t st; tag8_t rel;
            PCLKCell48_x64::UnpackCLK48x_64(full_clk_48, st, rel, p);
            return PackedCell64_t::ComposeCLK48V(full_clk_48, ST_CLAIMED, rel);
        }
    }
    /* data */
public:
    AtomicDSA(size_t capacity_pow2, int node = 0, HWCallback hw_cb = nullptr, void* cb_user = nullptr) :
        Capacity_(capacity_pow2), CB_(hw_cb), CBUser_(cb_user), Node_(0)
    {
        if (Capacity_ == 0)
        {
            throw std::invalid_argument("Capacity_ == 0");
            size_t bytes = sizeof(std::atomic<PackedCell64_t>) * Capacity_;
            Rawptr_ = reinterpret_cast<std::atomic<packed64_t>*>(AllocNW::AlignedAllocONnode(ATOMIC_THRESHOLD, bytes, Node_));
            if (!Rawptr_)
            {
                throw std::bad_alloc();
            }
            PackedCell64_t idle = MakeIdlePacked_();
            for (size_t i = 0; i < Capacity_; i++)
            {
                new(&Rawptr_[i]) std::atomic<PackedCell64_t>(idle);
            }
            Count_.store(0, MoStoreUnSeq_);
            ProducerCursor_(0, MoStoreUnSeq_);
            ConsumCursor_(0, MoStoreUnSeq_)
            
            
        }
        
    }
    ~AtomicDSA();
};



}