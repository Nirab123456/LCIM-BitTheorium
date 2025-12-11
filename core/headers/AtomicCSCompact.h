#pragma once

#include <cstdint>
#include <atomic>
#include <vector>
#include <cassert>
#include <optional>
#include <cstring>
#include <iostream>
#include <limits>

#if defined(__x86) || defined(__aarch64__)
    #define PREF_ALIGN_4_8 64
#else
    #define PREF_ALIGN_4_8 32
#endif


namespace AtomicCSCompact
{
    struct uint4_tt;
    struct Packed32Fields
    {
        uint32_t raw;
    };
    void AssertAtomicLockfree();


    template<typename... T>
    void FreeAll(T*&... ptrs)
    {
        auto del = [](auto*& p)
        {
            if (p)
            {
                delete[] p;
                p = nullptr;
            }
        };
        (del(ptrs), ...)
    }

    template<typename T>
    void InitAnyView(std::atomic<T>*& dataptr, size_t N, size_t& cn);


    template <
        size_t VALIB,
        size_t STRLB,
        size_t CLKB,
        typename OUT = uint64_t
    >

    struct BitPacker;
    class ACBCompact4_t
    {
    private:
        size_t n_ = 0;
        std::atomic<uint32_t>* data_ = nullptr;
    public:
        ACBCompact4_t() = default;
        ~ACBCompact4_t()
        {
            FreeAll(data_);
        } 

        struct FieldView4_t;
        size_t size() const { return n_;}

        static constexpr uint32_t C4TMask = 0xfu;
        static constexpr uint32_t CLKMask = 0xffffu;


    };
}