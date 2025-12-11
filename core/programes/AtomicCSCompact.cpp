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
    
}