#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"

#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <string_view>
#include <syncstream>
#include <thread>
#include <vector>

namespace TestSpace1
{
    using namespace BidirectionalInMemGraph;

    using Clock = std::chrono::steady_clock;
    using Microseconds = std::chrono::microseconds;

    struct TrialResult
    {
        bool Ok = false;
        uint64_t SharedCount = 0;
        uint64_t WeightedSum = 0;
        uint64_t DoneCount = 0;
        int64_t ElapsedUs = 0;
    };

    static void Jitter(uint32_t seed, uint32_t max_us = 100u)
    {
        std::minstd_rand rng(seed * 48271u + 17u);
        std::uniform_int_distribution<uint32_t> dist(0u, max_us);
        std::this_thread::sleep_for(Microseconds(dist(rng)));
    }

    template <class Fn>
    static bool TimedBool(std::string_view name, Fn&& fn)
    {
        const auto begin = Clock::now();
        const bool ok = fn();
        const auto end = Clock::now();

        std::osyncstream(std::cout)
            << name << " : " << (ok ? "PASS" : "FAIL")
            << "  "
            << std::chrono::duration_cast<Microseconds>(end - begin).count()
            << " us\n";

        return ok;
    }

    static bool VectorFetchAdd(
        std::vector<uint64_t>& words,
        size_t idx,
        uint64_t delta,
        uint32_t max_tries = 1'000'000u)
    {
        std::atomic_ref<uint64_t> target(words[idx]);
        uint64_t expected = target.load(std::memory_order_acquire);

        for (uint32_t attempt = 0; attempt < max_tries; ++attempt)
        {
            if (expected > UINT64_MAX - delta)
            {
                return false;
            }

            const uint64_t desired = expected + delta;

            if (target.compare_exchange_weak(
                    expected,
                    desired,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire))
            {
                return true;
            }
        }

        return false;
    }

    static bool APCFetchAdd(
        AdaptivePackedCellContainer& apc,
        uint32_t local_idx,
        uint64_t delta,
        uint32_t max_tries = 1'000'000u)
    {
        for (uint32_t attempt = 0; attempt < max_tries; ++attempt)
        {
            uint64_t observed = 0u;
            if (!apc.AtomicallyReadLongLongAPCUnit(local_idx, observed))
            {
                return false;
            }

            if (observed > UINT64_MAX - delta)
            {
                return false;
            }

            uint64_t expected = observed;
            if (apc.CompareExchangeStrongFromAPC(
                    local_idx,
                    expected,
                    observed + delta,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire))
            {
                return true;
            }
        }

        return false;
    }

    static TrialResult RunVectorSharedTrial(
        uint32_t worker_count,
        uint32_t iterations)
    {
        constexpr size_t START = 0u;
        constexpr size_t COUNT = 1u;
        constexpr size_t SUM = 2u;
        constexpr size_t DONE = 3u;
        constexpr size_t WORKER_BASE = 4u;

        std::vector<uint64_t> words(WORKER_BASE + worker_count, 0u);
        std::atomic<bool> failed{false};
        std::vector<std::thread> workers;
        workers.reserve(worker_count);

        for (uint32_t worker = 0u; worker < worker_count; ++worker)
        {
            workers.emplace_back([&, worker]
            {
                std::atomic_ref<uint64_t> start(words[START]);

                while (start.load(std::memory_order_acquire) == 0u)
                {
                    std::this_thread::yield();
                }

                for (uint32_t i = 0u; i < iterations; ++i)
                {
                    if (
                        !VectorFetchAdd(words, COUNT, 1u) ||
                        !VectorFetchAdd(words, SUM, static_cast<uint64_t>(worker + 1u))
                    )
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    if ((i & 1023u) == 0u)
                    {
                        Jitter(worker * 1009u + i, 30u);
                    }
                }

                std::atomic_ref<uint64_t> own(words[WORKER_BASE + worker]);
                own.store(iterations, std::memory_order_release);

                if (!VectorFetchAdd(words, DONE, 1u))
                {
                    failed.store(true, std::memory_order_release);
                }
            });
        }

        const auto begin = Clock::now();
        std::atomic_ref<uint64_t>(words[START]).store(1u, std::memory_order_release);

        const auto deadline = begin + std::chrono::seconds(10);
        std::atomic_ref<uint64_t> done(words[DONE]);

        while (done.load(std::memory_order_acquire) != worker_count)
        {
            if (failed.load(std::memory_order_acquire) || Clock::now() >= deadline)
            {
                failed.store(true, std::memory_order_release);
                break;
            }
            std::this_thread::yield();
        }

        for (std::thread& thread : workers)
        {
            thread.join();
        }

        const auto end = Clock::now();

        TrialResult result{};
        result.SharedCount = std::atomic_ref<uint64_t>(words[COUNT]).load(std::memory_order_acquire);
        result.WeightedSum = std::atomic_ref<uint64_t>(words[SUM]).load(std::memory_order_acquire);
        result.DoneCount = done.load(std::memory_order_acquire);
        result.ElapsedUs = std::chrono::duration_cast<Microseconds>(end - begin).count();

        const uint64_t expected_count = static_cast<uint64_t>(worker_count) * iterations;
        const uint64_t expected_sum = static_cast<uint64_t>(iterations) *
            (static_cast<uint64_t>(worker_count) * (worker_count + 1u) / 2u);

        bool workers_ok = true;
        for (uint32_t worker = 0u; worker < worker_count; ++worker)
        {
            workers_ok = workers_ok &&
                std::atomic_ref<uint64_t>(words[WORKER_BASE + worker]).load(std::memory_order_acquire) == iterations;
        }

        result.Ok =
            !failed.load(std::memory_order_acquire) &&
            result.SharedCount == expected_count &&
            result.WeightedSum == expected_sum &&
            result.DoneCount == worker_count &&
            workers_ok;

        return result;
    }

    struct APCRegionBounds
    {
        uint32_t Begin = 0u;
        uint32_t End = 0u;
        bool Valid = false;
    };

    static APCRegionBounds GetStateRegionBounds(
        AdaptivePackedCellContainer& apc)
    {
        uint64_t packed = FABRIC_CELL_SENTINAL;

        if (!apc.ReadAPCMetaUnit(
                HeaderIdentifierOfAPC::STATE_BOUNDS,
                packed,
                true))
        {
            return {};
        }

        auto carrier = LayoutBoundsOrchestrator::GetLayoutCarrierFromValidLayoutCell(
            packed,
            MacroColumnOfAPC::STATE_SLOT
        );

        if (!carrier.IsValid || carrier.BeginIndex >= carrier.EndIndex)
        {
            return {};
        }

        return APCRegionBounds{
            carrier.BeginIndex,
            carrier.EndIndex,
            true
        };
    }

    static TrialResult RunAPCSharedTrial(
        AdaptivePackedCellContainer& apc,
        uint32_t worker_count,
        uint32_t iterations)
    {
        constexpr uint32_t START_OFF = 0u;
        constexpr uint32_t COUNT_OFF = 1u;
        constexpr uint32_t SUM_OFF = 2u;
        constexpr uint32_t DONE_OFF = 3u;
        constexpr uint32_t WORKER_BASE_OFF = 4u;

        const APCRegionBounds bounds = GetStateRegionBounds(apc);
        if (
            !bounds.Valid ||
            bounds.End - bounds.Begin < WORKER_BASE_OFF + worker_count
        )
        {
            return {};
        }

        const uint32_t START = bounds.Begin + START_OFF;
        const uint32_t COUNT = bounds.Begin + COUNT_OFF;
        const uint32_t SUM = bounds.Begin + SUM_OFF;
        const uint32_t DONE = bounds.Begin + DONE_OFF;
        const uint32_t WORKER_BASE = bounds.Begin + WORKER_BASE_OFF;

        for (uint32_t i = START; i < WORKER_BASE + worker_count; ++i)
        {
            apc.AtomicallyWriteU64ToAPC(i, 0u);
        }

        std::atomic<bool> failed{false};
        std::vector<std::thread> workers;
        workers.reserve(worker_count);

        for (uint32_t worker = 0u; worker < worker_count; ++worker)
        {
            workers.emplace_back([&, worker]
            {
                for (;;)
                {
                    uint64_t start = 0u;
                    if (!apc.AtomicallyReadLongLongAPCUnit(START, start))
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    if (start != 0u)
                    {
                        break;
                    }

                    std::this_thread::yield();
                }

                for (uint32_t i = 0u; i < iterations; ++i)
                {
                    if (
                        !APCFetchAdd(apc, COUNT, 1u) ||
                        !APCFetchAdd(apc, SUM, static_cast<uint64_t>(worker + 1u))
                    )
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    if ((i & 1023u) == 0u)
                    {
                        Jitter(worker * 1009u + i, 30u);
                    }
                }

                apc.AtomicallyWriteU64ToAPC(WORKER_BASE + worker, iterations);

                if (!APCFetchAdd(apc, DONE, 1u))
                {
                    failed.store(true, std::memory_order_release);
                }
            });
        }

        const auto begin = Clock::now();
        apc.AtomicallyWriteU64ToAPC(START, 1u);

        const auto deadline = begin + std::chrono::seconds(10);
        uint64_t done = 0u;

        for (;;)
        {
            if (!apc.AtomicallyReadLongLongAPCUnit(DONE, done))
            {
                failed.store(true, std::memory_order_release);
                break;
            }

            if (done == worker_count)
            {
                break;
            }

            if (failed.load(std::memory_order_acquire) || Clock::now() >= deadline)
            {
                failed.store(true, std::memory_order_release);
                break;
            }

            std::this_thread::yield();
        }

        for (std::thread& thread : workers)
        {
            thread.join();
        }

        const auto end = Clock::now();

        TrialResult result{};
        uint64_t count = 0u;
        uint64_t sum = 0u;
        apc.AtomicallyReadLongLongAPCUnit(COUNT, count);
        apc.AtomicallyReadLongLongAPCUnit(SUM, sum);
        apc.AtomicallyReadLongLongAPCUnit(DONE, done);

        result.SharedCount = count;
        result.WeightedSum = sum;
        result.DoneCount = done;
        result.ElapsedUs = std::chrono::duration_cast<Microseconds>(end - begin).count();

        const uint64_t expected_count = static_cast<uint64_t>(worker_count) * iterations;
        const uint64_t expected_sum = static_cast<uint64_t>(iterations) *
            (static_cast<uint64_t>(worker_count) * (worker_count + 1u) / 2u);

        bool workers_ok = true;
        for (uint32_t worker = 0u; worker < worker_count; ++worker)
        {
            uint64_t worker_value = 0u;
            workers_ok = workers_ok &&
                apc.AtomicallyReadLongLongAPCUnit(WORKER_BASE + worker, worker_value) &&
                worker_value == iterations;
        }

        result.Ok =
            !failed.load(std::memory_order_acquire) &&
            result.SharedCount == expected_count &&
            result.WeightedSum == expected_sum &&
            result.DoneCount == worker_count &&
            workers_ok;

        return result;
    }

    struct AxisSnapshot
    {
        uint32_t Slot = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint64_t OwnedEdge = FABRIC_CELL_SENTINAL;
        uint64_t InheritedEdge = FABRIC_CELL_SENTINAL;
        uint64_t RootFirstChild = FABRIC_CELL_SENTINAL;
        uint64_t Previous = FABRIC_CELL_SENTINAL;
        uint64_t Next = FABRIC_CELL_SENTINAL;
        bool Live = false;
    };

    static bool ReadAxisSnapshot(
        VagueTemoraryPremativeFabric& fabric,
        uint32_t slot,
        InstallAxisToBuffer::BidirectionalAxis axis,
        AxisSnapshot& out)
    {
        InstallAxisToBuffer::BufferOfAPCIdentity identity{};
        const auto state = fabric.ReadIdentityBufferOfAPC(slot, identity);

        if (!state.has_value() || state.value() != StateOfAPC::LIVE)
        {
            return false;
        }

        const auto map = InstallAxisToBuffer::ConstructAxisMap(axis);

        out.Slot = slot;
        out.OwnedEdge = InstallAxisToBuffer::ValueOfAnIdentityFromBuffer(
            identity, map.OwnedEgdeTableIdx);
        out.InheritedEdge = InstallAxisToBuffer::ValueOfAnIdentityFromBuffer(
            identity, map.InheritedEgdeTableIdx);
        out.RootFirstChild = InstallAxisToBuffer::ValueOfAnIdentityFromBuffer(
            identity, map.RootOwnedChild);
        out.Previous = InstallAxisToBuffer::ValueOfAnIdentityFromBuffer(
            identity, map.PreviousSibling);
        out.Next = InstallAxisToBuffer::ValueOfAnIdentityFromBuffer(
            identity, map.NextSibling);
        out.Live = true;
        return true;
    }

    static bool ValidateAxisChain(
        VagueTemoraryPremativeFabric& fabric,
        uint32_t root,
        const std::vector<uint32_t>& children,
        InstallAxisToBuffer::BidirectionalAxis axis)
    {
        AxisSnapshot root_snapshot{};
        if (!ReadAxisSnapshot(fabric, root, axis, root_snapshot))
        {
            return false;
        }

        if (!APCDataStructure::IsValid32BitAPCUnit(root_snapshot.OwnedEdge))
        {
            return false;
        }

        if (children.empty())
        {
            return root_snapshot.RootFirstChild == FABRIC_CELL_SENTINAL;
        }

        if (root_snapshot.RootFirstChild != children.front())
        {
            return false;
        }

        const uint64_t expected_edge = root_snapshot.OwnedEdge;

        for (size_t i = 0u; i < children.size(); ++i)
        {
            AxisSnapshot child{};
            if (!ReadAxisSnapshot(fabric, children[i], axis, child))
            {
                return false;
            }

            const uint64_t expected_previous =
                (i == 0u) ? root : children[i - 1u];

            const uint64_t expected_next =
                (i + 1u < children.size()) ? children[i + 1u] : FABRIC_CELL_SENTINAL;

            if (
                child.InheritedEdge != expected_edge ||
                child.Previous != expected_previous ||
                child.Next != expected_next
            )
            {
                return false;
            }
        }

        return true;
    }

    static bool ValidateAllGraphLocksReleased(
        VagueTemoraryPremativeFabric& fabric,
        const std::vector<uint32_t>& slots)
    {
        for (uint32_t slot : slots)
        {
            InstallAxisToBuffer::GraphMutationValues values{};
            if (
                !fabric.ReadGraphMutationFlags(slot, values) ||
                !InstallAxisToBuffer::IsIdentityGraphUnlocked(values.Flags)
            )
            {
                return false;
            }
        }

        return true;
    }

    static bool BuildSingleAtomicStateConfig(
        LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& layout,
        SchemaDefinition::InitialRegionalDtypeConf& dtype,
        SchemaDefinition::InitialRegionalProtocol& protocol)
    {
        layout.FeedForward = 0u;
        layout.FeedBackward = 0u;
        layout.Lateral = 0u;
        layout.StateSlot = 1u;
        layout.ErrorSlot = 0u;
        layout.Weightless = 0u;
        layout.WeightSlot = 0u;
        layout.AUXSlot = 0u;
        layout.HeterogenousPtr = 0u;
        layout.FreeSlot = 0u;

        dtype.STATE_SLOT = SchemaDefinition::DataTypeOfMacroColumn::UINT64_T;
        protocol.STATE_SLOT = SchemaDefinition::SchemaProtocols::ATOMIC_WORD_ARRAY;
        return true;
    }

    static bool RunGraphMutationTest()
    {
        using Axis = InstallAxisToBuffer::BidirectionalAxis;
        using Inheritance = InstallAxisToBuffer::DescOfInharitance;

        constexpr uint32_t SLOT_COUNT = 32u;
        constexpr uint32_t SLOT_WORDS = 512u;
        constexpr size_t NODE_COUNT = 7u;

        enum Node : size_t
        {
            R = 0u,
            A,
            B,
            C,
            N,
            D,
            E
        };

        VagueTemoraryPremativeFabric fabric;

        if (!fabric.InitializeFabricWithPtrTable(
                SLOT_COUNT,
                SLOT_WORDS,
                APCDataStructure::BRANCH_VERSION,
                CoreOfFabricCoordinator::DEFAULT_THREAD_TABLE_CAPACITY))
        {
            std::cout << "InitializeFabricWithPtrTable : FAIL\n";
            return false;
        }

        LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier layout{};
        SchemaDefinition::InitialRegionalDtypeConf dtype{};
        SchemaDefinition::InitialRegionalProtocol protocol{};

        if (!BuildSingleAtomicStateConfig(layout, dtype, protocol))
        {
            return false;
        }

        std::array<std::unique_ptr<AdaptivePackedCellContainer>, NODE_COUNT> nodes{};
        std::array<uint32_t, NODE_COUNT> slots{};
        slots.fill(APCDataStructure::APC_INDEX_BOUND_SENTINAL);

        for (auto& node : nodes)
        {
            node = std::make_unique<AdaptivePackedCellContainer>();
        }

        if (!TimedBool("create R with H+V roots", [&]
        {
            return fabric.CreateAPC(
                *nodes[R],
                true,
                true,
                layout,
                dtype,
                protocol,
                APCDataStructure::BRANCH_VERSION
            );
        }))
        {
            return false;
        }

        uint64_t root_slot = FABRIC_CELL_SENTINAL;
        if (!nodes[R]->GetThisSlotIdx(root_slot))
        {
            return false;
        }
        slots[R] = static_cast<uint32_t>(root_slot);

        std::atomic<bool> create_failed{false};
        std::barrier create_start(static_cast<std::ptrdiff_t>(NODE_COUNT));
        std::vector<std::thread> creators;
        creators.reserve(NODE_COUNT - 1u);

        for (size_t node_idx = 1u; node_idx < NODE_COUNT; ++node_idx)
        {
            creators.emplace_back([&, node_idx]
            {
                create_start.arrive_and_wait();
                Jitter(static_cast<uint32_t>(100u + node_idx), 250u);

                const bool wants_h_root = node_idx == A;
                const bool wants_v_root = node_idx == D;

                const auto begin = Clock::now();
                const bool created = fabric.CreateAPC(
                    *nodes[node_idx],
                    wants_h_root,
                    wants_v_root,
                    layout,
                    dtype,
                    protocol,
                    APCDataStructure::BRANCH_VERSION
                );
                const auto end = Clock::now();

                uint64_t slot = FABRIC_CELL_SENTINAL;
                const bool have_slot = created && nodes[node_idx]->GetThisSlotIdx(slot);

                if (have_slot)
                {
                    slots[node_idx] = static_cast<uint32_t>(slot);
                }
                else
                {
                    create_failed.store(true, std::memory_order_release);
                }

                std::osyncstream(std::cout)
                    << "create node " << node_idx
                    << " : " << (have_slot ? "PASS" : "FAIL")
                    << "  "
                    << std::chrono::duration_cast<Microseconds>(end - begin).count()
                    << " us\n";
            });
        }

        create_start.arrive_and_wait();

        for (auto& thread : creators)
        {
            thread.join();
        }

        if (create_failed.load(std::memory_order_acquire))
        {
            return false;
        }

        std::atomic<bool> link_failed{false};
        std::barrier link_start(3);

        std::thread horizontal([&]
        {
            link_start.arrive_and_wait();

            Jitter(201u);
            if (!TimedBool("H R -> A FIRST_CHILD", [&]
            {
                return fabric.LinkTwoAPC(slots[R], slots[A], Axis::HORIZONTAL, Inheritance::FIRST_CHILD);
            })) link_failed.store(true, std::memory_order_release);

            Jitter(202u);
            if (!TimedBool("H A -> B LINKED_CHILD", [&]
            {
                return fabric.LinkTwoAPC(slots[A], slots[B], Axis::HORIZONTAL, Inheritance::LINKED_CHILD);
            })) link_failed.store(true, std::memory_order_release);

            Jitter(203u);
            if (!TimedBool("H B -> C LINKED_CHILD", [&]
            {
                return fabric.LinkTwoAPC(slots[B], slots[C], Axis::HORIZONTAL, Inheritance::LINKED_CHILD);
            })) link_failed.store(true, std::memory_order_release);

            Jitter(204u);
            if (!TimedBool("H A-own-root -> N FIRST_CHILD", [&]
            {
                return fabric.LinkTwoAPC(slots[A], slots[N], Axis::HORIZONTAL, Inheritance::FIRST_CHILD);
            })) link_failed.store(true, std::memory_order_release);
        });

        std::thread vertical([&]
        {
            link_start.arrive_and_wait();

            Jitter(301u);
            if (!TimedBool("V R -> D FIRST_CHILD", [&]
            {
                return fabric.LinkTwoAPC(slots[R], slots[D], Axis::VERTICAL, Inheritance::FIRST_CHILD);
            })) link_failed.store(true, std::memory_order_release);

            Jitter(302u);
            if (!TimedBool("V D -> E LINKED_CHILD", [&]
            {
                return fabric.LinkTwoAPC(slots[D], slots[E], Axis::VERTICAL, Inheritance::LINKED_CHILD);
            })) link_failed.store(true, std::memory_order_release);
        });

        link_start.arrive_and_wait();
        horizontal.join();
        vertical.join();

        if (link_failed.load(std::memory_order_acquire))
        {
            return false;
        }

        if (
            !ValidateAxisChain(fabric, slots[R], {slots[A], slots[B], slots[C]}, Axis::HORIZONTAL) ||
            !ValidateAxisChain(fabric, slots[A], {slots[N]}, Axis::HORIZONTAL) ||
            !ValidateAxisChain(fabric, slots[R], {slots[D], slots[E]}, Axis::VERTICAL)
        )
        {
            std::cout << "post-link topology validation : FAIL\n";
            return false;
        }

        std::atomic<bool> relink_failed{false};
        std::barrier relink_start(3);

        std::thread horizontal_relink([&]
        {
            relink_start.arrive_and_wait();

            Jitter(401u);
            if (!TimedBool("H unlink B", [&]
            {
                return fabric.UnlinkTwoAPC(slots[B], Axis::HORIZONTAL);
            })) relink_failed.store(true, std::memory_order_release);

            Jitter(402u);
            if (!TimedBool("H append B after C", [&]
            {
                return fabric.LinkTwoAPC(slots[C], slots[B], Axis::HORIZONTAL, Inheritance::LINKED_CHILD);
            })) relink_failed.store(true, std::memory_order_release);

            Jitter(403u);
            if (!TimedBool("H unlink nested N", [&]
            {
                return fabric.UnlinkTwoAPC(slots[N], Axis::HORIZONTAL);
            })) relink_failed.store(true, std::memory_order_release);

            Jitter(404u);
            if (!TimedBool("H reattach nested N", [&]
            {
                return fabric.LinkTwoAPC(slots[A], slots[N], Axis::HORIZONTAL, Inheritance::FIRST_CHILD);
            })) relink_failed.store(true, std::memory_order_release);
        });

        std::thread vertical_relink([&]
        {
            relink_start.arrive_and_wait();

            Jitter(501u);
            if (!TimedBool("V unlink D", [&]
            {
                return fabric.UnlinkTwoAPC(slots[D], Axis::VERTICAL);
            })) relink_failed.store(true, std::memory_order_release);

            Jitter(502u);
            if (!TimedBool("V append D after E", [&]
            {
                return fabric.LinkTwoAPC(slots[E], slots[D], Axis::VERTICAL, Inheritance::LINKED_CHILD);
            })) relink_failed.store(true, std::memory_order_release);
        });

        relink_start.arrive_and_wait();
        horizontal_relink.join();
        vertical_relink.join();

        if (relink_failed.load(std::memory_order_acquire))
        {
            return false;
        }

        const bool final_topology_ok =
            ValidateAxisChain(fabric, slots[R], {slots[A], slots[C], slots[B]}, Axis::HORIZONTAL) &&
            ValidateAxisChain(fabric, slots[A], {slots[N]}, Axis::HORIZONTAL) &&
            ValidateAxisChain(fabric, slots[R], {slots[E], slots[D]}, Axis::VERTICAL);

        std::vector<uint32_t> all_slots(slots.begin(), slots.end());
        const bool locks_ok = ValidateAllGraphLocksReleased(fabric, all_slots);

        std::cout
            << "final topology validation : " << (final_topology_ok ? "PASS" : "FAIL") << '\n'
            << "all graph locks released  : " << (locks_ok ? "PASS" : "FAIL") << '\n';

        return final_topology_ok && locks_ok;
    }

    int GPTGeneratedTest1()
    {
        constexpr uint32_t WORKERS = 8u;
        constexpr uint32_t ITERATIONS = 25'000u;

        VagueTemoraryPremativeFabric shared_test_fabric;

        if (!shared_test_fabric.InitializeFabricWithPtrTable(
                8u,
                512u,
                APCDataStructure::BRANCH_VERSION,
                CoreOfFabricCoordinator::DEFAULT_THREAD_TABLE_CAPACITY))
        {
            std::cout << "shared-test fabric initialization : FAIL\n";
            return 1;
        }

        LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier layout{};
        SchemaDefinition::InitialRegionalDtypeConf dtype{};
        SchemaDefinition::InitialRegionalProtocol protocol{};

        if (!BuildSingleAtomicStateConfig(layout, dtype, protocol))
        {
            return 1;
        }

        AdaptivePackedCellContainer shared_apc;

        if (!shared_test_fabric.CreateAPC(
                shared_apc,
                false,
                false,
                layout,
                dtype,
                protocol,
                APCDataStructure::BRANCH_VERSION))
        {
            std::cout << "shared APC creation : FAIL\n";
            return 1;
        }

        const TrialResult vector_result = RunVectorSharedTrial(WORKERS, ITERATIONS);
        const TrialResult apc_result = RunAPCSharedTrial(shared_apc, WORKERS, ITERATIONS);

        std::cout
            << "\nVECTOR shared-memory correctness : " << (vector_result.Ok ? "PASS" : "FAIL") << '\n'
            << "  count=" << vector_result.SharedCount
            << " sum=" << vector_result.WeightedSum
            << " done=" << vector_result.DoneCount
            << " time=" << vector_result.ElapsedUs << " us\n"
            << "APC shared-memory correctness    : " << (apc_result.Ok ? "PASS" : "FAIL") << '\n'
            << "  count=" << apc_result.SharedCount
            << " sum=" << apc_result.WeightedSum
            << " done=" << apc_result.DoneCount
            << " time=" << apc_result.ElapsedUs << " us\n\n";

        if (!vector_result.Ok || !apc_result.Ok)
        {
            return 2;
        }

        const bool graph_ok = RunGraphMutationTest();
        std::cout << "\nDYNAMIC GRAPH TEST : " << (graph_ok ? "PASS" : "FAIL") << '\n';

        return graph_ok ? 0 : 3;
    }


}


