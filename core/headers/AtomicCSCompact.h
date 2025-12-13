#pragma once
// AtomicCSCompact.h - declarations only (templates are defined/instantiated in .cpp)

#include <cstdint>
#include <atomic>
#include <optional>
#include <cstddef>
#include <type_traits>

namespace AtomicCSCompact
{

// small nibble type
struct uint4_tt {
    uint8_t v;
    constexpr uint4_tt() : v(0) {}
    constexpr explicit uint4_tt(uint8_t x) : v(uint8_t(x & 0x0Fu)) {}
    constexpr operator uint8_t() const { return uint8_t(v & 0x0Fu); }
    constexpr uint4_tt& operator=(uint8_t x) { v = uint8_t(x & 0x0Fu); return *this; }
};


template<typename T>
void InitAView(std::atomic<T>*& dataptr, size_t N, size_t& cn)
{
    FreeAll(dataptr);
    if (N == 0)
    {
        cn = 0;
        return;
    }
    cn = N;
    const size_t alignment = std::max<size_t>(alignof(std::atomic<T>), static_cast<size_t>(PREF_ALIGN_4_8));
    try
    {
        dataptr = new(std::align_val_t(alignment)) std::atomic<uint32_t>[cn]();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        FreeAll(dataptr);
        throw;
    }
}

// FreeAll helper (inline)
template<typename... T>
inline void FreeAll(T*&... ptrs) {
    auto del = [](auto*& p) {
        if (p) { delete[] p; p = nullptr; }
    };
    (del(ptrs), ...);
}

// forward-declare the packer and field-view templates
template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT = uint64_t>
struct BitPacker;

template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT = uint64_t>
struct ARFieldView;

// Packed atomic array declaration (definitions in .cpp)
// VALBITS : bits for value
// STRLB   : bits for each of st and rel
// CLKB    : bits for clock
template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT = uint64_t>
class PackedACArray
{
    static_assert(((VALBITS + VALBITS) + (STRLB + STRLB) + CLKB) <= (sizeof(OUT) * 8),
                  "(v + inv + st + rel + clk) exceed the intended OUT width");

public:
    using out_t = OUT;
    using valin_t = std::conditional_t<VALBITS <= 8, uint8_t,
                    std::conditional_t<VALBITS <= 16, uint16_t, uint32_t>>;
    using strl_t = std::conditional_t<STRLB <= 8, uint8_t, uint16_t>;
    using clk_t  = std::conditional_t<CLKB <= 8,  uint8_t,
                    std::conditional_t<CLKB <= 16, uint16_t, uint32_t>>;

    using FieldView = ARFieldView<VALBITS, STRLB, CLKB, OUT>;

    // ctor/dtor and move
    PackedACArray() noexcept;
    ~PackedACArray();
    PackedACArray(const PackedACArray&) = delete;
    PackedACArray& operator=(const PackedACArray&) = delete;
    PackedACArray(PackedACArray&&) noexcept;
    PackedACArray& operator=(PackedACArray&&) noexcept;

    // allocate/free
    void init(std::size_t n);
    void free_all() noexcept;

    // capacity
    std::size_t sizePA() const noexcept;
    bool emptyPA() const noexcept;

    // Read: returns std::nullopt on out-of-range or integrity check failure
    std::optional<FieldView> Read(std::size_t idx, std::memory_order order = std::memory_order_acquire) const noexcept;

    // CAS-style write: returns true if successful
    bool WriteCas(std::size_t idx, valin_t newValue,
                  std::optional<strl_t> setST = {},
                  std::optional<strl_t> setREL = {},
                  std::memory_order casOrder = std::memory_order_acq_rel) noexcept;

    // Exclusive committer path (no CAS): commit new value and bump clock
    void CommitStore(std::size_t idx, valin_t newValue, strl_t setST = 0, strl_t setREL = 0,
                     std::memory_order mo = std::memory_order_release) noexcept;

    // debug
    void debug_print(std::size_t idx) const noexcept;

    // is underlying atomic lock free?
    static bool is_lock_free() noexcept;


private:
    std::size_t n_;
    std::atomic<OUT>* data_;
};

// convenience typedefs (explicit instantiations will exist in .cpp)
using RelPacked4  = PackedACArray<4, 4, 16, uint32_t>;
using RelPacked8  = PackedACArray<8, 4,  8, uint32_t>;
using RelPacked16 = PackedACArray<16,8, 16, uint64_t>;

} // namespace AtomicCSCompact
