#include <cstdint>
#include <type_traits>
#include <cassert>

#define ATOMIC_THREASHHOLD 64

namespace AtomicCScompact
{
    enum class PackedMode :
        int
        {
            MODE_VALUE32 = 0,
            MODE_CLK48 = 1
        };
    
    using packed64_t        = uint64_t;
    using val32_t           = uint32_t;
    using clk16_t           = uint16_t;
    using fullclk48_t       = uint64_t;
    using tag8_t            = uint8_t;
    
    static inline constexpr packed64_t MaskBits(unsigned n) noexcept
    {
        return (n >= ATOMIC_THREASHHOLD) ? ~packed64_t(0) : ((packed64_t(1) << n) - packed64_t(1));
    }

    struct PackedCellValue32
    {
        static constexpr unsigned VALBITS = 32;
        static constexpr unsigned CLKBITS = 16;
        static constexpr unsigned STBITS = 8;
        static constexpr unsigned RELBITS = 8;

        static_assert((VALBITS + CLKBITS + STBITS + RELBITS) == ATOMIC_THREASHHOLD, "Layout must fit in Max(64 bit) width");

        static inline packed64_t CellPacking(val32_t value, clk16_t clk, tag8_t st, tag8_t rel) noexcept
        {
            packed64_t p = packed64_t(value) & MaskBits(VALBITS);
            p |= (packed64_t(clk) & MaskBits(CLKBITS)) << VALBITS;
            p |= (packed64_t(st) & MaskBits(STBITS)) << (VALBITS + CLKBITS);
            p |= (packed64_t(rel) & MaskBits(RELBITS)) << (VALBITS + CLKBITS + STBITS);
            return p;
        }
        static inline val32_t UnpackValue(packed64_t p) noexcept
        {
            return val32_t(p & MaskBits(VALBITS));
        }
        static inline clk16_t UnpackCLK16(packed64_t p) noexcept
        {
            return clk16_t((p >> VALBITS) & MaskBits(CLKBITS));
        }
        static inline tag8_t UnpackST(packed64_t p) noexcept
        {
            return tag8_t((p >> (VALBITS + CLKBITS)) &  MaskBits(STBITS));
        }
        static inline tag8_t UnpackREL(packed64_t p) noexcept
        {
            return tag8_t((p >> (VALBITS + CLKBITS + STBITS)) & MaskBits(RELBITS));
        }
        
    };

    struct PackedCellCLOCK48 
    {
        static constexpr unsigned FULLCLKBITS = 48;
        static constexpr unsigned STBITS = 8;
        static constexpr unsigned RELBITS = 8;

        static_assert((FULLCLKBITS + STBITS + RELBITS) == 64, "Layout must fit in Max(64 bit) width");

        static inline packed64_t CLKCellPacking(fullclk48_t fullclk, tag8_t st, tag8_t rel) noexcept
        {
            packed64_t p = (packed64_t) (fullclk & MaskBits(FULLCLKBITS));
            p |= (packed64_t(st) & MaskBits(STBITS)) << FULLCLKBITS;
            p |= (packed64_t(rel) & MaskBits(RELBITS)) << (FULLCLKBITS + STBITS);
            return p;
        }
        static inline fullclk48_t UnpackFullClk(packed64_t p) noexcept
        {
            return fullclk48_t(p & MaskBits(FULLCLKBITS));
        }
        static inline tag8_t UnpackST(packed64_t p) noexcept
        {
            return tag8_t((p >> FULLCLKBITS) & MaskBits(STBITS));
        }
        static inline tag8_t UnpackREL(packed64_t p) noexcept
        {
            return tag8_t((p >> (FULLCLKBITS +STBITS)) & MaskBits(RELBITS));
        }
    };

    struct PackedCell
    {
        PackedMode mode;
        PackedCell(PackedMode m = PackedMode::MODE_VALUE32) noexcept:
            mode(m)
        {}

        static inline packed64_t CompressValue32(val32_t v, clk16_t c, tag8_t st, tag8_t rel) noexcept
        {
            return PackedCellValue32::CellPacking(v, c, st, rel);
        }
        static inline packed64_t CompressClk48(fullclk48_t c, tag8_t st, tag8_t rel) noexcept
        {
            return PackedCellCLOCK48::CLKCellPacking(c, st, rel);
        }

        static inline val32_t ExtractValue32(packed64_t p) noexcept
        {
            return PackedCellValue32::UnpackValue(p);
        }
        static inline clk16_t ExtractCLK16(packed64_t p) noexcept
        {
            return PackedCellValue32::UnpackCLK16(p);
        }
        static inline fullclk48_t ExtractFullCLK(packed64_t p) noexcept
        {
            return PackedCellCLOCK48::UnpackFullClk(p);
        }
        static inline tag8_t ExtractST_m(packed64_t p, PackedMode m) noexcept
        {
            if (m == PackedMode::MODE_VALUE32)
            {
                return PackedCellValue32::UnpackST(p);
            }
            else if (m == PackedMode::MODE_CLK48)
            {
                return PackedCellCLOCK48::UnpackST(p);
            }
        }
        static inline tag8_t ExtractREL_m(packed64_t p, PackedMode m) noexcept
        {
            if (m == PackedMode::MODE_VALUE32)
            {
                return PackedCellValue32::UnpackREL(p);
            }
            else if (m == PackedMode::MODE_CLK48)
            {
                return PackedCellCLOCK48::UnpackREL(p);
            }
        }
    };
}