// AtomicCSCompact.cpp
#include "AtomicCSCompact.h"

#include <cstdlib>
#include <new>
#include <algorithm>
#include <iostream>

namespace AtomicCSCompact
{

// ---------------- BitPacker ----------------
template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
struct BitPacker {
    static_assert(std::is_unsigned_v<OUT>, "OUT must be unsigned");
    static constexpr size_t TOTAL_BITS = (VALBITS + VALBITS) + (STRLB + STRLB) + CLKB;
    static_assert(TOTAL_BITS <= sizeof(OUT) * 8, "Total bits exceed OUT");

    static constexpr OUT VAL_MASK  = (VALBITS == 0) ? OUT(0) : ((OUT(1) << VALBITS) - OUT(1));
    static constexpr OUT STRL_MASK = (STRLB == 0) ? OUT(0) : ((OUT(1) << STRLB) - OUT(1));
    static constexpr OUT CLK_MASK  = (CLKB == 0) ? OUT(0) : ((OUT(1) << CLKB) - OUT(1));

    // pack: val (lsb) | inv | rel | st | clk
    static inline OUT pack(OUT val, OUT inv, OUT st, OUT rel, OUT clk) noexcept {
        OUT out = 0;
        out |= (val & VAL_MASK);
        out |= ((inv & VAL_MASK) << VALBITS);
        out |= ((rel & STRL_MASK) << (VALBITS + VALBITS));
        out |= ((st  & STRL_MASK) << (VALBITS + VALBITS + STRLB));
        out |= ((clk & CLK_MASK)  << (VALBITS + VALBITS + STRLB + STRLB));
        return out;
    }

    template<typename Vt, typename Stt, typename Clkt>
    static inline void unpack(OUT packed, Vt &val, Vt &inv, Stt &st, Stt &rel, Clkt &clk) noexcept {
        OUT cursor = packed;
        val = static_cast<Vt>(cursor & VAL_MASK);
        cursor >>= VALBITS;
        inv = static_cast<Vt>(cursor & VAL_MASK);
        cursor >>= VALBITS;
        rel = static_cast<Stt>(cursor & STRL_MASK);
        cursor >>= STRLB;
        st  = static_cast<Stt>(cursor & STRL_MASK);
        cursor >>= STRLB;
        clk = static_cast<Clkt>(cursor & CLK_MASK);
    }
};

// ---------------- ARFieldView (POD) ----------------
template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
struct ARFieldView {
    using valin_t = std::conditional_t<VALBITS <= 8, uint8_t,
                    std::conditional_t<VALBITS <= 16, uint16_t, uint32_t>>;
    using strl_t = std::conditional_t<STRLB <= 8, uint8_t, uint16_t>;
    using clk_t  = std::conditional_t<CLKB <= 8, uint8_t,
                    std::conditional_t<CLKB <= 16, uint16_t, uint32_t>>;

    valin_t value;
    valin_t inv;
    strl_t st;
    strl_t rel;
    clk_t clk;
};

// ---------------- PackedACArray implementations ----------------

template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
PackedACArray<VALBITS,STRLB,CLKB,OUT>::PackedACArray() noexcept : n_(0), data_(nullptr) {}

template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
PackedACArray<VALBITS,STRLB,CLKB,OUT>::~PackedACArray() { free_all(); }

template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
PackedACArray<VALBITS,STRLB,CLKB,OUT>::PackedACArray(PackedACArray&& o) noexcept
: n_(o.n_), data_(o.data_) { o.n_ = 0; o.data_ = nullptr; }

template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
PackedACArray<VALBITS,STRLB,CLKB,OUT>&
PackedACArray<VALBITS,STRLB,CLKB,OUT>::operator=(PackedACArray&& o) noexcept {
    if (this != &o) {
        free_all();
        n_ = o.n_; data_ = o.data_;
        o.n_ = 0; o.data_ = nullptr;
    }
    return *this;
}

template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
void PackedACArray<VALBITS,STRLB,CLKB,OUT>::init(std::size_t n) {
    if (data_)
    {
        FreeAll(data_);
    }
    InitAView(data_, n, n_);
}

template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
void PackedACArray<VALBITS,STRLB,CLKB,OUT>::free_all() noexcept {
    if (data_) {
        FreeAll(data_);
    }
}

template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
std::size_t PackedACArray<VALBITS,STRLB,CLKB,OUT>::sizePA() const noexcept { return n_; }

template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
bool PackedACArray<VALBITS,STRLB,CLKB,OUT>::emptyPA() const noexcept { return n_ == 0; }

template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
std::optional<typename PackedACArray<VALBITS,STRLB,CLKB,OUT>::FieldView>
PackedACArray<VALBITS,STRLB,CLKB,OUT>::Read(std::size_t idx, std::memory_order order) const noexcept
{
    if (idx >= n_) return std::nullopt;
    OUT raw = data_[idx].load(order);
    FieldView f{};
    BitPacker<VALBITS,STRLB,CLKB,OUT>::unpack(raw, f.value, f.inv, f.st, f.rel, f.clk);
    OUT expected_inv = (BitPacker<VALBITS,STRLB,CLKB,OUT>::VAL_MASK & (~OUT(f.value)));
    if (OUT(f.inv) != expected_inv) return std::nullopt;
    return f;
}

template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
bool PackedACArray<VALBITS,STRLB,CLKB,OUT>::WriteCas(std::size_t idx, valin_t newValue,
            std::optional<strl_t> setST, std::optional<strl_t> setREL, std::memory_order casOrder) noexcept
{
    if (idx >= n_) return false;
    OUT valmask = OUT(newValue) & BitPacker<VALBITS,STRLB,CLKB,OUT>::VAL_MASK;
    OUT invmask = (OUT(~OUT(newValue)) & BitPacker<VALBITS,STRLB,CLKB,OUT>::VAL_MASK);

    for (;;) {
        OUT old = data_[idx].load(std::memory_order_acquire);
        valin_t oldv; valin_t oldinv; strl_t oldst; strl_t oldrel; clk_t oldclk;
        BitPacker<VALBITS,STRLB,CLKB,OUT>::unpack(old, oldv, oldinv, oldst, oldrel, oldclk);

        clk_t pend = clk_t(oldclk + 1u);
        strl_t stv = setST.has_value() ? setST.value() : oldst;
        strl_t relv = setREL.has_value() ? setREL.value() : oldrel;

        OUT pending = BitPacker<VALBITS,STRLB,CLKB,OUT>::pack(valmask, invmask, OUT(stv), OUT(relv), OUT(pend));

        OUT expected = old;
        if ( data_[idx].compare_exchange_strong(expected, pending, casOrder, std::memory_order_acquire) ) {
            // commit final
            clk_t finalclk = clk_t(pend + 1u);
            OUT finalw = BitPacker<VALBITS,STRLB,CLKB,OUT>::pack(valmask, invmask, OUT(stv), OUT(relv), OUT(finalclk));
            data_[idx].store(finalw, std::memory_order_release);
            return true;
        }
        // else CAS failed; retry
    }
}

template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
void PackedACArray<VALBITS,STRLB,CLKB,OUT>::CommitStore(std::size_t idx, valin_t newValue, strl_t stv, strl_t relv, std::memory_order mo) noexcept
{
    if (idx >= n_) return;
    OUT old = data_[idx].load(std::memory_order_relaxed);
    valin_t oldv; valin_t oldinv; strl_t oldst; strl_t oldrel; clk_t oldclk;
    BitPacker<VALBITS,STRLB,CLKB,OUT>::unpack(old, oldv, oldinv, oldst, oldrel, oldclk);
    clk_t newclk = clk_t(oldclk + 2u);
    OUT valmask = OUT(newValue) & BitPacker<VALBITS,STRLB,CLKB,OUT>::VAL_MASK;
    OUT invmask = (OUT(~OUT(newValue)) & BitPacker<VALBITS,STRLB,CLKB,OUT>::VAL_MASK);
    OUT packed = BitPacker<VALBITS,STRLB,CLKB,OUT>::pack(valmask, invmask, OUT(stv), OUT(relv), OUT(newclk));
    data_[idx].store(packed, mo);
}

template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
void PackedACArray<VALBITS,STRLB,CLKB,OUT>::debug_print(std::size_t idx) const noexcept {
    auto opt = Read(idx);
    if (!opt) { std::cout << "idx=" << idx << " <invalid>\n"; return; }
    const auto &f = *opt;
    std::cout << "idx=" << idx << " val=" << +f.value << " inv=" << +f.inv
              << " st=" << +f.st << " rel=" << +f.rel << " clk=" << +f.clk << "\n";
}

template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
bool PackedACArray<VALBITS,STRLB,CLKB,OUT>::is_lock_free() noexcept {
    return std::atomic<OUT>().is_lock_free();
}

// explicit instantiations (the ones used by your program)
template class PackedACArray<4,4,16,uint32_t>;
template class PackedACArray<8,4,8,uint32_t>;
template class PackedACArray<16,8,16,uint64_t>;

} // namespace AtomicCSCompact
