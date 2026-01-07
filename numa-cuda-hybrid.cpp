#pragma once

/*
 * AtomicCIM_PackedAoS.hpp
 *
 * Revised to follow the "single authoritative 64-bit word" (AoS) design and
 * the user's constraints:
 *  - Requires C++20 (std::atomic::wait / notify) — no fallbacks.
 *  - Requires libnuma on Linux (HAVE_LIBNUMA) or Windows NUMA APIs (VirtualAllocExNuma).
 *  - Uses a single std::atomic<packed_t> array as the absolute truth; CPU and GPU
 *    operate on the same pointer/address. All pack/unpack happens via PackedCell.
 *
 * Notes:
 *  - This header intentionally rejects builds that do not provide C++20
 *    atomic wait/notify or a NUMA allocation API.
 *  - State tag encodings are project conventions and must be mirrored on
 *    any GPU/consumer code.
 */

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <vector>
#include <functional>
#include <algorithm>

#if defined(__x86_64__) || defined(_M_X64)
  #include <immintrin.h>
#endif

// Require C++20 std::atomic wait/notify support
#if not defined(__cpp_lib_atomic_wait)
  #error "This header requires C++20 std::atomic::wait / notify support (no fallbacks)."
#endif

// Require NUMA API availability (user promised libnuma on Linux or Windows NUMA API)
#if !defined(HAVE_LIBNUMA) && !defined(_WIN32)
  #error "This header requires either HAVE_LIBNUMA (Linux) or Windows NUMA (VirtualAllocExNuma)."
#endif

#if defined(HAVE_LIBNUMA)
  #include <numa.h>
  #include <numaif.h>
  #include <unistd.h>
#elif defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  // VirtualAllocExNuma is available on modern Windows SDKs.
#endif

namespace atomiccim {

using packed_t = uint64_t;
using val32_t  = uint32_t;
using clk16_t  = uint16_t;
using clk48_t  = uint64_t;
using tag8_t   = uint8_t;

static inline constexpr packed_t mask_bits(unsigned n) noexcept {
    return (n >= 64) ? ~packed_t(0) : ((packed_t(1) << n) - packed_t(1));
}

struct PackedCellValue32 {
    static constexpr unsigned VALBITS = 32;
    static constexpr unsigned CLKBITS = 16;
    static constexpr unsigned STBITS  = 8;
    static constexpr unsigned RELBITS = 8;
    static_assert((VALBITS + CLKBITS + STBITS + RELBITS) == 64, "layout must be 64");

    static inline packed_t pack(val32_t value, clk16_t clk, tag8_t st, tag8_t rel) noexcept {
        packed_t p = packed_t(value) & mask_bits(VALBITS);
        p |= (packed_t(clk)  & mask_bits(CLKBITS)) << VALBITS;
        p |= (packed_t(st)   & mask_bits(STBITS))  << (VALBITS + CLKBITS);
        p |= (packed_t(rel)  & mask_bits(RELBITS)) << (VALBITS + CLKBITS + STBITS);
        return p;
    }

    static inline val32_t unpack_value(packed_t p) noexcept { return val32_t(p & mask_bits(VALBITS)); }
    static inline clk16_t unpack_clk(packed_t p) noexcept  { return clk16_t((p >> VALBITS) & mask_bits(CLKBITS)); }
    static inline tag8_t  unpack_st(packed_t p) noexcept   { return tag8_t((p >> (VALBITS + CLKBITS)) & mask_bits(STBITS)); }
    static inline tag8_t  unpack_rel(packed_t p) noexcept  { return tag8_t((p >> (VALBITS + CLKBITS + STBITS)) & mask_bits(RELBITS)); }
};

struct PackedCellClk48 {
    static constexpr unsigned CLKBITS = 48;
    static constexpr unsigned STBITS  = 8;
    static constexpr unsigned RELBITS = 8;
    static_assert((CLKBITS + STBITS + RELBITS) == 64, "layout must be 64");

    static inline packed_t pack(clk48_t clk, tag8_t st, tag8_t rel) noexcept {
        packed_t p = (packed_t)(clk & mask_bits(CLKBITS));
        p |= (packed_t(st)  & mask_bits(STBITS))  << CLKBITS;
        p |= (packed_t(rel) & mask_bits(RELBITS)) << (CLKBITS + STBITS);
        return p;
    }

    static inline clk48_t  unpack_clk(packed_t p) noexcept { return clk48_t(p & mask_bits(CLKBITS)); }
    static inline tag8_t   unpack_st(packed_t p) noexcept  { return tag8_t((p >> CLKBITS) & mask_bits(STBITS)); }
    static inline tag8_t   unpack_rel(packed_t p) noexcept { return tag8_t((p >> (CLKBITS + STBITS)) & mask_bits(RELBITS)); }
};

// State encodings (project convention). Keep them explicit and stable.
namespace PackedState {
    static constexpr tag8_t IDLE      = 0x00;
    static constexpr tag8_t PUBLISHED = 0x01; // CPU wrote and rang doorbell
    static constexpr tag8_t CLAIMED   = 0x02; // GPU claimed via CAS
    static constexpr tag8_t PROCESSING= 0x03; // being processed
    static constexpr tag8_t COMPLETE  = 0x04; // GPU finished, result in value
    static constexpr tag8_t RETIRED   = 0x05; // CPU consumed result
}

// PackedCell facade — canonical pack/unpack helpers. All access must go through these.
struct PackedCell {
    static inline packed_t compose_value32(val32_t v, clk16_t c, tag8_t st, tag8_t rel) noexcept {
        return PackedCellValue32::pack(v,c,st,rel);
    }
    static inline packed_t compose_clk48(clk48_t c, tag8_t st, tag8_t rel) noexcept {
        return PackedCellClk48::pack(c,st,rel);
    }

    static inline val32_t extract_value32(packed_t p) noexcept { return PackedCellValue32::unpack_value(p); }
    static inline clk16_t extract_clk16(packed_t p) noexcept  { return PackedCellValue32::unpack_clk(p); }
    static inline clk48_t extract_clk48(packed_t p) noexcept  { return PackedCellClk48::unpack_clk(p); }
    static inline tag8_t  extract_st_value32(packed_t p) noexcept { return PackedCellValue32::unpack_st(p); }
    static inline tag8_t  extract_rel_value32(packed_t p) noexcept { return PackedCellValue32::unpack_rel(p); }
    static inline tag8_t  extract_st_clk48(packed_t p) noexcept  { return PackedCellClk48::unpack_st(p); }
    static inline tag8_t  extract_rel_clk48(packed_t p) noexcept { return PackedCellClk48::unpack_rel(p); }
};

// NUMA-aware allocation helpers. These require libnuma on Linux (HAVE_LIBNUMA)
// or Windows VirtualAllocExNuma on Windows. No fallbacks — user guarantees availability.
namespace alloc {

inline std::size_t page_size() {
#if defined(_WIN32)
    SYSTEM_INFO si; GetSystemInfo(&si); return static_cast<std::size_t>(si.dwPageSize);
#else
    long ps = sysconf(_SC_PAGESIZE);
    return (ps > 0) ? static_cast<std::size_t>(ps) : 4096u;
#endif
}

#if defined(HAVE_LIBNUMA)
inline void* aligned_alloc_onnode(std::size_t alignment, std::size_t size, int node) {
    if (alignment == 0) alignment = page_size();
    if (numa_available() < 0) throw std::runtime_error("libnuma: numa_available() < 0");
    if (node < 0 || node > numa_max_node()) throw std::invalid_argument("invalid numa node");
    std::size_t ps = page_size();
    std::size_t rounded = ((size + ps - 1) / ps) * ps;
    void* p = numa_alloc_onnode(rounded, node);
    if (!p) throw std::bad_alloc();
    return p;
}

inline void aligned_free_onnode(void* p, std::size_t size) noexcept {
    if (!p) return;
    std::size_t ps = page_size();
    std::size_t rounded = ((size + ps - 1) / ps) * ps;
    numa_free(p, rounded);
}

#elif defined(_WIN32)
inline void* aligned_alloc_onnode(std::size_t /*alignment*/, std::size_t size, int node) {
    // VirtualAllocExNuma is page-granular and will commit pages on the preferred node
    std::size_t ps = page_size();
    std::size_t rounded = ((size + ps - 1) / ps) * ps;
    HANDLE hProc = GetCurrentProcess();
    DWORD allocType = MEM_RESERVE | MEM_COMMIT;
    DWORD protect = PAGE_READWRITE;
    LPVOID result = VirtualAllocExNuma(hProc, nullptr, rounded, allocType, protect, static_cast<DWORD>(node));
    if (!result) throw std::bad_alloc();
    return result;
}

inline void aligned_free_onnode(void* p, std::size_t /*size*/) noexcept {
    if (!p) return; VirtualFree(p, 0, MEM_RELEASE);
}
#endif

} // namespace alloc

// AtomicPackedArray: authoritative AoS 64-bit array. Designed for CPU+GPU sharing.
class AtomicPackedArray {
public:
    AtomicPackedArray() : n_(0), meta_(nullptr) {}
    ~AtomicPackedArray() { free_all(); }

    // init: allocate single atomic array of packed_t pinned to numa_node.
    // The caller must ensure node is valid on the platform.
    void init(size_t n, int numa_node = 0) {
        free_all();
        if (n == 0) throw std::invalid_argument("n==0");
        n_ = n;
        size_t mbytes = sizeof(std::atomic<packed_t>) * n_;
        // allocate page-granular memory on requested node
        meta_ = reinterpret_cast<std::atomic<packed_t>*>(alloc::aligned_alloc_onnode(0, mbytes, numa_node));
        // Initialize to IDLE/zero and construct atomics in-place
        std::memset(reinterpret_cast<void*>(meta_), 0, mbytes);
        for (size_t i = 0; i < n_; ++i) std::atomic_init(&meta_[i], packed_t(0));
    }

    void free_all() noexcept {
        if (meta_) {
            // call destructors
            for (size_t i = 0; i < n_; ++i) meta_[i].~atomic();
            // free via NUMA-aware free
            alloc::aligned_free_onnode(reinterpret_cast<void*>(meta_), sizeof(std::atomic<packed_t>) * n_);
            meta_ = nullptr;
        }
        n_ = 0;
        epoch_table_.clear();
        region_dirty_.clear();
        region_locks_.clear();
    }

    size_t size() const noexcept { return n_; }

    // CPU: atomic load of packed word
    packed_t load_packed(size_t idx, std::memory_order mo = std::memory_order_acquire) const noexcept {
        if (idx >= n_) return packed_t(0);
        return meta_[idx].load(mo);
    }

    // CPU: atomic store publish (release semantics)
    void store_packed(size_t idx, packed_t p, std::memory_order mo = std::memory_order_release) noexcept {
        if (idx >= n_) return;
        meta_[idx].store(p, mo);
        // notify any waiters on this atomic using free-function notify
        std::atomic_notify_one(&meta_[idx]);
    }

    // CPU convenience: reserve by CAS old->pending; returns true on success and sets observed to previous value
    bool reserve_by_cas(size_t idx, packed_t expected_old, uint16_t batch_low, tag8_t rel_hint, packed_t &observed_out) noexcept {
        if (idx >= n_) return false;
        val32_t curv = PackedCell::extract_value32(expected_old);
        packed_t pending = PackedCell::compose_value32(curv, static_cast<clk16_t>(batch_low), PackedState::PUBLISHED, rel_hint);
        packed_t observed = expected_old;
        bool ok = meta_[idx].compare_exchange_strong(observed, pending, std::memory_order_acq_rel, std::memory_order_relaxed);
        observed_out = observed;
        if (ok) std::atomic_notify_one(&meta_[idx]);
        return ok;
    }

    // CPU commit: CAS pending->committed (committed_packed must be fully composed). Returns true on CAS success.
    bool commit_update(size_t idx, packed_t expected_pending, packed_t committed_packed, bool notify = true) noexcept {
        if (idx >= n_) return false;
        packed_t observed = expected_pending;
        bool ok = meta_[idx].compare_exchange_strong(observed, committed_packed, std::memory_order_acq_rel, std::memory_order_relaxed);
        if (!ok) {
            // unconditional store to ensure forward progress
            meta_[idx].store(committed_packed, std::memory_order_release);
        }
        if (notify) std::atomic_notify_one(&meta_[idx]);
        return ok;
    }

    // Consumer (GPU-like) helper: attempt to claim a PUBLISHED cell. Returns true if this thread claimed it.
    bool try_claim_published(size_t idx, packed_t expected_published, packed_t desired_claimed) noexcept {
        if (idx >= n_) return false;
        packed_t observed = expected_published;
        bool ok = meta_[idx].compare_exchange_strong(observed, desired_claimed, std::memory_order_acq_rel, std::memory_order_relaxed);
        if (ok) std::atomic_notify_one(&meta_[idx]);
        return ok;
    }

    // Consumer marks COMPLETE by atomic store (release). This sets state to COMPLETE and updates value/clk as necessary.
    void mark_complete(size_t idx, packed_t complete_packed) noexcept {
        if (idx >= n_) return;
        meta_[idx].store(complete_packed, std::memory_order_release);
        std::atomic_notify_one(&meta_[idx]);
    }

    // Consumer may use atomic_exchange to hand back a cell (example for RETIRE semantics)
    packed_t atomic_exchange(size_t idx, packed_t newval) noexcept {
        if (idx >= n_) return packed_t(0);
        packed_t prev = meta_[idx].exchange(newval, std::memory_order_acq_rel);
        std::atomic_notify_one(&meta_[idx]);
        return prev;
    }

    // wait_for_change: wait until meta_[idx] != expected or timeout (ms). Returns true if change detected.
    bool wait_for_change(size_t idx, packed_t expected, int timeout_ms = 100) noexcept {
        if (idx >= n_) return false;
        if (timeout_ms < 0) {
            // use C++20 free-function atomic_wait (blocks until notified and value changes)
            std::atomic_wait(&meta_[idx], expected);
            return true;
        }
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            packed_t cur = meta_[idx].load(std::memory_order_acquire);
            if (cur != expected) return true;
            // block until change/notification (or spurious wakeup), then re-check
            std::atomic_wait(&meta_[idx], expected);
        }
        return false;
    }

    // Scan for ranges with a matching rel tag. Works purely from atomic words.
    std::vector<std::pair<size_t,size_t>> scan_rel_ranges(tag8_t rel_tag) const noexcept {
        std::vector<std::pair<size_t,size_t>> ranges;
        if (n_ == 0) return ranges;
        size_t i = 0;
        while (i < n_) {
            packed_t p = meta_[i].load(std::memory_order_acquire);
            tag8_t r = PackedCell::extract_rel_value32(p);
            if (r != rel_tag) { ++i; continue; }
            size_t start = i; ++i;
            while (i < n_) {
                p = meta_[i].load(std::memory_order_acquire);
                if (PackedCell::extract_rel_value32(p) != rel_tag) break;
                ++i;
            }
            ranges.emplace_back(start, i - start);
        }
        return ranges;
    }

    // Epoch support and helpers (effective TS extracted from packed word)
    using epoch_t = uint64_t;
    void init_epoch(size_t region_size) {
        if (region_size == 0) throw std::invalid_argument("region_size==0");
        if (n_ == 0) throw std::runtime_error("array not initialized");
        epoch_region_size_ = region_size;
        num_regions_ = (n_ + region_size - 1) / region_size;
        epoch_table_.assign(num_regions_, epoch_t(0));
        region_dirty_.assign(num_regions_, false);
        // region_locks_ contains atomics; default-initialize the vector and set values explicitly
        region_locks_.clear();
        region_locks_.resize(num_regions_);
        for (size_t i = 0; i < num_regions_; ++i) region_locks_[i].store(uint8_t(0), std::memory_order_relaxed);
    }

    size_t region_of(size_t idx) const noexcept { return epoch_region_size_ ? (idx / epoch_region_size_) : 0; }

    uint64_t read_effective_ts(size_t idx, std::memory_order mo = std::memory_order_acquire) const noexcept {
        if (idx >= n_) return 0;
        packed_t p = meta_[idx].load(mo);
        clk16_t c = PackedCell::extract_clk16(p);
        epoch_t e = epoch_table_.empty() ? epoch_t(0) : epoch_table_[region_of(idx)];
        return (e << 16) | uint64_t(c);
    }

    bool region_epoch_bump_lazy(size_t region_idx) noexcept {
        if (region_idx >= num_regions_) return false;
        uint8_t expected = 0;
        if (!region_locks_[region_idx].compare_exchange_strong(expected, uint8_t(1), std::memory_order_acq_rel)) return false;
        epoch_table_[region_idx]++;
        region_dirty_[region_idx] = true;
        region_locks_[region_idx].store(0, std::memory_order_release);
        if (epoch_bump_cb_) epoch_bump_cb_(region_idx, epoch_table_[region_idx]);
        return true;
    }

    bool region_is_dirty(size_t region_idx) const noexcept { return (region_idx < region_dirty_.size()) ? region_dirty_[region_idx] : false; }
    void clear_region_dirty(size_t region_idx) noexcept { if (region_idx < region_dirty_.size()) region_dirty_[region_idx] = false; }
    void set_epoch_bump_callback(std::function<void(size_t, epoch_t)> cb) { epoch_bump_cb_ = std::move(cb); }

    // Expose the raw pointer for GPU kernels to map the same address.
    std::atomic<packed_t>* raw_atomic_ptr() noexcept { return meta_; }

private:
    size_t n_;
    std::atomic<packed_t>* meta_{nullptr};

    // epoch support
    size_t epoch_region_size_{0};
    size_t num_regions_{0};
    std::vector<epoch_t> epoch_table_;
    std::vector<bool> region_dirty_;
    std::vector<std::atomic<uint8_t>> region_locks_;
    std::function<void(size_t, epoch_t)> epoch_bump_cb_;
};

} // namespace atomiccim
