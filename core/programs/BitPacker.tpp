#pragma once 
#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace AtomicCScompact
{

    template<size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
    struct BitPacker
    {
        static_assert(std::is_unsigned_v<OUT>,"OUT must be unsigned");
        static_assert((VALBITS + (2 * STRLB) + CLKB) <= sizeof(OUT) * SIZE_OF_BYTE_IN_BITS,
                        "Packed width exceed OUT length");
        static constexpr OUT ValMask = (VALBITS == 0) ? OUT(0) : ((OUT(1) << VALBITS) - OUT(1));
        static constexpr OUT StrlMask = (STRLB == 0) ? OUT(0) : ((OUT(1) << STRLB) - OUT(1));
        static constexpr OUT ClkMask = (CLKB == 0) ? OUT(0) : ((OUT(1) << CLKB) - OUT(1));
        static constexpr size_t RelSft = VALBITS;
        static constexpr size_t StSft = VALBITS + STRLB;
        static constexpr size_t ClkSft = VALBITS + 2*STRLB;
        static inline OUT PackDevil(OUT value, OUT st, OUT rel, OUT clk) noexcept
        {
            OUT out = 0;
            out |= (value & ValMask);
            out |= ((rel & StrlMask) << RelSft);
            out |= ((st & StrlMask) << StSft);
            out |= ((clk & ClkMask) << ClkSft);
            return out;
        }

        template<typename Vt, typename SRt, typename Ct>
        static inline void unpack(
            OUT packed, Vt &value, SRt &st, SRt &rel, Ct &clk
        )
        {
            OUT cursor = packed;
            value = static_cast<Vt>(cursor & ValMask);
            cursor >>= VALBITS;
            rel = static_cast<SRt>(cursor & StrlMask);
            cursor >>= STRLB;
            st = static_cast<SRt>(cursor & StrlMask);
            cursor >>= STRLB;
            clk = static_cast<Ct>(cursor & ClkMask);
        }
        static inline OUT unpackVal(OUT p) noexcept
        {
            return (p & ValMask);
        }
        static inline OUT unpackST(OUT p)
        {
            return ( (p >> StSft) & StrlMask );
        }
        static inline OUT unpackREL(OUT p)
        {
            return ( (p >> RelSft) & StrlMask);
        }
        static inline OUT unpackCLK(OUT p)
        {
            return ( (p >> ClkSft) & ClkMask);
        }
    };

}