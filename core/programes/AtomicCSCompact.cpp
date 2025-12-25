#include "AtomicCSCompact.h"
#include <new>
#include <iostream>
#include <cstring>
#include <cassert>

namespace AtomicCScompact{

    template<size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
    struct BitPacker
    {
        static_assert(std::is_unsigned_v<OUT>,"OUT must be unsigned");
        static_assert((VALBITS + (2 * STRLB) + CLKB) <= sizeof(OUT) * SIZE_OF_BYTE_IN_BITS,
                        "Packed width exceed OUT length");
        static constexpr OUT ValMask = (VALBITS == 0) ? OUT(0) : ((OUT(1) << VALBITS) - OUT(1));
        static constexpr OUT StrlMask = (STRLB == 0) ? OUT(0) : ((OUT(1) << STRLB) - OUT(1));
        static constexpr OUT ClkMask = (CLKB == 0) ? OUT(0) : ((OUT(1) << CLKB) - OUT(1));
        static inline OUT PackDevil(OUT value, OUT st, OUT rel, OUT clk) noexcept
        {
            OUT out = 0;
            out |= (value & ValMask);
            out |= ((rel & StrlMask) << VALBITS);
            out |= ((st & StrlMask) << (VALBITS + STRLB));
            out |= ((clk & ClkMask) << (VALBITS + STRLB * 2));
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
    };

    template<size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
    PackedACarray<VALBITS, STRLB, CLKB, OUT>::PackedACarray() noexcept:
                    n_(0), data_(nullptr)
    {}

    template<size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
    PackedACarray<VALBITS, STRLB, CLKB, OUT>:: ~PackedACarray()
    {
        free_all();
    }

    template<size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
    PackedACarray<VALBITS, STRLB, CLKB, OUT>::PackedACarray(PackedACarray&& o) noexcept:
        n_(o.n_), data_(o.data_)
    {
        o.n_ = 0;
        o.data_ = nullptr;
    }

    template<size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
    PackedACarray<VALBITS, STRLB, CLKB, OUT>&
    PackedACarray<VALBITS, STRLB, CLKB, OUT>::operator= (PackedACarray&& o) noexcept
    {
        if (this != &o)
        {
            free_all();
            n_ = o.n_;
            data_ = o.data_;
            o.n_ = 0;
            o.data_ = nullptr;
        }
    }

    template<size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
    void PackedACarray<VALBITS, STRLB, CLKB, OUT>::init(size_t n, uint8_t PrefAllignment)
    {
        free_all();
        if (n == 0)
        {
            n_ = 0;
            return;
        }
        n_ = n;
        const allignment = std::max<size_t>(alignof(std::atomic<OUT>, static_cast<size_t>(PrefAllignment)));
        try
        {
            data_ = static_cast<std::atomic<OUT>*>(operator new[](n_ * sizeof(std::atomic<OUT>), std::align_val_t(PrefAllignment)));        
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            free_all();
            throw;
        }
    }

    template<size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
    void PackedACarray<VALBITS, STRLB, CLKB, OUT>::free_all() noexcept
    {
        if (!data_)
        {
            return;
        }

        delete[] data_;
        data_ = nullptr;
    }

    template<size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
    std::optional<typename PackedACarray<VALBITS, STRLB, CLKB, OUT>::ACFieldView>
    PackedACarray<VALBITS, STRLB, CLKB, OUT>::Read(std::size_t idx, std::memory_order mo) const noexcept
    {
        if (idx >= n_)
        {
            return std::nullopt;
        }
        OUT raw = data_[idx].load(mo);
        ACFieldView f {};
        BitPacker<VALBITS, STRLB, CLKB, OUT>::unpack(raw, f.value, f.st, f.rel, f.clk);
        if ((f.clk & 1u) != 0u)
        {
            return std::nullopt;
        }
        return f;
    }

    template<size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
    bool PackedACarray<VALBITS,STRLB,CLKB,OUT>::writeCAS(std::size_t idx, valin_t newValue,
            std::optional<strl_t> setST, std::optional<strl_t> setREL, std::memory_order order) noexcept
    {
        if (idx >= n_) return false;
        out_t valueMask = out_t(newValue) & VALMASK;

        while (true) {
            out_t old = data_[idx].load(std::memory_order_acquire);
            valin_t oldv;
            strl_t oldst;
            strl_t oldrel;
            clk_t oldclk;
            unpack(old, oldv, oldst, oldrel, oldclk);

            strl_t newst = setST.has_value() ? setST.value() : oldst;
            strl_t newrel = setREL.has_value() ? setREL.value() : oldrel;
            clk_t pendingClk = static_cast<clk_t>(oldclk + 1u);

            out_t pending = pack(valueMask, out_t(newrel), out_t(newst), out_t(pendingClk));

            out_t expected = old;
            if (data_[idx].compare_exchange_strong(expected, pending, order, std::memory_order_acquire)) {
                // commit atomically the final state (advance clock)
                clk_t finalClk = static_cast<clk_t>(pendingClk + 1u);
                out_t finalw = pack(valueMask, out_t(newrel), out_t(newst), out_t(finalClk));
                data_[idx].store(finalw, std::memory_order_release);
                return true;
            }
            // else expected is updated; try again
        }
    }

    template<size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
    void PackedACarray<VALBITS, STRLB, CLKB, OUT>::CommitStore(std::size_t idx, valin_t newValue, strl_t setST, strl_t setREL, std::memory_order mo) noexcept
    {
        if (idx >= n_)
        {
            return;
        }

        OUT old = data_[idx].load(std::memory_order_relaxed);
        valin_t oldv;
        strl_t oldst;
        strl_t oldrel;
        clk16_t oldclk;
        BP_.unpack(old, oldv, oldst, oldrel, oldclk);
        clk16_t newclk = static_cast<clk16_t>(oldclk + 2u);
        OUT packed = BP_.PackDevil(OUT(newValue) & BP_::ValMask, OUT(setST), OUT(setREL), OUT(newclk));
        data_[idx].store(packed, mo);
    } 

    template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
    void PackedACarray<VALBITS,STRLB,CLKB,OUT>::CommitBlock(std::size_t base, const valin_t *vals, std::size_t count,
                        strl_t setST, strl_t setREL, std::memory_order mo) noexcept
    {
        if (base + count > n_)
        {
            return;
        }

        for (std::size_t i = 0; i < count; i++)
        {
            OUT old = data_[base + i].load(std::memory_order_relaxed);
            valin_t oldv;
            strl_t oldst;
            strl_t oldrel;
            clk16_t oldclk;
            BP_.unpack(old, oldv, oldst, oldrel, oldclk);
            clk16_t newclk = static_cast<clk16_t>(oldclk + 2u);
            OUT packed = BP_.PackDevil(OUT(vals[i] & BP_::ValMask, OUT(setST), OUT(setREL), OUT(newclk)));
            data_[base + i].store(packed, mo);
        }
    }


    template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
    void PackedACarray<VALBITS,STRLB,CLKB,OUT>::debugPrint(std::size_t idx) const noexcept
    {
        if (idx >= n_)
        {
            std::cout << "idx = " << idx << "Out of range" << std::endl;
            return;
        }

        OUT raw = data_[idx].load(std::memory_order_acquire);
        valin_t v;
        strl_t st;
        strel_t rel;
        clk16_t clk;

        BP_.unpack(raw, v, st, rel, clk);
        std::cout << "idx=" << idx << " v=" << +v << " st=" << +st << " rel=" << +rel << " clk=" << +clk << "\n";        
    }


    template class PackedACarray<32, 8, 16, uint64_t>;

}