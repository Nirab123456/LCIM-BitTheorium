#include "AtomicCSCompact.h"

namespace AtomicCSCompact
{
    template<typename T>
    void InitAnyView(std::atomic<T>*& dataptr, size_t N, size_t& cn)
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

    //bit packer
    template <
        size_t VALIB,
        size_t STRLB,
        size_t CLKB,
        typename OUT = uint64_t
    >
    struct BitPacker{
        static_assert(std::is_unsigned_v<OUT>, "OUT must be an unsigned integral");
        static constexpr size_t TotatBytes = VALIB + VALIB + STRLB + STRLB + CLKB;
        static_assert(TotatBytes <= sizeof(OUT)*8, "Total bits/bytes are greater than return size");

        static constexpr OUT ValIMask = (VALIB == 0 ? OUT(0) : ((OUT(1) << VALIB) - OUT(1)));
        static constexpr OUT STRLMask = (STRLB == 0 ? OUT(0) : ((OUT(1) << STRLB) - OUT(1) ));
        static constexpr OUT CLKMask = (CLKB == 0 ? OUT(0) : ((OUT(1) << CLKB) - OUT(1) ));

        template<typename VIt, typename STRLt, typename CLKt>
        static constexpr OUT PackAnyT(VIt Value, VIt InverseValue, STRLt State, STRLt Relation, CLKt Clock)
        {
            OUT v = static_cast<OUT>(Value) & ValIMask;
            OUT iv = static_cast<OUT>(InverseValue) & ValIMask;
            OUT st = static_cast<OUT>(State) & STRLMask;
            OUT rel = static_cast<OUT>(Relation) & STRLMask;
            OUT clk = static_cast<OUT>(Clock) & CLKMask;

            OUT outPAT = 0;
            outPAT |= clk;
            outPAT <<= (STRLB + STRLB) + (VALIB + VALIB);
            outPAT |= (st << (STRLB + (VALIB + VALIB)));
            outPAT |= (rel << (VALIB + VALIB));
            outPAT |= (iv << VALIB);
            outPAT |= v;
            return outPAT;
        }

        template<typename VIt, typename STRLt, typename CLKt>
        static constexpr void UnpackAnyT(OUT packed, VIt& value, VIt& inv, STRLt& st, STRLt& rel, CLKt& clk) noexcept
        {

            OUT cursor = packed;

            value = static_cast<VIt>(cursor & ValIMask);
            cursor >>= VALIB;

            inv = static_cast<VIt>(cursor & ValIMask);
            cursor >>= VALIB;

            rel = static_cast<STRLt>(cursor & STRLMask);
            cursor >>= STRLB;

            st = static_cast<STRLt>(cursor & STRLMask);
            cursor >>= STRLB;
            
            clk = static_cast<CLKt>(cursor & CLKMask);
            
        }
    };


    template <
        size_t VALIB,
        size_t STRLB,
        size_t CLKB,
        typename OUT = uint64_t
    >
    struct ARFieldView
    {
        using valin_t = std::conditional_t<VALIB <= 8, uint8_t,
            std::conditional_t<VALIB <= 16, uint16_t, uint32_t>>;
        using strl_t = std::conditional_t<STRLB <= 8, uint8_t>>;        
        using clk_t = std::conditional_t<CLKB <=8, uint8_t,
            std::consitional_t<CLKB <= 16, uint16_t, uint32_t>>;
        valin_t value;
        valin_t inv;
        strl_t st;
        strl_t rel;
        clk_t clk;
    };



    struct uint4_tt
    {
        uint8_t v;
        uint4_tt() : 
            v(0)
        {}
        explicit constexpr uint4_tt(uint8_t x) :
            v(uint8_t(x & 0x0fu))
        {}
        constexpr operator uint8_t() const
        {
            return (v & 0x0fu);
        }
        constexpr uint4_tt& operator = (uint8_t x)
        {
            v = uint8_t(x & 0x0fu);
            return *this;
        }
    };
    
    void AssertAtomicLockfree()
    {
        static bool checked = false;
        if (checked) return;
        checked = true;
        static_assert(sizeof(void*) >= 4,"pointer size assumption");
    }
    

    //packed array
    template <
        size_t VALIB,
        size_t STRLB,
        size_t CLKB,
        typename OUT = uint64_t
    >
    class PackedACArray 
    {

    public:

        
    std::optional<ARFieldView> 
        Read(size_t idx, std::memory_order order = std::memory_order_acquire) const noexcept
        {
            ARFieldView<VALIB, STRLB, CLKB, OUT> innerview{};
            if (idx >= n_)
            {
                return std::nullopt;
            }

            OUT raw = data_[idx].load(order);

            ARFieldView arfv;

            PackDevil.UnpackAnyT(raw, arfv.value, arfv.inv, arfv.st, arfv.rel, arfv.clk);

            OUT expectedINV = (PackDevil.ValIMask & (~(OUT(arfv.value))));
            if (OUT(arfv.inv) != expectedINV)
            {
                return std::nullopt;
            }
            return arfv;
        }

        bool WriteCas(size_t idx, valin_t inva, 
            std::optional<strl_t> setST = {},
            std::optional<strl_t> setREL = {},
            std::memory_order CASOrder = std:: memory_order_acq_rel
        ) noexcept
        {
            static_assert(std::is_integral_v<valin_t>, "valin_t integral");
            if (idx >=n_)
            {
                return false;
            }

            OUT ValMasked = OUT(inva) & PackDevil.ValIMask;
            OUT InvMasked = (OUT(~OUT(inva)) & PackDevil.ValIMask);
            while (true)
            {
                OUT old = data_[idx].load(std::memory_order_acquire);
                valin_t oldv;
                valin_t oldinv;
                strl_t oldst;
                strl_t oldrel;
                clk_t oldclk;
                PackDevil.UnpackAnyT(old, oldv, oldinv, oldst, oldrel, oldclk);

                clk_t pend = clk_t(oldclk + 1);
                strl_t stv = setST.has
            }
            
        }

    };







}