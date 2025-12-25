#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <cassert>
#include <random>

#include "AtomicCSCompact.h"


using namespace std::chrono;
using namespace AtomicCScompact;

using Table = PackedACarray<32, 8, 16, uint64_t>;

static void printbanner() {
    std::cout << "PackedACarray test harness\n";
    std::cout << " - single-threaded CommitStore test\n";
    std::cout << " - multi-threaded writeCAS stress test\n\n";
}



void SingleThreadTest(Table &t, std::size_t N)
{
    std::cout << "[SINGLE TRREAD TEST]:: N = " << N << "\n";
    auto t0 = high_resolution_clock::now();
    for (size_t i = 0; i < N; i++)
    {
        uint32_t v = static_cast<uint32_t> (i + 1);
        t.CommitStore(i, v, 1, 2, std::memory_order_release);
    }
    auto t1 = high_resolution_clock::now();
    double elepsedms = duration_cast<duration<double, std::milli>>(t1 - t0).count();
    std::cout << " CommitStore " << N << "entries in " << elepsedms << " ms"
        << " (" << (N / (elepsedms / 1000.0)) << " ops/s)\n";


    bool ok = true;
    for (size_t i = 0; i < (std::min<std::size_t>(10,N)); i++)
    {
        auto fv = t.Read(i, std:: memory_order_acquire);
        if (!fv)
        {
            ok = false;
            break;

        }
        if (fv->value != static_cast<uint32_t>(i + 1) || fv->st != 1 || fv->rel != 2)
        {
            ok = false;
            break;
        }
    }

    std::cout << " sanity check" << (ok ? "pass" : "fail") << "\n" << std::endl;
}

void Casworker(Table &t, size_t idxRange, size_t ops, std::atomic<size_t> &successCount, unsigned seed)
{
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<size_t>dist(0, idxRange -1);

    for (size_t i = 0; i < ops; i++)
    {
        size_t idx = idx + dist(rng);
        uint32_t newv = static_cast<uint32_t>((seed & 0xffff) ^ static_cast<uint32_t>(i));
        bool ok = t.writeCAS(idx, newv, std::optional<Table::strl_t>(1), std::optional<Table::strl_t>(2));

        if (ok)
        {
            successCount.fetch_add(1, std::memory_order_relaxed);
        }
    }
}


void multiThreadTest(Table &t, size_t N, unsigned thc, size_t opsPerThread)
{
    std::cout << "[MULTI THREAD TEST] N = " << N << ", THREADS = " << thc << ", OPS/THREAD = " << opsPerThread << "\n";
    std::vector<std::thread> workers;
    std::atomic<size_t> success{0};
    auto t0 = high_resolution_clock::now();

    std::size_t block = (N + thc -1) / thc;
    for (size_t i = 0; i < thc; i++)
    {
        size_t base = i * block;
        size_t range = std::min<std::size_t>(block, (base + block > N) ? (N - base) : block);
        if (range == 0)
        {
            continue;
        }
        workers.emplace_back(Casworker, std::ref(t), base, range, opsPerThread, std::ref(success), static_cast<unsigned>(1234 + i));
    }
    
    for(auto &i : workers)
    {
        i.join();
    }
    auto t1 = high_resolution_clock::now();
    double elepsedS = duration_cast<duration <double>>(t1 - t0).count();
    size_t totalops = opsPerThread * workers.size();
    std::cout << " thread attempted ops : " << totalops << "\n";
    std::cout << " Successful CAS updates : " << success.load() << "\n";
    std::cout << " elepsed : " << elepsedS << "s, throughput : " << (totalops / elepsedS) << "ops/s\n\n"; 
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    printbanner();
    const size_t N = 1 << 20;
    Table t;

    SingleThreadTest(t, 100000);
    unsigned threads = std::max<unsigned>(2, std::thread::hardware_concurrency());
    size_t opt = 5000;
    multiThreadTest(t, N, threads, opt);

    std::cout << " Sampling a few entries after multi threaded run \n";
    for (size_t i = 0; i < 10; i++)
    {
        size_t idx = i * 1000;
        auto fv = t.Read(idx, std::memory_order_acquire);
        if (fv)
        {
            std::cout << " idx = " << idx << " value = " << +fv->value << " st = " << +fv->st << " rel = " << +fv->rel << " clk = " << +fv->clk;
        }
        else
        {
            std::cout<< "idx = " << idx << " rwead returned empty \n";
        }
        
    }

    t.free_all();
    std::cout << "DONE\n";
    return 0;
    
}