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
    void FreeAll(T*&... ptrs);
    template<typename T>
    void InitAnyView(std::atomic<T>*& dataptr, size_t N, size_t& cn);

    //bit packer
    template <
        size_t VALIB,
        size_t STRLB,
        size_t CLKB,
        typename OUT = uint64_t
    >
    struct BitPacker;

    //field view
    template <
        size_t VALIB,
        size_t STRLB,
        size_t CLKB,
        typename OUT = uint64_t
    >
    struct ARFieldView;

    //packed array
    template <
        size_t VALIB,
        size_t STRLB,
        size_t CLKB,
        typename OUT = uint64_t
    >
    class PackedACArray 
    {
        static_assert(((VALIB + VALIB) + (STRLB +STRLB) + CLKB) <= (sizeof(OUT) * 8), "(v + inv + st + rel) exceed the intended OUT width");
        using PackDevil = BitPacker<VALIB, STRLB, CLKB, OUT>;
    
    private:
        size_t n_;
        std::atomic<OUT>* data_;
    
    public:


        PackedACArray() noexcept :
            n_(0), data_{nullptr}
        {}
        ~PackedACArray(const PackedACArray&) = delete;
        PackedACArray& operator = (const PackedACArray&) = delete;
        PackedACArray(PackedACArray&& o) noexcept
        {
            n_ = o.n_;
            data_ = o.data_;
            o.n_ = 0;
            o.data_ = nullptr;
        }
        PackedACArray& operator = (PackedACArray&& o) noexcept
        {
            if (this != &o)
            {
                FreeAll(data_);
                n_ = o.n_;
                data_ = o.data_;
                o.n_ = 0,
                o.data_ = nullptr;
            }
            return* this;
        }


        size_t sizePA() const noexcept { return n_; }
        bool IsemptyPA() const noexcept { return n_ == 0; }

        
    std::optional<ARFieldView> 
        Read(size_t idx, std::memory_order order = std::memory_order_acquire) const noexcept;

    std::optional<ARFieldView<VALIB, STRLB, CLKB, OUT>>
        bool WriteCas(size_t idx, valin_t nvalue, 
            std::optional<strl_t> setST = {},
            std::optional<strl_t> setREL = {},
            std::memory_order CASOrder = std:: memory_order_acq_rel
        ) noexcept;
    };


}