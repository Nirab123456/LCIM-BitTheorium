#pragma once

#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

namespace APCGlobalMutexContentionBenchmark
{
    using namespace BidirectionalInMemGraph;

    using Axis = InstallAxisToBuffer::BidirectionalAxis;
    using Inheritance = InstallAxisToBuffer::DescOfInharitance;
    using Clock = std::chrono::steady_clock;

    // ---------------------------------------------------------------------
    // Workload geometry: FOUR independent hot branches.
    //
    // A single-hot-root test forces BOTH systems into one logical
    // serialization domain and therefore cannot validate APC/Fabric's core
    // fine-grained synchronization bet.  This benchmark instead keeps four
    // independent HORIZONTAL roots inside the same graph/Fabric.
    //
    // Each root has one permanent anchor plus four worker-owned children.
    // Workers are mapped round-robin to roots.  At 16 workers this yields
    // four workers contending per root: real local contention, while four
    // independent mutation domains remain available to APC/Fabric.
    // The vector baseline still protects the ENTIRE graph with one mutex.
    // ---------------------------------------------------------------------
    constexpr uint32_t SHARD_COUNT = 4u;

    // level 0 = one worker and zero additional contenders.
    // level 15 = sixteen workers and fifteen additional contenders.
    constexpr uint32_t MIN_CONTENTION_LEVEL = 0u;
    constexpr uint32_t MAX_CONTENTION_LEVEL = 15u;
    constexpr uint32_t MAX_WORKERS = MAX_CONTENTION_LEVEL + 1u;
    static_assert(MAX_WORKERS % SHARD_COUNT == 0u);

    constexpr size_t OWNER_BASE = 0u;
    constexpr size_t ANCHOR_BASE = OWNER_BASE + SHARD_COUNT;
    constexpr size_t FIRST_WORKER_CHILD_INDEX = ANCHOR_BASE + SHARD_COUNT;
    constexpr size_t NODE_COUNT = FIRST_WORKER_CHILD_INDEX + MAX_WORKERS;
    constexpr size_t WORKER_CHILDREN_PER_SHARD = MAX_WORKERS / SHARD_COUNT;
    constexpr size_t CHILDREN_PER_SHARD = 1u + WORKER_CHILDREN_PER_SHARD;

    constexpr size_t OwnerForShard(uint32_t shard) noexcept
    {
        return OWNER_BASE + shard;
    }

    constexpr size_t AnchorForShard(uint32_t shard) noexcept
    {
        return ANCHOR_BASE + shard;
    }

    constexpr uint32_t ShardForWorker(uint32_t worker) noexcept
    {
        return worker % SHARD_COUNT;
    }

    constexpr size_t ChildForWorker(uint32_t worker) noexcept
    {
        return FIRST_WORKER_CHILD_INDEX + worker;
    }

    constexpr uint32_t WorkerForChild(size_t child) noexcept
    {
        return static_cast<uint32_t>(child - FIRST_WORKER_CHILD_INDEX);
    }

    constexpr size_t OwnerForChild(size_t child) noexcept
    {
        return OwnerForShard(ShardForWorker(WorkerForChild(child)));
    }

    constexpr uint32_t SLOT_WORDS = MINIMUM_APC_CELL_COUNT;
    constexpr uint32_t FABRIC_SLOT_COUNT = static_cast<uint32_t>(NODE_COUNT + 8u);

    // Benchmark controls.  These are deliberately finite so the full sweep
    // remains practical.  Increase MEASURED_CYCLES_PER_WORKER for paper runs.
    constexpr uint32_t WARMUP_CYCLES_PER_WORKER = 1'000u;
    constexpr uint32_t MEASURED_CYCLES_PER_WORKER = 20'000u;
    constexpr uint32_t MEASURED_RUNS = 3u;

    // Only one in every N completed cycles is timestamped for p99 latency.
    // Throughput therefore is not dominated by steady_clock::now() calls.
    // Keep this a power of two.
    constexpr uint32_t LATENCY_SAMPLE_STRIDE = 16u;
    static_assert((LATENCY_SAMPLE_STRIDE & (LATENCY_SAMPLE_STRIDE - 1u)) == 0u);

    // IMPORTANT: one internal try makes an APC public-call rejection visible
    // to the benchmark instead of hiding up to DEFAULT_MAX_TRIES attempts
    // inside one Attach/Detach call.  The benchmark then retries WITHOUT any
    // external topology lock and records the latency/rejection distribution.
    constexpr uint32_t APC_INTERNAL_TRIES_PER_PUBLIC_CALL = 1u;

    // Prevent a broken/progress-starved run from spinning forever.
    constexpr uint64_t MAX_RETRY_EVENTS_PER_CYCLE = 1'000'000ull;

    // Bound retry-latency memory without affecting throughput accounting.
    constexpr size_t MAX_FAILED_ATTEMPT_SAMPLES_PER_THREAD = 100'000u;

    // ---------------------------------------------------------------------
    // Statistics.
    // ---------------------------------------------------------------------
    struct ThreadStats
    {
        uint64_t CompletedCycles = 0u;
        uint64_t MutationRejects = 0u;
        uint64_t TraversalRestarts = 0u;
        uint64_t RetryEvents = 0u;

        std::vector<uint64_t> CycleLatencyNs{};
        std::vector<uint64_t> RetriedCycleLatencyNs{};
        std::vector<uint64_t> FailedMutationAttemptLatencyNs{};
        std::vector<uint64_t> RetryEventsPerCycle{};
        std::vector<uint64_t> MutexWaitLatencyNs{};

        void ReserveForMeasuredRun()
        {
            CycleLatencyNs.reserve(MEASURED_CYCLES_PER_WORKER);
            RetriedCycleLatencyNs.reserve(MEASURED_CYCLES_PER_WORKER / 2u);
            RetryEventsPerCycle.reserve(MEASURED_CYCLES_PER_WORKER);
            MutexWaitLatencyNs.reserve(MEASURED_CYCLES_PER_WORKER * 2u);
            FailedMutationAttemptLatencyNs.reserve(
                std::min<size_t>(
                    MAX_FAILED_ATTEMPT_SAMPLES_PER_THREAD,
                    static_cast<size_t>(MEASURED_CYCLES_PER_WORKER)));
        }
    };

    struct RunResult
    {
        bool Ok = false;
        uint32_t Workers = 0u;
        uint64_t CompletedCycles = 0u;
        int64_t ElapsedNs = 0;

        double ThroughputCyclesPerSecond = 0.0;
        double MutationRejectsPer1000Cycles = 0.0;
        double TraversalRestartsPer1000Cycles = 0.0;
        double RetryEventsPerCompletedCycle = 0.0;

        uint64_t P99CycleLatencyNs = 0u;
        uint64_t P99MutexWaitLatencyNs = 0u;
        uint64_t P99FailedMutationAttemptLatencyNs = 0u;
        uint64_t P99RetriedCycleLatencyNs = 0u;
        uint64_t P99RetryEventsPerCycle = 0u;
    };

    struct LevelResult
    {
        uint32_t ContentionLevel = 0u;
        uint32_t Workers = 0u;

        double VectorThroughput = 0.0;
        double VectorP99CycleNs = 0.0;
        double VectorP99MutexWaitNs = 0.0;

        double APCThroughput = 0.0;
        double APCP99CycleNs = 0.0;
        double APCP99FailedAttemptNs = 0.0;
        double APCP99RetriedCycleNs = 0.0;
        double APCRejectsPer1000 = 0.0;
        double APCTraversalRestartsPer1000 = 0.0;
        double APCRetryEventsPerCycle = 0.0;
        double APCP99RetryEventsPerCycle = 0.0;

        bool Ok = false;
    };

    static uint64_t P99(std::vector<uint64_t>& values)
    {
        if (values.empty())
        {
            return 0u;
        }

        const size_t rank = static_cast<size_t>(
            (99ull * static_cast<uint64_t>(values.size()) + 99ull) / 100ull);
        const size_t index = std::min(values.size() - 1u, rank - 1u);

        std::nth_element(values.begin(), values.begin() + index, values.end());
        return values[index];
    }

    template <size_t N>
    static double Median(std::array<double, N> values)
    {
        std::sort(values.begin(), values.end());
        return values[N / 2u];
    }

    static double NsToUs(double ns) noexcept
    {
        return ns / 1000.0;
    }

    static void RetryBackoff(uint64_t retry_events) noexcept
    {
        // The benchmark does not use an external topology lock for APC.
        // This occasional scheduler hint only prevents pathological user-space
        // hot spinning after repeated public-call rejection.
        if ((retry_events & 63ull) == 0ull)
        {
            std::this_thread::yield();
        }
    }

    // =====================================================================
    // Vector + pointer baseline protected by ONE GLOBAL std::mutex.
    // =====================================================================
    struct PointerNode;

    struct PointerAxis
    {
        bool OwnsRoot = false;
        PointerNode* FirstChild = nullptr;
        PointerNode* LastChild = nullptr;
        size_t ChildCount = 0u;

        PointerNode* Owner = nullptr;
        PointerNode* Previous = nullptr;
        PointerNode* Next = nullptr;
    };

    struct PointerNode
    {
        PointerAxis H{};
    };

    class VectorGlobalMutexBackend
    {
    public:
        using Handle = PointerNode*;

        bool Initialize()
        {
            Nodes_.resize(NODE_COUNT);
            for (uint32_t shard = 0u; shard < SHARD_COUNT; ++shard)
            {
                Nodes_[OwnerForShard(shard)].H.OwnsRoot = true;
            }
            return true;
        }

        Handle HandleAt(size_t idx) noexcept
        {
            return idx < Nodes_.size() ? &Nodes_[idx] : nullptr;
        }

        size_t IndexOf(Handle handle) const noexcept
        {
            if (!handle || Nodes_.empty()) return NODE_COUNT;
            const PointerNode* first = Nodes_.data();
            const PointerNode* last = first + Nodes_.size();
            if (handle < first || handle >= last) return NODE_COUNT;
            return static_cast<size_t>(handle - first);
        }

        bool Attach(size_t predecessor, size_t child, Inheritance inheritance) noexcept
        {
            if (predecessor >= NODE_COUNT || child >= NODE_COUNT || predecessor == child)
            {
                return false;
            }
            return Attach_(HandleAt(predecessor), HandleAt(child), inheritance);
        }

        bool Detach(size_t child) noexcept
        {
            return child < NODE_COUNT && Detach_(HandleAt(child));
        }

        Handle FindNext(Handle from, Inheritance inheritance) noexcept
        {
            if (!from) return nullptr;
            return inheritance == Inheritance::FIRST_CHILD
                ? from->H.FirstChild
                : from->H.Next;
        }

        Handle FindPrevious(Handle from) noexcept
        {
            return from ? from->H.Previous : nullptr;
        }

        bool LocksReleased() noexcept { return true; }

    private:
        std::vector<PointerNode> Nodes_{};

        static bool Attach_(Handle predecessor, Handle child, Inheritance inheritance) noexcept
        {
            if (!predecessor || !child || predecessor == child) return false;

            PointerAxis& child_axis = child->H;
            if (child_axis.Owner || child_axis.Previous || child_axis.Next)
            {
                return false;
            }

            if (inheritance == Inheritance::FIRST_CHILD)
            {
                PointerAxis& owner_axis = predecessor->H;
                if (!owner_axis.OwnsRoot || owner_axis.FirstChild ||
                    owner_axis.LastChild || owner_axis.ChildCount != 0u)
                {
                    return false;
                }

                owner_axis.FirstChild = child;
                owner_axis.LastChild = child;
                owner_axis.ChildCount = 1u;
                child_axis.Owner = predecessor;
                child_axis.Previous = predecessor;
                return true;
            }

            if (inheritance == Inheritance::LINKED_CHILD)
            {
                PointerAxis& predecessor_axis = predecessor->H;
                if (!predecessor_axis.Owner || predecessor_axis.Next)
                {
                    return false;
                }

                PointerAxis& owner_axis = predecessor_axis.Owner->H;
                if (!owner_axis.OwnsRoot || owner_axis.LastChild != predecessor)
                {
                    return false;
                }

                predecessor_axis.Next = child;
                child_axis.Owner = predecessor_axis.Owner;
                child_axis.Previous = predecessor;
                owner_axis.LastChild = child;
                ++owner_axis.ChildCount;
                return true;
            }

            return false;
        }

        static bool Detach_(Handle child) noexcept
        {
            if (!child) return false;

            PointerAxis& child_axis = child->H;
            Handle owner = child_axis.Owner;
            Handle previous = child_axis.Previous;
            Handle next = child_axis.Next;
            if (!owner || !previous) return false;

            PointerAxis& owner_axis = owner->H;
            if (!owner_axis.OwnsRoot || owner_axis.ChildCount == 0u)
            {
                return false;
            }

            if (previous == owner)
            {
                if (owner_axis.FirstChild != child) return false;
                owner_axis.FirstChild = next;
            }
            else
            {
                if (previous->H.Next != child) return false;
                previous->H.Next = next;
            }

            if (next)
            {
                next->H.Previous = previous;
            }

            if (owner_axis.LastChild == child)
            {
                owner_axis.LastChild = previous == owner ? nullptr : previous;
            }

            --owner_axis.ChildCount;
            if (owner_axis.ChildCount == 0u)
            {
                owner_axis.FirstChild = nullptr;
                owner_axis.LastChild = nullptr;
            }

            child_axis.Owner = nullptr;
            child_axis.Previous = nullptr;
            child_axis.Next = nullptr;
            return true;
        }
    };

    // =====================================================================
    // APC/Fabric backend: NO external graph mutex.
    // =====================================================================
    class APCNoExternalLockBackend
    {
    public:
        using Handle = AdaptivePackedCellContainer*;

        bool Initialize()
        {
            Slots_.fill(APCDataStructure::APC_INDEX_BOUND_SENTINAL);

            if (!Fabric_.InitializeFabricWithPtrTable(
                    FABRIC_SLOT_COUNT,
                    SLOT_WORDS,
                    CoreOfFabricCoordinator::DEFAULT_THREAD_TABLE_CAPACITY))
            {
                return false;
            }

            LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier layout{};
            layout.FeedForward = 1u;
            layout.FeedBackward = 0u;
            layout.Lateral = 0u;
            layout.StateSlot = 0u;
            layout.ErrorSlot = 0u;
            layout.Weightless = 0u;
            layout.WeightSlot = 0u;
            layout.AUXSlot = 0u;
            layout.HeterogenousPtr = 0u;
            layout.FreeSlot = 0u;

            SchemaDefinition::InitialRegionalDtypeConf dtype{};
            dtype.FEEDFORWARD_MESSAGE = SchemaDefinition::DataTypeOfMacroColumn::UINT64_T;

            SchemaDefinition::InitialRegionalProtocol protocol{};
            protocol.FEEDFORWARD_MESSAGE = SchemaDefinition::SchemaProtocols::IMMUTABLE_SNAPSHOT;

            for (size_t i = 0u; i < NODE_COUNT; ++i)
            {
                const bool wants_h_root = i < SHARD_COUNT;

                if (!Fabric_.CreateAPC(
                        Nodes_[i],
                        wants_h_root,
                        false,
                        layout,
                        dtype,
                        protocol,
                        APCDataStructure::BRANCH_VERSION))
                {
                    return false;
                }

                uint64_t slot = FABRIC_CELL_SENTINAL;
                if (!Nodes_[i].GetThisSlotIdx(slot) ||
                    !APCDataStructure::IsValid32BitAPCUnit(slot) ||
                    slot >= FABRIC_SLOT_COUNT)
                {
                    return false;
                }

                Slots_[i] = static_cast<uint32_t>(slot);
            }

            return true;
        }

        Handle HandleAt(size_t idx) noexcept
        {
            return idx < NODE_COUNT ? &Nodes_[idx] : nullptr;
        }

        size_t IndexOf(Handle handle) const noexcept
        {
            if (!handle) return NODE_COUNT;
            const AdaptivePackedCellContainer* first = Nodes_.data();
            const AdaptivePackedCellContainer* last = first + Nodes_.size();
            if (handle < first || handle >= last) return NODE_COUNT;
            return static_cast<size_t>(handle - first);
        }

        bool Attach(
            size_t predecessor,
            size_t child,
            Inheritance inheritance,
            uint32_t max_tries = DEFAULT_MAX_TRIES) noexcept
        {
            return predecessor < NODE_COUNT && child < NODE_COUNT &&
                Nodes_[predecessor].AttachAnotherToMe(
                    Nodes_[child],
                    Axis::HORIZONTAL,
                    inheritance,
                    max_tries);
        }

        bool Detach(
            size_t child,
            uint32_t max_tries = DEFAULT_MAX_TRIES) noexcept
        {
            return child < NODE_COUNT &&
                Nodes_[child].DetachMeFromAnotherEdge(
                    Axis::HORIZONTAL,
                    max_tries);
        }

        Handle FindNext(Handle from, Inheritance inheritance) noexcept
        {
            return from
                ? from->FindMyNext(Axis::HORIZONTAL, inheritance)
                : nullptr;
        }

        Handle FindPrevious(Handle from) noexcept
        {
            return from
                ? from->FindPrevious(Axis::HORIZONTAL)
                : nullptr;
        }

        bool LocksReleased() noexcept
        {
            for (size_t i = 0u; i < NODE_COUNT; ++i)
            {
                InstallAxisToBuffer::GraphMutationValues values{};
                if (!Fabric_.ReadGraphMutationFlags(Slots_[i], values) ||
                    !values.IsValid ||
                    !InstallAxisToBuffer::IsIdentityGraphUnlocked(values.Flags))
                {
                    return false;
                }
            }
            return true;
        }

    private:
        VagueTemoraryPremativeFabric Fabric_{};
        std::array<AdaptivePackedCellContainer, NODE_COUNT> Nodes_{};
        std::array<uint32_t, NODE_COUNT> Slots_{};
    };

    // =====================================================================
    // Common topology helpers.
    // =====================================================================
    template <typename Backend>
    static bool BuildSharedSiblingTopology(Backend& b)
    {
        if (!b.Initialize()) return false;

        for (uint32_t shard = 0u; shard < SHARD_COUNT; ++shard)
        {
            const size_t owner = OwnerForShard(shard);
            const size_t anchor = AnchorForShard(shard);

            if (!b.Attach(owner, anchor, Inheritance::FIRST_CHILD))
            {
                return false;
            }

            size_t predecessor = anchor;
            for (uint32_t worker = shard;
                 worker < MAX_WORKERS;
                 worker += SHARD_COUNT)
            {
                const size_t child = ChildForWorker(worker);
                if (!b.Attach(predecessor, child, Inheritance::LINKED_CHILD))
                {
                    return false;
                }
                predecessor = child;
            }
        }

        return true;
    }

    // Explicit APC build overload uses the normal/default internal retry
    // budget.  Only the MEASURED contention phase forces max_tries=1.
    static bool BuildSharedSiblingTopology(APCNoExternalLockBackend& b)
    {
        if (!b.Initialize()) return false;

        for (uint32_t shard = 0u; shard < SHARD_COUNT; ++shard)
        {
            const size_t owner = OwnerForShard(shard);
            const size_t anchor = AnchorForShard(shard);

            if (!b.Attach(
                    owner,
                    anchor,
                    Inheritance::FIRST_CHILD,
                    DEFAULT_MAX_TRIES))
            {
                return false;
            }

            size_t predecessor = anchor;
            for (uint32_t worker = shard;
                 worker < MAX_WORKERS;
                 worker += SHARD_COUNT)
            {
                const size_t child = ChildForWorker(worker);
                if (!b.Attach(
                        predecessor,
                        child,
                        Inheritance::LINKED_CHILD,
                        DEFAULT_MAX_TRIES))
                {
                    return false;
                }
                predecessor = child;
            }
        }

        return true;
    }

    template <typename Backend>
    static bool FindCurrentTailIndex(
        Backend& b,
        size_t owner_index,
        size_t& tail_index) noexcept
    {
        auto current = b.FindNext(
            b.HandleAt(owner_index),
            Inheritance::FIRST_CHILD);

        // Every branch has a permanent anchor, so empty is never expected.
        if (!current)
        {
            return false;
        }

        for (size_t step = 0u; step < CHILDREN_PER_SHARD; ++step)
        {
            auto next = b.FindNext(current, Inheritance::LINKED_CHILD);
            if (!next)
            {
                const size_t idx = b.IndexOf(current);
                if (idx >= NODE_COUNT)
                {
                    return false;
                }
                tail_index = idx;
                return true;
            }
            current = next;
        }

        // More than CHILDREN_PER_SHARD hops implies a cycle or inconsistent
        // read.  APC treats this as an optimistic traversal restart.
        return false;
    }

    template <typename Backend>
    static bool ValidateSharedSiblingTopology(Backend& b)
    {
        std::array<bool, NODE_COUNT> seen{};

        for (uint32_t shard = 0u; shard < SHARD_COUNT; ++shard)
        {
            const size_t owner_index = OwnerForShard(shard);
            const size_t anchor_index = AnchorForShard(shard);
            auto owner = b.HandleAt(owner_index);
            if (!owner) return false;

            auto current = b.FindNext(owner, Inheritance::FIRST_CHILD);
            if (current != b.HandleAt(anchor_index))
            {
                return false;
            }

            typename Backend::Handle previous = owner;
            size_t count = 0u;

            while (current)
            {
                const size_t idx = b.IndexOf(current);
                if (idx < ANCHOR_BASE || idx >= NODE_COUNT || seen[idx])
                {
                    return false;
                }

                // Child must belong to this shard.  Anchors use their own
                // fixed shard; worker children use round-robin ownership.
                if (idx == anchor_index)
                {
                    // expected first child
                }
                else if (idx >= FIRST_WORKER_CHILD_INDEX)
                {
                    if (ShardForWorker(WorkerForChild(idx)) != shard)
                    {
                        return false;
                    }
                }
                else
                {
                    return false;
                }

                if (b.FindPrevious(current) != previous)
                {
                    return false;
                }

                seen[idx] = true;
                ++count;
                if (count > CHILDREN_PER_SHARD)
                {
                    return false;
                }

                previous = current;
                current = b.FindNext(current, Inheritance::LINKED_CHILD);
            }

            if (count != CHILDREN_PER_SHARD)
            {
                return false;
            }
        }

        for (uint32_t shard = 0u; shard < SHARD_COUNT; ++shard)
        {
            if (!seen[AnchorForShard(shard)]) return false;
        }
        for (uint32_t worker = 0u; worker < MAX_WORKERS; ++worker)
        {
            if (!seen[ChildForWorker(worker)]) return false;
        }

        return b.LocksReleased();
    }

    // =====================================================================
    // One logical worker cycle.
    // =====================================================================
    static bool VectorCycle(
        VectorGlobalMutexBackend& b,
        std::mutex& global_graph_mutex,
        size_t child,
        ThreadStats* measured,
        bool sample_latency) noexcept
    {
        Clock::time_point cycle_begin{};
        if (sample_latency)
        {
            cycle_begin = Clock::now();
        }

        // Primitive 1: detach under the global graph mutex.
        Clock::time_point wait_begin{};
        if (sample_latency)
        {
            wait_begin = Clock::now();
        }

        global_graph_mutex.lock();

        if (sample_latency && measured)
        {
            const auto wait_end = Clock::now();
            measured->MutexWaitLatencyNs.push_back(
                static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        wait_end - wait_begin).count()));
        }

        const bool detached = b.Detach(child);
        global_graph_mutex.unlock();
        if (!detached)
        {
            return false;
        }

        // Primitive 2: find a stable tail and reattach while holding the same
        // global mutex.  Releasing the mutex between detach and attach is
        // intentional: APC also exposes those as two independent operations.
        if (sample_latency)
        {
            wait_begin = Clock::now();
        }

        global_graph_mutex.lock();

        if (sample_latency && measured)
        {
            const auto wait_end = Clock::now();
            measured->MutexWaitLatencyNs.push_back(
                static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        wait_end - wait_begin).count()));
        }

        const size_t owner = OwnerForChild(child);
        size_t tail = NODE_COUNT;
        const bool tail_ok = FindCurrentTailIndex(b, owner, tail);
        const bool attached = tail_ok &&
            b.Attach(tail, child, Inheritance::LINKED_CHILD);

        global_graph_mutex.unlock();
        if (!attached)
        {
            return false;
        }

        if (measured)
        {
            ++measured->CompletedCycles;
            measured->RetryEventsPerCycle.push_back(0u);

            if (sample_latency)
            {
                const auto cycle_end = Clock::now();
                measured->CycleLatencyNs.push_back(
                    static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            cycle_end - cycle_begin).count()));
            }
        }

        return true;
    }

    static bool APCCycle(
        APCNoExternalLockBackend& b,
        size_t child,
        ThreadStats* measured,
        bool sample_latency) noexcept
    {
        Clock::time_point cycle_begin{};
        if (sample_latency)
        {
            cycle_begin = Clock::now();
        }

        uint64_t retry_events = 0u;
        uint64_t mutation_rejects = 0u;
        uint64_t traversal_restarts = 0u;

        // Phase 1: detach this worker's private child.  A false return with a
        // one-try budget is an observable retry/rejection event.
        for (;;)
        {
            Clock::time_point attempt_begin{};
            if (sample_latency)
            {
                attempt_begin = Clock::now();
            }

            const bool ok = b.Detach(
                child,
                APC_INTERNAL_TRIES_PER_PUBLIC_CALL);

            if (ok)
            {
                break;
            }

            ++retry_events;
            ++mutation_rejects;

            if (sample_latency && measured &&
                measured->FailedMutationAttemptLatencyNs.size() <
                    MAX_FAILED_ATTEMPT_SAMPLES_PER_THREAD)
            {
                const auto attempt_end = Clock::now();
                measured->FailedMutationAttemptLatencyNs.push_back(
                    static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            attempt_end - attempt_begin).count()));
            }

            if (retry_events >= MAX_RETRY_EVENTS_PER_CYCLE)
            {
                return false;
            }
            RetryBackoff(retry_events);
        }

        // Phase 2: optimistically discover the current tail and try to attach.
        // Another worker may invalidate that tail before LinkTwoAPC commits;
        // that is exactly the contention/retry behavior this benchmark wants.
        for (;;)
        {
            const size_t owner = OwnerForChild(child);
            size_t tail = NODE_COUNT;
            if (!FindCurrentTailIndex(b, owner, tail))
            {
                ++retry_events;
                ++traversal_restarts;
                if (retry_events >= MAX_RETRY_EVENTS_PER_CYCLE)
                {
                    return false;
                }
                RetryBackoff(retry_events);
                continue;
            }

            Clock::time_point attempt_begin{};
            if (sample_latency)
            {
                attempt_begin = Clock::now();
            }

            const bool ok = b.Attach(
                tail,
                child,
                Inheritance::LINKED_CHILD,
                APC_INTERNAL_TRIES_PER_PUBLIC_CALL);

            if (ok)
            {
                break;
            }

            ++retry_events;
            ++mutation_rejects;

            if (sample_latency && measured &&
                measured->FailedMutationAttemptLatencyNs.size() <
                    MAX_FAILED_ATTEMPT_SAMPLES_PER_THREAD)
            {
                const auto attempt_end = Clock::now();
                measured->FailedMutationAttemptLatencyNs.push_back(
                    static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            attempt_end - attempt_begin).count()));
            }

            if (retry_events >= MAX_RETRY_EVENTS_PER_CYCLE)
            {
                return false;
            }
            RetryBackoff(retry_events);
        }

        if (measured)
        {
            ++measured->CompletedCycles;
            measured->RetryEventsPerCycle.push_back(retry_events);
            measured->MutationRejects += mutation_rejects;
            measured->TraversalRestarts += traversal_restarts;
            measured->RetryEvents += retry_events;

            if (sample_latency)
            {
                const auto cycle_end = Clock::now();
                const uint64_t cycle_ns = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        cycle_end - cycle_begin).count());

                measured->CycleLatencyNs.push_back(cycle_ns);
                if (retry_events != 0u)
                {
                    measured->RetriedCycleLatencyNs.push_back(cycle_ns);
                }
            }
        }

        return true;
    }

    // =====================================================================
    // One measured run at one contention level.
    // =====================================================================
    static RunResult RunVectorOnce(uint32_t workers)
    {
        RunResult out{};
        out.Workers = workers;

        VectorGlobalMutexBackend backend{};
        if (!BuildSharedSiblingTopology(backend) ||
            !ValidateSharedSiblingTopology(backend))
        {
            return out;
        }

        std::mutex global_graph_mutex{};
        std::vector<ThreadStats> stats(workers);
        for (auto& s : stats) s.ReserveForMeasuredRun();

        std::atomic<bool> abort{false};
        std::atomic<uint32_t> warmup_ready{0u};
        std::atomic<bool> go{false};
        std::barrier launch(static_cast<std::ptrdiff_t>(workers + 1u));
        std::vector<std::thread> threads{};
        threads.reserve(workers);

        for (uint32_t worker = 0u; worker < workers; ++worker)
        {
            threads.emplace_back([&, worker]
            {
                const size_t child = ChildForWorker(worker);
                launch.arrive_and_wait();

                for (uint32_t i = 0u; i < WARMUP_CYCLES_PER_WORKER; ++i)
                {
                    if (!VectorCycle(backend, global_graph_mutex, child, nullptr, false))
                    {
                        abort.store(true, std::memory_order_release);
                        break;
                    }
                }

                warmup_ready.fetch_add(1u, std::memory_order_acq_rel);
                while (!go.load(std::memory_order_acquire) &&
                       !abort.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }

                if (abort.load(std::memory_order_acquire)) return;

                for (uint32_t i = 0u; i < MEASURED_CYCLES_PER_WORKER; ++i)
                {
                    const bool sample_latency =
                        (i & (LATENCY_SAMPLE_STRIDE - 1u)) == 0u;

                    if (!VectorCycle(
                            backend,
                            global_graph_mutex,
                            child,
                            &stats[worker],
                            sample_latency))
                    {
                        abort.store(true, std::memory_order_release);
                        return;
                    }
                }
            });
        }

        launch.arrive_and_wait();
        while (warmup_ready.load(std::memory_order_acquire) != workers &&
               !abort.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }

        if (abort.load(std::memory_order_acquire))
        {
            go.store(true, std::memory_order_release);
            for (auto& t : threads) t.join();
            return out;
        }

        const auto begin = Clock::now();
        go.store(true, std::memory_order_release);
        for (auto& t : threads) t.join();
        const auto end = Clock::now();

        std::vector<uint64_t> cycle_latencies{};
        std::vector<uint64_t> mutex_wait_latencies{};
        cycle_latencies.reserve(
            static_cast<size_t>(workers) * MEASURED_CYCLES_PER_WORKER);
        mutex_wait_latencies.reserve(
            static_cast<size_t>(workers) * MEASURED_CYCLES_PER_WORKER * 2u);

        for (auto& s : stats)
        {
            out.CompletedCycles += s.CompletedCycles;
            cycle_latencies.insert(
                cycle_latencies.end(),
                s.CycleLatencyNs.begin(),
                s.CycleLatencyNs.end());
            mutex_wait_latencies.insert(
                mutex_wait_latencies.end(),
                s.MutexWaitLatencyNs.begin(),
                s.MutexWaitLatencyNs.end());
        }

        out.ElapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end - begin).count();
        out.ThroughputCyclesPerSecond = out.ElapsedNs > 0
            ? 1.0e9 * static_cast<double>(out.CompletedCycles) /
                static_cast<double>(out.ElapsedNs)
            : 0.0;
        out.P99CycleLatencyNs = P99(cycle_latencies);
        out.P99MutexWaitLatencyNs = P99(mutex_wait_latencies);

        out.Ok = !abort.load(std::memory_order_acquire) &&
            out.CompletedCycles ==
                static_cast<uint64_t>(workers) * MEASURED_CYCLES_PER_WORKER &&
            ValidateSharedSiblingTopology(backend);

        return out;
    }

    static RunResult RunAPCOnce(uint32_t workers)
    {
        RunResult out{};
        out.Workers = workers;

        APCNoExternalLockBackend backend{};
        if (!BuildSharedSiblingTopology(backend) ||
            !ValidateSharedSiblingTopology(backend))
        {
            return out;
        }

        std::vector<ThreadStats> stats(workers);
        for (auto& s : stats) s.ReserveForMeasuredRun();

        std::atomic<bool> abort{false};
        std::atomic<uint32_t> warmup_ready{0u};
        std::atomic<bool> go{false};
        std::barrier launch(static_cast<std::ptrdiff_t>(workers + 1u));
        std::vector<std::thread> threads{};
        threads.reserve(workers);

        for (uint32_t worker = 0u; worker < workers; ++worker)
        {
            threads.emplace_back([&, worker]
            {
                const size_t child = ChildForWorker(worker);
                launch.arrive_and_wait();

                for (uint32_t i = 0u; i < WARMUP_CYCLES_PER_WORKER; ++i)
                {
                    if (!APCCycle(backend, child, nullptr, false))
                    {
                        abort.store(true, std::memory_order_release);
                        break;
                    }
                }

                warmup_ready.fetch_add(1u, std::memory_order_acq_rel);
                while (!go.load(std::memory_order_acquire) &&
                       !abort.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }

                if (abort.load(std::memory_order_acquire)) return;

                for (uint32_t i = 0u; i < MEASURED_CYCLES_PER_WORKER; ++i)
                {
                    const bool sample_latency =
                        (i & (LATENCY_SAMPLE_STRIDE - 1u)) == 0u;

                    if (!APCCycle(
                            backend,
                            child,
                            &stats[worker],
                            sample_latency))
                    {
                        abort.store(true, std::memory_order_release);
                        return;
                    }
                }
            });
        }

        launch.arrive_and_wait();
        while (warmup_ready.load(std::memory_order_acquire) != workers &&
               !abort.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }

        if (abort.load(std::memory_order_acquire))
        {
            go.store(true, std::memory_order_release);
            for (auto& t : threads) t.join();
            return out;
        }

        const auto begin = Clock::now();
        go.store(true, std::memory_order_release);
        for (auto& t : threads) t.join();
        const auto end = Clock::now();

        std::vector<uint64_t> cycle_latencies{};
        std::vector<uint64_t> retried_cycle_latencies{};
        std::vector<uint64_t> failed_attempt_latencies{};
        std::vector<uint64_t> retry_events_per_cycle{};

        cycle_latencies.reserve(
            static_cast<size_t>(workers) * MEASURED_CYCLES_PER_WORKER);
        retried_cycle_latencies.reserve(
            static_cast<size_t>(workers) * MEASURED_CYCLES_PER_WORKER / 2u);
        failed_attempt_latencies.reserve(
            static_cast<size_t>(workers) *
            std::min<size_t>(
                MAX_FAILED_ATTEMPT_SAMPLES_PER_THREAD,
                MEASURED_CYCLES_PER_WORKER));
        retry_events_per_cycle.reserve(
            static_cast<size_t>(workers) * MEASURED_CYCLES_PER_WORKER);

        uint64_t total_rejects = 0u;
        uint64_t total_traversal_restarts = 0u;
        uint64_t total_retry_events = 0u;

        for (auto& s : stats)
        {
            out.CompletedCycles += s.CompletedCycles;
            total_rejects += s.MutationRejects;
            total_traversal_restarts += s.TraversalRestarts;
            total_retry_events += s.RetryEvents;

            cycle_latencies.insert(
                cycle_latencies.end(),
                s.CycleLatencyNs.begin(),
                s.CycleLatencyNs.end());
            retried_cycle_latencies.insert(
                retried_cycle_latencies.end(),
                s.RetriedCycleLatencyNs.begin(),
                s.RetriedCycleLatencyNs.end());
            failed_attempt_latencies.insert(
                failed_attempt_latencies.end(),
                s.FailedMutationAttemptLatencyNs.begin(),
                s.FailedMutationAttemptLatencyNs.end());
            retry_events_per_cycle.insert(
                retry_events_per_cycle.end(),
                s.RetryEventsPerCycle.begin(),
                s.RetryEventsPerCycle.end());
        }

        out.ElapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end - begin).count();
        out.ThroughputCyclesPerSecond = out.ElapsedNs > 0
            ? 1.0e9 * static_cast<double>(out.CompletedCycles) /
                static_cast<double>(out.ElapsedNs)
            : 0.0;

        if (out.CompletedCycles != 0u)
        {
            out.MutationRejectsPer1000Cycles =
                1000.0 * static_cast<double>(total_rejects) /
                static_cast<double>(out.CompletedCycles);
            out.TraversalRestartsPer1000Cycles =
                1000.0 * static_cast<double>(total_traversal_restarts) /
                static_cast<double>(out.CompletedCycles);
            out.RetryEventsPerCompletedCycle =
                static_cast<double>(total_retry_events) /
                static_cast<double>(out.CompletedCycles);
        }

        out.P99CycleLatencyNs = P99(cycle_latencies);
        out.P99FailedMutationAttemptLatencyNs = P99(failed_attempt_latencies);
        out.P99RetriedCycleLatencyNs = P99(retried_cycle_latencies);
        out.P99RetryEventsPerCycle = P99(retry_events_per_cycle);

        out.Ok = !abort.load(std::memory_order_acquire) &&
            out.CompletedCycles ==
                static_cast<uint64_t>(workers) * MEASURED_CYCLES_PER_WORKER &&
            ValidateSharedSiblingTopology(backend);

        return out;
    }

    // =====================================================================
    // Multi-run aggregation and report.
    // =====================================================================
    static bool MeasureLevel(uint32_t contention_level, LevelResult& out)
    {
        const uint32_t workers = contention_level + 1u;
        out.ContentionLevel = contention_level;
        out.Workers = workers;

        std::array<double, MEASURED_RUNS> vector_tput{};
        std::array<double, MEASURED_RUNS> vector_p99_cycle{};
        std::array<double, MEASURED_RUNS> vector_p99_wait{};

        std::array<double, MEASURED_RUNS> apc_tput{};
        std::array<double, MEASURED_RUNS> apc_p99_cycle{};
        std::array<double, MEASURED_RUNS> apc_p99_failed_attempt{};
        std::array<double, MEASURED_RUNS> apc_p99_retried_cycle{};
        std::array<double, MEASURED_RUNS> apc_rejects_per_1000{};
        std::array<double, MEASURED_RUNS> apc_traversal_restarts_per_1000{};
        std::array<double, MEASURED_RUNS> apc_retry_events_per_cycle{};
        std::array<double, MEASURED_RUNS> apc_p99_retry_events{};

        for (uint32_t run = 0u; run < MEASURED_RUNS; ++run)
        {
            RunResult v{};
            RunResult a{};

            // Alternate order to reduce systematic thermal/first-run bias.
            if ((run & 1u) == 0u)
            {
                v = RunVectorOnce(workers);
                a = RunAPCOnce(workers);
            }
            else
            {
                a = RunAPCOnce(workers);
                v = RunVectorOnce(workers);
            }

            if (!v.Ok || !a.Ok)
            {
                std::cout
                    << "  level " << contention_level
                    << " run " << (run + 1u)
                    << " failed: vector=" << (v.Ok ? "PASS" : "FAIL")
                    << " APC=" << (a.Ok ? "PASS" : "FAIL") << '\n';
                return false;
            }

            vector_tput[run] = v.ThroughputCyclesPerSecond;
            vector_p99_cycle[run] = static_cast<double>(v.P99CycleLatencyNs);
            vector_p99_wait[run] = static_cast<double>(v.P99MutexWaitLatencyNs);

            apc_tput[run] = a.ThroughputCyclesPerSecond;
            apc_p99_cycle[run] = static_cast<double>(a.P99CycleLatencyNs);
            apc_p99_failed_attempt[run] =
                static_cast<double>(a.P99FailedMutationAttemptLatencyNs);
            apc_p99_retried_cycle[run] =
                static_cast<double>(a.P99RetriedCycleLatencyNs);
            apc_rejects_per_1000[run] = a.MutationRejectsPer1000Cycles;
            apc_traversal_restarts_per_1000[run] =
                a.TraversalRestartsPer1000Cycles;
            apc_retry_events_per_cycle[run] = a.RetryEventsPerCompletedCycle;
            apc_p99_retry_events[run] =
                static_cast<double>(a.P99RetryEventsPerCycle);
        }

        out.VectorThroughput = Median(vector_tput);
        out.VectorP99CycleNs = Median(vector_p99_cycle);
        out.VectorP99MutexWaitNs = Median(vector_p99_wait);

        out.APCThroughput = Median(apc_tput);
        out.APCP99CycleNs = Median(apc_p99_cycle);
        out.APCP99FailedAttemptNs = Median(apc_p99_failed_attempt);
        out.APCP99RetriedCycleNs = Median(apc_p99_retried_cycle);
        out.APCRejectsPer1000 = Median(apc_rejects_per_1000);
        out.APCTraversalRestartsPer1000 =
            Median(apc_traversal_restarts_per_1000);
        out.APCRetryEventsPerCycle = Median(apc_retry_events_per_cycle);
        out.APCP99RetryEventsPerCycle = Median(apc_p99_retry_events);
        out.Ok = true;
        return true;
    }

    inline int Run()
    {
        std::cout
            << "  workers 1-4 occupy separate roots; local contention begins at worker 5\n\n"
            << "Per level: " << MEASURED_RUNS << " measured runs, median reported; "
            << WARMUP_CYCLES_PER_WORKER << " warmup + "
            << MEASURED_CYCLES_PER_WORKER << " measured cycles/worker/run\n"
            << "Latency sampling: 1/" << LATENCY_SAMPLE_STRIDE
            << " completed cycles (throughput counts every cycle)\n\n";

        std::array<LevelResult, MAX_CONTENTION_LEVEL + 1u> levels{};

        std::cout
            << "level workers | vector Mc/s  p99cycle(us) p99mutexwait(us)"
            << " | APC Mc/s  p99cycle(us) p99retrycycle(us) rejects/1k  retries/cycle  APC/vector\n"
            << "------------------------------------------------------------------------------------------------------------------------\n";

        for (uint32_t level = MIN_CONTENTION_LEVEL;
             level <= MAX_CONTENTION_LEVEL;
             ++level)
        {
            LevelResult r{};
            if (!MeasureLevel(level, r))
            {
                std::cout << "OVERALL: FAIL at contention level " << level << '\n';
                return 1;
            }

            levels[level] = r;
            const double ratio = r.VectorThroughput > 0.0
                ? r.APCThroughput / r.VectorThroughput
                : 0.0;

            std::cout
                << std::setw(5) << level << ' '
                << std::setw(7) << r.Workers << " | "
                << std::setw(10) << std::fixed << std::setprecision(3)
                << (r.VectorThroughput / 1.0e6) << ' '
                << std::setw(12) << std::setprecision(3) << NsToUs(r.VectorP99CycleNs) << ' '
                << std::setw(16) << NsToUs(r.VectorP99MutexWaitNs)
                << " | "
                << std::setw(8) << (r.APCThroughput / 1.0e6) << ' '
                << std::setw(12) << NsToUs(r.APCP99CycleNs) << ' '
                << std::setw(17) << NsToUs(r.APCP99RetriedCycleNs) << ' '
                << std::setw(10) << std::setprecision(2) << r.APCRejectsPer1000 << ' '
                << std::setw(14) << std::setprecision(3) << r.APCRetryEventsPerCycle << ' '
                << std::setw(10) << std::setprecision(3) << ratio << 'x'
                << '\n';
        }

        std::cout
            << "\nAPC RETRY DIAGNOSTICS\n"
            << "level workers | p99 failed APC mutation attempt (us) | p99 retry events/cycle | traversal restarts/1k\n"
            << "----------------------------------------------------------------------------------------------------\n";

        for (const LevelResult& r : levels)
        {
            std::cout
                << std::setw(5) << r.ContentionLevel << ' '
                << std::setw(7) << r.Workers << " | "
                << std::setw(32) << std::fixed << std::setprecision(3)
                << NsToUs(r.APCP99FailedAttemptNs) << " | "
                << std::setw(22) << std::setprecision(1)
                << r.APCP99RetryEventsPerCycle << " | "
                << std::setw(21) << std::setprecision(2)
                << r.APCTraversalRestartsPer1000
                << '\n';
        }

        std::cout
            << "\nCSV_SUMMARY\n"
            << "contention_level,workers,vector_cycles_per_s,vector_p99_cycle_ns,vector_p99_mutex_wait_ns,"
            << "apc_cycles_per_s,apc_p99_cycle_ns,apc_p99_failed_attempt_ns,apc_p99_retried_cycle_ns,"
            << "apc_rejects_per_1000,apc_traversal_restarts_per_1000,apc_retry_events_per_cycle,"
            << "apc_p99_retry_events_per_cycle,apc_to_vector_throughput_ratio\n";

        for (const LevelResult& r : levels)
        {
            const double ratio = r.VectorThroughput > 0.0
                ? r.APCThroughput / r.VectorThroughput
                : 0.0;

            std::cout
                << r.ContentionLevel << ','
                << r.Workers << ','
                << std::fixed << std::setprecision(3)
                << r.VectorThroughput << ','
                << r.VectorP99CycleNs << ','
                << r.VectorP99MutexWaitNs << ','
                << r.APCThroughput << ','
                << r.APCP99CycleNs << ','
                << r.APCP99FailedAttemptNs << ','
                << r.APCP99RetriedCycleNs << ','
                << r.APCRejectsPer1000 << ','
                << r.APCTraversalRestartsPer1000 << ','
                << r.APCRetryEventsPerCycle << ','
                << r.APCP99RetryEventsPerCycle << ','
                << ratio << '\n';
        }

        const LevelResult& low = levels[MIN_CONTENTION_LEVEL];
        const LevelResult& high = levels[MAX_CONTENTION_LEVEL];
        const double low_ratio = low.VectorThroughput > 0.0
            ? low.APCThroughput / low.VectorThroughput
            : 0.0;
        const double high_ratio = high.VectorThroughput > 0.0
            ? high.APCThroughput / high.VectorThroughput
            : 0.0;

        std::cout
            << "\nCORE-BET SIGNAL\n"
            << "  uncontended APC/vector throughput ratio : "
            << std::fixed << std::setprecision(3) << low_ratio << "x\n"
            << "  max-contention APC/vector ratio         : "
            << high_ratio << "x\n"
            << "  APC ratio improvement from level 0->15 : "
            << (low_ratio > 0.0 ? high_ratio / low_ratio : 0.0) << "x\n"
            << "  APC max-contention p99 cycle            : "
            << NsToUs(high.APCP99CycleNs) << " us\n"
            << "  APC max-contention p99 retry-cycle      : "
            << NsToUs(high.APCP99RetriedCycleNs) << " us\n"
            << "  APC max-contention rejects/1000 cycles  : "
            << high.APCRejectsPer1000 << "\n"
            << "================================================================================\n"
            << "OVERALL: PASS\n"
            << "================================================================================\n";

        return 0;
    }
}
