
//=== File: AtomicCIM/PackedCell.hpp ===
#pragma once

/*
 PackedCell.hpp

 Two packing modes (64-bit):
  - MODE_VALUE32 : [ value:32 | clk16:16 | st:8 | rel:8 ]
  - MODE_CLK48    : [ clk48:48  | st:8    | rel:8 ]

 LayoutKind:
  - LAYOUT_AOS : array of 64-bit packed atomics per element (classic).
  - LAYOUT_SOA : Structure-of-Arrays for values/clocks/tags and a small metadata atomic
                 used for single-word commits. This enables vectorized worker kernels.

 ST field reserved values:
   0x00 : ST_FREE
   0x01 : ST_COMMITTED
   0x02 : ST_PENDING
   0x03 : ST_EPOCH_BUMP
   0x04 : ST_LOCKED
   0xF0 - 0xFF : ST_USER_RESERVED

 Effective timestamp for VALUE32 mode:
   T_effective = ( epoch_table[region_of(idx)] << 16 ) | clk16
*/

#include <cstdint>
#include <type_traits>
#include <cassert>

namespace atomiccim {

enum class PackedMode : int { MODE_VALUE32 = 0, MODE_CLK48 = 1 };
enum class LayoutKind : int { LAYOUT_AOS = 0, LAYOUT_SOA = 1 };

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

struct PackedCell {
    PackedMode mode;
    PackedCell(PackedMode m = PackedMode::MODE_VALUE32) noexcept : mode(m) {}

    static inline packed_t compose_value32(val32_t v, clk16_t c, tag8_t st, tag8_t rel) noexcept {
        return PackedCellValue32::pack(v,c,st,rel);
    }
    static inline packed_t compose_clk48(clk48_t c, tag8_t st, tag8_t rel) noexcept {
        return PackedCellClk48::pack(c,st,rel);
    }

    static inline val32_t extract_value32(packed_t p) noexcept { return PackedCellValue32::unpack_value(p); }
    static inline clk16_t extract_clk16(packed_t p) noexcept  { return PackedCellValue32::unpack_clk(p); }
    static inline clk48_t extract_clk48(packed_t p) noexcept  { return PackedCellClk48::unpack_clk(p); }
    static inline tag8_t  extract_st(packed_t p, PackedMode m) noexcept {
        return (m==PackedMode::MODE_VALUE32) ? PackedCellValue32::unpack_st(p) : PackedCellClk48::unpack_st(p);
    }
    static inline tag8_t  extract_rel(packed_t p, PackedMode m) noexcept {
        return (m==PackedMode::MODE_VALUE32) ? PackedCellValue32::unpack_rel(p) : PackedCellClk48::unpack_rel(p);
    }
};

} // namespace atomiccim

//=== File: AtomicCIM/AtomicArray.hpp ===
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <vector>
#include <functional>
#include <algorithm>

#include "PackedCell.hpp"
#include "Alloc.hpp"
#if !defined(__cpp_lib_atomic_wait)
  #include "Wake.hpp"
#endif

#if defined(__x86_64__) || defined(_M_X64)
  #include <immintrin.h>
#endif

namespace atomiccim {

template<PackedMode MODE, LayoutKind LAYOUT>
class AtomicArray {
public:
    using packed_t = ::atomiccim::packed_t;
    using epoch_t  = uint64_t;

    AtomicArray() : n_(0), meta_(nullptr), values_(nullptr), clocks_(nullptr), tags_(nullptr),
                    epoch_region_size_(0), numa_nodes_(0) {}
    ~AtomicArray() { free_all(); }

    // init: allocate memory. Optionally pass numa_node (if libnuma available).
    void init(size_t n, size_t alignment = 64, int numa_node = -1) {
        free_all();
        n_ = n;
        // Two modes:
        //  - LAYOUT_AOS: meta_ is atomic<packed_t> array containing full packed word (single atomic per element)
        //  - LAYOUT_SOA: meta_ is atomic<uint64_t> (metadata commit word) and values_/clocks_/tags_ are separate arrays for vectorized updates
        if constexpr (LAYOUT == LayoutKind::LAYOUT_AOS) {
            size_t bytes = sizeof(std::atomic<packed_t>)*n_;
            if (numa_node >= 0) {
#if defined(HAVE_LIBNUMA)
                void* p = ::numa_alloc_onnode(bytes, numa_node);
                if (!p) throw std::bad_alloc();
                meta_ = reinterpret_cast<std::atomic<packed_t>*>(p);
#else
                (void)numa_node;
                meta_ = reinterpret_cast<std::atomic<packed_t>*>(alloc::aligned_alloc_portable(alignment, bytes));
#endif
            } else {
                meta_ = reinterpret_cast<std::atomic<packed_t>*>(alloc::aligned_alloc_portable(alignment, bytes));
            }
            for (size_t i = 0; i < n_; ++i) std::atomic_init(&meta_[i], packed_t(0));
        } else {
            // LAYOUT_SOA: allocate values, clocks, tags, plus meta_ (single atomic per element used as commit barrier)
            size_t vbytes = sizeof(val32_t) * n_;
            size_t cbytes = sizeof(clk16_t) * n_;
            size_t tbytes = sizeof(uint16_t) * n_; // st<<8 | rel
            size_t mbytes = sizeof(std::atomic<uint64_t>) * n_;
            if (numa_node >= 0) {
#if defined(HAVE_LIBNUMA)
                values_ = reinterpret_cast<val32_t*>(alloc::aligned_alloc_portable(64, vbytes)); // for portability we use aligned_alloc_portable
                clocks_ = reinterpret_cast<clk16_t*>(alloc::aligned_alloc_portable(64, cbytes));
                tags_   = reinterpret_cast<uint16_t*>(alloc::aligned_alloc_portable(64, tbytes));
                meta_   = reinterpret_cast<std::atomic<packed_t>*>(alloc::aligned_alloc_portable(64, mbytes));
#else
                (void)numa_node;
                values_ = reinterpret_cast<val32_t*>(alloc::aligned_alloc_portable(64, vbytes));
                clocks_ = reinterpret_cast<clk16_t*>(alloc::aligned_alloc_portable(64, cbytes));
                tags_   = reinterpret_cast<uint16_t*>(alloc::aligned_alloc_portable(64, tbytes));
                meta_   = reinterpret_cast<std::atomic<packed_t>*>(alloc::aligned_alloc_portable(64, mbytes));
#endif
            } else {
                values_ = reinterpret_cast<val32_t*>(alloc::aligned_alloc_portable(64, vbytes));
                clocks_ = reinterpret_cast<clk16_t*>(alloc::aligned_alloc_portable(64, cbytes));
                tags_   = reinterpret_cast<uint16_t*>(alloc::aligned_alloc_portable(64, tbytes));
                meta_   = reinterpret_cast<std::atomic<packed_t>*>(alloc::aligned_alloc_portable(64, mbytes));
            }
            // initialize
            std::memset(values_, 0, vbytes);
            std::memset(clocks_, 0, cbytes);
            std::memset(tags_, 0, tbytes);
            for (size_t i = 0; i < n_; ++i) std::atomic_init(&meta_[i], packed_t(0));
        }

        // epoch/wake init
        wake_ = std::make_unique<WakeFallback>();
    }

    void free_all() noexcept {
        if constexpr (LAYOUT == LayoutKind::LAYOUT_AOS) {
            if (!meta_) { n_ = 0; return; }
            for (size_t i = 0; i < n_; ++i) meta_[i].~atomic();
            alloc::aligned_free_portable(meta_);
            meta_ = nullptr;
        } else {
            if (!meta_) { n_ = 0; return; }
            for (size_t i = 0; i < n_; ++i) meta_[i].~atomic();
            alloc::aligned_free_portable(meta_);
            alloc::aligned_free_portable(values_);
            alloc::aligned_free_portable(clocks_);
            alloc::aligned_free_portable(tags_);
            meta_ = nullptr;
            values_ = nullptr;
            clocks_ = nullptr;
            tags_ = nullptr;
        }
        n_ = 0;
        epoch_region_size_ = 0;
        epoch_table_.clear();
        region_dirty_.clear();
        region_locks_.clear();
    }

    size_t size() const noexcept { return n_; }

    // epoch regions
    void init_epoch(size_t region_size) {
        if (region_size == 0) throw std::invalid_argument("region_size==0");
        if (n_ == 0) throw std::runtime_error("array not initialized");
        epoch_region_size_ = region_size;
        num_regions_ = (n_ + region_size - 1) / region_size;
        epoch_table_.assign(num_regions_, epoch_t(0));
        region_dirty_.assign(num_regions_, false);
        region_locks_.assign(num_regions_, std::atomic<uint8_t>(0));
    }

    // read effective ts for VALUE32 mode
    uint64_t read_effective_ts(size_t idx, std::memory_order mo = std::memory_order_acquire) const noexcept {
        if constexpr (MODE != PackedMode::MODE_VALUE32) return 0;
        if (idx >= n_) return 0;
        if constexpr (LAYOUT == LayoutKind::LAYOUT_AOS) {
            packed_t p = meta_[idx].load(mo);
            clk16_t c = PackedCell::extract_clk16(p);
            epoch_t e = epoch_table_.empty() ? epoch_t(0) : epoch_table_[region_of(idx)];
            return (e << 16) | uint64_t(c);
        } else {
            clk16_t c = clocks_[idx];
            epoch_t e = epoch_table_.empty() ? epoch_t(0) : epoch_table_[region_of(idx)];
            // lazy effective: region epoch may be newer -> return max semantics
            return (e << 16) | uint64_t(c);
        }
    }

    // region_of
    size_t region_of(size_t idx) const noexcept { return epoch_region_size_ ? (idx / epoch_region_size_) : 0; }

    // read element: returns packed (value/clocks/tags) for consumer; uses acquire semantics
    packed_t read_packed(size_t idx, std::memory_order mo = std::memory_order_acquire) const noexcept {
        if (idx >= n_) return packed_t(0);
        if constexpr (LAYOUT == LayoutKind::LAYOUT_AOS) {
            return meta_[idx].load(mo);
        } else {
            // SOA: build packed word from separate arrays and region epoch if dirty
            val32_t v = values_[idx];
            clk16_t c = clocks_[idx];
            uint16_t tag = tags_[idx];
            tag8_t st = tag >> 8;
            tag8_t rel = tag & 0xFF;
            // effective clk may be superseded by region epoch; we keep raw clk here
            return PackedCell::compose_value32(v, c, st, rel);
        }
    }

    // reserve_for_update: sets ST_PENDING and stamps batch_low; returns true on success
    bool reserve_for_update(size_t idx, packed_t expected_old, uint16_t batch_low, tag8_t rel_hint = 0) noexcept {
        if (idx >= n_) return false;
        if constexpr (LAYOUT == LayoutKind::LAYOUT_AOS) {
            // older code: CAS old->pending
            val32_t v = PackedCell::extract_value32(expected_old);
            packed_t pending = PackedCell::compose_value32(v, static_cast<clk16_t>(batch_low), static_cast<tag8_t>(0x02), rel_hint);
            packed_t observed = expected_old;
            return meta_[idx].compare_exchange_strong(observed, pending, std::memory_order_acq_rel, std::memory_order_relaxed);
        } else {
            // SoA: we only update the metadata atomic to mark pending (value remains unchanged until commit).
            // Build pending metadata with st=PENDING and clk=batch_low and rel_hint.
            // readers will see pending and wait; writers later commit by updating values_ then storing final meta with release.
            packed_t observed_meta = meta_[idx].load(std::memory_order_acquire);
            // we don't verify expected_old exactly here for SoA; we try to set pending only if meta still equals observed_meta
            val32_t curv = values_[idx];
            packed_t pending = PackedCell::compose_value32(curv, static_cast<clk16_t>(batch_low), static_cast<tag8_t>(0x02), rel_hint);
            return meta_[idx].compare_exchange_strong(observed_meta, pending, std::memory_order_acq_rel, std::memory_order_relaxed);
        }
    }

    // commit_update: in AoS we CAS pending->committed; in SoA we must ensure values_ are written (with release semantics)
    bool commit_update(size_t idx, packed_t expected_pending, packed_t committed_packed, bool use_nt_store = false) noexcept {
        if (idx >= n_) return false;
        if constexpr (LAYOUT == LayoutKind::LAYOUT_AOS) {
            bool ok = meta_[idx].compare_exchange_strong(expected_pending, committed_packed, std::memory_order_acq_rel, std::memory_order_relaxed);
            if (ok) notify_idx(idx);
            return ok;
        } else {
            // SoA: writer must have already written the value into values_[idx] and clocks_[idx]/tags_[idx] appropriately.
            // Now perform final publish by storing metadata (committed_packed) with release and notify.
            meta_[idx].store(committed_packed, std::memory_order_release);
            notify_idx(idx);
            return true;
        }
    }

    // store_atomic used by worker for direct commits (SoA worker uses non-temporal store for values then commit metadata)
    void store_atomic_value_and_commit(size_t idx, val32_t value, clk16_t clk, tag8_t st, tag8_t rel, bool use_nt_store = false) noexcept {
        if (idx >= n_) return;
        if constexpr (LAYOUT == LayoutKind::LAYOUT_AOS) {
            packed_t p = PackedCell::compose_value32(value, clk, st, rel);
            meta_[idx].store(p, std::memory_order_release);
            notify_idx(idx);
        } else {
            // SoA path: write value (possibly non-temporal), write clocks/tags, then store meta
            if (use_nt_store) {
#if defined(__x86_64__)
                // non-temporal 64-bit store: use _mm_stream_si64; we have 32-bit values and 16-bit clocks+tags
                // We'll store the 64-bit combined (value | (clk<<32)) as a single streaming store when possible.
                // Build 64-bit combine for minimal store; otherwise fall back to scalar.
                uint64_t combined = (uint64_t(value) & 0xFFFFFFFFu) | (uint64_t(clk) << 32);
                _mm_stream_si64(reinterpret_cast<long long*>(&values_[idx]), (long long)combined);
                // store tags normally
                tags_[idx] = (uint16_t(((uint16_t)st << 8) | rel));
                // ensure ordering: mfence then meta store
                _mm_mfence();
#else
                (void)use_nt_store;
                values_[idx] = value;
                clocks_[idx] = clk;
                tags_[idx] = uint16_t(((uint16_t)st << 8) | rel);
#endif
            } else {
                values_[idx] = value;
                clocks_[idx] = clk;
                tags_[idx] = uint16_t(((uint16_t)st << 8) | rel);
            }
            // publish metadata
            packed_t meta = PackedCell::compose_value32(value, clk, st, rel);
            meta_[idx].store(meta, std::memory_order_release);
            notify_idx(idx);
        }
    }

    // wait_for_change: use std::atomic::wait if available, else Wake fallback
    bool wait_for_change(size_t idx, packed_t expected, int timeout_ms = 100) noexcept {
        if (idx >= n_) return false;
#if defined(__cpp_lib_atomic_wait)
        // std::atomic::wait (C++20)
        if (timeout_ms < 0) {
            meta_[idx].wait(expected);
            return true;
        }
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            packed_t cur = meta_[idx].load(std::memory_order_acquire);
            if (cur != expected) return true;
            meta_[idx].wait(expected);
        }
        return false;
#else
        // fallback: Wake object per array
        auto deadline = (timeout_ms < 0) ? std::chrono::steady_clock::time_point::max()
                                         : std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            packed_t cur = meta_[idx].load(std::memory_order_acquire);
            if (cur != expected) return true;
            wake_->wait_for_ms(4);
        }
        return false;
#endif
    }

    // scan_rel_ranges (works for both layouts)
    std::vector<std::pair<size_t,size_t>> scan_rel_ranges(tag8_t rel_tag) const noexcept {
        std::vector<std::pair<size_t,size_t>> ranges;
        if (n_ == 0) return ranges;
        size_t i = 0;
        while (i < n_) {
            packed_t p = read_packed(i, std::memory_order_acquire);
            tag8_t r = PackedCell::extract_rel(p, MODE);
            if (r != rel_tag) { ++i; continue; }
            size_t start = i;
            ++i;
            while (i < n_) {
                p = read_packed(i, std::memory_order_acquire);
                if (PackedCell::extract_rel(p, MODE) != rel_tag) break;
                ++i;
            }
            ranges.emplace_back(start, i - start);
        }
        return ranges;
    }

    // Lazy epoch bump: mark region dirty and increment epoch table; do NOT touch all cells.
    bool region_epoch_bump_lazy(size_t region_idx) noexcept {
        if (region_idx >= num_regions_) return false;
        uint8_t expected = 0;
        if (!region_locks_[region_idx].compare_exchange_strong(expected, uint8_t(1), std::memory_order_acq_rel)) return false;
        // increment epoch table and mark region dirty
        epoch_table_[region_idx]++;
        region_dirty_[region_idx] = true;
        // release lock immediately
        region_locks_[region_idx].store(0, std::memory_order_release);
        // optional callback
        if (epoch_bump_cb_) epoch_bump_cb_(region_idx, epoch_table_[region_idx]);
        return true;
    }

    // When a reader/writer access a dirty region, it may 'materialize' the epoch for that cell:
    // it will interpret effective timestamp as max(cell_ts, region_ts) rather than updating the whole region.
    // Provide helper to test and clear dirty flag for region if worker has handled its locality.
    bool region_is_dirty(size_t region_idx) const noexcept {
        return (region_idx < region_dirty_.size()) ? region_dirty_[region_idx] : false;
    }

    void clear_region_dirty(size_t region_idx) noexcept {
        if (region_idx < region_dirty_.size()) region_dirty_[region_idx] = false;
    }

    // NUMA knobs: set explicit node count for striping decisions (informational)
    void set_numa_nodes(unsigned nodes) noexcept { numa_nodes_ = nodes; }

    // Set epoch bump callback
    void set_epoch_bump_callback(std::function<void(size_t, epoch_t)> cb) { epoch_bump_cb_ = std::move(cb); }

private:
    void notify_idx(size_t idx) noexcept {
#if defined(__cpp_lib_atomic_wait)
        meta_[idx].notify_all();
#else
        wake_->notify_one();
#endif
    }

    // For non-temporal stores we use x86 intrinsics when available; otherwise fall back to scalar.
    static inline void nt_store_64(void* addr, uint64_t v) noexcept {
#if defined(__x86_64__)
        _mm_stream_si64(reinterpret_cast<long long*>(addr), (long long)v);
#else
        // fallback
        *reinterpret_cast<uint64_t*>(addr) = v;
#endif
    }

    size_t n_;
    // For both layouts we keep a meta_ array of atomic<packed_t> used for wait/notify and final publish
    std::atomic<packed_t>* meta_{nullptr};

    // SoA members (only valid when LAYOUT_SOA)
    val32_t* values_{nullptr};
    clk16_t* clocks_{nullptr};
    uint16_t* tags_{nullptr}; // high byte = st, low byte = rel

    // epoch support
    size_t epoch_region_size_{0};
    size_t num_regions_{0};
    std::vector<epoch_t> epoch_table_;
    std::vector<bool> region_dirty_;
    std::vector<std::atomic<uint8_t>> region_locks_;
    std::function<void(size_t, epoch_t)> epoch_bump_cb_;

    // Wake fallback if std::atomic::wait is not available
#if defined(__cpp_lib_atomic_wait)
    struct NoWake { void notify_one(){} bool wait_for_ms(int){return false;} };
    std::unique_ptr<NoWake> wake_;
#else
    using WakeFallback = atomiccim::Wake;
    std::unique_ptr<WakeFallback> wake_;
#endif

    unsigned numa_nodes_{0};
};

} // namespace atomiccim

//=== File: AtomicCIM/AsyncWorker.hpp ===
#pragma once

#include <thread>
#include <vector>
#include <algorithm>
#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <cstring>
#include <immintrin.h> // for intrinsics if available

#include "MPMCQueue.hpp"
#include "AtomicArray.hpp"
#include "Descriptor.hpp"

namespace atomiccim {

template<PackedMode MODE, LayoutKind LAYOUT>
class AsyncWorker {
public:
    using Arr = AtomicArray<MODE, LAYOUT>;
    using Desc = Descriptor;

    AsyncWorker(Arr &arr, size_t ring_capacity_pow2 = 1<<14)
      : arr_(arr), q_(new MPMCQueue<Desc>(ring_capacity_pow2, &AsyncWorker::default_high_water_cb, this)),
        running_(false), next_batch_id_(1) {}

    ~AsyncWorker() { stop(); delete q_; }

    bool submit(const Desc &d) noexcept { return q_->push(d); }
    bool submit_blocking(const Desc &d, int timeout_ms = -1) noexcept { return q_->push_blocking(d, timeout_ms); }

    void start() {
        bool exp = false;
        if (!running_.compare_exchange_strong(exp, true)) return;
        thread_ = std::thread([this]{ this->loop(); });
    }

    void stop() {
        bool exp = true;
        if (!running_.compare_exchange_strong(exp, false)) return;
        if (thread_.joinable()) thread_.join();
    }

private:
    static void default_high_water_cb(size_t cur, size_t cap, void* user) {
        (void)cur; (void)cap; (void)user;
    }

    uint64_t next_batch_id() {
        return next_batch_id_.fetch_add(1, std::memory_order_relaxed);
    }

    void loop() {
        std::vector<Desc> batch;
        batch.reserve(2048);
        while (running_.load(std::memory_order_acquire)) {
            batch.clear();
            size_t drained = q_->drain_batch(batch, 1024);
            if (drained == 0) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue; }

            // coalesce by idx and rel
            std::sort(batch.begin(), batch.end(), [](const Desc&a,const Desc&b){
                if (a.idx != b.idx) return a.idx < b.idx;
                if (a.rel != b.rel) return a.rel < b.rel;
                return a.op < b.op;
            });

            size_t i = 0;
            while (i < batch.size()) {
                size_t j = i + 1;
                while (j < batch.size() && batch[j].idx == batch[j-1].idx + 1 && batch[j].rel == batch[i].rel) ++j;
                process_group(batch, i, j);
                i = j;
            }
        }
    }

    void process_group(const std::vector<Desc>& batch, size_t i, size_t j) {
        uint64_t bid = next_batch_id();
        bool need_reserve = false;
        for (size_t k = i; k < j; ++k) if (batch[k].op_flags & 0x01) { need_reserve = true; break; }

        // Attempt reservations (if requested)
        if (need_reserve) {
            for (size_t k = i; k < j; ++k) {
                const Desc &d = batch[k];
                packed_t cur = arr_.read_packed(d.idx, std::memory_order_acquire);
                uint16_t low = static_cast<uint16_t>(bid & 0xFFFF);
                arr_.reserve_for_update(d.idx, cur, low, d.rel);
                // Note: production code must check return values and handle reservation failures.
            }
        }

        // Compute stage
        // If LAYOUT == SOA and op==APPLY_GRAD, run vectorized kernel operating on contiguous values_
        std::vector<std::pair<size_t, packed_t>> commits;
        commits.reserve(j - i);

        if constexpr (LAYOUT == LayoutKind::LAYOUT_SOA && MODE == PackedMode::MODE_VALUE32) {
            // Find contiguous block ranges inside [i,j) that are APPLY_GRAD
            size_t k = i;
            while (k < j) {
                if (batch[k].op == 4) { // APPLY_GRAD
                    size_t start = k;
                    size_t end = k+1;
                    while (end < j && batch[end].op == 4 && batch[end].idx == batch[end-1].idx + 1) ++end;
                    // process contiguous APPLY_GRAD range [start,end)
                    process_apply_grad_range(batch, start, end, bid, commits);
                    k = end;
                } else {
                    // other ops, handle scalar-by-scalar for now
                    const Desc &d = batch[k];
                    if (d.op == 1) { // SET
                        packed_t newp = static_cast<packed_t>(d.arg);
                        tag8_t rel = d.rel;
                        if constexpr (MODE == PackedMode::MODE_VALUE32) {
                            val32_t v = PackedCell::extract_value32(newp);
                            newp = PackedCell::compose_value32(v, static_cast<clk16_t>(bid & 0xFFFF), static_cast<tag8_t>(0x01), rel);
                        }
                        commits.emplace_back(d.idx, newp);
                    } else if (d.op == 5) { // OP_EPOCH_BUMP
                        size_t region = static_cast<size_t>(d.arg);
                        arr_.region_epoch_bump_lazy(region);
                    }
                    ++k;
                }
            }
        } else {
            // Fallback scalar path: construct commits for each descriptor
            for (size_t k = i; k < j; ++k) {
                const Desc &d = batch[k];
                if (d.op == 1) {
                    packed_t newp = static_cast<packed_t>(d.arg);
                    tag8_t rel = d.rel;
                    if constexpr (MODE == PackedMode::MODE_VALUE32) {
                        val32_t v = PackedCell::extract_value32(newp);
                        newp = PackedCell::compose_value32(v, static_cast<clk16_t>(bid & 0xFFFF), static_cast<tag8_t>(0x01), rel);
                    }
                    commits.emplace_back(d.idx, newp);
                } else if (d.op == 4) {
                    // cheap fallback: take arg low bits as value
                    val32_t v = static_cast<val32_t>(d.arg & 0xFFFFFFFF);
                    packed_t newp = PackedCell::compose_value32(v, static_cast<clk16_t>(bid & 0xFFFF), static_cast<tag8_t>(0x01), d.rel);
                    commits.emplace_back(d.idx, newp);
                } else if (d.op == 5) {
                    size_t region = static_cast<size_t>(d.arg);
                    arr_.region_epoch_bump_lazy(region);
                }
            }
        }

        // Commit step: decide whether to use non-temporal stores if batch is large
        const size_t NT_THRESHOLD = 512;
        bool use_nt = (commits.size() >= NT_THRESHOLD);

        for (auto &c : commits) {
            if constexpr (LAYOUT == LayoutKind::LAYOUT_AOS) {
                arr_.commit_update(c.first, c.second /*expected not tracked here*/, c.second);
            } else {
                // For SoA: extract fields and perform store_atomic_value_and_commit
                if constexpr (MODE == PackedMode::MODE_VALUE32) {
                    val32_t v = PackedCell::extract_value32(c.second);
                    clk16_t clk = PackedCell::extract_clk16(c.second);
                    tag8_t st = PackedCell::extract_st(c.second, MODE);
                    tag8_t rel = PackedCell::extract_rel(c.second, MODE);
                    arr_.store_atomic_value_and_commit(c.first, v, clk, st, rel, use_nt);
                } else {
                    // other modes: store meta directly
                    arr_.store_atomic_value_and_commit(c.first, 0, 0, static_cast<tag8_t>(0x01), static_cast<tag8_t>(0), use_nt);
                }
            }
        }
    }

    // Vectorized kernel: APPLY_GRAD for contiguous range of descriptors [start,end)
    // For demonstration we assume d.arg contains a pointer to int32_t gradient buffer base + offset (user must arrange.)
    void process_apply_grad_range(const std::vector<Desc>& batch, size_t start, size_t end, uint64_t bid,
                                  std::vector<std::pair<size_t, packed_t>> &commits)
    {
        size_t count = end - start;
        size_t base_idx = batch[start].idx;
        // Interpret arg of the first desc as pointer-to-int32 gradients (user convention)
        // WARNING: this is a demo: real system must pack pointers/offsets consistently in descriptors.
        const int32_t* grad_ptr = reinterpret_cast<const int32_t*>(static_cast<uintptr_t>(batch[start].arg));
        // process in vector width of 8 32-bit ints for AVX2, 16 for AVX-512
#if defined(__AVX512F__)
        constexpr size_t VW = 16;
#elif defined(__AVX2__)
        constexpr size_t VW = 8;
#else
        constexpr size_t VW = 1;
#endif
        size_t k = 0;
        while (k < count) {
            size_t chunk = std::min(VW, count - k);
#if defined(__AVX512F__)
            // load 16 ints from grad_ptr + k and store into values_ using non-temporal store if beneficial
            __m512i g = _mm512_loadu_si512(reinterpret_cast<const void*>(grad_ptr + k));
            // convert to 32-bit values and store to values_ (assume values_ is int32-compatible)
            // We will fallback to scalar if conversion / type mismatch
            for (size_t t = 0; t < chunk; ++t) {
                int32_t gv = grad_ptr[k + t];
                val32_t v = static_cast<val32_t>(gv);
                size_t idx = base_idx + k + t;
                // write value and commit meta later
                // Use write into Arr::values_ directly via an accessor (we don't have direct access, so create commit pair)
                packed_t meta = PackedCell::compose_value32(v, static_cast<clk16_t>(bid & 0xFFFF), static_cast<tag8_t>(0x01), batch[start].rel);
                commits.emplace_back(idx, meta);
            }
#elif defined(__AVX2__)
            // AVX2 path: load 8 ints
            __m256i g = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(grad_ptr + k));
            int32_t tmp[VW];
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(tmp), g);
            for (size_t t = 0; t < chunk; ++t) {
                val32_t v = static_cast<val32_t>(tmp[t]);
                size_t idx = base_idx + k + t;
                packed_t meta = PackedCell::compose_value32(v, static_cast<clk16_t>(bid & 0xFFFF), static_cast<tag8_t>(0x01), batch[start].rel);
                commits.emplace_back(idx, meta);
            }
#else
            // Scalar fallback
            for (size_t t = 0; t < chunk; ++t) {
                val32_t v = static_cast<val32_t>(grad_ptr[k + t]);
                size_t idx = base_idx + k + t;
                packed_t meta = PackedCell::compose_value32(v, static_cast<clk16_t>(bid & 0xFFFF), static_cast<tag8_t>(0x01), batch[start].rel);
                commits.emplace_back(idx, meta);
            }
#endif
            k += chunk;
        }
    }

    Arr &arr_;
    MPMCQueue<Desc>* q_;
    std::atomic<bool> running_;
    std::thread thread_;
    std::atomic<uint64_t> next_batch_id_;

    std::mutex undo_mu_;
    std::unordered_map<uint64_t, std::vector<std::pair<size_t, packed_t>>> undo_buffer_;
};

} // namespace atomiccim


// === File: AtomicCIM/Descriptor.hpp ===
#pragma once

#include <cstdint>

namespace atomiccim {

// Extended descriptor used by AsyncWorker. Designed to be compact but extendable.
struct Descriptor {
    uint8_t op;        // 1 = SET, 2 = BATCH_SET, 3 = ADD, 4 = APPLY_GRAD, 5 = OP_EPOCH_BUMP, etc.
    uint8_t op_flags;  // bitfield: RESERVE_BEFORE_COMPUTE=0x01, HIGH_PRIORITY=0x02, USE_UNDO=0x04, etc.
    uint8_t st;        // optional hint for new st
    uint8_t rel;       // relation tag
    uint32_t idx;      // base index
    uint32_t count;    // number of elements for batch ops
    uint64_t batch_id; // full 64-bit batch id generated by worker
    uint64_t undo_hint; // optional undo hint (pointer or small preimage index)
    uint64_t arg;      // opaque argument (pointer, immediate value, or kernel id)
};

// op_flags semantics (recommended):
//   0x01 RESERVE_BEFORE_COMPUTE: worker should attempt reserve (ST_PENDING) before computing results
//   0x02 HIGH_PRIORITY: process this descriptor ahead of normal ones
//   0x04 USE_UNDO: create undo entry before commit
//   0x08 SKIP_NOTIFY: don't call notify after commit (rare)
//   0x10 FORCE_EPOCH_BUMP: if reservation fails due to wrap, trigger epoch bump

} // namespace atomiccim


//=== File: AtomicCIM/AtomicCIM.hpp ===
#pragma once

#include "Alloc.hpp"
#include "PackedCell.hpp"
#include "MPMCQueue.hpp"
#include "AtomicArray.hpp"
#include "Descriptor.hpp"
#include "AsyncWorker.hpp"

/*
 UPGRADE / README (summary):

 - Layouts:
   * LAYOUT_AOS: single std::atomic<uint64_t> per element (classic).
   * LAYOUT_SOA: separate arrays for values/clocks/tags and a metadata atomic per element used for commit.
     Use LAYOUT_SOA to allow vectorized worker kernels and non-temporal stores.

 - Epoch semantics:
   * Initialize with init_epoch(region_size). Each region has epoch_table_[r].
   * Use region_epoch_bump_lazy(region) to bump a region's epoch cheaply.
   * Readers compute effective timestamp as (epoch<<16)|clk and treat region dirty flags accordingly.
   * A background maintenance task may be used to normalize cells if per-cell clocks must be physically advanced.

 - SIMD & NT stores:
   * Use LAYOUT_SOA to accelerate APPLY_GRAD with AVX2/AVX512 kernels (worker writes contiguous values_).
   * For large blocks, worker uses non-temporal stores to avoid polluting caches. Ensure alignment and cache-line sized stores for best performance.

 - NUMA:
   * Compile with -DHAVE_LIBNUMA and link -lnuma to enable explicit allocation on nodes.
   * Use set_numa_nodes() and region-striping strategies to place hot regions near worker threads.

 - Operational knobs:
   * ring capacity for descriptor queue (MPMCQueue constructor).
   * push_blocking timeout/backoff.
   * high-watermark callback to apply backpressure.
   * epoch bump callback.

 - Safety:
   * Writers must ensure data stores to values_ happen-before metadata store that publishes commit.
   * Readers use acquire on meta_ then read values_ safely.

 - Future:
   * Full SIMD write path writing directly into SoA buffers (not just building commits).
   * Fine-grained per-element futex mapping for ultra-low-latency waiting.
   * Persistent epoch table with crash-recovery.
*/

#endif // ATOMICCIM_UMBRELLA

//=== File: AtomicCIM/Alloc.hpp ===
#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>

#if defined(HAVE_LIBNUMA)
  #include <numa.h>
  #include <numaif.h>
#endif

namespace atomiccim::alloc {

inline void * aligned_alloc_portable(std::size_t alignment, std::size_t size) {
#if defined(_MSC_VER)
    void* p = _aligned_malloc(size, alignment);
    if (!p) throw std::bad_alloc();
    return p;
#else
    // posix/aligned_alloc requires size multiple of alignment on some platforms
    size_t msize = ((size + alignment - 1) / alignment) * alignment;
    void* p = std::aligned_alloc(alignment, msize);
    if (!p) throw std::bad_alloc();
    return p;
#endif
}

inline void aligned_free_portable(void* p) noexcept {
#if defined(_MSC_VER)
    _aligned_free(p);
#else
    std::free(p);
#endif
}

#if defined(HAVE_LIBNUMA)
inline void * aligned_alloc_onnode(std::size_t alignment, std::size_t size, int node) {
    // Use libnuma allocation on given node
    // Note: numa_alloc_onnode does not guarantee alignment; we can over-allocate and align manually if needed.
    // For simplicity we use numa_alloc_onnode and assume default alignment is OK for our usage (usually page-aligned).
    void* p = numa_alloc_onnode(size, node);
    if (!p) throw std::bad_alloc();
    return p;
}
inline void free_onnode(void* p, std::size_t size) noexcept { numa_free(p, size); }
#endif

} // namespace atomiccim::alloc
