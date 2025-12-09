#include "AtomicCimSys.h"


namespace AtomicCIMBits{

    std::optional<uint32_t>ACBits_32t::ReadBits() const noexcept
    {
        if (!validate())
        {
            return std::nullopt;
        }
        while (true)
        {
            metaU32_t m1 = MetaPtr_->load(std::memory_order_acquire);
            valueU64_t v64 = Value64tPtr_->load(std::memory_order_acquire);
            metaU32_t m2 = MetaPtr_->load(std::memory_order_acquire);
            if (m1 != m2)
            {
                continue;
            }
            uint32_t low = UnpackLow32(v64);
            uint32_t high = UnpackHigh32(v64);
            if (RELCIM_UNLIKELY(high != static_cast<uint32_t>(~low)))
            {
                return std::nullopt;
            }
            return low;
        }
        
    }

    void ACBits_32t::WriteCas(uint32_t new_bits, uint8_t new_st, uint8_t new_rel)
    {
        assert(validate());
        valueU64_t newv = PackV32Inv32(new_bits);
        while (true)
        {
            metaU32_t oldmeta = MetaPtr_->load(std::memory_order_acquire);
            uint16_t oldclk = MPUclk(oldmeta);
            //increment to odd-> pending
            uint16_t pend = static_cast<uint16_t>(oldclk + 1u);
            //update meata
            metaU32_t pending = MetaPack(new_st, new_rel, pend);
            if (MetaPtr_->compare_exchange_strong(oldmeta, pending, std::memory_order_acq_rel, std::memory_order_acquire))
            {
                Value64tPtr_->store(newv, std::memory_order_release);
                uint16_t comm = static_cast<uint16_t>(pend + 1u);
                metaU32_t final_meta = MetaPack(new_st, new_rel, comm);
                MetaPtr_->store(final_meta, std::memory_order_release);
                return;
            }
            //else retry
        }
        
    }

    void ACBits_32t::InitStore(uint32_t bits, uint8_t st = 0, uint8_t rel = 0) noexcept
    {
        assert(validate());
        Value64tPtr_->store(PackV32Inv32(bits), std::memory_order_relaxed);
        MetaPtr_->store(MetaPack(st, rel, 0u), std::memory_order_release);
    }


    void ACBArray32_t::Init(size_t N)
    {
        FreeAll();
        if (N == 0)
        {
            return;
        }
        n_ = N;

        Value64tPtrA_ = reinterpret_cast<std::atomic<valueU64_t>*>(
            ::operator new[](sizeof(std::atomic<valueU64_t>)*n_, std::align_val_t(64))
        );
        MetaPtrA_ = reinterpret_cast<std::atomic<metaU32_t>*>(
            ::operator new[](sizeof(std::atomic<metaU32_t>)* n_, std::align_val_t(64))
        );

        if (!Value64tPtrA_ || !MetaPtrA_)
        {
            free(Value64tPtrA_);
            free(MetaPtrA_);
            throw std::bad_alloc();
        }
    }

}
