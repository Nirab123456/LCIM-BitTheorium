#include "core/relbittest.h"
#include <iostream>
#include <vector>

char* mainTAG = "main.cpp";

std::vector<uint32_t> RelSievePrime(uint32_t N)
{
    if (N < 2) return {};

    using lane_t = uint64_t;
    constexpr uint32_t BITS = sizeof(lane_t) * 8;
    size_t lanesNeeded = (N + 1 + BITS - 1) / BITS; // include index N
    TestRBA<lane_t> R(lanesNeeded);

    // set initial mask: mark numbers >=2 as candidates
    for (size_t i = 0; i < R.lanes; ++i) {
        lane_t word = 0;
        size_t base = i * BITS;
        for (uint32_t b = 0; b < BITS; ++b) {
            uint32_t num = base + b;
            if (num >= 2 && num <= N) word |= (lane_t(1) << b);
        }
        R.WriteLane(i, word);
    }

    auto is_set = [&](uint32_t x)->bool {
        size_t lane = x / BITS;
        uint32_t bit = x % BITS;
        return ((R.V[lane] >> bit) & 1ull) != 0;
    };
    auto clear_bit = [&](uint32_t x) {
        size_t lane = x / BITS;
        uint32_t bit = x % BITS;
        R.V[lane] &= ~(lane_t(1) << bit);
        R.inV[lane] = ~R.V[lane];
        R.st[lane] = ~((lane_t)0);
    };

    uint32_t lim = static_cast<uint32_t>(std::floor(std::sqrt((double)N)));
    for (uint32_t p = 2; p <= lim; ++p) {
        if (!is_set(p)) continue;
        uint32_t start = p * p;
        for (uint32_t m = start; m <= N; m += p) {
            clear_bit(m);
        }
    }

    std::vector<uint32_t> primes;
    for (uint32_t i = 2; i <= N; ++i) if (is_set(i)) primes.push_back(i);
    return primes;
}

void SmallUnitTests()
{
    std::cout<< mainTAG << "->SmallUnitTests():Correctness Test"<<std::endl;
    TestRBA<uint32_t> A(16), B(16), out(16);
    for (size_t i = 0; i < 16; ++i) {
        A.WriteLane(i, uint32_t( (i*2654435761u) ^ 0xAAAAAAAAu ));
        B.WriteLane(i, uint32_t( (i*747796405u) ^ 0x55555555u ));
    }
    TestRBA<uint32_t>::RIAnd(A, B, out);
    for (size_t i = 0; i < 16; i++)
    {
        assert(out.inV[i] == ~(out.V[i]));
        assert(out.inV[i] == (A.V[i] & B.V[i]));
    }
    std::cout << mainTAG << "->SmallUnitTests()\n";
}

void RIAnd_MicroBenchmark(size_t lanes = 10 * 1000 * 1000 / 8)
{
    using lane_t = uint64_t;
    std::cout << mainTAG << "->RIAnd_MicroBenchmark()::number of lanes = " << lanes << std::endl;
    TestRBA<lane_t> A(lanes), B(lanes), out(lanes);
    for (size_t i = 0; i < lanes; ++i) {
        A.V[i] = ((uint64_t)i * 6364136223846793005ull) ^ 0xFFFFFFFFFFFFFFFFull;
        B.V[i] = ((uint64_t)i * 1442695040888963407ull);
        A.inV[i] = ~A.V[i]; B.inV[i] = ~B.V[i];
        A.st[i] = 0; B.st[i] = 0;
        A.rel[i] = (i&1) ? ~((lane_t)0) : 0;
        B.rel[i] = (i&2) ? ~((lane_t)0) : 0;
    }
    SimpleTimer t;
    t.start();
    size_t runs = 3;
    for (size_t i = 0; i < runs; i++)
    {
        TestRBA<lane_t>::RIAnd(A, B, out);
    }
    double ms = t.stopms() / runs;
    double bytes = double(lanes) * sizeof(lane_t) * 6;
    double bytes_per_ms = bytes / ms;
    std::cout << mainTAG << std::fixed << std::setprecision(3);
    std::cout << "[bench] AND average time: " << ms << " ms (approx throughput: " << (bytes_per_ms*1000.0/1024.0/1024.0) << " MB/s)\n";
}

void InjFaultDemo()
{
    std::cout << mainTAG << "->InjFaultDemo(): Starting......";
    using lane_t = uint32_t;
    TestRBA<lane_t>R(8);
    size_t bad_lane = 3;
    lane_t flip = 1u << 0;
    std::cout<< mainTAG << "->InjFaultDemo():Bad count before injection = " << R.CheckInvarients() << "\n";
    R.InjectFaultBits(bad_lane, flip);
    size_t bad_after = R.CheckInvarients();
    std::cout << mainTAG << "->InjFaultDemo():[fault] after injection, invariants mismatches: " << bad_after << "\n";
    assert(bad_after >= 1);
    R.normalize();
    std::cout << mainTAG << "->InjFaultDemo():[fault] after normalize, mismatches: " << R.CheckInvarients() << " (should be 0)\n";
}


template<typename LaneT>
size_t CountRELTags(const TestRBA<LaneT> &R)
{
    size_t c = 0;
    for (size_t i = 0; i < R.lanes; i++)
    {
        if (R.rel[i] != 0)
        {
            c++;
        }
    }
    return c;
}

int main(int argc, char** argv)
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout << "=== RelBit single-file prototype ===\n";
    std::cout << "sizeof lanes: 32-bit lane = " << sizeof(uint32_t) << "  bytes; 64-bit lane = " << sizeof(uint64_t) << " bytes\n";
    std::cout << "Starting unit tests and demos...\n\n";
    SmallUnitTests();
    {
        uint32_t N = 20000;
        if (argc > 1)
        {
            try
            {
                N = (uint32_t)std::stoul(argv[1]);
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
        }
        std::cout << mainTAG << "->main()Computing primes up to " << N << " using RelBit sieve...\n";
        SimpleTimer t; t.start();
        auto primes = RelSievePrime(N);
        double ms = t.stopms();
        std::cout <<mainTAG << "->main():[demo] found " << primes.size() << " primes in " << std::fixed << std::setprecision(3) << ms/1000.0 << " s\n";
        std::cout << "[demo] first 20 primes: ";
        for (size_t i = 0; i < std::min<size_t>(20, primes.size()); ++i) {
            std::cout << primes[i] << (i+1==std::min<size_t>(20,primes.size()) ? "\n" : ", ");
        }        
    }
    RIAnd_MicroBenchmark();
    InjFaultDemo();
    TestRBA<uint64_t>R(16);
    for (size_t i = 0; i < R.lanes; i++)
    {
        R.WriteLane(i,(uint64_t)i);
        if ((i & 3) == 0)
        {
            R.rel[i] = ~((uint64_t)0);
        }
    }
    std::cout << "[demo] relation-tagged lanes: " << CountRELTags(R) << " / " << R.lanes << "\n";
    return 0;
    
}