#include <cstdint>
#include <type_traits>
#include <cassert>
#include "AllocNW.hpp"

#define ATOMIC_THREASHHOLD 64

namespace AtomicCScompact
{
    static constexpr unsigned CLK_B48 = 48;
    static constexpr unsigned VALBITS = 32;
    static constexpr unsigned CLK_B16 = 16;
    static constexpr unsigned STBITS  = 8;
    static constexpr unsigned RELBITS = 8;
    using packed64_t        = uint64_t;
    using val32_t           = uint32_t;
    using clk16_t           = uint16_t;
    using tag8_t            = uint8_t;
    

    enum class PackedMode :
        int
        {
            MODE_VALUE32 = 0,
            MODE_CLKVAL48 = 1
        };

    static inline constexpr packed64_t MaskBits(unsigned n) noexcept
    {
        if (n >= ATOMIC_THREASHHOLD)
        {
           return ~packed64_t(0);
        }
        else
        {
            return ((packed64_t(1) << n) - packed64_t(1));   
        }
    }

    struct PCellVal32_x64t
    {
        static inline packed64_t PackV32x_64(val32_t value, clk16_t clk, tag8_t st, tag8_t rel) noexcept
        {
            packed64_t p = packed64_t(value) & MaskBits(VALBITS);
            p |= ((packed64_t(clk) & MaskBits(CLK_B16)) << VALBITS);
            p |= ((packed64_t(st) & MaskBits(STBITS)) << (VALBITS + CLK_B16));
            p |= ((packed64_t(rel) & MaskBits(rel)) << (VALBITS + CLK_B16 + STBITS));
            return p;
        }
        static inline val32_t UnpackVal(packed64_t p) noexcept
        {
            return val32_t(p & MaskBits(VALBITS));
        }
        static inline clk16_t UnpackCLK16(packed64_t p) noexcept{
            return clk16_t((p >> VALBITS) & MaskBits(CLK_B16));
        }
        static inline tag8_t UnpackST(packed64_t p) noexcept
        {
            return tag8_t((p >>(VALBITS + CLK_B16)) & MaskBits(STBITS));
        }
        static inline tag8_t UnpackREL(packed64_t p) noexcept
        {
            return tag8_t((p >>(VALBITS + CLK_B16 +STBITS)) & MaskBits(RELBITS));
        }

    };

    struct PCLKCell48_x64
    {
        static inline packed64_t PackCLK48x_64(uint64_t clkvalue, tag8_t st, tag8_t rel) noexcept
        {
            packed64_t p = (packed64_t)(clkvalue & MaskBits(CLK_B48));
            p |= (packed64_t(st) & MaskBits(STBITS)) << CLK_B48;
            p |= (packed64_t(rel) & MaskBits(RELBITS)) << (CLK_B48 + STBITS);
            return p;
        }
        static inline uint64_t UnpackCLK48(packed64_t p) noexcept
        {
            return uint64_t(p & MaskBits(CLK_B48));
        }
        static inline tag8_t UnpackST(packed64_t p) noexcept
        {
            return tag8_t((p >> CLK_B48) & MaskBits(STBITS));
        }
        static inline tag8_t UnpackREL(packed64_t p) noexcept
        {
            return tag8_t((p >> (CLK_B48 + STBITS)) & MaskBits(RELBITS));
        }

    };

    struct PackedCell64_t 
    {
        static inline packed64_t ComposeVal32(val32_t v, clk16_t clk, tag8_t st, tag8_t rel) noexcept
        {
            return PCellVal32_x64t::PackV32x_64(v, clk, st, rel);
        }
        static inline packed64_t ComposeCLK48V(uint64_t v, tag8_t st, tag8_t rel) noexcept
        {
            return PCLKCell48_x64::PackCLK48x_64(v, st, rel);
        }

        static inline val32_t extract_value32(packed64_t p) noexcept { return PCellVal32_x64t::UnpackVal(p); }
        static inline clk16_t extract_clk16(packed64_t p) noexcept  { return PCellVal32_x64t::UnpackCLK16(p); }
        static inline uint64_t extract_clk48(packed64_t p) noexcept  { return PCLKCell48_x64::UnpackCLK48(p); }
        static inline tag8_t  extract_st_value32(packed64_t p) noexcept { return PCellVal32_x64t::UnpackST(p); }
        static inline tag8_t  extract_rel_value32(packed64_t p) noexcept { return PCellVal32_x64t::UnpackREL(p); }
        static inline tag8_t  extract_st_clk48(packed64_t p) noexcept  { return PCLKCell48_x64::UnpackST(p); }
        static inline tag8_t  extract_rel_clk48(packed64_t p) noexcept { return PCLKCell48_x64::UnpackREL(p); }    

    };

}