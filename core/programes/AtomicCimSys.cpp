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
            uint16_t oldclk = MUPclk(oldmeta);
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

        try
        {
            Value64tPtrA_ = new(std::align_val_t(BIT64)) std::atomic<valueU64_t>[n_]();
            MetaPtrA_ = new(std::align_val_t(BIT64)) std::atomic<metaU32_t>[n_]();
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            FreeAll();
            throw;
        }
    }


    void ACBArray32_t::FreeAll()
    {
        if (Value64tPtrA_)
        {
            delete[] Value64tPtrA_;
            Value64tPtrA_ = nullptr;
        }
        if (MetaPtrA_)
        {
            delete[] MetaPtrA_;
            MetaPtrA_ = nullptr;
        }
    }

    template<typename T> std::optional<T>ACBArray32_t::Read_t(size_t i) const noexcept
    {
        static_assert(std::is_same_v<T,uint32_t> || std::is_same_v<T,float>, "T must be uint32_t or float");
        assert(i < n_);
        const cast<ACBArray32_t*>(this)->ACBArrayview_at(i);
        ACBits_32t v(const_cast<std::atomic<valueU64_t>*>(&Value64tPtrA_[i]), const_cast<std::atomic<metaU32_t>*>(&meta[i]));
        auto opt = v.ReadBits();
        if (!opt.has_value())
        {
            return std::nullopt;
        }
        if constexpr(std::is_same_v<T, uint32_t>)
        {
            return opt.value;
        }
        else
        {
            return BitCastFrom<T>(opt.value);
        }
    }
    
    template<typename T>
    void ACBArray32_t::WriteCAS(size_t i, T val, uint8_t st, uint8_t rel) noexcept
    {
        static_assert(std::is_same_v<T, uint32_t> || std::is_same_v<T, float, "T must be uint32_t or float");
        assert(i < n_);
        storageBits_t stb = BitCastFrom<storageBits_t>(val);
        ACBits_32t(&Value64tPtrA_[i], &MetaPtrA_[i]).WriteCas(stb, st, rel);
    }

    void ACBArray32_t::CommitBlock(size_t base, const storageBits_t *vals, size_t count, uint8_t st, uint8_t rel)
    {
        assert(base + count <= n_);
        for (size_t i = 0; i < count; i++)
        {

            Value64tPtrA_[base + i].store(ACBits_32t::PackV32Inv32(vals[i]), std::memory_order_relaxed);
        }
        std::atomic_thread_fence(std::memory_order_release);
        for (size_t i = 0; i < count; i++)
        {
            metaU32_t oldm = MetaPtrA_[base + i].load(std::memory_order_relaxed);
            uint16_t oldclk = MUPclk(oldm);
            metaU32_t newmeta = MetaPack(st, rel, static_cast<uint16_t>(oldclk +EVEN)); 
            MetaPtrA_[base + i].store(newmeta, std::memory_order_release);
        }
    }
    void ACBArray32_t::SetInit(size_t i, uint32_t bits, uint8_t st = 0, uint8_t rel = 0) noexcept
    {
        assert(i < n_);
        Value64tPtrA_[i].store(ACBits_32t::PackV32Inv32(bits), std::memory_order_relaxed);
        MetaPtrA_[i].store(MetaPack(st, rel, 0u), std::memory_order_relaxed);
    }

    void ACBArray32_t::DebugPrint(size_t idx, std::ostream &os = std::cout) const
    {
        assert(idx < n_);
        valueU64_t v = Value64tPtrA_[idx].load(std::memory_order_acquire);
        uint8_t low = ACBits_32t::UnpackLow32(v);
        uint8_t high = ACBits_32t::UnpackHigh32(v);
        metaU32_t m = MetaPtrA_[idx].load(std::memory_order_acquire);
        os  << "idx = " << idx << " val = 0x" << std::hex << low << " inverse = 0x" << high
            << " st = 0x" <<(int)MUPst(m) << " rel = 0x" << (int)MUPrel(m)
            << " clk = 0x" << MUPclk(m) << std::dec << "\n";
    }
    

}
