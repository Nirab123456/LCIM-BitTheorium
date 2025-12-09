#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <cassert>
#include <type_traits>
#include <iostream>
#include <memory>
#include <optional>
#include <new>


//unsure
#if defined(__GNUC__) || defined(__clang__)
# define RELCIM_LIKELY(x)   __builtin_expect(!!(x), 1)
# define RELCIM_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
# define RELCIM_LIKELY(x)   (x)
# define RELCIM_UNLIKELY(x) (x)
#endif
//

namespace AtomicCIMBits{
    using metaU32_t = uint32_t;
    using valueU64_t = uint64_t;
    using storageBits_t = uint32_t;

    //meta helpers
    //packing->meta(all)
    inline constexpr metaU32_t MetaPack(uint8_t st, uint8_t rel, uint16_t clk) noexcept
    {
        return ((static_cast<uint32_t>(st << 24)) | (static_cast<uint32_t>(rel << 16)) | (static_cast<uint32_t>(clk)));
    }
    //unpack->st
    inline constexpr uint8_t MUPst(metaU32_t meta) noexcept
    {
        return static_cast<uint8_t>((meta >> 24) & 0xffu);
    }
    //unpack->rel
    inline constexpr uint8_t MPUrel(metaU32_t meta) noexcept
    {
        return static_cast<uint8_t>((meta >> 16) & 0xffu);
    }
    //unpack->clk
    inline constexpr uint16_t MPUclk(metaU32_t meta) noexcept
    {
        return static_cast<uint16_t>(meta & 0xffffu);
    }

    //portable bit caster
    template<typename to_, typename from_>
    inline to_ BitCastFrom(const from_ &src)
    {
        static_assert(sizeof(to_) == sizeof(from_), "Bit Cast size mitchmatch");
        to_ dst;
        std::memcpy(&dst, &src, sizeof(to_));
        retuen dst;
    } 

//non-owing view
struct ACBits_32t{
    std::atomic<valueU64_t>* Value64tPtr_;
    std::atomic<metaU32_t>* MetaPtr_;
    
    ACBits_32t() noexcept :
        Value64tPtr_(nullptr), MetaPtr_(nullptr)
    {}
    ACBits_32t(std::atomic<valueU64_t>*v, std::atomic<metaU32_t>*m) noexcept:
        Value64tPtr_(v), MetaPtr_(m)
    {}

    inline static valueU64_t PackV32Inv32(uint32_t vbits) noexcept
    {
        return (static_cast<valueU64_t>((~vbits) << 32) | static_cast<valueU64_t>(vbits));
    }
    //pura value ->low
    inline static uint32_t UnpackLow32(valueU64_t v64) noexcept
    {
        return (static_cast<uint32_t>(v64 & 0xffffffffu));
    }
    //inverse value->high
    inline static uint32_t UnpackHigh32(valueU64_t v64) noexcept
    {
        return (static_cast<uint32_t>((v64 >> 32) & 0xffffffffu));
    }

    bool validate() const noexcept 
    {
        return (Value64tPtr_ && MetaPtr_);   
    }
    //read
    std::optional<uint32_t> ReadBits() const noexcept;
    //write
    void WriteCas(uint32_t new_bits, uint8_t new_st = 0, uint8_t new_rel = 0) noexcept;
    
    void InitStore(uint32_t bits, uint8_t st = 0, uint8_t rel = 0) noexcept;
};


class ACBArray32_t
{
    private:
        size_t n_;
        std::atomic<valueU64_t>* Value64tPtrA_;
        std::atomic<metaU32_t>* MetaPtrA_;
    public:
        ACBArray32_t() noexcept :
            n_(0), Value64tPtrA_(nullptr), MetaPtrA_(nullptr)
        {}
        ~ACBArray32_t(){
            FreeAll();
        }
        ACBArray32_t(const ACBArray32_t&) = delete;
        ACBArray32_t& operator = (const ACBArray32_t) = delete;

        void Init(size_t N);
        void FreeAll() noexcept;
};



}