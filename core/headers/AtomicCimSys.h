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
#include <bit>        // std::bit_cast
#include <new>

#define BIT64 64
#define ODD 1u
#define EVEN 2u

#if defined(__GNUC__) || defined(__clang__)
# define RELCIM_LIKELY(x)   __builtin_expect(!!(x), 1)
# define RELCIM_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
# define RELCIM_LIKELY(x)   (x)
# define RELCIM_UNLIKELY(x) (x)
#endif

namespace AtomicCIMBits {

using metaU32_t = uint32_t;
using valueU64_t = uint64_t;
using storageBits_t = uint32_t;

// meta helpers
inline constexpr metaU32_t MetaPack(uint8_t st, uint8_t rel, uint16_t clk) noexcept
{
    return ( static_cast<uint32_t>(static_cast<uint32_t>(st) << 24)
           | static_cast<uint32_t>(static_cast<uint32_t>(rel) << 16)
           | static_cast<uint32_t>(clk) );
}
inline constexpr uint8_t MUPst(metaU32_t meta) noexcept { return static_cast<uint8_t>((meta >> 24) & 0xffu); }
inline constexpr uint8_t MUPrel(metaU32_t meta) noexcept { return static_cast<uint8_t>((meta >> 16) & 0xffu); }
inline constexpr uint16_t MUPclk(metaU32_t meta) noexcept { return static_cast<uint16_t>(meta & 0xffffu); }

// safe bitcast helper (C++20)
template<typename To, typename From>
inline To SafeBitCast(const From &f) noexcept {
    static_assert(sizeof(To) == sizeof(From), "SafeBitCast size mismatch");
    return std::bit_cast<To>(f);
}

// non-owning element view
struct ACBits_32t {
    std::atomic<valueU64_t>* Value64tPtr_;
    std::atomic<metaU32_t>* MetaPtr_;

    ACBits_32t() noexcept : Value64tPtr_(nullptr), MetaPtr_(nullptr) {}
    ACBits_32t(std::atomic<valueU64_t>* v, std::atomic<metaU32_t>* m) noexcept :
        Value64tPtr_(v), MetaPtr_(m) {}

    inline static valueU64_t PackV32Inv32(uint32_t vbits) noexcept {
        // make sure ~vbits is widened before shifting
        return ( (static_cast<valueU64_t>(~static_cast<uint32_t>(vbits)) << 32)
               | static_cast<valueU64_t>(static_cast<uint32_t>(vbits)) );
    }

    inline static uint32_t UnpackLow32(valueU64_t v64) noexcept {
        return static_cast<uint32_t>(v64 & 0xFFFFFFFFu);
    }
    inline static uint32_t UnpackHigh32(valueU64_t v64) noexcept {
        return static_cast<uint32_t>((v64 >> 32) & 0xFFFFFFFFu);
    }

    bool validate() const noexcept { return (Value64tPtr_ && MetaPtr_); }

    // read: triple-phase meta->value->meta, integrity inv check
    std::optional<uint32_t> ReadBits() const noexcept {
        if (!validate()) return std::nullopt;
        for (;;) {
            metaU32_t m1 = MetaPtr_->load(std::memory_order_acquire);
            valueU64_t v64 = Value64tPtr_->load(std::memory_order_acquire);
            metaU32_t m2 = MetaPtr_->load(std::memory_order_acquire);
            if (m1 != m2) continue;
            uint32_t low = UnpackLow32(v64);
            uint32_t high = UnpackHigh32(v64);
            if (RELCIM_UNLIKELY(high != static_cast<uint32_t>(~low))) {
                return std::nullopt;
            }
            return low;
        }
    }

    // per-element CAS-style write: mark pending via CAS on meta, write value64, commit meta
    void WriteCas(uint32_t new_bits, uint8_t new_st = 0, uint8_t new_rel = 0) noexcept {
        assert(validate());
        valueU64_t newv = PackV32Inv32(new_bits);
        for (;;) {
            metaU32_t oldmeta = MetaPtr_->load(std::memory_order_acquire);
            uint16_t oldclk = MUPclk(oldmeta);
            uint16_t pend = static_cast<uint16_t>(oldclk + 1u); // odd = pending
            metaU32_t pending = MetaPack(new_st, new_rel, pend);
            // try to claim
            if (MetaPtr_->compare_exchange_strong(oldmeta, pending, std::memory_order_acq_rel, std::memory_order_acquire)) {
                // we own the element; now write data then commit
                Value64tPtr_->store(newv, std::memory_order_release);
                uint16_t comm = static_cast<uint16_t>(pend + 1u);
                metaU32_t final_meta = MetaPack(new_st, new_rel, comm);
                MetaPtr_->store(final_meta, std::memory_order_release);
                return;
            }
            // else CAS failed; retry
        }
    }

    // initialization store (non-atomic init)
    void InitStore(uint32_t bits, uint8_t st = 0, uint8_t rel = 0) noexcept {
        assert(validate());
        Value64tPtr_->store(PackV32Inv32(bits), std::memory_order_relaxed);
        MetaPtr_->store(MetaPack(st, rel, 0u), std::memory_order_release);
    }
};


// ACBArray32_t: owning container for arrays of value64/meta32
class ACBArray32_t {
private:
    size_t n_;
    std::atomic<valueU64_t>* Value64tPtrA_;
    std::atomic<metaU32_t>* MetaPtrA_;
public:
    ACBArray32_t() noexcept : n_(0), Value64tPtrA_(nullptr), MetaPtrA_(nullptr) {}
    ~ACBArray32_t() { FreeAll(); }
    ACBArray32_t(const ACBArray32_t&) = delete;
    ACBArray32_t& operator=(const ACBArray32_t&) = delete;

    void Init(size_t N) {
        FreeAll();
        if (N == 0) return;
        n_ = N;
        // allocate aligned arrays; new[] with alignment (C++17/20)
        Value64tPtrA_ = nullptr;
        MetaPtrA_ = nullptr;
        try {
            Value64tPtrA_ = new(std::align_val_t(64)) std::atomic<valueU64_t>[n_]();
            MetaPtrA_ = new(std::align_val_t(64)) std::atomic<metaU32_t>[n_]();
        } catch (...) {
            FreeAll();
            throw;
        }
    }

    void FreeAll() noexcept {
        if (Value64tPtrA_) {
            delete[] Value64tPtrA_;
            Value64tPtrA_ = nullptr;
        }
        if (MetaPtrA_) {
            delete[] MetaPtrA_;
            MetaPtrA_ = nullptr;
        }
        n_ = 0;
    }

    size_t Size() const noexcept { return n_; }

    // non-owning view at index
    ACBits_32t ACBArrayview_at(size_t i) noexcept {
        assert(i < n_);
        return ACBits_32t(&Value64tPtrA_[i], &MetaPtrA_[i]);
    }

    // strongly-typed readers/writers (templates defined inline)
    template<typename T>
    std::optional<T> Read_t(size_t i) const noexcept {
        static_assert(std::is_same_v<T, uint32_t> || std::is_same_v<T, float>,
                      "T must be uint32_t or float");
        if (i >= n_) return std::nullopt;
        ACBits_32t view(const_cast<std::atomic<valueU64_t>*>(&Value64tPtrA_[i]),
                        const_cast<std::atomic<metaU32_t>*>(&MetaPtrA_[i]));
        auto opt = view.ReadBits();
        if (!opt.has_value()) return std::nullopt;
        if constexpr (std::is_same_v<T, uint32_t>) {
            return opt.value();
        } else { // float
            return SafeBitCast<T>(opt.value());
        }
    }

    template<typename T>
    void WriteCAS(size_t i, T val, uint8_t st = 0, uint8_t rel = 0) noexcept {
        static_assert(std::is_same_v<T, uint32_t> || std::is_same_v<T, float>,
                      "T must be uint32_t or float");
        assert(i < n_);
        uint32_t bits = SafeBitCast<uint32_t>(val);
        ACBits_32t view(&Value64tPtrA_[i], &MetaPtrA_[i]);
        view.WriteCas(bits, st, rel);
    }

    // bulk commit: writer must ensure exclusive access for the region or accept that this is the committer path
    void CommitBlock(size_t base, const storageBits_t *vals, size_t count, uint8_t st = 0, uint8_t rel = 0) noexcept {
        assert(base + count <= n_);
        // write values first (relaxed)
        for (size_t i = 0; i < count; ++i) {
            Value64tPtrA_[base + i].store(ACBits_32t::PackV32Inv32(vals[i]), std::memory_order_relaxed);
        }
        // barrier to ensure data visible before metas
        std::atomic_thread_fence(std::memory_order_release);
        // commit metas
        for (size_t i = 0; i < count; ++i) {
            metaU32_t oldm = MetaPtrA_[base + i].load(std::memory_order_relaxed);
            uint16_t oldclk = MUPclk(oldm);
            uint16_t newclk = static_cast<uint16_t>(oldclk + EVEN);
            metaU32_t newm = MetaPack(st, rel, newclk);
            MetaPtrA_[base + i].store(newm, std::memory_order_release);
        }
    }

    void SetInit(size_t i, uint32_t bits, uint8_t st = 0, uint8_t rel = 0) noexcept {
        assert(i < n_);
        Value64tPtrA_[i].store(ACBits_32t::PackV32Inv32(bits), std::memory_order_relaxed);
        MetaPtrA_[i].store(MetaPack(st, rel, 0u), std::memory_order_relaxed);
    }

    void DebugPrint(size_t idx, std::ostream &os = std::cout) const {
        assert(idx < n_);
        valueU64_t v = Value64tPtrA_[idx].load(std::memory_order_acquire);
        uint32_t low = ACBits_32t::UnpackLow32(v);
        uint32_t high = ACBits_32t::UnpackHigh32(v);
        metaU32_t m = MetaPtrA_[idx].load(std::memory_order_acquire);
        os << "idx=" << idx << " val=0x" << std::hex << low << " inv=0x" << high
           << " st=0x" << static_cast<int>(MUPst(m))
           << " rel=0x" << static_cast<int>(MUPrel(m))
           << " clk=0x" << MUPclk(m) << std::dec << "\n";
    }
};

} // namespace AtomicCIMBits
