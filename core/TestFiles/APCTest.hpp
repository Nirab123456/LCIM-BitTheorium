#pragma once

#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <syncstream>
#include <thread>
#include <vector>

namespace TestSpace1
{
    using namespace BidirectionalInMemGraph;

    using Axis = InstallAxisToBuffer::BidirectionalAxis;
    using Inheritance = InstallAxisToBuffer::DescOfInharitance;
    using Clock = std::chrono::steady_clock;
    using Microseconds = std::chrono::microseconds;

    constexpr uint32_t VALUE_COUNT = 256u;
    constexpr uint32_t PRODUCER_COUNT = 2u;
    constexpr uint32_t FF_WORKER_COUNT = 3u;
    constexpr uint32_t FB_WORKER_COUNT = 2u;
    constexpr uint32_t FINAL_WORKER_COUNT = 1u;

    // 2048 - 96 header = 1952 payload words. Four equal active regions
    // leave ~488 words each, safely larger than VALUE_COUNT.
    constexpr uint32_t SLOT_WORDS = 2048u;
    constexpr uint32_t FABRIC_SLOT_COUNT = 16u;
    constexpr uint32_t DYNAMIC_BRANCH_ROUNDS = 500u;
    constexpr uint32_t CHASE_ROUNDS = 100'000u;

    struct PipelineResult
    {
        bool Ok = false;
        uint64_t SensorFFProduced = 0u;
        uint64_t PredictorFBProduced = 0u;
        uint64_t StateIntegrated = 0u;
        uint64_t ErrorComputed = 0u;
        uint64_t FinalCollected = 0u;
        uint64_t OutputChecksumScaled2 = 0u;
        int64_t ElapsedUs = 0;
    };

    struct TimedResult
    {
        bool Ok = false;
        uint64_t Checksum = 0u;
        int64_t ElapsedUs = 0;
    };

    static void Jitter(uint32_t seed, uint32_t max_us = 20u)
    {
        std::minstd_rand rng(seed * 48271u + 17u);
        std::uniform_int_distribution<uint32_t> dist(0u, max_us);
        std::this_thread::sleep_for(Microseconds(dist(rng)));
    }

    // ---------------------------------------------------------------------
    // Current APC region/schema helpers.
    // Test-local on purpose: once BuildAViewOverRegion() is finished, this
    // small resolver should be replaced by the real APC view API.
    // ---------------------------------------------------------------------

    struct APCRegionRef
    {
        uint32_t Begin = 0u;
        uint32_t End = 0u;
        SchemaDefinition::RegionSchemaRecord Schema{};
        bool Valid = false;

        uint32_t Size() const noexcept
        {
            return End >= Begin ? End - Begin : 0u;
        }
    };

    static constexpr HeaderIdentifierOfAPC BoundsHeaderFor(
        MacroColumnOfAPC region) noexcept
    {
        switch (region)
        {
        case MacroColumnOfAPC::FEEDFORWARD_MESSAGE:
            return HeaderIdentifierOfAPC::FEEDFORWARD_BOUNDS;
        case MacroColumnOfAPC::FEEDBACKWARD_MESSAGE:
            return HeaderIdentifierOfAPC::FEEDBACKWARD_BOUNDS;
        case MacroColumnOfAPC::LATERAL_MESAGE:
            return HeaderIdentifierOfAPC::LATERAL_BOUNDS;
        case MacroColumnOfAPC::STATE_SLOT:
            return HeaderIdentifierOfAPC::STATE_BOUNDS;
        case MacroColumnOfAPC::ERROR_SLOT:
            return HeaderIdentifierOfAPC::ERROR_BOUNDS;
        case MacroColumnOfAPC::WEIGHTLESS_LOOKUP:
            return HeaderIdentifierOfAPC::WEIGHTLESS_BOUNDS;
        case MacroColumnOfAPC::WEIGHT_SLOT:
            return HeaderIdentifierOfAPC::WEIGHT_BOUNDS;
        case MacroColumnOfAPC::AUX_SLOT:
            return HeaderIdentifierOfAPC::AUX_BOUNDS;
        case MacroColumnOfAPC::HETEROGENOUS_PTR:
            return HeaderIdentifierOfAPC::HETEROGENOUS_PTR_BOUNDS;
        case MacroColumnOfAPC::FREE_SLOT:
            return HeaderIdentifierOfAPC::FREE_BOUNDS;
        default:
            return HeaderIdentifierOfAPC::EOF_APC_HEADER;
        }
    }

    static bool ResolveRegion(
        AdaptivePackedCellContainer& apc,
        MacroColumnOfAPC region,
        SchemaDefinition::SchemaProtocols expected_protocol,
        APCRegionRef& out)
    {
        out = APCRegionRef{};

        const HeaderIdentifierOfAPC bounds_header = BoundsHeaderFor(region);
        if (bounds_header == HeaderIdentifierOfAPC::EOF_APC_HEADER)
        {
            return false;
        }

        uint64_t packed_bounds = FABRIC_CELL_SENTINAL;
        uint64_t packed_schema = FABRIC_CELL_SENTINAL;

        if (
            !apc.ReadAPCMetaUnit(bounds_header, packed_bounds, true) ||
            !apc.ReadAPCMetaUnit(
                APCDataStructure::SchemaHeaderIndexFromColumnName(region),
                packed_schema,
                true))
        {
            return false;
        }

        const auto bounds =
            LayoutBoundsOrchestrator::GetLayoutCarrierFromValidLayoutCell(
                packed_bounds,
                region);

        if (
            !bounds.IsValid ||
            bounds.BeginIndex >= bounds.EndIndex ||
            bounds.BeginIndex < AdaptivePackedCellContainer::PayloadBegin())
        {
            return false;
        }

        SchemaDefinition::RegionSchemaRecord schema{};
        schema.ParentColumn = region;

        if (
            !SchemaDefinition::LayoutSchemaFromPackedCell(schema, packed_schema) ||
            !schema.IsValidSchema ||
            schema.ParentColumn != region ||
            schema.Dtype != SchemaDefinition::DataTypeOfMacroColumn::UINT64_T ||
            schema.Protocol != expected_protocol)
        {
            return false;
        }

        out.Begin = bounds.BeginIndex;
        out.End = bounds.EndIndex;
        out.Schema = schema;
        out.Valid = true;
        return true;
    }

    static bool ZeroAPCRegion(
        AdaptivePackedCellContainer& apc,
        const APCRegionRef& region,
        uint32_t words_to_zero)
    {
        if (!region.Valid || words_to_zero > region.Size())
        {
            return false;
        }

        std::vector<uint64_t> zeros(words_to_zero, 0u);
        return apc.ForceCopyToAPCFromBuffer(
            region.Begin,
            words_to_zero,
            zeros.data());
    }

    static bool APCSnapshotStore(
        AdaptivePackedCellContainer& apc,
        const APCRegionRef& region,
        uint32_t idx,
        uint64_t value)
    {
        if (!region.Valid || idx >= region.Size())
        {
            return false;
        }

        // IMMUTABLE_SNAPSHOT is written once per element in this test.
        // We use the existing atomic word store as the publication event.
        apc.AtomicallyWriteU64ToAPC(region.Begin + idx, value);
        return true;
    }

    static bool APCSnapshotLoad(
        AdaptivePackedCellContainer& apc,
        const APCRegionRef& region,
        uint32_t idx,
        uint64_t& value)
    {
        if (!region.Valid || idx >= region.Size())
        {
            return false;
        }

        return apc.AtomicallyReadLongLongAPCUnit(region.Begin + idx, value);
    }

    static bool APCPrivateStore(
        AdaptivePackedCellContainer& apc,
        const APCRegionRef& region,
        uint32_t idx,
        uint64_t value)
    {
        if (!region.Valid || idx >= region.Size())
        {
            return false;
        }

        return apc.ForceCopyToAPCFromBuffer(
            region.Begin + idx,
            1u,
            &value);
    }

    static bool APCPrivateLoad(
        AdaptivePackedCellContainer& apc,
        const APCRegionRef& region,
        uint32_t idx,
        uint64_t& value)
    {
        if (!region.Valid || idx >= region.Size())
        {
            return false;
        }

        return apc.CopyFromAPCToBuffer(
            region.Begin + idx,
            1u,
            &value,
            false);
    }

    static bool BuildFourRegionConfig(
        LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& layout,
        SchemaDefinition::InitialRegionalDtypeConf& dtype,
        SchemaDefinition::InitialRegionalProtocol& protocol)
    {
        // Exactly four active regions.
        layout.FeedForward = 1u;
        layout.FeedBackward = 1u;
        layout.Lateral = 0u;
        layout.StateSlot = 1u;
        layout.ErrorSlot = 1u;
        layout.Weightless = 0u;
        layout.WeightSlot = 0u;
        layout.AUXSlot = 0u;
        layout.HeterogenousPtr = 0u;
        layout.FreeSlot = 0u;

        dtype.FEEDFORWARD_MESSAGE = SchemaDefinition::DataTypeOfMacroColumn::UINT64_T;
        dtype.FEEDBACKWARD_MESSAGE = SchemaDefinition::DataTypeOfMacroColumn::UINT64_T;
        dtype.STATE_SLOT = SchemaDefinition::DataTypeOfMacroColumn::UINT64_T;
        dtype.ERROR_SLOT = SchemaDefinition::DataTypeOfMacroColumn::UINT64_T;

        // This is the requested PRIVATE + snapshot configuration.
        // The snapshot regions are atomically published/read in the test.
        protocol.FEEDFORWARD_MESSAGE = SchemaDefinition::SchemaProtocols::IMMUTABLE_SNAPSHOT;
        protocol.FEEDBACKWARD_MESSAGE = SchemaDefinition::SchemaProtocols::IMMUTABLE_SNAPSHOT;
        protocol.STATE_SLOT = SchemaDefinition::SchemaProtocols::PRIVATE_REGION;
        protocol.ERROR_SLOT = SchemaDefinition::SchemaProtocols::PRIVATE_REGION;

        return true;
    }

    // ---------------------------------------------------------------------
    // Raw vector/pointer-chasing baseline graph.
    // It intentionally mirrors the APC invariant that a node can be a child
    // and own a root on the same axis at the same time.
    // ---------------------------------------------------------------------

    struct VectorNode;

    struct VectorAxisState
    {
        bool OwnsRoot = false;

        // Owned-root side.
        VectorNode* RootFirstChild = nullptr;
        VectorNode* RootEnd = nullptr;
        uint32_t RootCount = 0u;

        // Inherited side.
        VectorNode* Owner = nullptr;
        VectorNode* Previous = nullptr;
        VectorNode* Next = nullptr;
    };

    struct VectorNode
    {
        uint32_t Id = 0u;
        std::vector<uint64_t> FeedForward;
        std::vector<uint64_t> FeedBackward;
        std::vector<uint64_t> State;
        std::vector<uint64_t> Error;
        VectorAxisState Horizontal{};
        VectorAxisState Vertical{};

        VectorNode(uint32_t id, uint32_t region_words)
            : Id(id),
              FeedForward(region_words, 0u),
              FeedBackward(region_words, 0u),
              State(region_words, 0u),
              Error(region_words, 0u)
        {
        }
    };

    static VectorAxisState& AxisState(VectorNode& node, Axis axis) noexcept
    {
        return axis == Axis::HORIZONTAL ? node.Horizontal : node.Vertical;
    }

    // static const VectorAxisState& AxisState(const VectorNode& node, Axis axis) noexcept
    // {
    //     return axis == Axis::HORIZONTAL ? node.Horizontal : node.Vertical;
    // }

    static bool VectorLinkTwoAPC(
        VectorNode& predecessor,
        VectorNode& child,
        Axis axis,
        Inheritance inheritance) noexcept
    {
        if (&predecessor == &child)
        {
            return false;
        }

        VectorAxisState& child_axis = AxisState(child, axis);
        if (
            child_axis.Owner ||
            child_axis.Previous ||
            child_axis.Next)
        {
            return false;
        }

        if (inheritance == Inheritance::FIRST_CHILD)
        {
            VectorAxisState& owner_axis = AxisState(predecessor, axis);
            if (
                !owner_axis.OwnsRoot ||
                owner_axis.RootFirstChild ||
                owner_axis.RootEnd ||
                owner_axis.RootCount != 0u)
            {
                return false;
            }

            owner_axis.RootFirstChild = &child;
            owner_axis.RootEnd = &child;
            owner_axis.RootCount = 1u;

            child_axis.Owner = &predecessor;
            child_axis.Previous = &predecessor;
            child_axis.Next = nullptr;
            return true;
        }

        if (inheritance == Inheritance::LINKED_CHILD)
        {
            VectorAxisState& predecessor_axis = AxisState(predecessor, axis);
            VectorNode* owner = predecessor_axis.Owner;

            if (!owner || predecessor_axis.Next)
            {
                return false;
            }

            VectorAxisState& owner_axis = AxisState(*owner, axis);
            if (
                !owner_axis.OwnsRoot ||
                owner_axis.RootEnd != &predecessor)
            {
                return false;
            }

            predecessor_axis.Next = &child;
            child_axis.Owner = owner;
            child_axis.Previous = &predecessor;
            child_axis.Next = nullptr;
            owner_axis.RootEnd = &child;
            ++owner_axis.RootCount;
            return true;
        }

        return false;
    }

    static bool VectorUnlinkTwoAPC(VectorNode& child, Axis axis) noexcept
    {
        VectorAxisState& child_axis = AxisState(child, axis);
        VectorNode* owner = child_axis.Owner;
        VectorNode* previous = child_axis.Previous;
        VectorNode* next = child_axis.Next;

        if (!owner || !previous)
        {
            return false;
        }

        VectorAxisState& owner_axis = AxisState(*owner, axis);
        if (!owner_axis.OwnsRoot || owner_axis.RootCount == 0u)
        {
            return false;
        }

        if (previous == owner)
        {
            if (owner_axis.RootFirstChild != &child)
            {
                return false;
            }
            owner_axis.RootFirstChild = next;
        }
        else
        {
            VectorAxisState& previous_axis = AxisState(*previous, axis);
            if (previous_axis.Next != &child)
            {
                return false;
            }
            previous_axis.Next = next;
        }

        if (next)
        {
            AxisState(*next, axis).Previous = previous;
        }

        if (owner_axis.RootEnd == &child)
        {
            owner_axis.RootEnd = (previous == owner) ? nullptr : previous;
        }

        --owner_axis.RootCount;
        if (owner_axis.RootCount == 0u)
        {
            owner_axis.RootFirstChild = nullptr;
            owner_axis.RootEnd = nullptr;
        }

        // Crucially, the child's own root fields are left untouched.
        child_axis.Owner = nullptr;
        child_axis.Previous = nullptr;
        child_axis.Next = nullptr;
        return true;
    }

    static VectorNode* VectorOwnedFirstChild(VectorNode& node, Axis axis) noexcept
    {
        return AxisState(node, axis).RootFirstChild;
    }

    // ---------------------------------------------------------------------
    // APC graph chasing helpers.
    // ---------------------------------------------------------------------

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
        Axis axis,
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

    static bool ResolveOwnedFirstChildSlot(
        VagueTemoraryPremativeFabric& fabric,
        uint32_t owner_slot,
        Axis axis,
        uint32_t& child_slot)
    {
        AxisSnapshot snapshot{};
        if (!ReadAxisSnapshot(fabric, owner_slot, axis, snapshot))
        {
            return false;
        }

        if (!APCDataStructure::IsValid32BitAPCUnit(snapshot.RootFirstChild))
        {
            return false;
        }

        child_slot = static_cast<uint32_t>(snapshot.RootFirstChild);
        return true;
    }

    // ---------------------------------------------------------------------
    // Vector region access. Snapshot arrays use atomic_ref; private arrays
    // are raw single-writer memory published by external release flags.
    // ---------------------------------------------------------------------

    static void VectorSnapshotStore(
        std::vector<uint64_t>& region,
        uint32_t idx,
        uint64_t value)
    {
        std::atomic_ref<uint64_t>(region[idx]).store(
            value,
            std::memory_order_release);
    }

    static uint64_t VectorSnapshotLoad(
        std::vector<uint64_t>& region,
        uint32_t idx)
    {
        return std::atomic_ref<uint64_t>(region[idx]).load(
            std::memory_order_acquire);
    }

    static bool WaitVectorSnapshot(
        std::vector<uint64_t>& region,
        uint32_t idx,
        uint64_t& value,
        const Clock::time_point& deadline)
    {
        for (;;)
        {
            value = VectorSnapshotLoad(region, idx);
            if (value != 0u)
            {
                return true;
            }
            if (Clock::now() >= deadline)
            {
                return false;
            }
            std::this_thread::yield();
        }
    }

    static bool WaitAPCSnapshot(
        AdaptivePackedCellContainer& apc,
        const APCRegionRef& region,
        uint32_t idx,
        uint64_t& value,
        const Clock::time_point& deadline)
    {
        for (;;)
        {
            if (!APCSnapshotLoad(apc, region, idx, value))
            {
                return false;
            }
            if (value != 0u)
            {
                return true;
            }
            if (Clock::now() >= deadline)
            {
                return false;
            }
            std::this_thread::yield();
        }
    }

    static bool WaitReady(
        std::vector<uint64_t>& ready,
        uint32_t idx,
        const Clock::time_point& deadline)
    {
        std::atomic_ref<uint64_t> flag(ready[idx]);
        while (flag.load(std::memory_order_acquire) == 0u)
        {
            if (Clock::now() >= deadline)
            {
                return false;
            }
            std::this_thread::yield();
        }
        return true;
    }

    static void PublishReady(std::vector<uint64_t>& ready, uint32_t idx)
    {
        std::atomic_ref<uint64_t>(ready[idx]).store(
            1u,
            std::memory_order_release);
    }

    // ---------------------------------------------------------------------
    // Vector pipeline: same logical work as the old packed-cell test.
    // final_scaled2 = 2*state + error == 2*(old float output).
    // For input i=1..N: state=i+1, error=1, output=i+1.5.
    // ---------------------------------------------------------------------

    static PipelineResult RunVectorPipeline(
        VectorNode& sensor,
        VectorNode& predictor,
        VectorNode& comparator,
        VectorNode& integrator,
        VectorNode& motor)
    {
        std::fill(sensor.FeedForward.begin(), sensor.FeedForward.end(), 0u);
        std::fill(predictor.FeedBackward.begin(), predictor.FeedBackward.end(), 0u);
        std::fill(comparator.Error.begin(), comparator.Error.end(), 0u);
        std::fill(integrator.State.begin(), integrator.State.end(), 0u);
        std::fill(motor.FeedForward.begin(), motor.FeedForward.end(), 0u);

        std::vector<uint64_t> state_ready(VALUE_COUNT, 0u);
        std::vector<uint64_t> error_ready(VALUE_COUNT, 0u);

        std::atomic<uint32_t> next_ff{0u};
        std::atomic<uint32_t> next_fb{0u};
        std::atomic<bool> failed{false};

        std::atomic<uint64_t> sensor_ff_produced{0u};
        std::atomic<uint64_t> predictor_fb_produced{0u};
        std::atomic<uint64_t> state_integrated{0u};
        std::atomic<uint64_t> error_computed{0u};
        std::atomic<uint64_t> final_collected{0u};
        std::atomic<uint64_t> checksum{0u};

        constexpr std::ptrdiff_t worker_total =
            PRODUCER_COUNT + FF_WORKER_COUNT + FB_WORKER_COUNT + FINAL_WORKER_COUNT;
        std::barrier start(worker_total + 1);
        std::vector<std::thread> threads;
        threads.reserve(static_cast<size_t>(worker_total));

        const auto begin = Clock::now();
        const auto deadline = begin + std::chrono::seconds(10);

        for (uint32_t p = 0u; p < PRODUCER_COUNT; ++p)
        {
            threads.emplace_back([&, p]
            {
                start.arrive_and_wait();
                for (uint32_t idx = p; idx < VALUE_COUNT; idx += PRODUCER_COUNT)
                {
                    const uint64_t i = static_cast<uint64_t>(idx) + 1u;
                    VectorSnapshotStore(sensor.FeedForward, idx, i);
                    VectorSnapshotStore(predictor.FeedBackward, idx, i + 1u);
                    sensor_ff_produced.fetch_add(1u, std::memory_order_relaxed);
                    predictor_fb_produced.fetch_add(1u, std::memory_order_relaxed);

                    if ((idx & 31u) == 0u)
                    {
                        Jitter(1000u + p * 4099u + idx);
                    }
                }
            });
        }

        for (uint32_t w = 0u; w < FF_WORKER_COUNT; ++w)
        {
            threads.emplace_back([&, w]
            {
                start.arrive_and_wait();
                for (;;)
                {
                    const uint32_t idx = next_ff.fetch_add(1u, std::memory_order_relaxed);
                    if (idx >= VALUE_COUNT)
                    {
                        break;
                    }

                    VectorNode* target = VectorOwnedFirstChild(sensor, Axis::HORIZONTAL);
                    if (target != &integrator)
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    uint64_t input = 0u;
                    if (!WaitVectorSnapshot(sensor.FeedForward, idx, input, deadline))
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }
                    target->State[idx] = input + 1u; // PRIVATE_REGION analogue.
                    PublishReady(state_ready, idx);
                    state_integrated.fetch_add(1u, std::memory_order_relaxed);

                    if ((idx & 63u) == 0u)
                    {
                        Jitter(2000u + w * 4099u + idx);
                    }
                }
            });
        }

        for (uint32_t w = 0u; w < FB_WORKER_COUNT; ++w)
        {
            threads.emplace_back([&, w]
            {
                start.arrive_and_wait();
                for (;;)
                {
                    const uint32_t idx = next_fb.fetch_add(1u, std::memory_order_relaxed);
                    if (idx >= VALUE_COUNT)
                    {
                        break;
                    }

                    VectorNode* target = VectorOwnedFirstChild(predictor, Axis::VERTICAL);
                    if (target != &comparator)
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    uint64_t feedback = 0u;
                    if (!WaitVectorSnapshot(predictor.FeedBackward, idx, feedback, deadline))
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }
                    const uint64_t expected_input = static_cast<uint64_t>(idx) + 1u;
                    target->Error[idx] = feedback - expected_input; // always 1.
                    PublishReady(error_ready, idx);
                    error_computed.fetch_add(1u, std::memory_order_relaxed);

                    if ((idx & 63u) == 0u)
                    {
                        Jitter(3000u + w * 4099u + idx);
                    }
                }
            });
        }

        for (uint32_t w = 0u; w < FINAL_WORKER_COUNT; ++w)
        {
            threads.emplace_back([&, w]
            {
                start.arrive_and_wait();
                uint64_t local_sum = 0u;

                for (uint32_t idx = 0u; idx < VALUE_COUNT; ++idx)
                {
                    if (
                        !WaitReady(state_ready, idx, deadline) ||
                        !WaitReady(error_ready, idx, deadline))
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    VectorNode* motor_from_h = VectorOwnedFirstChild(integrator, Axis::HORIZONTAL);
                    VectorNode* motor_from_v = VectorOwnedFirstChild(comparator, Axis::VERTICAL);
                    if (motor_from_h != &motor || motor_from_v != &motor)
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    const uint64_t state = integrator.State[idx];
                    const uint64_t error = comparator.Error[idx];
                    const uint64_t scaled2 = 2u * state + error;

                    VectorSnapshotStore(motor.FeedForward, idx, scaled2);
                    uint64_t collected = 0u;
                    if (!WaitVectorSnapshot(motor.FeedForward, idx, collected, deadline))
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }
                    local_sum += collected;
                    final_collected.fetch_add(1u, std::memory_order_relaxed);

                    if ((idx & 63u) == 0u)
                    {
                        Jitter(4000u + w * 4099u + idx);
                    }
                }

                checksum.fetch_add(local_sum, std::memory_order_relaxed);
            });
        }

        start.arrive_and_wait();
        for (auto& thread : threads)
        {
            thread.join();
        }

        const auto end = Clock::now();

        PipelineResult result{};
        result.SensorFFProduced = sensor_ff_produced.load(std::memory_order_acquire);
        result.PredictorFBProduced = predictor_fb_produced.load(std::memory_order_acquire);
        result.StateIntegrated = state_integrated.load(std::memory_order_acquire);
        result.ErrorComputed = error_computed.load(std::memory_order_acquire);
        result.FinalCollected = final_collected.load(std::memory_order_acquire);
        result.OutputChecksumScaled2 = checksum.load(std::memory_order_acquire);
        result.ElapsedUs = std::chrono::duration_cast<Microseconds>(end - begin).count();

        const uint64_t expected_checksum =
            static_cast<uint64_t>(VALUE_COUNT) * (static_cast<uint64_t>(VALUE_COUNT) + 4u);

        result.Ok =
            !failed.load(std::memory_order_acquire) &&
            result.SensorFFProduced == VALUE_COUNT &&
            result.PredictorFBProduced == VALUE_COUNT &&
            result.StateIntegrated == VALUE_COUNT &&
            result.ErrorComputed == VALUE_COUNT &&
            result.FinalCollected == VALUE_COUNT &&
            result.OutputChecksumScaled2 == expected_checksum;

        return result;
    }

    struct APCPipelineRegions
    {
        APCRegionRef SensorFF{};
        APCRegionRef PredictorFB{};
        APCRegionRef ComparatorError{};
        APCRegionRef IntegratorState{};
        APCRegionRef MotorFF{};
    };

    static bool ResolvePipelineRegions(
        AdaptivePackedCellContainer& sensor,
        AdaptivePackedCellContainer& predictor,
        AdaptivePackedCellContainer& comparator,
        AdaptivePackedCellContainer& integrator,
        AdaptivePackedCellContainer& motor,
        APCPipelineRegions& out)
    {
        return
            ResolveRegion(
                sensor,
                MacroColumnOfAPC::FEEDFORWARD_MESSAGE,
                SchemaDefinition::SchemaProtocols::IMMUTABLE_SNAPSHOT,
                out.SensorFF) &&
            ResolveRegion(
                predictor,
                MacroColumnOfAPC::FEEDBACKWARD_MESSAGE,
                SchemaDefinition::SchemaProtocols::IMMUTABLE_SNAPSHOT,
                out.PredictorFB) &&
            ResolveRegion(
                comparator,
                MacroColumnOfAPC::ERROR_SLOT,
                SchemaDefinition::SchemaProtocols::PRIVATE_REGION,
                out.ComparatorError) &&
            ResolveRegion(
                integrator,
                MacroColumnOfAPC::STATE_SLOT,
                SchemaDefinition::SchemaProtocols::PRIVATE_REGION,
                out.IntegratorState) &&
            ResolveRegion(
                motor,
                MacroColumnOfAPC::FEEDFORWARD_MESSAGE,
                SchemaDefinition::SchemaProtocols::IMMUTABLE_SNAPSHOT,
                out.MotorFF) &&
            out.SensorFF.Size() >= VALUE_COUNT &&
            out.PredictorFB.Size() >= VALUE_COUNT &&
            out.ComparatorError.Size() >= VALUE_COUNT &&
            out.IntegratorState.Size() >= VALUE_COUNT &&
            out.MotorFF.Size() >= VALUE_COUNT;
    }

    static PipelineResult RunAPCPipeline(
        VagueTemoraryPremativeFabric& fabric,
        AdaptivePackedCellContainer& sensor,
        AdaptivePackedCellContainer& predictor,
        AdaptivePackedCellContainer& comparator,
        AdaptivePackedCellContainer& integrator,
        AdaptivePackedCellContainer& motor,
        uint32_t sensor_slot,
        uint32_t predictor_slot,
        uint32_t comparator_slot,
        uint32_t integrator_slot,
        uint32_t motor_slot,
        const std::array<AdaptivePackedCellContainer*, FABRIC_SLOT_COUNT>& slot_map)
    {
        APCPipelineRegions regions{};
        if (!ResolvePipelineRegions(sensor, predictor, comparator, integrator, motor, regions))
        {
            return {};
        }

        if (
            !ZeroAPCRegion(sensor, regions.SensorFF, VALUE_COUNT) ||
            !ZeroAPCRegion(predictor, regions.PredictorFB, VALUE_COUNT) ||
            !ZeroAPCRegion(comparator, regions.ComparatorError, VALUE_COUNT) ||
            !ZeroAPCRegion(integrator, regions.IntegratorState, VALUE_COUNT) ||
            !ZeroAPCRegion(motor, regions.MotorFF, VALUE_COUNT))
        {
            return {};
        }

        std::vector<uint64_t> state_ready(VALUE_COUNT, 0u);
        std::vector<uint64_t> error_ready(VALUE_COUNT, 0u);

        std::atomic<uint32_t> next_ff{0u};
        std::atomic<uint32_t> next_fb{0u};
        std::atomic<bool> failed{false};

        std::atomic<uint64_t> sensor_ff_produced{0u};
        std::atomic<uint64_t> predictor_fb_produced{0u};
        std::atomic<uint64_t> state_integrated{0u};
        std::atomic<uint64_t> error_computed{0u};
        std::atomic<uint64_t> final_collected{0u};
        std::atomic<uint64_t> checksum{0u};

        constexpr std::ptrdiff_t worker_total =
            PRODUCER_COUNT + FF_WORKER_COUNT + FB_WORKER_COUNT + FINAL_WORKER_COUNT;
        std::barrier start(worker_total + 1);
        std::vector<std::thread> threads;
        threads.reserve(static_cast<size_t>(worker_total));

        auto ResolveSlotToAPC___ = [&](uint32_t slot) noexcept -> AdaptivePackedCellContainer*
        {
            if (slot >= slot_map.size())
            {
                return nullptr;
            }
            return slot_map[slot];
        };

        const auto begin = Clock::now();
        const auto deadline = begin + std::chrono::seconds(10);

        for (uint32_t p = 0u; p < PRODUCER_COUNT; ++p)
        {
            threads.emplace_back([&, p]
            {
                start.arrive_and_wait();
                for (uint32_t idx = p; idx < VALUE_COUNT; idx += PRODUCER_COUNT)
                {
                    const uint64_t i = static_cast<uint64_t>(idx) + 1u;

                    if (
                        !APCSnapshotStore(sensor, regions.SensorFF, idx, i) ||
                        !APCSnapshotStore(predictor, regions.PredictorFB, idx, i + 1u))
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    sensor_ff_produced.fetch_add(1u, std::memory_order_relaxed);
                    predictor_fb_produced.fetch_add(1u, std::memory_order_relaxed);

                    if ((idx & 31u) == 0u)
                    {
                        Jitter(5000u + p * 4099u + idx);
                    }
                }
            });
        }

        for (uint32_t w = 0u; w < FF_WORKER_COUNT; ++w)
        {
            threads.emplace_back([&, w]
            {
                start.arrive_and_wait();

                for (;;)
                {
                    const uint32_t idx = next_ff.fetch_add(1u, std::memory_order_relaxed);
                    if (idx >= VALUE_COUNT)
                    {
                        break;
                    }

                    uint32_t target_slot = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
                    if (!ResolveOwnedFirstChildSlot(fabric, sensor_slot, Axis::HORIZONTAL, target_slot))
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    AdaptivePackedCellContainer* target = ResolveSlotToAPC___(target_slot);
                    if (target != &integrator || target_slot != integrator_slot)
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    uint64_t input = 0u;
                    if (
                        !WaitAPCSnapshot(sensor, regions.SensorFF, idx, input, deadline) ||
                        !APCPrivateStore(*target, regions.IntegratorState, idx, input + 1u))
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    PublishReady(state_ready, idx);
                    state_integrated.fetch_add(1u, std::memory_order_relaxed);

                    if ((idx & 63u) == 0u)
                    {
                        Jitter(6000u + w * 4099u + idx);
                    }
                }
            });
        }

        for (uint32_t w = 0u; w < FB_WORKER_COUNT; ++w)
        {
            threads.emplace_back([&, w]
            {
                start.arrive_and_wait();

                for (;;)
                {
                    const uint32_t idx = next_fb.fetch_add(1u, std::memory_order_relaxed);
                    if (idx >= VALUE_COUNT)
                    {
                        break;
                    }

                    uint32_t target_slot = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
                    if (!ResolveOwnedFirstChildSlot(fabric, predictor_slot, Axis::VERTICAL, target_slot))
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    AdaptivePackedCellContainer* target = ResolveSlotToAPC___(target_slot);
                    if (target != &comparator || target_slot != comparator_slot)
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    uint64_t feedback = 0u;
                    if (!WaitAPCSnapshot(predictor, regions.PredictorFB, idx, feedback, deadline))
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    const uint64_t expected_input = static_cast<uint64_t>(idx) + 1u;
                    const uint64_t error = feedback - expected_input;

                    if (!APCPrivateStore(*target, regions.ComparatorError, idx, error))
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    PublishReady(error_ready, idx);
                    error_computed.fetch_add(1u, std::memory_order_relaxed);

                    if ((idx & 63u) == 0u)
                    {
                        Jitter(7000u + w * 4099u + idx);
                    }
                }
            });
        }

        for (uint32_t w = 0u; w < FINAL_WORKER_COUNT; ++w)
        {
            threads.emplace_back([&, w]
            {
                start.arrive_and_wait();
                uint64_t local_sum = 0u;

                for (uint32_t idx = 0u; idx < VALUE_COUNT; ++idx)
                {
                    if (
                        !WaitReady(state_ready, idx, deadline) ||
                        !WaitReady(error_ready, idx, deadline))
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    uint32_t motor_h_slot = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
                    uint32_t motor_v_slot = APCDataStructure::APC_INDEX_BOUND_SENTINAL;

                    if (
                        !ResolveOwnedFirstChildSlot(fabric, integrator_slot, Axis::HORIZONTAL, motor_h_slot) ||
                        !ResolveOwnedFirstChildSlot(fabric, comparator_slot, Axis::VERTICAL, motor_v_slot) ||
                        motor_h_slot != motor_slot ||
                        motor_v_slot != motor_slot ||
                        ResolveSlotToAPC___(motor_h_slot) != &motor ||
                        ResolveSlotToAPC___(motor_v_slot) != &motor)
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    uint64_t state = 0u;
                    uint64_t error = 0u;
                    if (
                        !APCPrivateLoad(integrator, regions.IntegratorState, idx, state) ||
                        !APCPrivateLoad(comparator, regions.ComparatorError, idx, error))
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    const uint64_t scaled2 = 2u * state + error;
                    if (!APCSnapshotStore(motor, regions.MotorFF, idx, scaled2))
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    uint64_t collected = 0u;
                    if (!WaitAPCSnapshot(motor, regions.MotorFF, idx, collected, deadline))
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }

                    local_sum += collected;
                    final_collected.fetch_add(1u, std::memory_order_relaxed);

                    if ((idx & 63u) == 0u)
                    {
                        Jitter(8000u + w * 4099u + idx);
                    }
                }

                checksum.fetch_add(local_sum, std::memory_order_relaxed);
            });
        }

        start.arrive_and_wait();
        for (auto& thread : threads)
        {
            thread.join();
        }

        const auto end = Clock::now();

        PipelineResult result{};
        result.SensorFFProduced = sensor_ff_produced.load(std::memory_order_acquire);
        result.PredictorFBProduced = predictor_fb_produced.load(std::memory_order_acquire);
        result.StateIntegrated = state_integrated.load(std::memory_order_acquire);
        result.ErrorComputed = error_computed.load(std::memory_order_acquire);
        result.FinalCollected = final_collected.load(std::memory_order_acquire);
        result.OutputChecksumScaled2 = checksum.load(std::memory_order_acquire);
        result.ElapsedUs = std::chrono::duration_cast<Microseconds>(end - begin).count();

        const uint64_t expected_checksum =
            static_cast<uint64_t>(VALUE_COUNT) * (static_cast<uint64_t>(VALUE_COUNT) + 4u);

        result.Ok =
            !failed.load(std::memory_order_acquire) &&
            result.SensorFFProduced == VALUE_COUNT &&
            result.PredictorFBProduced == VALUE_COUNT &&
            result.StateIntegrated == VALUE_COUNT &&
            result.ErrorComputed == VALUE_COUNT &&
            result.FinalCollected == VALUE_COUNT &&
            result.OutputChecksumScaled2 == expected_checksum;

        return result;
    }

    // ---------------------------------------------------------------------
    // Content-equivalence check after both pipelines finish.
    // ---------------------------------------------------------------------

    static bool ComparePipelinePayloads(
        VectorNode& integrator_vector,
        VectorNode& comparator_vector,
        VectorNode& motor_vector,
        AdaptivePackedCellContainer& integrator_apc,
        AdaptivePackedCellContainer& comparator_apc,
        AdaptivePackedCellContainer& motor_apc)
    {
        APCRegionRef state{};
        APCRegionRef error{};
        APCRegionRef motor_ff{};

        if (
            !ResolveRegion(
                integrator_apc,
                MacroColumnOfAPC::STATE_SLOT,
                SchemaDefinition::SchemaProtocols::PRIVATE_REGION,
                state) ||
            !ResolveRegion(
                comparator_apc,
                MacroColumnOfAPC::ERROR_SLOT,
                SchemaDefinition::SchemaProtocols::PRIVATE_REGION,
                error) ||
            !ResolveRegion(
                motor_apc,
                MacroColumnOfAPC::FEEDFORWARD_MESSAGE,
                SchemaDefinition::SchemaProtocols::IMMUTABLE_SNAPSHOT,
                motor_ff))
        {
            return false;
        }

        for (uint32_t idx = 0u; idx < VALUE_COUNT; ++idx)
        {
            uint64_t apc_state = 0u;
            uint64_t apc_error = 0u;
            uint64_t apc_motor = 0u;

            if (
                !APCPrivateLoad(integrator_apc, state, idx, apc_state) ||
                !APCPrivateLoad(comparator_apc, error, idx, apc_error) ||
                !APCSnapshotLoad(motor_apc, motor_ff, idx, apc_motor))
            {
                return false;
            }

            if (
                integrator_vector.State[idx] != apc_state ||
                comparator_vector.Error[idx] != apc_error ||
                VectorSnapshotLoad(motor_vector.FeedForward, idx) != apc_motor)
            {
                std::cout
                    << "payload mismatch at index " << idx
                    << " state(vector/apc)=" << integrator_vector.State[idx] << '/' << apc_state
                    << " error(vector/apc)=" << comparator_vector.Error[idx] << '/' << apc_error
                    << " motor2(vector/apc)=" << VectorSnapshotLoad(motor_vector.FeedForward, idx)
                    << '/' << apc_motor << '\n';
                return false;
            }
        }

        return true;
    }

    // ---------------------------------------------------------------------
    // Pure control-plane comparisons.
    // ---------------------------------------------------------------------

    static TimedResult RunVectorPointerChase(
        VectorNode& sensor,
        VectorNode& predictor,
        VectorNode& expected_motor)
    {
        const auto begin = Clock::now();
        uint64_t checksum = 0u;
        bool ok = true;

        for (uint32_t i = 0u; i < CHASE_ROUNDS; ++i)
        {
            VectorNode* h1 = VectorOwnedFirstChild(sensor, Axis::HORIZONTAL);
            VectorNode* h2 = h1 ? VectorOwnedFirstChild(*h1, Axis::HORIZONTAL) : nullptr;

            VectorNode* v1 = VectorOwnedFirstChild(predictor, Axis::VERTICAL);
            VectorNode* v2 = v1 ? VectorOwnedFirstChild(*v1, Axis::VERTICAL) : nullptr;

            if (h2 != &expected_motor || v2 != &expected_motor)
            {
                ok = false;
                break;
            }

            checksum += static_cast<uint64_t>(h2->Id + v2->Id + 1u);
        }

        const auto end = Clock::now();
        return TimedResult{
            ok,
            checksum,
            std::chrono::duration_cast<Microseconds>(end - begin).count()
        };
    }

    static TimedResult RunAPCSlotChase(
        VagueTemoraryPremativeFabric& fabric,
        uint32_t sensor_slot,
        uint32_t predictor_slot,
        uint32_t expected_motor_slot)
    {
        const auto begin = Clock::now();
        uint64_t checksum = 0u;
        bool ok = true;

        for (uint32_t i = 0u; i < CHASE_ROUNDS; ++i)
        {
            uint32_t h1 = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
            uint32_t h2 = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
            uint32_t v1 = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
            uint32_t v2 = APCDataStructure::APC_INDEX_BOUND_SENTINAL;

            if (
                !ResolveOwnedFirstChildSlot(fabric, sensor_slot, Axis::HORIZONTAL, h1) ||
                !ResolveOwnedFirstChildSlot(fabric, h1, Axis::HORIZONTAL, h2) ||
                !ResolveOwnedFirstChildSlot(fabric, predictor_slot, Axis::VERTICAL, v1) ||
                !ResolveOwnedFirstChildSlot(fabric, v1, Axis::VERTICAL, v2) ||
                h2 != expected_motor_slot ||
                v2 != expected_motor_slot)
            {
                ok = false;
                break;
            }

            checksum += static_cast<uint64_t>(h2 + v2 + 1u);
        }

        const auto end = Clock::now();
        return TimedResult{
            ok,
            checksum,
            std::chrono::duration_cast<Microseconds>(end - begin).count()
        };
    }

    static TimedResult RunVectorDynamicBranchMutation(
        VectorNode& integrator,
        VectorNode& comparator,
        VectorNode& motor)
    {
        std::atomic<bool> failed{false};
        std::barrier start(3);

        const auto begin = Clock::now();

        std::thread horizontal([&]
        {
            start.arrive_and_wait();
            for (uint32_t i = 0u; i < DYNAMIC_BRANCH_ROUNDS; ++i)
            {
                if (
                    !VectorUnlinkTwoAPC(motor, Axis::HORIZONTAL) ||
                    !VectorLinkTwoAPC(integrator, motor, Axis::HORIZONTAL, Inheritance::FIRST_CHILD))
                {
                    failed.store(true, std::memory_order_release);
                    return;
                }
                if ((i & 63u) == 0u)
                {
                    Jitter(9000u + i, 10u);
                }
            }
        });

        std::thread vertical([&]
        {
            start.arrive_and_wait();
            for (uint32_t i = 0u; i < DYNAMIC_BRANCH_ROUNDS; ++i)
            {
                if (
                    !VectorUnlinkTwoAPC(motor, Axis::VERTICAL) ||
                    !VectorLinkTwoAPC(comparator, motor, Axis::VERTICAL, Inheritance::FIRST_CHILD))
                {
                    failed.store(true, std::memory_order_release);
                    return;
                }
                if ((i & 63u) == 0u)
                {
                    Jitter(10000u + i, 10u);
                }
            }
        });

        start.arrive_and_wait();
        horizontal.join();
        vertical.join();

        const auto end = Clock::now();

        const bool topology_ok =
            VectorOwnedFirstChild(integrator, Axis::HORIZONTAL) == &motor &&
            VectorOwnedFirstChild(comparator, Axis::VERTICAL) == &motor &&
            AxisState(motor, Axis::HORIZONTAL).Owner == &integrator &&
            AxisState(motor, Axis::VERTICAL).Owner == &comparator;

        return TimedResult{
            !failed.load(std::memory_order_acquire) && topology_ok,
            static_cast<uint64_t>(2u) * DYNAMIC_BRANCH_ROUNDS,
            std::chrono::duration_cast<Microseconds>(end - begin).count()
        };
    }

    static TimedResult RunAPCDynamicBranchMutation(
        VagueTemoraryPremativeFabric& fabric,
        uint32_t integrator_slot,
        uint32_t comparator_slot,
        uint32_t motor_slot)
    {
        std::atomic<bool> failed{false};
        std::barrier start(3);

        const auto begin = Clock::now();

        std::thread horizontal([&]
        {
            start.arrive_and_wait();
            for (uint32_t i = 0u; i < DYNAMIC_BRANCH_ROUNDS; ++i)
            {
                if (
                    !fabric.UnlinkTwoAPC(motor_slot, Axis::HORIZONTAL) ||
                    !fabric.LinkTwoAPC(
                        integrator_slot,
                        motor_slot,
                        Axis::HORIZONTAL,
                        Inheritance::FIRST_CHILD))
                {
                    failed.store(true, std::memory_order_release);
                    return;
                }
                if ((i & 63u) == 0u)
                {
                    Jitter(11000u + i, 10u);
                }
            }
        });

        std::thread vertical([&]
        {
            start.arrive_and_wait();
            for (uint32_t i = 0u; i < DYNAMIC_BRANCH_ROUNDS; ++i)
            {
                if (
                    !fabric.UnlinkTwoAPC(motor_slot, Axis::VERTICAL) ||
                    !fabric.LinkTwoAPC(
                        comparator_slot,
                        motor_slot,
                        Axis::VERTICAL,
                        Inheritance::FIRST_CHILD))
                {
                    failed.store(true, std::memory_order_release);
                    return;
                }
                if ((i & 63u) == 0u)
                {
                    Jitter(12000u + i, 10u);
                }
            }
        });

        start.arrive_and_wait();
        horizontal.join();
        vertical.join();

        const auto end = Clock::now();

        uint32_t motor_h = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t motor_v = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        const bool topology_ok =
            ResolveOwnedFirstChildSlot(fabric, integrator_slot, Axis::HORIZONTAL, motor_h) &&
            ResolveOwnedFirstChildSlot(fabric, comparator_slot, Axis::VERTICAL, motor_v) &&
            motor_h == motor_slot &&
            motor_v == motor_slot;

        bool locks_released = true;
        for (const uint32_t slot : {integrator_slot, comparator_slot, motor_slot})
        {
            InstallAxisToBuffer::GraphMutationValues values{};
            if (
                !fabric.ReadGraphMutationFlags(slot, values) ||
                !InstallAxisToBuffer::IsIdentityGraphUnlocked(values.Flags))
            {
                locks_released = false;
                break;
            }
        }

        return TimedResult{
            !failed.load(std::memory_order_acquire) && topology_ok && locks_released,
            static_cast<uint64_t>(2u) * DYNAMIC_BRANCH_ROUNDS,
            std::chrono::duration_cast<Microseconds>(end - begin).count()
        };
    }

    // ---------------------------------------------------------------------
    // Complete benchmark entry point.
    // ---------------------------------------------------------------------
}
