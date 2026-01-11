#pragma once
// PackedCell.hpp
// Single-header, minimal-duplication packed 64-bit cell utilities.
// Two pack modes supported:
//   MODE_VALUE32 : [ value:32 | clk16:16 | st:8 | rel:8 ]
//   MODE_CLK48   : [ clk48:48  | st:8  | rel:8 ]
//
// Hot path optimization: top-16 bits (st|rel) are extracted via a single >>48 + &0xFFFF.
// Minimal branching via constexpr mode-dispatch.

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace AtomicCScompact {

using packed64_t = uint64_t;
using val32_t    = uint32_t;
using clk16_t    = uint16_t;
using clk48_t    = uint64_t;
using tag8_t     = uint8_t;
using strel_t    = uint16_t;

enum class PackedMode : int { MODE_VALUE32 = 0, MODE_CLK48 = 1 };

// Layout constants
static inline constexpr unsigned VALBITS  = 32u;
static inline constexpr unsigned CLK16B   = 16u;
static inline constexpr unsigned CLK48B   = 48u;
static inline constexpr unsigned STRELB   = 16u; // st(8) | rel(8) in top 16 bits

// Safe low-mask generator (no UB for 64)
static inline constexpr packed64_t low_mask(unsigned n) noexcept {
    if (n == 0) return packed64_t(0);
    if (n >= 64) return ~packed64_t(0);
    return (~packed64_t(0)) >> (64 - n);
}

// Compact, minimal API. All functions are noexcept and small.
struct PackedCell {
    // Compose (value32 layout)
    static inline packed64_t compose_value32(val32_t v, clk16_t clk, tag8_t st, tag8_t rel) noexcept {
        packed64_t p = (packed64_t(v) & low_mask(VALBITS));
        p |= (packed64_t(clk) & low_mask(CLK16B)) << VALBITS;
        p |= (packed64_t(st)  & low_mask(8u)) << (VALBITS + CLK16B);
        p |= (packed64_t(rel) & low_mask(8u)) << (VALBITS + CLK16B + 8u);
        return p;
    }

    // Compose (clk48 layout)
    static inline packed64_t compose_clk48(clk48_t clk, tag8_t st, tag8_t rel) noexcept {
        packed64_t p = (packed64_t(clk) & low_mask(CLK48B));
        p |= (packed64_t(st)  & low_mask(8u)) << CLK48B;
        p |= (packed64_t(rel) & low_mask(8u)) << (CLK48B + 8u);
        return p;
    }

    // Extract value/clocks (fast, inlined)
    static inline val32_t extract_value32(packed64_t p) noexcept {
        return static_cast<val32_t>(p & low_mask(VALBITS));
    }
    static inline clk16_t extract_clk16(packed64_t p) noexcept {
        return static_cast<clk16_t>((p >> VALBITS) & low_mask(CLK16B));
    }
    static inline clk48_t extract_clk48(packed64_t p) noexcept {
        return static_cast<clk48_t>(p & low_mask(CLK48B));
    }

    // Hot-path: extract combined st|rel (single shift + mask)
    static inline strel_t extract_strel(packed64_t p) noexcept {
        return static_cast<strel_t>((p >> (64 - STRELB)) & low_mask(STRELB));
    }
    static inline tag8_t st_from_strel(strel_t s) noexcept { return static_cast<tag8_t>((s >> 8) & 0xFFu); }
    static inline tag8_t rel_from_strel(strel_t s) noexcept { return static_cast<tag8_t>(s & 0xFFu); }

    // Set only the st|rel 16-bit top field efficiently.
    static inline packed64_t set_strel(packed64_t p, strel_t s) noexcept {
        constexpr packed64_t top_mask = low_mask(STRELB) << (64 - STRELB);
        p = (p & ~top_mask) | ( (packed64_t(s & low_mask(STRELB))) << (64 - STRELB) );
        return p;
    }

    // Convenience setters for st or rel only
    static inline packed64_t set_st(packed64_t p, tag8_t st) noexcept {
        strel_t old = extract_strel(p);
        strel_t neu = static_cast<strel_t>((static_cast<strel_t>(st) << 8) | (old & 0xFFu));
        return set_strel(p, neu);
    }
    static inline packed64_t set_rel(packed64_t p, tag8_t rel) noexcept {
        strel_t old = extract_strel(p);
        strel_t neu = static_cast<strel_t>((old & 0xFF00u) | static_cast<strel_t>(rel));
        return set_strel(p, neu);
    }

    // Decompose both layouts in one call (avoids multiple loads on hot path)
    static inline void decompose_value32(packed64_t p, val32_t &v, clk16_t &clk, tag8_t &st, tag8_t &rel) noexcept {
        v = extract_value32(p);
        clk = extract_clk16(p);
        strel_t sr = extract_strel(p);
        st = st_from_strel(sr);
        rel = rel_from_strel(sr);
    }
    static inline void decompose_clk48(packed64_t p, clk48_t &clk, tag8_t &st, tag8_t &rel) noexcept {
        clk = extract_clk48(p);
        strel_t sr = extract_strel(p);
        st = st_from_strel(sr);
        rel = rel_from_strel(sr);
    }

    // Generic reinterpret helper: read packed value as T (trivial copy of bits).
    // Use only for trivially-copyable T of size <= 8; implemented via memcpy to avoid UB.
    template<typename T>
    static inline T as_value(packed64_t p) noexcept {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        static_assert(sizeof(T) <= sizeof(packed64_t), "T must fit into 64 bits");
        T out;
        std::memcpy(&out, &p, sizeof(T));
        return out;
    }

}; // struct PackedCell

// Small proxy that auto-packs/unpacks. It behaves like a thin reference to an atomic slot.
// It is intentionally copyable and cheap; it is not a full-featured type conversion system,
// but it provides convenient setters/getters that avoid manual composition in user code.
template<PackedMode MODE>
struct PackedProxy {
    packed64_t raw;

    // implicit construction from components (VALUE32)
    static inline PackedProxy make_value32(val32_t v, clk16_t clk, tag8_t st, tag8_t rel) noexcept {
        PackedProxy p;
        if constexpr (MODE == PackedMode::MODE_VALUE32) p.raw = PackedCell::compose_value32(v, clk, st, rel);
        else p.raw = PackedCell::compose_clk48(static_cast<clk48_t>(clk), st, rel);
        return p;
    }
    static inline PackedProxy make_clk48(clk48_t clk, tag8_t st, tag8_t rel) noexcept {
        PackedProxy p;
        if constexpr (MODE == PackedMode::MODE_CLK48) p.raw = PackedCell::compose_clk48(clk, st, rel);
        else p.raw = PackedCell::compose_value32(static_cast<val32_t>(clk & 0xFFFFFFFFu), static_cast<clk16_t>((clk>>32)&0xFFFFu), st, rel);
        return p;
    }

    // quick accessors
    inline val32_t value32() const noexcept { return PackedCell::extract_value32(raw); }
    inline clk16_t clk16()  const noexcept { return PackedCell::extract_clk16(raw); }
    inline clk48_t clk48()  const noexcept { return PackedCell::extract_clk48(raw); }
    inline tag8_t  st()     const noexcept { return PackedCell::st_from_strel(PackedCell::extract_strel(raw)); }
    inline tag8_t  rel()    const noexcept { return PackedCell::rel_from_strel(PackedCell::extract_strel(raw)); }

    // set single fields (returning new packed)
    inline PackedProxy with_st(tag8_t s) const noexcept { PackedProxy r; r.raw = PackedCell::set_st(raw, s); return r; }
    inline PackedProxy with_rel(tag8_t rmask) const noexcept { PackedProxy r; r.raw = PackedCell::set_rel(raw, rmask); return r; }
};

} // namespace AtomicCScompact


#pragma once
// PackedStRel.h
// Canonical states and relation masks. Use bitmask relations (one slot can address many consumers).

#include "PackedCell.hpp"

namespace AtomicCScompact {

// States (8-bit)
static constexpr tag8_t ST_IDLE        = 0x00;
static constexpr tag8_t ST_PUBLISHED   = 0x01;
static constexpr tag8_t ST_PENDING     = 0x02;
static constexpr tag8_t ST_CLAIMED     = 0x03;
static constexpr tag8_t ST_PROCESSING  = 0x04;
static constexpr tag8_t ST_COMPLETE    = 0x05;
static constexpr tag8_t ST_RETIRED     = 0x06;
static constexpr tag8_t ST_EPOCH_BUMP  = 0x07;
static constexpr tag8_t ST_LOCKED      = 0x08;
// Reserve 0xF0..0xFF for user extensions

// Relation bit masks (8-bit)
static constexpr tag8_t REL_NONE      = 0x00;
static constexpr tag8_t REL_NODE0     = 0x01;
static constexpr tag8_t REL_NODE1     = 0x02;
static constexpr tag8_t REL_PAGE      = 0x04;
static constexpr tag8_t REL_PATTERN   = 0x08;
static constexpr tag8_t REL_SELF      = 0x10;
static constexpr tag8_t REL_BROADCAST = 0xFF; // convenience

static inline strel_t make_strel(tag8_t st, tag8_t rel) noexcept {
    return static_cast<strel_t>((static_cast<strel_t>(st) << 8) | static_cast<strel_t>(rel));
}
static inline bool rel_matches(tag8_t slot_rel, tag8_t rel_mask) noexcept {
    return (static_cast<uint8_t>(slot_rel) & static_cast<uint8_t>(rel_mask)) != 0;
}

} // namespace AtomicCScompact
#pragma once
// MPMCArrayPacked.hpp
// Slot-array mailbox specialized for packed64_t (PackedCell).
// The array is NUMA-allocated via AllocNW::AlignedAllocONnode (no fallback).
// Designed for CPU<->GPU mailbox usage: consumers scan slots and claim by rel mask.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <chrono>
#include <thread>
#include <bit>
#include <cassert>

#include "PackedCell.hpp"
#include "PackedStRel.h"
#include "AllocNW.hpp"

namespace AtomicCScompact {

using packed64_t = ::AtomicCScompact::packed64_t;
using tag8_t     = ::AtomicCScompact::tag8_t;
using strel_t    = ::AtomicCScompact::strel_t;
using PackedMode = ::AtomicCScompact::PackedMode;

using HWCallback = void(*)(size_t current, size_t capacity, void* user);

static inline constexpr uint64_t HASH_CONST = 11400714819323198485ull;

template<PackedMode MODE>
class MPMCArrayPacked {
public:
    MPMCArrayPacked(size_t capacity, int node = 0, HWCallback hw_cb = nullptr, void* cb_user = nullptr)
      : capacity_(capacity), cb_(hw_cb), cb_user_(cb_user), node_(node)
    {
        if (capacity_ == 0) throw std::invalid_argument("capacity==0");
        size_t bytes = sizeof(std::atomic<packed64_t>) * capacity_;
        raw_ = reinterpret_cast<std::atomic<packed64_t>*>(AllocNW::AlignedAllocONnode(64, bytes, node_));
        if (!raw_) throw std::bad_alloc();
        packed64_t idle = make_idle();
        for (size_t i = 0; i < capacity_; ++i) new (&raw_[i]) std::atomic<packed64_t>(idle);
        occ_.store(0, std::memory_order_relaxed);
        prod_cursor_.store(0, std::memory_order_relaxed);
        cons_cursor_.store(0, std::memory_order_relaxed);
    }

    ~MPMCArrayPacked() {
        if (raw_) {
            for (size_t i = 0; i < capacity_; ++i) raw_[i].~atomic();
            size_t bytes = sizeof(std::atomic<packed64_t>) * capacity_;
            AllocNW::FreeONNode(static_cast<void*>(raw_), bytes);
            raw_ = nullptr;
        }
    }

    MPMCArrayPacked(const MPMCArrayPacked&) = delete;
    MPMCArrayPacked& operator=(const MPMCArrayPacked&) = delete;

    size_t capacity() const noexcept { return capacity_; }
    size_t occupancy() const noexcept { return occ_.load(std::memory_order_acquire); }

    // publish: place item with ST_PUBLISHED into any free slot. Returns index or SIZE_MAX.
    size_t publish(packed64_t item, int max_probes = -1) noexcept {
        // ensure ST_PUBLISHED in top byte
        strel_t sr = PackedCell::extract_strel(item);
        if (PackedCell::st_from_strel(sr) != ST_PUBLISHED) {
            tag8_t rel = PackedCell::rel_from_strel(sr);
            if constexpr (MODE == PackedMode::MODE_VALUE32)
                item = PackedCell::compose_value32(PackedCell::extract_value32(item), PackedCell::extract_clk16(item), ST_PUBLISHED, rel);
            else
                item = PackedCell::compose_clk48(PackedCell::extract_clk48(item), ST_PUBLISHED, rel);
        }

        size_t start = prod_cursor_.fetch_add(1, std::memory_order_relaxed);
        size_t idx = start % capacity_;
        int probes = 0;
        while (true) {
            packed64_t cur = raw_[idx].load(std::memory_order_acquire);
            strel_t csr = PackedCell::extract_strel(cur);
            if (PackedCell::st_from_strel(csr) == ST_IDLE) {
                packed64_t expected = cur;
                if (raw_[idx].compare_exchange_strong(expected, item, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    size_t occ = occ_.fetch_add(1, std::memory_order_acq_rel) + 1;
                    check_hw(occ);
                    return idx;
                }
            }
            ++probes;
            if (max_probes >= 0 && probes >= max_probes) return SIZE_MAX;
            if (probes >= static_cast<int>(capacity_)) return SIZE_MAX;
            idx = (idx + 1) % capacity_;
        }
    }

    // blocking publish with timeout (ms)
    size_t publish_blocking(packed64_t item, int timeout_ms = -1) noexcept {
        auto start = std::chrono::steady_clock::now();
        while (true) {
            size_t idx = publish(item, static_cast<int>(capacity_));
            if (idx != SIZE_MAX) return idx;
            if (timeout_ms == 0) return SIZE_MAX;
            if (timeout_ms > 0) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() >= timeout_ms) return SIZE_MAX;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    }

    // consumer claim: try to claim any published slot whose rel matches rel_mask
    bool claim_one(tag8_t rel_mask, size_t &out_idx, packed64_t &out_observed, int max_scans = -1) noexcept {
        size_t start = hash_start(rel_mask);
        size_t idx = start;
        int scans = 0;
        while (true) {
            packed64_t cur = raw_[idx].load(std::memory_order_acquire);
            strel_t csr = PackedCell::extract_strel(cur);
            tag8_t st = PackedCell::st_from_strel(csr);
            if (st == ST_PUBLISHED) {
                tag8_t rel = PackedCell::rel_from_strel(csr);
                if (rel_matches(rel, rel_mask)) {
                    packed64_t desired = PackedCell::set_strel(cur, make_strel(ST_CLAIMED, rel));
                    packed64_t exp = cur;
                    if (raw_[idx].compare_exchange_strong(exp, desired, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        out_idx = idx;
                        out_observed = cur;
                        return true;
                    }
                }
            }
            ++scans;
            if (max_scans >= 0 && scans >= max_scans) return false;
            if (scans >= static_cast<int>(capacity_)) return false;
            idx = (idx + 1) % capacity_;
        }
    }

    // claim batch
    size_t claim_batch(tag8_t rel_mask, std::vector<std::pair<size_t, packed64_t>> &out, size_t max_count) noexcept {
        out.clear();
        if (max_count == 0) return 0;
        size_t start = hash_start(rel_mask);
        size_t idx = start;
        size_t scans = 0;
        while (out.size() < max_count && scans < capacity_) {
            packed64_t cur = raw_[idx].load(std::memory_order_acquire);
            strel_t csr = PackedCell::extract_strel(cur);
            if (PackedCell::st_from_strel(csr) == ST_PUBLISHED) {
                tag8_t rel = PackedCell::rel_from_strel(csr);
                if (rel_matches(rel, rel_mask)) {
                    packed64_t desired = PackedCell::set_strel(cur, make_strel(ST_CLAIMED, rel));
                    packed64_t expected = cur;
                    if (raw_[idx].compare_exchange_strong(expected, desired, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        out.emplace_back(idx, cur);
                    }
                }
            }
            ++scans;
            idx = (idx + 1) % capacity_;
        }
        return out.size();
    }

    // commit: consumer writes final packed (will set COMPLETE if not set)
    void commit_index(size_t idx, packed64_t committed) noexcept {
        if (idx >= capacity_) return;
        strel_t csr = PackedCell::extract_strel(committed);
        tag8_t st = PackedCell::st_from_strel(csr);
        tag8_t rel = PackedCell::rel_from_strel(csr);
        if (st != ST_COMPLETE) {
            if constexpr (MODE == PackedMode::MODE_VALUE32)
                committed = PackedCell::compose_value32(PackedCell::extract_value32(committed), PackedCell::extract_clk16(committed), ST_COMPLETE, rel);
            else
                committed = PackedCell::compose_clk48(PackedCell::extract_clk48(committed), ST_COMPLETE, rel);
        }
        raw_[idx].store(committed, std::memory_order_release);
        std::atomic_notify_all(&raw_[idx]);
    }

    // recycle by CPU: reset to IDLE and decrement occupancy
    packed64_t recycle(size_t idx) noexcept {
        if (idx >= capacity_) return packed64_t(0);
        packed64_t prev = raw_[idx].load(std::memory_order_acquire);
        raw_[idx].store(make_idle(), std::memory_order_release);
        occ_.fetch_sub(1, std::memory_order_acq_rel);
        return prev;
    }

    // wait for change on slot
    bool wait_slot_change(size_t idx, packed64_t expected, int timeout_ms = -1) const noexcept {
        if (idx >= capacity_) return false;
        if (timeout_ms < 0) {
            std::atomic_wait(&raw_[idx], expected);
            return true;
        }
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            packed64_t cur = raw_[idx].load(std::memory_order_acquire);
            if (cur != expected) return true;
            std::atomic_wait(&raw_[idx], expected);
        }
        return false;
    }

    // debug scan
    std::vector<size_t> find_state(tag8_t st_filter) const noexcept {
        std::vector<size_t> v;
        v.reserve(64);
        for (size_t i = 0; i < capacity_; ++i) {
            packed64_t p = raw_[i].load(std::memory_order_acquire);
            tag8_t st = PackedCell::st_from_strel(PackedCell::extract_strel(p));
            if (st == st_filter) v.push_back(i);
        }
        return v;
    }

private:
    inline packed64_t make_idle() const noexcept {
        if constexpr (MODE == PackedMode::MODE_VALUE32)
            return PackedCell::compose_value32(val32_t(0), clk16_t(0), ST_IDLE, tag8_t(0));
        else
            return PackedCell::compose_clk48(clk48_t(0), ST_IDLE, tag8_t(0));
    }

    inline size_t hash_start(tag8_t rel_mask) const noexcept {
        uint64_t key = static_cast<uint64_t>(rel_mask);
        uint64_t mixed = key * HASH_CONST;
        // compute bit width (C++20)
        unsigned bw = std::bit_width(capacity_);
        unsigned shift = (bw < 64) ? (64 - bw) : 0;
        size_t idx = static_cast<size_t>(mixed >> shift);
        if ((capacity_ & (capacity_ - 1)) != 0) idx %= capacity_;
        return idx;
    }

    inline void check_hw(size_t occ) noexcept {
        if (!cb_) return;
        if (occ * 10 >= capacity_ * 8) cb_(occ, capacity_, cb_user_);
    }

    std::atomic<packed64_t>* raw_{nullptr};
    size_t capacity_{0};
    std::atomic<size_t> occ_{0};
    std::atomic<size_t> prod_cursor_{0};
    std::atomic<size_t> cons_cursor_{0};
    HWCallback cb_{nullptr};
    void* cb_user_{nullptr};
    int node_{0};
};

} // namespace AtomicCScompact
#pragma once
// AtomicPCArray.hpp
// Single-array-of-64bit-atomics. Exposes auto pack/unpack helpers and
// a page/region relation index to look up ranges quickly by relation bitmask.

#include <atomic>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <functional>

#include "PackedCell.hpp"
#include "PackedStRel.h"
#include "AllocNW.hpp"

namespace AtomicCScompact {

template<PackedMode MODE>
class AtomicPCArray {
public:
    using packed_t = packed64_t;
    AtomicPCArray() noexcept : n_(0), meta_(nullptr), owned_bytes_(0), region_size_(0) {}
    ~AtomicPCArray() { free_all(); }

    void init_on_node(size_t n, int node, size_t alignment = 64) {
        free_all();
        if (n == 0) throw std::invalid_argument("n==0");
        std::atomic<packed_t> test{0};
        if (!test.is_lock_free()) throw std::runtime_error("atomic<packed_t> not lock-free");
        n_ = n;
        owned_bytes_ = sizeof(std::atomic<packed_t>) * n_;
        void* p = AllocNW::AlignedAllocONnode(alignment, owned_bytes_, node);
        meta_ = reinterpret_cast<std::atomic<packed_t>*>(p);
        for (size_t i = 0; i < n_; ++i) new (&meta_[i]) std::atomic<packed_t>(make_idle());
    }

    void init_from_existing(std::atomic<packed_t>* backing, size_t n) {
        free_all();
        if (!backing) throw std::invalid_argument("backing==nullptr");
        n_ = n;
        meta_ = backing;
        owned_bytes_ = 0;
    }

    void free_all() noexcept {
        if (meta_) {
            for (size_t i = 0; i < n_; ++i) meta_[i].~atomic();
            if (owned_bytes_ != 0) AllocNW::FreeONNode(static_cast<void*>(meta_), owned_bytes_);
            meta_ = nullptr;
        }
        n_ = 0;
        owned_bytes_ = 0;
        region_size_ = 0;
        region_rel_.clear();
    }

    size_t size() const noexcept { return n_; }

    // read / store helpers
    packed_t load(size_t idx, std::memory_order mo = std::memory_order_acquire) const noexcept {
        if (idx >= n_) return packed_t(0);
        return meta_[idx].load(mo);
    }
    void store(size_t idx, packed_t v, std::memory_order mo = std::memory_order_release) noexcept {
        if (idx >= n_) return;
        meta_[idx].store(v, mo);
        std::atomic_notify_all(&meta_[idx]);
    }
    bool compare_exchange(size_t idx, packed_t &expected, packed_t desired) noexcept {
        if (idx >= n_) return false;
        return meta_[idx].compare_exchange_strong(expected, desired, std::memory_order_acq_rel, std::memory_order_relaxed);
    }

    // high-level helpers auto pack/unpack so user rarely calls compose manually
    // set_value: write a value (producer). It publishes with ST_PUBLISHED and rel hint.
    void set_value(size_t idx, val32_t v, clk16_t clk, tag8_t rel) noexcept {
        packed_t p = PackedCell::compose_value32(v, clk, ST_PUBLISHED, rel);
        store(idx, p, std::memory_order_release);
    }

    // read_value: returns value & st & rel via references
    void read_value(size_t idx, val32_t &v_out, clk16_t &clk_out, tag8_t &st_out, tag8_t &rel_out) const noexcept {
        packed_t p = load(idx);
        PackedCell::decompose_value32(p, v_out, clk_out, st_out, rel_out);
    }

    // reserve/commit helpers (CAS-based)
    bool reserve_for_update(size_t idx, packed_t expected, uint16_t batch_low, tag8_t rel_hint) noexcept {
        // build pending packed based on observed expected
        packed_t pending;
        if constexpr (MODE == PackedMode::MODE_VALUE32) {
            val32_t v = PackedCell::extract_value32(expected);
            pending = PackedCell::compose_value32(v, static_cast<clk16_t>(batch_low), ST_PENDING, rel_hint);
        } else {
            clk48_t c = PackedCell::extract_clk48(expected);
            pending = PackedCell::compose_clk48(c, ST_PENDING, rel_hint);
        }
        packed_t exp = expected;
        return compare_exchange(idx, exp, pending);
    }

    bool commit_update(size_t idx, packed_t expected_pending, packed_t committed) noexcept {
        bool ok = compare_exchange(idx, expected_pending, committed);
        if (ok) std::atomic_notify_all(&meta_[idx]);
        return ok;
    }

    // region/rel index: page-based mapping for fast lookup of slots matching a relation mask.
    // init_region(size) splits array into regions of region_size and keeps an OR-rel mask per region.
    void init_region_index(size_t region_size) {
        if (region_size == 0) throw std::invalid_argument("region_size==0");
        region_size_ = region_size;
        num_regions_ = (n_ + region_size_ - 1) / region_size_;
        region_rel_.assign(num_regions_, static_cast<tag8_t>(0));
        // populate initial region_rel_
        for (size_t r = 0; r < num_regions_; ++r) {
            size_t base = r * region_size_;
            size_t end = std::min(n_, base + region_size_);
            tag8_t accum = 0;
            for (size_t i = base; i < end; ++i) {
                packed_t p = load(i);
                accum |= PackedCell::rel_from_strel(PackedCell::extract_strel(p));
            }
            region_rel_[r] = accum;
        }
    }

    // update a single slot's relation hint and update region table (atomic in-store then region OR)
    void update_rel_hint(size_t idx, tag8_t rel) noexcept {
        if (idx >= n_) return;
        packed_t p = load(idx);
        // set top-rel bits efficiently
        packed_t newp = PackedCell::set_rel(p, rel);
        store(idx, newp);
        if (region_size_) {
            size_t r = idx / region_size_;
            // atomic OR: single byte region_rel_ not atomic, but it's a best-effort hint;
            // we update with a simple fetch_or on 32-bit to avoid heavy locking.
            // Use compare_exchange loop to be safe and portable:
            tag8_t cur = region_rel_[r];
            tag8_t want = static_cast<tag8_t>(cur | rel);
            region_rel_[r] = want;
        }
    }

    // query ranges for a rel_mask: uses region index to speed up (page-based)
    std::vector<std::pair<size_t,size_t>> scan_rel_ranges(tag8_t rel_mask) const noexcept {
        std::vector<std::pair<size_t,size_t>> out;
        if (n_ == 0) return out;
        if (region_size_ == 0) {
            // fallback linear scan
            size_t i = 0;
            while (i < n_) {
                packed_t p = load(i);
                tag8_t r = PackedCell::rel_from_strel(PackedCell::extract_strel(p));
                if (!rel_matches(r, rel_mask)) { ++i; continue; }
                size_t s = i++;
                while (i < n_) {
                    p = load(i);
                    if (!rel_matches(PackedCell::rel_from_strel(PackedCell::extract_strel(p)), rel_mask)) break;
                    ++i;
                }
                out.emplace_back(s, i - s);
            }
            return out;
        }
        // region-accelerated
        size_t r = 0;
        while (r < num_regions_) {
            tag8_t rr = region_rel_[r];
            if ((rr & rel_mask) == 0) { ++r; continue; }
            // probe region element-by-element to find contiguous runs
            size_t base = r * region_size_;
            size_t end  = std::min(n_, base + region_size_);
            size_t i = base;
            while (i < end) {
                packed_t p = load(i);
                tag8_t rl = PackedCell::rel_from_strel(PackedCell::extract_strel(p));
                if (!rel_matches(rl, rel_mask)) { ++i; continue; }
                size_t s = i++;
                while (i < end) {
                    p = load(i);
                    if (!rel_matches(PackedCell::rel_from_strel(PackedCell::extract_strel(p)), rel_mask)) break;
                    ++i;
                }
                out.emplace_back(s, i - s);
            }
            ++r;
        }
        return out;
    }

    // helpers to set single slot to idle
    void set_idle(size_t idx) noexcept {
        if (idx >= n_) return;
        store(idx, make_idle());
    }

    // convenience: get discrete fields without user packing
    void get_fields(size_t idx, val32_t &v, clk16_t &clk, tag8_t &st, tag8_t &rel) const noexcept {
        packed_t p = load(idx);
        if constexpr (MODE == PackedMode::MODE_VALUE32)
            PackedCell::decompose_value32(p, v, clk, st, rel);
        else
            PackedCell::decompose_clk48(p, reinterpret_cast<clk48_t&>(clk), st, rel); // caution: clk out truncated for MODE_CLK48
    }

private:
    inline packed_t make_idle() const noexcept {
        if constexpr (MODE == PackedMode::MODE_VALUE32)
            return PackedCell::compose_value32(val32_t(0), clk16_t(0), ST_IDLE, tag8_t(0));
        else
            return PackedCell::compose_clk48(clk48_t(0), ST_IDLE, tag8_t(0));
    }

    size_t n_{0};
    std::atomic<packed_t>* meta_{nullptr};
    size_t owned_bytes_{0};

    // region index
    size_t region_size_{0};
    size_t num_regions_{0};
    std::vector<tag8_t> region_rel_;

    // memory node
    int node_{0};
};

} // namespace AtomicCScompact
