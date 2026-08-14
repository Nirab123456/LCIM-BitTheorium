#pragma once

#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

namespace APCFabricVsVectorTest
{
    using namespace BidirectionalInMemGraph;

    using Axis = InstallAxisToBuffer::BidirectionalAxis;
    using Inheritance = InstallAxisToBuffer::DescOfInharitance;
    using Clock = std::chrono::steady_clock;
    using Microseconds = std::chrono::microseconds;

    constexpr uint32_t VALUE_COUNT = 256u;
    constexpr uint32_t SLOT_WORDS = 2048u;
    constexpr uint32_t FABRIC_SLOT_COUNT = 16u;
    constexpr uint32_t CHASE_ROUNDS = 100'000u;
    constexpr uint32_t MUTATION_ROUNDS = 500u;
    constexpr uint32_t MEASURED_RUNS = 5u;

    enum class Node : uint8_t
    {
        SENSOR = 0,
        PREDICTOR,
        COMPARATOR,
        INTEGRATOR,
        MOTOR,
        SIDE_OWNER,
        SIDE_A,
        SIDE_B,
        NONE
    };

    enum class Region : uint8_t
    {
        FEEDFORWARD,
        FEEDBACKWARD,
        STATE,
        ERROR
    };

    constexpr size_t NODE_COUNT = static_cast<size_t>(Node::NONE);

    static constexpr size_t Idx(Node n) noexcept
    {
        return static_cast<size_t>(n);
    }

    static constexpr bool IsNode(Node n) noexcept
    {
        return n != Node::NONE && Idx(n) < NODE_COUNT;
    }

    struct TimedResult
    {
        bool Ok = false;
        uint64_t Checksum = 0u;
        int64_t ElapsedUs = 0;
    };

    struct MixedResult
    {
        bool Ok = false;
        TimedResult Dataflow{};
        TimedResult Mutation{};
        int64_t ElapsedUs = 0;
    };

    struct Samples
    {
        std::array<int64_t, MEASURED_RUNS> Dataflow{};
        std::array<int64_t, MEASURED_RUNS> Chase{};
        std::array<int64_t, MEASURED_RUNS> Mutation{};
        std::array<int64_t, MEASURED_RUNS> Mixed{};
    };

    template <size_t N>
    static int64_t Median(std::array<int64_t, N> values)
    {
        std::sort(values.begin(), values.end());
        return values[N / 2u];
    }

    static void PerturbSchedule(uint32_t i) noexcept
    {
        if ((i & 31u) == 0u)
        {
            std::this_thread::yield();
        }
    }

    // =====================================================================
    // Vector backend: same logical graph, stored in std::vector + indices.
    // =====================================================================

    struct VectorAxisState
    {
        bool OwnsRoot = false;
        Node RootFirst = Node::NONE;
        Node RootEnd = Node::NONE;
        uint32_t RootCount = 0u;

        Node Owner = Node::NONE;
        Node Previous = Node::NONE;
        Node Next = Node::NONE;
    };

    struct VectorNode
    {
        std::vector<uint64_t> FF;
        std::vector<uint64_t> FB;
        std::vector<uint64_t> State;
        std::vector<uint64_t> Error;
        VectorAxisState H{};
        VectorAxisState V{};
    };

    class VectorBackend
    {
    public:
        bool Initialize()
        {
            for (auto& node : Nodes_)
            {
                node.FF.assign(VALUE_COUNT, 0u);
                node.FB.assign(VALUE_COUNT, 0u);
                node.State.assign(VALUE_COUNT, 0u);
                node.Error.assign(VALUE_COUNT, 0u);
            }

            AxisState_(Node::SENSOR, Axis::HORIZONTAL).OwnsRoot = true;
            AxisState_(Node::INTEGRATOR, Axis::HORIZONTAL).OwnsRoot = true;
            AxisState_(Node::PREDICTOR, Axis::VERTICAL).OwnsRoot = true;
            AxisState_(Node::COMPARATOR, Axis::VERTICAL).OwnsRoot = true;
            AxisState_(Node::SIDE_OWNER, Axis::HORIZONTAL).OwnsRoot = true;
            AxisState_(Node::SIDE_OWNER, Axis::VERTICAL).OwnsRoot = true;
            return true;
        }

        bool Attach(Node predecessor, Node child, Axis axis, Inheritance inheritance) noexcept
        {
            if (!IsNode(predecessor) || !IsNode(child) || predecessor == child)
            {
                return false;
            }

            VectorAxisState& child_axis = AxisState_(child, axis);
            if (IsNode(child_axis.Owner) || IsNode(child_axis.Previous) || IsNode(child_axis.Next))
            {
                return false;
            }

            if (inheritance == Inheritance::FIRST_CHILD)
            {
                VectorAxisState& owner_axis = AxisState_(predecessor, axis);
                if (!owner_axis.OwnsRoot || IsNode(owner_axis.RootFirst) ||
                    IsNode(owner_axis.RootEnd) || owner_axis.RootCount != 0u)
                {
                    return false;
                }

                owner_axis.RootFirst = child;
                owner_axis.RootEnd = child;
                owner_axis.RootCount = 1u;
                child_axis.Owner = predecessor;
                child_axis.Previous = predecessor;
                return true;
            }

            if (inheritance == Inheritance::LINKED_CHILD)
            {
                VectorAxisState& predecessor_axis = AxisState_(predecessor, axis);
                if (!IsNode(predecessor_axis.Owner) || IsNode(predecessor_axis.Next))
                {
                    return false;
                }

                VectorAxisState& owner_axis = AxisState_(predecessor_axis.Owner, axis);
                if (!owner_axis.OwnsRoot || owner_axis.RootEnd != predecessor)
                {
                    return false;
                }

                predecessor_axis.Next = child;
                child_axis.Owner = predecessor_axis.Owner;
                child_axis.Previous = predecessor;
                owner_axis.RootEnd = child;
                ++owner_axis.RootCount;
                return true;
            }

            return false;
        }

        bool Detach(Node child, Axis axis) noexcept
        {
            if (!IsNode(child))
            {
                return false;
            }

            VectorAxisState& child_axis = AxisState_(child, axis);
            const Node owner = child_axis.Owner;
            const Node previous = child_axis.Previous;
            const Node next = child_axis.Next;

            if (!IsNode(owner) || !IsNode(previous))
            {
                return false;
            }

            VectorAxisState& owner_axis = AxisState_(owner, axis);
            if (!owner_axis.OwnsRoot || owner_axis.RootCount == 0u)
            {
                return false;
            }

            if (previous == owner)
            {
                if (owner_axis.RootFirst != child)
                {
                    return false;
                }
                owner_axis.RootFirst = next;
            }
            else
            {
                VectorAxisState& previous_axis = AxisState_(previous, axis);
                if (previous_axis.Next != child)
                {
                    return false;
                }
                previous_axis.Next = next;
            }

            if (IsNode(next))
            {
                AxisState_(next, axis).Previous = previous;
            }

            if (owner_axis.RootEnd == child)
            {
                owner_axis.RootEnd = previous == owner ? Node::NONE : previous;
            }

            --owner_axis.RootCount;
            if (owner_axis.RootCount == 0u)
            {
                owner_axis.RootFirst = Node::NONE;
                owner_axis.RootEnd = Node::NONE;
            }

            child_axis.Owner = Node::NONE;
            child_axis.Previous = Node::NONE;
            child_axis.Next = Node::NONE;
            return true;
        }

        bool DetachChild(Node owner, Node child, Axis axis) noexcept
        {
            if (!IsNode(owner) || !IsNode(child) || AxisState_(child, axis).Owner != owner)
            {
                return false;
            }
            return Detach(child, axis);
        }

        bool FirstChild(Node owner, Axis axis, Node& out) noexcept
        {
            if (!IsNode(owner))
            {
                return false;
            }
            out = AxisState_(owner, axis).RootFirst;
            return IsNode(out);
        }

        bool Previous(Node child, Axis axis, Node& out) noexcept
        {
            if (!IsNode(child))
            {
                return false;
            }
            out = AxisState_(child, axis).Previous;
            return IsNode(out);
        }

        bool Next(Node child, Axis axis, Node& out) noexcept
        {
            if (!IsNode(child))
            {
                return false;
            }
            out = AxisState_(child, axis).Next;
            return IsNode(out);
        }

        bool ResetPipelineData()
        {
            std::fill(Data_(Node::SENSOR, Region::FEEDFORWARD).begin(),
                      Data_(Node::SENSOR, Region::FEEDFORWARD).end(), 0u);
            std::fill(Data_(Node::PREDICTOR, Region::FEEDBACKWARD).begin(),
                      Data_(Node::PREDICTOR, Region::FEEDBACKWARD).end(), 0u);
            std::fill(Data_(Node::COMPARATOR, Region::ERROR).begin(),
                      Data_(Node::COMPARATOR, Region::ERROR).end(), 0u);
            std::fill(Data_(Node::INTEGRATOR, Region::STATE).begin(),
                      Data_(Node::INTEGRATOR, Region::STATE).end(), 0u);
            std::fill(Data_(Node::MOTOR, Region::FEEDFORWARD).begin(),
                      Data_(Node::MOTOR, Region::FEEDFORWARD).end(), 0u);
            return true;
        }

        bool SnapshotStore(Node node, Region region, uint32_t idx, uint64_t value)
        {
            auto& data = Data_(node, region);
            if (idx >= data.size()) return false;
            std::atomic_ref<uint64_t>(data[idx]).store(value, std::memory_order_release);
            return true;
        }

        bool SnapshotLoad(Node node, Region region, uint32_t idx, uint64_t& value)
        {
            auto& data = Data_(node, region);
            if (idx >= data.size()) return false;
            value = std::atomic_ref<uint64_t>(data[idx]).load(std::memory_order_acquire);
            return true;
        }

        bool PrivateStore(Node node, Region region, uint32_t idx, uint64_t value)
        {
            auto& data = Data_(node, region);
            if (idx >= data.size()) return false;
            data[idx] = value;
            return true;
        }

        bool PrivateLoad(Node node, Region region, uint32_t idx, uint64_t& value)
        {
            auto& data = Data_(node, region);
            if (idx >= data.size()) return false;
            value = data[idx];
            return true;
        }

        bool LocksReleased() noexcept { return true; }

    private:
        std::array<VectorNode, NODE_COUNT> Nodes_{};

        VectorAxisState& AxisState_(Node node, Axis axis) noexcept
        {
            VectorNode& n = Nodes_[Idx(node)];
            return axis == Axis::HORIZONTAL ? n.H : n.V;
        }

        std::vector<uint64_t>& Data_(Node node, Region region)
        {
            VectorNode& n = Nodes_[Idx(node)];
            switch (region)
            {
            case Region::FEEDFORWARD: return n.FF;
            case Region::FEEDBACKWARD: return n.FB;
            case Region::STATE: return n.State;
            case Region::ERROR: return n.Error;
            }
            return n.FF;
        }
    };

    // =====================================================================
    // APC/Fabric backend: same interface as VectorBackend.
    // =====================================================================

    struct APCRegion
    {
        uint32_t Begin = 0u;
        uint32_t End = 0u;
        bool Valid = false;

        uint32_t Size() const noexcept { return End >= Begin ? End - Begin : 0u; }
    };

    struct APCNodeRegions
    {
        APCRegion FF{};
        APCRegion FB{};
        APCRegion State{};
        APCRegion Error{};
    };

    class APCBackend
    {
    public:
        bool Initialize()
        {
            Slot_.fill(APCDataStructure::APC_INDEX_BOUND_SENTINAL);
            LogicalBySlot_.fill(Node::NONE);

            if (!Fabric_.InitializeFabricWithPtrTable(
                    FABRIC_SLOT_COUNT,
                    SLOT_WORDS,
                    CoreOfFabricCoordinator::DEFAULT_THREAD_TABLE_CAPACITY))
            {
                return false;
            }

            LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier layout{};
            SchemaDefinition::InitialRegionalDtypeConf dtype{};
            SchemaDefinition::InitialRegionalProtocol protocol{};
            BuildRegionConfiguration_(layout, dtype, protocol);

            for (size_t i = 0u; i < NODE_COUNT; ++i)
            {
                const Node node = static_cast<Node>(i);
                const bool wants_h_root =
                    node == Node::SENSOR || node == Node::INTEGRATOR || node == Node::SIDE_OWNER;
                const bool wants_v_root =
                    node == Node::PREDICTOR || node == Node::COMPARATOR || node == Node::SIDE_OWNER;

                if (!Fabric_.CreateAPC(
                        Nodes_[i],
                        wants_h_root,
                        wants_v_root,
                        layout,
                        dtype,
                        protocol,
                        APCDataStructure::BRANCH_VERSION))
                {
                    return false;
                }

                uint64_t raw_slot = FABRIC_CELL_SENTINAL;
                if (!Nodes_[i].GetThisSlotIdx(raw_slot) ||
                    !APCDataStructure::IsValid32BitAPCUnit(raw_slot) ||
                    raw_slot >= FABRIC_SLOT_COUNT)
                {
                    return false;
                }

                Slot_[i] = static_cast<uint32_t>(raw_slot);
                LogicalBySlot_[static_cast<size_t>(raw_slot)] = node;

                if (!ResolveAllRegions_(node))
                {
                    return false;
                }
            }

            return true;
        }

        bool Attach(Node predecessor, Node child, Axis axis, Inheritance inheritance) noexcept
        {
            return
                IsNode(predecessor) && IsNode(child) &&
                APC_(predecessor).AttachAnotherToMe(APC_(child), axis, inheritance);
        }

        bool Detach(Node child, Axis axis) noexcept
        {
            return IsNode(child) && APC_(child).DetachMeFromAnotherEdge(axis);
        }

        bool DetachChild(Node owner, Node child, Axis axis) noexcept
        {
            return
                IsNode(owner) && IsNode(child) &&
                APC_(owner).DetachMyChild(APC_(child), axis);
        }

        bool FirstChild(Node owner, Axis axis, Node& out) noexcept
        {
            InstallAxisToBuffer::BufferOfAPCIdentity identity{};
            const auto state = Fabric_.ReadIdentityBufferOfAPC(SlotOf_(owner), identity);
            if (!state.has_value() || state.value() != StateOfAPC::LIVE)
            {
                return false;
            }

            const auto map = InstallAxisToBuffer::ConstructAxisMap(axis);
            const uint64_t raw = InstallAxisToBuffer::ValueOfAnIdentityFromBuffer(
                identity, map.RootOwnedChild);
            return LogicalFromSlot_(raw, out);
        }

        bool Previous(Node child, Axis axis, Node& out) noexcept
        {
            return Relative_(child, axis, true, out);
        }

        bool Next(Node child, Axis axis, Node& out) noexcept
        {
            return Relative_(child, axis, false, out);
        }

        bool ResetPipelineData()
        {
            return
                Zero_(Node::SENSOR, Region::FEEDFORWARD) &&
                Zero_(Node::PREDICTOR, Region::FEEDBACKWARD) &&
                Zero_(Node::COMPARATOR, Region::ERROR) &&
                Zero_(Node::INTEGRATOR, Region::STATE) &&
                Zero_(Node::MOTOR, Region::FEEDFORWARD);
        }

        bool SnapshotStore(Node node, Region region, uint32_t idx, uint64_t value)
        {
            APCRegion& r = Region_(node, region);
            if (!r.Valid || idx >= r.Size()) return false;
            APC_(node).AtomicallyWriteU64ToAPC(r.Begin + idx, value);
            return true;
        }

        bool SnapshotLoad(Node node, Region region, uint32_t idx, uint64_t& value)
        {
            APCRegion& r = Region_(node, region);
            return
                r.Valid && idx < r.Size() &&
                APC_(node).AtomicallyReadLongLongAPCUnit(r.Begin + idx, value);
        }

        bool PrivateStore(Node node, Region region, uint32_t idx, uint64_t value)
        {
            APCRegion& r = Region_(node, region);
            return
                r.Valid && idx < r.Size() &&
                APC_(node).ForceCopyToAPCFromBuffer(r.Begin + idx, 1u, &value);
        }

        bool PrivateLoad(Node node, Region region, uint32_t idx, uint64_t& value)
        {
            APCRegion& r = Region_(node, region);
            return
                r.Valid && idx < r.Size() &&
                APC_(node).CopyFromAPCToBuffer(r.Begin + idx, 1u, &value, false);
        }

        bool LocksReleased() noexcept
        {
            for (uint32_t slot : Slot_)
            {
                InstallAxisToBuffer::GraphMutationValues values{};
                if (!Fabric_.ReadGraphMutationFlags(slot, values) ||
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
        std::array<uint32_t, NODE_COUNT> Slot_{};
        std::array<Node, FABRIC_SLOT_COUNT> LogicalBySlot_{};
        std::array<APCNodeRegions, NODE_COUNT> Regions_{};

        AdaptivePackedCellContainer& APC_(Node node) noexcept
        {
            return Nodes_[Idx(node)];
        }

        uint32_t SlotOf_(Node node) const noexcept
        {
            return Slot_[Idx(node)];
        }

        static void BuildRegionConfiguration_(
            LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& layout,
            SchemaDefinition::InitialRegionalDtypeConf& dtype,
            SchemaDefinition::InitialRegionalProtocol& protocol)
        {
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

            protocol.FEEDFORWARD_MESSAGE = SchemaDefinition::SchemaProtocols::IMMUTABLE_SNAPSHOT;
            protocol.FEEDBACKWARD_MESSAGE = SchemaDefinition::SchemaProtocols::IMMUTABLE_SNAPSHOT;
            protocol.STATE_SLOT = SchemaDefinition::SchemaProtocols::PRIVATE_REGION;
            protocol.ERROR_SLOT = SchemaDefinition::SchemaProtocols::PRIVATE_REGION;
        }

        static constexpr HeaderIdentifierOfAPC BoundsHeader_(Region region) noexcept
        {
            switch (region)
            {
            case Region::FEEDFORWARD: return HeaderIdentifierOfAPC::FEEDFORWARD_BOUNDS;
            case Region::FEEDBACKWARD: return HeaderIdentifierOfAPC::FEEDBACKWARD_BOUNDS;
            case Region::STATE: return HeaderIdentifierOfAPC::STATE_BOUNDS;
            case Region::ERROR: return HeaderIdentifierOfAPC::ERROR_BOUNDS;
            }
            return HeaderIdentifierOfAPC::EOF_APC_HEADER;
        }

        static constexpr MacroColumnOfAPC MacroColumn_(Region region) noexcept
        {
            switch (region)
            {
            case Region::FEEDFORWARD: return MacroColumnOfAPC::FEEDFORWARD_MESSAGE;
            case Region::FEEDBACKWARD: return MacroColumnOfAPC::FEEDBACKWARD_MESSAGE;
            case Region::STATE: return MacroColumnOfAPC::STATE_SLOT;
            case Region::ERROR: return MacroColumnOfAPC::ERROR_SLOT;
            }
            return MacroColumnOfAPC::FREE_SLOT;
        }

        static constexpr SchemaDefinition::SchemaProtocols Protocol_(Region region) noexcept
        {
            return
                region == Region::FEEDFORWARD || region == Region::FEEDBACKWARD
                ? SchemaDefinition::SchemaProtocols::IMMUTABLE_SNAPSHOT
                : SchemaDefinition::SchemaProtocols::PRIVATE_REGION;
        }

        bool ResolveRegion_(Node node, Region region, APCRegion& out)
        {
            const MacroColumnOfAPC column = MacroColumn_(region);
            uint64_t packed_bounds = FABRIC_CELL_SENTINAL;
            uint64_t packed_schema = FABRIC_CELL_SENTINAL;

            if (!APC_(node).ReadAPCMetaUnit(BoundsHeader_(region), packed_bounds, true) ||
                !APC_(node).ReadAPCMetaUnit(
                    APCDataStructure::SchemaHeaderIndexFromColumnName(column),
                    packed_schema,
                    true))
            {
                return false;
            }

            const auto bounds = LayoutBoundsOrchestrator::GetLayoutCarrierFromValidLayoutCell(
                packed_bounds, column);

            SchemaDefinition::RegionSchemaRecord schema{};
            schema.ParentColumn = column;

            if (!bounds.IsValid || bounds.BeginIndex >= bounds.EndIndex ||
                bounds.BeginIndex < AdaptivePackedCellContainer::PayloadBegin() ||
                !SchemaDefinition::LayoutSchemaFromPackedCell(schema, packed_schema) ||
                !schema.IsValidSchema ||
                schema.Dtype != SchemaDefinition::DataTypeOfMacroColumn::UINT64_T ||
                schema.Protocol != Protocol_(region))
            {
                return false;
            }

            out.Begin = bounds.BeginIndex;
            out.End = bounds.EndIndex;
            out.Valid = out.Size() >= VALUE_COUNT;
            return out.Valid;
        }

        bool ResolveAllRegions_(Node node)
        {
            APCNodeRegions& r = Regions_[Idx(node)];
            return
                ResolveRegion_(node, Region::FEEDFORWARD, r.FF) &&
                ResolveRegion_(node, Region::FEEDBACKWARD, r.FB) &&
                ResolveRegion_(node, Region::STATE, r.State) &&
                ResolveRegion_(node, Region::ERROR, r.Error);
        }

        APCRegion& Region_(Node node, Region region) noexcept
        {
            APCNodeRegions& r = Regions_[Idx(node)];
            switch (region)
            {
            case Region::FEEDFORWARD: return r.FF;
            case Region::FEEDBACKWARD: return r.FB;
            case Region::STATE: return r.State;
            case Region::ERROR: return r.Error;
            }
            return r.FF;
        }

        bool Zero_(Node node, Region region)
        {
            APCRegion& r = Region_(node, region);
            std::array<uint64_t, VALUE_COUNT> zero{};
            return r.Valid && APC_(node).ForceCopyToAPCFromBuffer(r.Begin, VALUE_COUNT, zero.data());
        }

        bool LogicalFromSlot_(uint64_t raw_slot, Node& out) const noexcept
        {
            if (!APCDataStructure::IsValid32BitAPCUnit(raw_slot) || raw_slot >= LogicalBySlot_.size())
            {
                return false;
            }
            out = LogicalBySlot_[static_cast<size_t>(raw_slot)];
            return IsNode(out);
        }

        bool Relative_(Node child, Axis axis, bool previous, Node& out) noexcept
        {
            InstallAxisToBuffer::BufferOfAPCIdentity identity{};
            const auto state = Fabric_.ReadIdentityBufferOfAPC(SlotOf_(child), identity);
            if (!state.has_value() || state.value() != StateOfAPC::LIVE)
            {
                return false;
            }

            const auto map = InstallAxisToBuffer::ConstructAxisMap(axis);
            const uint64_t raw = InstallAxisToBuffer::ValueOfAnIdentityFromBuffer(
                identity,
                previous ? map.PreviousSibling : map.NextSibling);
            return LogicalFromSlot_(raw, out);
        }
    };

    // =====================================================================
    // One scenario, used identically by both backends.
    // =====================================================================

    template <typename Backend>
    static bool BuildScenario(Backend& b)
    {
        return
            b.Initialize() &&
            b.Attach(Node::SENSOR, Node::INTEGRATOR, Axis::HORIZONTAL, Inheritance::FIRST_CHILD) &&
            b.Attach(Node::INTEGRATOR, Node::MOTOR, Axis::HORIZONTAL, Inheritance::FIRST_CHILD) &&
            b.Attach(Node::PREDICTOR, Node::COMPARATOR, Axis::VERTICAL, Inheritance::FIRST_CHILD) &&
            b.Attach(Node::COMPARATOR, Node::MOTOR, Axis::VERTICAL, Inheritance::FIRST_CHILD) &&
            b.Attach(Node::SIDE_OWNER, Node::SIDE_A, Axis::HORIZONTAL, Inheritance::FIRST_CHILD) &&
            b.Attach(Node::SIDE_A, Node::SIDE_B, Axis::HORIZONTAL, Inheritance::LINKED_CHILD) &&
            b.Attach(Node::SIDE_OWNER, Node::SIDE_A, Axis::VERTICAL, Inheritance::FIRST_CHILD) &&
            b.Attach(Node::SIDE_A, Node::SIDE_B, Axis::VERTICAL, Inheritance::LINKED_CHILD);
    }

    template <typename Backend>
    static bool ValidateTopology(Backend& b)
    {
        Node sensor_h = Node::NONE;
        Node integrator_h = Node::NONE;
        Node predictor_v = Node::NONE;
        Node comparator_v = Node::NONE;
        Node side_h = Node::NONE;
        Node side_v = Node::NONE;
        Node side_a_next_h = Node::NONE;
        Node side_a_next_v = Node::NONE;
        Node side_b_prev_h = Node::NONE;
        Node side_b_prev_v = Node::NONE;

        return
            b.FirstChild(Node::SENSOR, Axis::HORIZONTAL, sensor_h) && sensor_h == Node::INTEGRATOR &&
            b.FirstChild(Node::INTEGRATOR, Axis::HORIZONTAL, integrator_h) && integrator_h == Node::MOTOR &&
            b.FirstChild(Node::PREDICTOR, Axis::VERTICAL, predictor_v) && predictor_v == Node::COMPARATOR &&
            b.FirstChild(Node::COMPARATOR, Axis::VERTICAL, comparator_v) && comparator_v == Node::MOTOR &&
            b.FirstChild(Node::SIDE_OWNER, Axis::HORIZONTAL, side_h) && side_h == Node::SIDE_A &&
            b.FirstChild(Node::SIDE_OWNER, Axis::VERTICAL, side_v) && side_v == Node::SIDE_A &&
            b.Next(Node::SIDE_A, Axis::HORIZONTAL, side_a_next_h) && side_a_next_h == Node::SIDE_B &&
            b.Next(Node::SIDE_A, Axis::VERTICAL, side_a_next_v) && side_a_next_v == Node::SIDE_B &&
            b.Previous(Node::SIDE_B, Axis::HORIZONTAL, side_b_prev_h) && side_b_prev_h == Node::SIDE_A &&
            b.Previous(Node::SIDE_B, Axis::VERTICAL, side_b_prev_v) && side_b_prev_v == Node::SIDE_A &&
            b.LocksReleased();
    }

    template <typename Backend>
    static bool WaitSnapshot(
        Backend& b,
        Node node,
        Region region,
        uint32_t idx,
        uint64_t& value,
        const Clock::time_point& deadline)
    {
        for (;;)
        {
            if (!b.SnapshotLoad(node, region, idx, value))
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
        std::array<std::atomic<bool>, VALUE_COUNT>& flags,
        uint32_t idx,
        const Clock::time_point& deadline)
    {
        while (!flags[idx].load(std::memory_order_acquire))
        {
            if (Clock::now() >= deadline)
            {
                return false;
            }
            std::this_thread::yield();
        }
        return true;
    }

    template <typename Backend>
    static TimedResult RunDataflow(Backend& b)
    {
        if (!b.ResetPipelineData())
        {
            return {};
        }

        std::array<std::atomic<bool>, VALUE_COUNT> state_ready{};
        std::array<std::atomic<bool>, VALUE_COUNT> error_ready{};
        std::atomic<bool> failed{false};
        std::atomic<uint64_t> checksum{0u};
        std::barrier start(5);

        const auto begin = Clock::now();
        const auto deadline = begin + std::chrono::seconds(10);

        std::thread producer([&]
        {
            start.arrive_and_wait();
            for (uint32_t i = 0u; i < VALUE_COUNT; ++i)
            {
                const uint64_t input = static_cast<uint64_t>(i) + 1u;
                if (!b.SnapshotStore(Node::SENSOR, Region::FEEDFORWARD, i, input) ||
                    !b.SnapshotStore(Node::PREDICTOR, Region::FEEDBACKWARD, i, input + 1u))
                {
                    failed.store(true, std::memory_order_release);
                    return;
                }
                PerturbSchedule(i);
            }
        });

        std::thread feedforward([&]
        {
            start.arrive_and_wait();
            for (uint32_t i = 0u; i < VALUE_COUNT; ++i)
            {
                Node target = Node::NONE;
                uint64_t input = 0u;
                if (!b.FirstChild(Node::SENSOR, Axis::HORIZONTAL, target) ||
                    target != Node::INTEGRATOR ||
                    !WaitSnapshot(b, Node::SENSOR, Region::FEEDFORWARD, i, input, deadline) ||
                    !b.PrivateStore(Node::INTEGRATOR, Region::STATE, i, input + 1u))
                {
                    failed.store(true, std::memory_order_release);
                    return;
                }
                state_ready[i].store(true, std::memory_order_release);
                PerturbSchedule(i + 1000u);
            }
        });

        std::thread feedback([&]
        {
            start.arrive_and_wait();
            for (uint32_t i = 0u; i < VALUE_COUNT; ++i)
            {
                Node target = Node::NONE;
                uint64_t feedback_value = 0u;
                if (!b.FirstChild(Node::PREDICTOR, Axis::VERTICAL, target) ||
                    target != Node::COMPARATOR ||
                    !WaitSnapshot(b, Node::PREDICTOR, Region::FEEDBACKWARD, i, feedback_value, deadline) ||
                    !b.PrivateStore(
                        Node::COMPARATOR,
                        Region::ERROR,
                        i,
                        feedback_value - (static_cast<uint64_t>(i) + 1u)))
                {
                    failed.store(true, std::memory_order_release);
                    return;
                }
                error_ready[i].store(true, std::memory_order_release);
                PerturbSchedule(i + 2000u);
            }
        });

        std::thread final_stage([&]
        {
            start.arrive_and_wait();
            uint64_t local_sum = 0u;
            for (uint32_t i = 0u; i < VALUE_COUNT; ++i)
            {
                Node motor_h = Node::NONE;
                Node motor_v = Node::NONE;
                uint64_t state = 0u;
                uint64_t error = 0u;

                if (!WaitReady(state_ready, i, deadline) ||
                    !WaitReady(error_ready, i, deadline) ||
                    !b.FirstChild(Node::INTEGRATOR, Axis::HORIZONTAL, motor_h) || motor_h != Node::MOTOR ||
                    !b.FirstChild(Node::COMPARATOR, Axis::VERTICAL, motor_v) || motor_v != Node::MOTOR ||
                    !b.PrivateLoad(Node::INTEGRATOR, Region::STATE, i, state) ||
                    !b.PrivateLoad(Node::COMPARATOR, Region::ERROR, i, error))
                {
                    failed.store(true, std::memory_order_release);
                    return;
                }

                const uint64_t scaled2 = 2u * state + error;
                uint64_t collected = 0u;
                if (!b.SnapshotStore(Node::MOTOR, Region::FEEDFORWARD, i, scaled2) ||
                    !b.SnapshotLoad(Node::MOTOR, Region::FEEDFORWARD, i, collected))
                {
                    failed.store(true, std::memory_order_release);
                    return;
                }
                local_sum += collected;
                PerturbSchedule(i + 3000u);
            }
            checksum.store(local_sum, std::memory_order_release);
        });

        start.arrive_and_wait();
        producer.join();
        feedforward.join();
        feedback.join();
        final_stage.join();
        const auto end = Clock::now();

        const uint64_t expected =
            static_cast<uint64_t>(VALUE_COUNT) * (static_cast<uint64_t>(VALUE_COUNT) + 4u);

        return {
            !failed.load(std::memory_order_acquire) &&
                checksum.load(std::memory_order_acquire) == expected,
            checksum.load(std::memory_order_acquire),
            std::chrono::duration_cast<Microseconds>(end - begin).count()
        };
    }

    template <typename Backend>
    static TimedResult RunChase(Backend& b)
    {
        const auto begin = Clock::now();
        uint64_t checksum = 0u;

        for (uint32_t i = 0u; i < CHASE_ROUNDS; ++i)
        {
            Node h1 = Node::NONE;
            Node h2 = Node::NONE;
            Node v1 = Node::NONE;
            Node v2 = Node::NONE;

            if (!b.FirstChild(Node::SENSOR, Axis::HORIZONTAL, h1) ||
                !b.FirstChild(h1, Axis::HORIZONTAL, h2) || h2 != Node::MOTOR ||
                !b.FirstChild(Node::PREDICTOR, Axis::VERTICAL, v1) ||
                !b.FirstChild(v1, Axis::VERTICAL, v2) || v2 != Node::MOTOR)
            {
                return {};
            }
            checksum += 2u;
        }

        const auto end = Clock::now();
        return {true, checksum, std::chrono::duration_cast<Microseconds>(end - begin).count()};
    }

    template <typename Backend>
    static TimedResult RunMainMutation(Backend& b)
    {
        std::atomic<bool> failed{false};
        std::barrier start(3);
        const auto begin = Clock::now();

        std::thread h([&]
        {
            start.arrive_and_wait();
            for (uint32_t i = 0u; i < MUTATION_ROUNDS; ++i)
            {
                if (!b.DetachChild(Node::INTEGRATOR, Node::MOTOR, Axis::HORIZONTAL) ||
                    !b.Attach(Node::INTEGRATOR, Node::MOTOR, Axis::HORIZONTAL, Inheritance::FIRST_CHILD))
                {
                    failed.store(true, std::memory_order_release);
                    return;
                }
                PerturbSchedule(i);
            }
        });

        std::thread v([&]
        {
            start.arrive_and_wait();
            for (uint32_t i = 0u; i < MUTATION_ROUNDS; ++i)
            {
                if (!b.DetachChild(Node::COMPARATOR, Node::MOTOR, Axis::VERTICAL) ||
                    !b.Attach(Node::COMPARATOR, Node::MOTOR, Axis::VERTICAL, Inheritance::FIRST_CHILD))
                {
                    failed.store(true, std::memory_order_release);
                    return;
                }
                PerturbSchedule(i + 1000u);
            }
        });

        start.arrive_and_wait();
        h.join();
        v.join();
        const auto end = Clock::now();

        return {
            !failed.load(std::memory_order_acquire) && ValidateTopology(b),
            static_cast<uint64_t>(MUTATION_ROUNDS) * 4u,
            std::chrono::duration_cast<Microseconds>(end - begin).count()
        };
    }

    template <typename Backend>
    static TimedResult RunSideMutation(Backend& b)
    {
        std::atomic<bool> failed{false};
        std::barrier start(3);
        const auto begin = Clock::now();

        std::thread h([&]
        {
            start.arrive_and_wait();
            for (uint32_t i = 0u; i < MUTATION_ROUNDS; ++i)
            {
                if (!b.Detach(Node::SIDE_B, Axis::HORIZONTAL) ||
                    !b.Attach(Node::SIDE_A, Node::SIDE_B, Axis::HORIZONTAL, Inheritance::LINKED_CHILD))
                {
                    failed.store(true, std::memory_order_release);
                    return;
                }
                PerturbSchedule(i);
            }
        });

        std::thread v([&]
        {
            start.arrive_and_wait();
            for (uint32_t i = 0u; i < MUTATION_ROUNDS; ++i)
            {
                if (!b.Detach(Node::SIDE_B, Axis::VERTICAL) ||
                    !b.Attach(Node::SIDE_A, Node::SIDE_B, Axis::VERTICAL, Inheritance::LINKED_CHILD))
                {
                    failed.store(true, std::memory_order_release);
                    return;
                }
                PerturbSchedule(i + 1000u);
            }
        });

        start.arrive_and_wait();
        h.join();
        v.join();
        const auto end = Clock::now();

        return {
            !failed.load(std::memory_order_acquire) && ValidateTopology(b),
            static_cast<uint64_t>(MUTATION_ROUNDS) * 4u,
            std::chrono::duration_cast<Microseconds>(end - begin).count()
        };
    }

    template <typename Backend>
    static MixedResult RunMixed(Backend& b)
    {
        MixedResult out{};
        const auto begin = Clock::now();

        std::thread pipeline([&]
        {
            out.Dataflow = RunDataflow(b);
        });

        out.Mutation = RunSideMutation(b);
        pipeline.join();
        const auto end = Clock::now();

        out.ElapsedUs = std::chrono::duration_cast<Microseconds>(end - begin).count();
        out.Ok = out.Dataflow.Ok && out.Mutation.Ok && ValidateTopology(b);
        return out;
    }

    template <typename A, typename B>
    static bool SamePayload(A& a, B& b)
    {
        for (uint32_t i = 0u; i < VALUE_COUNT; ++i)
        {
            uint64_t a_state = 0u, b_state = 0u;
            uint64_t a_error = 0u, b_error = 0u;
            uint64_t a_motor = 0u, b_motor = 0u;

            if (!a.PrivateLoad(Node::INTEGRATOR, Region::STATE, i, a_state) ||
                !b.PrivateLoad(Node::INTEGRATOR, Region::STATE, i, b_state) ||
                !a.PrivateLoad(Node::COMPARATOR, Region::ERROR, i, a_error) ||
                !b.PrivateLoad(Node::COMPARATOR, Region::ERROR, i, b_error) ||
                !a.SnapshotLoad(Node::MOTOR, Region::FEEDFORWARD, i, a_motor) ||
                !b.SnapshotLoad(Node::MOTOR, Region::FEEDFORWARD, i, b_motor) ||
                a_state != b_state || a_error != b_error || a_motor != b_motor)
            {
                return false;
            }
        }
        return true;
    }

    static double Ratio(int64_t apc_us, int64_t vector_us) noexcept
    {
        return vector_us > 0 ? static_cast<double>(apc_us) / static_cast<double>(vector_us) : 0.0;
    }

    static void PrintTiming(const char* label, int64_t vector_us, int64_t apc_us)
    {
        std::cout
            << std::left << std::setw(20) << label
            << " vector=" << std::right << std::setw(9) << vector_us << " us"
            << "  APC=" << std::setw(9) << apc_us << " us"
            << "  ratio=" << std::fixed << std::setprecision(3)
            << Ratio(apc_us, vector_us) << "x\n";
    }

    static int Fail(const char* phase, uint32_t run = 0u)
    {
        std::cout << "\nFAIL: " << phase;
        if (run != 0u) std::cout << " on measured run " << run;
        std::cout << "\n";
        return 1;
    }

    inline int RunAPCFabricVsVectorTest()
    {
        std::cout
            << "\n============================================================\n"
            << "APC/FABRIC vs std::vector - SINGLE COMPREHENSIVE TEST\n"
            << "============================================================\n"
            << "Main graph:\n"
            << "  H: SENSOR -> INTEGRATOR -> MOTOR\n"
            << "  V: PREDICTOR -> COMPARATOR -> MOTOR\n"
            << "Side sibling chain on both axes:\n"
            << "  SIDE_OWNER owns [SIDE_A, SIDE_B]\n\n"
            << "Checks FIRST_CHILD + LINKED_CHILD, both APC detach APIs, concurrent dataflow,\n"
            << "graph traversal, same-child H/V mutation, unrelated-branch mutation,\n"
            << "payload equality, final topology, and APC graph-lock release.\n\n";

        const auto vb = Clock::now();
        VectorBackend vector_backend{};
        if (!BuildScenario(vector_backend)) return Fail("vector construction");
        const auto ve = Clock::now();

        const auto ab = Clock::now();
        APCBackend apc_backend{};
        if (!BuildScenario(apc_backend)) return Fail("APC/Fabric construction");
        const auto ae = Clock::now();

        if (!ValidateTopology(vector_backend)) return Fail("initial vector topology");
        if (!ValidateTopology(apc_backend)) return Fail("initial APC topology/locks");

        PrintTiming(
            "construction",
            std::chrono::duration_cast<Microseconds>(ve - vb).count(),
            std::chrono::duration_cast<Microseconds>(ae - ab).count());

        const TimedResult warm_v = RunDataflow(vector_backend);
        const TimedResult warm_a = RunDataflow(apc_backend);
        if (!warm_v.Ok || !warm_a.Ok || !SamePayload(vector_backend, apc_backend))
        {
            return Fail("warm-up dataflow/payload equality");
        }

        Samples v_samples{};
        Samples a_samples{};

        auto FailMeasuredRun = [&](const char* phase, uint32_t run) -> int
        {
            const bool vector_topology = ValidateTopology(vector_backend);
            const bool apc_topology = ValidateTopology(apc_backend);
            const bool apc_locks = apc_backend.LocksReleased();
            const bool payload_equal = SamePayload(vector_backend, apc_backend);

            std::cout
                << "\nFAIL: " << phase << " on measured run " << run << '\n'
                << "STATE AFTER FAILURE\n"
                << "  vector topology : " << (vector_topology ? "PASS" : "FAIL") << '\n'
                << "  APC topology    : " << (apc_topology ? "PASS" : "FAIL") << '\n'
                << "  APC locks       : " << (apc_locks ? "PASS" : "FAIL") << '\n'
                << "  payload equality: " << (payload_equal ? "PASS" : "FAIL") << '\n';
            return 1;
        };

        for (uint32_t run = 0u; run < MEASURED_RUNS; ++run)
        {
            TimedResult vd{}, ad{}, vc{}, ac{}, vm{}, am{};
            MixedResult vx{}, ax{};

            if ((run & 1u) == 0u)
            {
                vd = RunDataflow(vector_backend);
                ad = RunDataflow(apc_backend);
            }
            else
            {
                ad = RunDataflow(apc_backend);
                vd = RunDataflow(vector_backend);
            }
            if (!vd.Ok) return FailMeasuredRun("vector dataflow", run + 1u);
            if (!ad.Ok) return FailMeasuredRun("APC dataflow", run + 1u);
            if (!SamePayload(vector_backend, apc_backend))
                return FailMeasuredRun("vector/APC payload equality", run + 1u);

            if ((run & 1u) == 0u)
            {
                vc = RunChase(vector_backend);
                ac = RunChase(apc_backend);
            }
            else
            {
                ac = RunChase(apc_backend);
                vc = RunChase(vector_backend);
            }
            if (!vc.Ok) return FailMeasuredRun("vector graph traversal", run + 1u);
            if (!ac.Ok) return FailMeasuredRun("APC graph traversal", run + 1u);

            if ((run & 1u) == 0u)
            {
                vm = RunMainMutation(vector_backend);
                am = RunMainMutation(apc_backend);
            }
            else
            {
                am = RunMainMutation(apc_backend);
                vm = RunMainMutation(vector_backend);
            }
            if (!vm.Ok) return FailMeasuredRun("vector same-child concurrent H/V mutation", run + 1u);
            if (!am.Ok) return FailMeasuredRun("APC same-child concurrent H/V mutation", run + 1u);

            if ((run & 1u) == 0u)
            {
                vx = RunMixed(vector_backend);
                ax = RunMixed(apc_backend);
            }
            else
            {
                ax = RunMixed(apc_backend);
                vx = RunMixed(vector_backend);
            }
            if (!vx.Ok) return FailMeasuredRun("vector mixed dataflow + LINKED_CHILD mutation", run + 1u);
            if (!ax.Ok) return FailMeasuredRun("APC mixed dataflow + LINKED_CHILD mutation", run + 1u);
            if (!SamePayload(vector_backend, apc_backend))
                return FailMeasuredRun("mixed vector/APC payload equality", run + 1u);

            if (!ValidateTopology(vector_backend) ||
                !ValidateTopology(apc_backend) ||
                !apc_backend.LocksReleased())
            {
                return FailMeasuredRun("post-run topology/lock invariant", run + 1u);
            }

            v_samples.Dataflow[run] = vd.ElapsedUs;
            a_samples.Dataflow[run] = ad.ElapsedUs;
            v_samples.Chase[run] = vc.ElapsedUs;
            a_samples.Chase[run] = ac.ElapsedUs;
            v_samples.Mutation[run] = vm.ElapsedUs;
            a_samples.Mutation[run] = am.ElapsedUs;
            v_samples.Mixed[run] = vx.ElapsedUs;
            a_samples.Mixed[run] = ax.ElapsedUs;

            std::cout << "run " << (run + 1u) << '/' << MEASURED_RUNS << " : PASS\n";
        }

        std::cout << "\nMEDIAN TIMINGS\n";
        PrintTiming("dataflow", Median(v_samples.Dataflow), Median(a_samples.Dataflow));
        PrintTiming("graph traversal", Median(v_samples.Chase), Median(a_samples.Chase));
        PrintTiming("H/V mutation", Median(v_samples.Mutation), Median(a_samples.Mutation));
        PrintTiming("mixed isolation", Median(v_samples.Mixed), Median(a_samples.Mixed));

        const bool final_ok =
            ValidateTopology(vector_backend) &&
            ValidateTopology(apc_backend) &&
            apc_backend.LocksReleased() &&
            SamePayload(vector_backend, apc_backend);

        std::cout
            << "\nFINAL INVARIANTS: " << (final_ok ? "PASS" : "FAIL") << '\n'
            << "============================================================\n"
            << "OVERALL: " << (final_ok ? "PASS" : "FAIL") << '\n'
            << "============================================================\n";

        return final_ok ? 0 : 1;
    }
}
