#pragma once 
#include <cstring>
#include <atomic>
#include <optional>
#include <cstddef>
#include <type_traits>

#define SIZE_OF_BYTE_IN_BITS 8

namespace AtomicCScompact {

    template<size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT>
    struct BitPacker;
    

    template <size_t VALBITS, size_t STRLB, size_t CLKB, typename OUT = uint64_t>
    class PackedACarray
    {
        static_assert((VALBITS + (2* STRLB) + CLKB) <= sizeof(OUT) * 8,
            "Packed field exceeded OUt width");
    public:
        using out_t = OUT;
        using valin_t = std::conditional_t<VALBITS <= 8, uint8_t,
                std::conditional_t<VALBITS <= 16, uint16_t,
                std::conditional_t<VALBITS <= 32, uint32_t, uint64_t>>>;
        using strl_t = std::conditional_t<STRLB <=8, uint8_t,
                std::conditional_t<STRLB <= 16, uint16_t, uint32_t>>;
        using clk16_t = std::conditional_t<CLKB <= 8, uint8_t,
                std::conditional_t<CLKB <= 16, uint16_t, uint32_t>>;

        struct ACFieldView {
            valin_t value;
            strl_t st;
            strl_t rel;
            clk16_t clk;
        };

        PackedACarray() noexcept;
        ~PackedACarray();
        PackedACarray(const PackedACarray&) = delete;
        PackedACarray& operator = (const PackedACarray&) = delete;
        PackedACarray(PackedACarray&&) noexcept;
        PackedACarray& operator = (PackedACarray&&) noexcept;
        
        void init(std::size_t n, uint8_t PrefAllignment = PREF_ALLIGNMENT_);
        void free_all() noexcept;

        std::size_t sizePA() const noexcept
        {
            return n_;
        }
        bool emptyPA() const noexcept
        {
            return n_ == 0;
        }
        std::optional<ACFieldView> Read(
            std::size_t idx, std::memory_order mo = std::memory_order_acquire
        )const noexcept;
        bool writeCAS(
            std::size_t idx, valin_t newValue,
            std::optional<strl_t> setST = {},
            std::optional<strl_t> setREL = {},
            std::memory_order casOrder = std::memory_order_acq_rel
        ) noexcept;

        void CommitStore(
            std::size_t idx, valin_t newValue, 
            strl_t SetST = 0,
            strl_t SetREL = 0,
            std::memory_order mo = std::memory_order_release
        ) noexcept;

        void CommitBlock(std::size_t base, const valin_t *vals, std::size_t count,
                            strl_t setST, strl_t setREL, std::memory_order mo) noexcept;

        void debugPrint(std::size_t idx) const noexcept;

        bool IsLkFree() noexcept
        {
            return std::atomic<OUT>().IsLkFree();
        }
    
    private:
        std::size_t n_;
        std::atomic<out_t>* data_;
        uint8_t PREF_ALLIGNMENT_ = 64;
        using BP_ = BitPacker<VALBITS, STRLB, CLKB, OUT>;
    };

    using RelPArry8_t = PackedACarray<8, 4, 16, uint32_t>;
    using RelPArry32_t = PackedACarray<32, 8, 16, uint64_t>;
    
}