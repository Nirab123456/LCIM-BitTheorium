#pragma once
#include <bits/stdc++.h>
#include <thread>
#include <chrono>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>

template<typename LaneT = uint64_t>
class TestRBA
{
public:
    using lane_t = LaneT;
    size_t lanes;
    std::vector<lane_t> V;
    std::vector<lane_t> inV;
    std::vector<lane_t> st;
    std::vector<lane_t> rel;

    static constexpr uint8_t BITS_PER_BYTE = 8;

    TestRBA(): lanes(0) {}
    TestRBA(size_t lanes_) { init(lanes_); }

    void init(size_t lanes_);
    static TestRBA MakeFrBits(size_t bits);
    void normalize();
    size_t CheckInvarients() const;

    static void RIAnd(const TestRBA &A, const TestRBA &B, TestRBA &out);
    static void RIOr(const TestRBA &A, const TestRBA &B, TestRBA &out);
    static void RIXOr(const TestRBA &A, const TestRBA &B, TestRBA &out);
    static void RINOr(const TestRBA &A, const TestRBA &B, TestRBA &out);
    static void RINot(const TestRBA &A, TestRBA &out);
    static void AddNoCarry(const TestRBA &A, const TestRBA &B, TestRBA &out);
    
    void WriteLane(size_t idx, lane_t new_v, lane_t new_rel = (lane_t)0);
    void InjectFaultBits(size_t lane_idx, lane_t flip_mask);
    void AtomicWriteLaneEmulated(size_t idx, lane_t new_v, lane_t new_rel = 0)

    // helpers
    void ClearState() { std::fill(st.begin(), st.end(), (lane_t)0); }
    void MarkAllState() { std::fill(st.begin(), st.end(), std::numeric_limits<lane_t>::max()); }
    void ClearRelation() { std::fill(rel.begin(), rel.end(), (lane_t)0); }
    void MarkAllRelation() { std::fill(rel.begin(), rel.end(), std::numeric_limits<lane_t>::max()); }
    void debug_print(size_t count = 4) const {
        for (size_t i = 0; i < min(count, lanes); ++i) {
            cout << "lane[" << i << "] v=0x" << hex << v[i] << " inv=0x" << inv[i]
                 << " st=0x" << st[i] << " rel=0x" << rel[i] << dec << "\n";
        }
    }};


// Optional simple timer (kept from your original)
struct SimpleTimer {
    std::chrono::time_point<std::chrono::high_resolution_clock> t0;
    void start() { t0 = std::chrono::high_resolution_clock::now(); }
    double stopms() {
        auto t1 = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(t1 - t0).count();
    }
};
