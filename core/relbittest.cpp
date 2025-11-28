#include "relbittest.h"

// Optional simple timer (kept from your original)
struct SimpleTimer {
    std::chrono::time_point<std::chrono::high_resolution_clock> t0;
    void start() { t0 = std::chrono::high_resolution_clock::now(); }
    double stopms() {
        auto t1 = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(t1 - t0).count();
    }
};

template<typename LaneT>
void TestRBA<LaneT>::init(size_t lanes_)
{
    lanes = lanes_;
    V.assign(lanes, (lane_t)0);
    inV.assign(lanes, ~((lane_t)0));
    st.assign(lanes, (lane_t)0);
    rel.assign(lanes, (lane_t)0);
}

template<typename LaneT>
TestRBA<LaneT> TestRBA<LaneT>::MakeFrBits(size_t bits)
{
    size_t perplane = sizeof(lane_t) * BITS_PER_BYTE;
    size_t lanes = (bits + perplane - 1) / perplane;
    return TestRBA(lanes);
}

template<typename LaneT>
void TestRBA<LaneT>::normalize()
{
    for (size_t i = 0; i < lanes; i++) {
        inV[i] = ~(V[i]);
    }
}

template<typename LaneT>
size_t TestRBA<LaneT>::CheckInvarients() const
{
    size_t bad = 0;
    for (size_t i = 0; i < lanes; i++) {
        if (inV[i] != ~(V[i])) {
            bad++;
        }
    }
    return bad;
}

template<typename LaneT>
void TestRBA<LaneT>::RIAnd(const TestRBA &A, const TestRBA &B, TestRBA &out)
{
    assert(A.lanes == B.lanes && B.lanes == out.lanes);
    size_t n = A.lanes;
    for (size_t i = 0; i < n; i++) {
        out.V[i] = A.V[i] & B.V[i];
        out.inV[i] = ~(out.V[i]);
        out.st[i] = A.st[i] | B.st[i];
        out.rel[i] = A.rel[i] | B.rel[i];
    }
}

template<typename LaneT>
void TestRBA<LaneT>::RIOr(const TestRBA &A, const TestRBA &B, TestRBA &out)
{
    assert(A.lanes == B.lanes && B.lanes == out.lanes);
    size_t n = A.lanes;
    for (size_t i = 0; i < n; i++) {
        out.V[i] = A.V[i] | B.V[i];
        out.inV[i] = ~(out.V[i]);
        out.st[i] = A.st[i] | B.st[i];
        out.rel[i] = A.rel[i] | B.rel[i];
    }
}

template<typename LaneT>
void TestRBA<LaneT>::RIXOr(const TestRBA &A, const TestRBA &B, TestRBA &out)
{
    assert(A.lanes == B.lanes && B.lanes == out.lanes);
    size_t n = A.lanes;
    for (size_t i = 0; i < n; i++) {
        out.V[i] = A.V[i] ^ B.V[i];
        out.inV[i] = ~(out.V[i]);
        out.st[i] = A.st[i] | B.st[i];
        out.rel[i] = A.rel[i] | B.rel[i];
    }
}

template<typename LaneT>
void TestRBA<LaneT>::RINot(const TestRBA &A, TestRBA &out)
{
    assert(A.lanes == out.lanes);
    size_t n = A.lanes;
    for (size_t i = 0; i < n; i++) {
        out.V[i] = ~(A.V[i]);
        out.inV[i] = ~(out.V[i]);
        out.st[i] = std::numeric_limits<lane_t>::max();
        out.rel[i] = A.rel[i];
    }
}

template<typename LaneT>
void TestRBA<LaneT>::RINOr(const TestRBA &A, const TestRBA &B, TestRBA &out)
{
    assert(A.lanes == B.lanes && B.lanes == out.lanes);
    size_t n = A.lanes;
    for (size_t i = 0; i < n; i++) {
        out.V[i] = ~(A.V[i] | B.V[i]);
        out.inV[i] = ~(out.V[i]);
        out.st[i] = A.st[i] | B.st[i];
        out.rel[i] = A.rel[i] | B.rel[i];
    }
}

/* -------------------------
   IMPORTANT: explicit instantiations for the template types used by your program
   If your main.cpp uses TestRBA<unsigned long> (as the linker error showed),
   you must explicitly instantiate that here (or move implementations to header).
   ------------------------- */
template class TestRBA<uint64_t>;
