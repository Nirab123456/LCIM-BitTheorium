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

    // helpers
    void ClearState() { std::fill(st.begin(), st.end(), (lane_t)0); }
    void MarkAllState() { std::fill(st.begin(), st.end(), std::numeric_limits<lane_t>::max()); }
    void ClearRelation() { std::fill(rel.begin(), rel.end(), (lane_t)0); }
    void MarkAllRelation() { std::fill(rel.begin(), rel.end(), std::numeric_limits<lane_t>::max()); }
};
