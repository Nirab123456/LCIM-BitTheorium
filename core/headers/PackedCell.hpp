#pragma once
#include <cstdint>
#include <type_traits>
#include <cassert>
#include <limits>
#include <atomic>

#define ATOMIC_THRESHOLD 64u

namespace AtomicCScompact
{
    #define NO_VAL 0u
    #define MAX_VAL 64u

    static constexpr std::memory_order MoLoad_      = std::memory_order_acquire;
    static constexpr std::memory_order MoStoreSeq_  = std::memory_order_release;
    static constexpr std::memory_order MoStoreUnSeq_= std::memory_order_relaxed;
    static constexpr std::memory_order EXsuccess_   = std::memory_order_acq_rel;
    static constexpr std::memory_order EXfailure_   = std::memory_order_relaxed;

    static constexpr unsigned CLK_B48 = 48u;
    static constexpr unsigned VALBITS  = 32u;
    static constexpr unsigned CLK_B16  = 16u;
    static constexpr unsigned STBITS   = 8u;
    static constexpr unsigned RELBITS  = 8u;

    using packed64_t = uint64_t;
    using val32_t    = uint32_t;
    using clk16_t    = uint16_t;
    using tag8_t     = uint8_t;

    enum class PackedMode : int
    {
        MODE_VALUE32 = 0,
        MODE_CLKVAL48 = 1
    };
    static inline constexpr packed64_t MaskBits(unsigned n) noexcept
    {
        if (n == NO_VAL) return packed64_t(0);
        if (n >= MAX_VAL) return ~packed64_t(0);
        // produce low-n ones without shifting by >= width
        return ( (~packed64_t(0)) >> (MAX_VAL - static_cast<unsigned>(n)) );
    }

    struct PackedCell64_t 
    {
        static inline packed64_t PackV32x_64(val32_t v, clk16_t clk, tag8_t st, tag8_t rel) noexcept {
            packed64_t p = (packed64_t(v) & MaskBits(VALBITS));
            p |= (packed64_t(clk) & MaskBits(CLK_B16)) << VALBITS;
            p |= (packed64_t(st)  & MaskBits(STBITS))  << (VALBITS + CLK_B16);
            p |= (packed64_t(rel) & MaskBits(RELBITS)) << (VALBITS + CLK_B16 + STBITS);
            return p;
        }

        static inline packed64_t PackCLK48x_64(clk16_t clk, tag8_t st, tag8_t rel) noexcept {
            packed64_t p = (packed64_t(clk) & MaskBits(CLK_B16));
            p |= (packed64_t(st)  & MaskBits(STBITS))  << CLK_B16;
            p |= (packed64_t(rel) & MaskBits(RELBITS)) << (CLK_B16 + STBITS);
            return p;
        }
        static inline val32_t ExtractValue32(packed64_t p) noexcept
        {
            return static_cast<val32_t>(p & MaskBits(VALBITS));
        }
        static inline clk16_t ExtractClk16(packed64_t p) noexcept
        {
            return static_cast<clk16_t>((p >> (VALBITS)))
        }
    };

}
