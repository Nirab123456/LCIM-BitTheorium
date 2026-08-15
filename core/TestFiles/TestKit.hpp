#pragma once

// ============================================================================
// APC / Fabric modular comprehensive test suite
//
// Tests
//   1. Public traversal/data/control baseline: std::vector+pointer vs APC/Fabric
//   2. Global-mutex vector vs APC/Fabric internal synchronization contention
//   3. One writer + N public traversal readers on one H relationship
//   4. Uncontended Acquire/Release graph-CAS attempt audit
//
// Design rule:
//   Test-specific workloads live in Test01..Test04.
//   Graph storage, APC fixture construction, timing/statistics and common
//   validation live in TestKit and are shared.
//
// Test 4 requires the small LCIM_ENABLE_GRAPH_CAS_TEST_PROBE patch supplied
// with this file. Tests 1-3 remain usable without the probe.
// ============================================================================

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
#include <limits>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace APCModularTests
{
namespace TestKit
{
    using namespace BidirectionalInMemGraph;

    using IAB = InstallAxisToBuffer;
    using Axis = IAB::BidirectionalAxis;
    using Inheritance = IAB::DescOfInharitance;
    using Clock = std::chrono::steady_clock;

    enum class Result : uint8_t
    {
        PASS,
        FAIL,
        SKIP
    };

    static constexpr const char* ResultName(Result r) noexcept
    {
        switch (r)
        {
        case Result::PASS: return "PASS";
        case Result::FAIL: return "FAIL";
        case Result::SKIP: return "SKIP";
        default: return "UNKNOWN";
        }
    }

    inline void Banner(const char* title)
    {
        std::cout
            << "\n================================================================================\n"
            << title << '\n'
            << "================================================================================\n";
    }

    inline void Divider()
    {
        std::cout
            << "--------------------------------------------------------------------------------\n";
    }

    inline double Ratio(double numerator, double denominator) noexcept
    {
        return denominator > 0.0 ? numerator / denominator : 0.0;
    }

    inline double NsToUs(double ns) noexcept
    {
        return ns / 1000.0;
    }

    template <size_t N>
    double Median(std::array<double, N> values)
    {
        static_assert(N > 0u);
        std::sort(values.begin(), values.end());
        return values[N / 2u];
    }

    inline uint64_t P99(std::vector<uint64_t> values)
    {
        if (values.empty()) return 0u;

        const size_t rank = static_cast<size_t>(
            (99ull * static_cast<uint64_t>(values.size()) + 99ull) / 100ull);
        const size_t index = std::min(values.size() - 1u, rank - 1u);

        std::nth_element(values.begin(), values.begin() + index, values.end());
        return values[index];
    }

    inline void PerturbSchedule(uint64_t i) noexcept
    {
        if ((i & 63ull) == 0ull)
        {
            std::this_thread::yield();
        }
    }

    template <size_t NodeCount>
    struct RootPlan
    {
        std::array<bool, NodeCount> Horizontal{};
        std::array<bool, NodeCount> Vertical{};
    };

    // ========================================================================
    // Shared pointer graph baseline.
    // std::vector owns all nodes. Pointers are installed only after resize().
    // ========================================================================

    template <size_t PayloadWords, bool EnableVertical>
    struct PointerNode;

    template <size_t PayloadWords, bool EnableVertical>
    struct PointerAxis
    {
        bool OwnsRoot = false;
        PointerNode<PayloadWords, EnableVertical>* FirstChild = nullptr;
        PointerNode<PayloadWords, EnableVertical>* LastChild = nullptr;
        size_t ChildCount = 0u;

        PointerNode<PayloadWords, EnableVertical>* Owner = nullptr;
        PointerNode<PayloadWords, EnableVertical>* Previous = nullptr;
        PointerNode<PayloadWords, EnableVertical>* Next = nullptr;
    };

    template <size_t PayloadWords>
    struct PointerNode<PayloadWords, true>
    {
        PointerAxis<PayloadWords, true> H{};
        PointerAxis<PayloadWords, true> V{};
        std::array<uint64_t, PayloadWords> Payload{};
    };

    template <size_t PayloadWords>
    struct PointerNode<PayloadWords, false>
    {
        PointerAxis<PayloadWords, false> H{};
        std::array<uint64_t, PayloadWords> Payload{};
    };

    template <size_t NodeCount, size_t PayloadWords, bool EnableVertical = true>
    class VectorGraphBackend
    {
    public:
        using Node = PointerNode<PayloadWords, EnableVertical>;
        using Handle = Node*;

        static constexpr size_t NODE_COUNT = NodeCount;
        static constexpr size_t PAYLOAD_WORDS = PayloadWords;

        bool Initialize(const RootPlan<NodeCount>& roots)
        {
            Nodes_.clear();
            Nodes_.resize(NodeCount);

            for (size_t i = 0u; i < NodeCount; ++i)
            {
                Nodes_[i].H.OwnsRoot = roots.Horizontal[i];
                if constexpr (EnableVertical)
                {
                    Nodes_[i].V.OwnsRoot = roots.Vertical[i];
                }
            }
            return true;
        }

        Handle HandleAt(size_t idx) noexcept
        {
            return idx < Nodes_.size() ? &Nodes_[idx] : nullptr;
        }

        size_t IndexOf(Handle handle) const noexcept
        {
            if (!handle || Nodes_.empty()) return NodeCount;
            const Node* first = Nodes_.data();
            const Node* last = first + Nodes_.size();
            if (handle < first || handle >= last) return NodeCount;
            return static_cast<size_t>(handle - first);
        }

        bool Attach(
            size_t predecessor,
            size_t child,
            Axis axis,
            Inheritance inheritance,
            uint32_t /*max_tries*/ = DEFAULT_MAX_TRIES) noexcept
        {
            if (predecessor >= NodeCount || child >= NodeCount || predecessor == child)
            {
                return false;
            }
            return Attach_(HandleAt(predecessor), HandleAt(child), axis, inheritance);
        }

        bool Detach(
            size_t child,
            Axis axis,
            uint32_t /*max_tries*/ = DEFAULT_MAX_TRIES) noexcept
        {
            return child < NodeCount && Detach_(HandleAt(child), axis);
        }

        bool DetachChild(
            size_t owner,
            size_t child,
            Axis axis,
            uint32_t /*max_tries*/ = DEFAULT_MAX_TRIES) noexcept
        {
            if (owner >= NodeCount || child >= NodeCount) return false;
            PointerAxis<PayloadWords, EnableVertical>& child_axis = Axis_(HandleAt(child), axis);
            if (child_axis.Owner != HandleAt(owner)) return false;
            return Detach_(HandleAt(child), axis);
        }

        Handle FindNext(Handle from, Axis axis, Inheritance inheritance) noexcept
        {
            if (!from) return nullptr;
            PointerAxis<PayloadWords, EnableVertical>& state = Axis_(from, axis);
            return inheritance == Inheritance::FIRST_CHILD
                ? state.FirstChild
                : state.Next;
        }

        Handle FindPrevious(Handle from, Axis axis) noexcept
        {
            return from ? Axis_(from, axis).Previous : nullptr;
        }

        bool StorePayload(size_t node, uint32_t word, uint64_t value, bool atomic) noexcept
        {
            if (node >= NodeCount || word >= PayloadWords) return false;
            uint64_t& cell = Nodes_[node].Payload[word];
            if (atomic)
            {
                std::atomic_ref<uint64_t>(cell).store(value, std::memory_order_release);
            }
            else
            {
                cell = value;
            }
            return true;
        }

        bool LoadPayload(Handle node, uint32_t word, uint64_t& value, bool atomic) noexcept
        {
            const size_t idx = IndexOf(node);
            if (idx >= NodeCount || word >= PayloadWords) return false;
            uint64_t& cell = Nodes_[idx].Payload[word];
            value = atomic
                ? std::atomic_ref<uint64_t>(cell).load(std::memory_order_acquire)
                : cell;
            return true;
        }

        bool LocksReleased() noexcept { return true; }

        size_t ApproxStorageBytes() const noexcept
        {
            return Nodes_.capacity() * sizeof(Node);
        }

    private:
        std::vector<Node> Nodes_{};

        static PointerAxis<PayloadWords, EnableVertical>& Axis_(Handle node, Axis axis) noexcept
        {
            if (axis == Axis::HORIZONTAL) return node->H;
            if constexpr (EnableVertical)
            {
                return node->V;
            }
            else
            {
                // H-only baseline specialization. Test 2 never requests V.
                return node->H;
            }
        }

        static bool Attach_(
            Handle predecessor,
            Handle child,
            Axis axis,
            Inheritance inheritance) noexcept
        {
            if (!predecessor || !child || predecessor == child) return false;

            PointerAxis<PayloadWords, EnableVertical>& child_axis = Axis_(child, axis);
            if (child_axis.Owner || child_axis.Previous || child_axis.Next)
            {
                return false;
            }

            if (inheritance == Inheritance::FIRST_CHILD)
            {
                PointerAxis<PayloadWords, EnableVertical>& owner_axis = Axis_(predecessor, axis);
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
                PointerAxis<PayloadWords, EnableVertical>& predecessor_axis = Axis_(predecessor, axis);
                if (!predecessor_axis.Owner || predecessor_axis.Next)
                {
                    return false;
                }

                PointerAxis<PayloadWords, EnableVertical>& owner_axis = Axis_(predecessor_axis.Owner, axis);
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

        static bool Detach_(Handle child, Axis axis) noexcept
        {
            if (!child) return false;

            PointerAxis<PayloadWords, EnableVertical>& child_axis = Axis_(child, axis);
            Handle owner = child_axis.Owner;
            Handle previous = child_axis.Previous;
            Handle next = child_axis.Next;
            if (!owner || !previous) return false;

            PointerAxis<PayloadWords, EnableVertical>& owner_axis = Axis_(owner, axis);
            if (!owner_axis.OwnsRoot || owner_axis.ChildCount == 0u) return false;

            if (previous == owner)
            {
                if (owner_axis.FirstChild != child) return false;
                owner_axis.FirstChild = next;
            }
            else
            {
                PointerAxis<PayloadWords, EnableVertical>& prev_axis = Axis_(previous, axis);
                if (prev_axis.Next != child) return false;
                prev_axis.Next = next;
            }

            if (next)
            {
                Axis_(next, axis).Previous = previous;
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

    // ========================================================================
    // Shared APC/Fabric fixture.
    // Navigation methods intentionally route through FindMyNext/FindPrevious.
    // Raw metadata access exists only for test oracles/diagnostics.
    // ========================================================================

    template <size_t NodeCount, size_t PayloadWords>
    class APCFabricBackend
    {
    public:
        using Handle = AdaptivePackedCellContainer*;

        static constexpr size_t NODE_COUNT = NodeCount;
        static constexpr size_t PAYLOAD_WORDS = PayloadWords;
        static constexpr uint32_t SLOT_WORDS = MINIMUM_APC_CELL_COUNT;
        static constexpr uint32_t FABRIC_SLOT_COUNT = static_cast<uint32_t>(NodeCount + 8u);

        bool Initialize(const RootPlan<NodeCount>& roots)
        {
            Slots_.fill(APCDataStructure::APC_INDEX_BOUND_SENTINAL);
            PayloadBegin_.fill(0u);

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

            for (size_t i = 0u; i < NodeCount; ++i)
            {
                if (!Fabric_.CreateAPC(
                        Nodes_[i],
                        roots.Horizontal[i],
                        roots.Vertical[i],
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

                if constexpr (PayloadWords > 0u)
                {
                    if (!ResolvePayloadBegin_(i)) return false;
                }
            }

            return true;
        }

        Handle HandleAt(size_t idx) noexcept
        {
            return idx < NodeCount ? &Nodes_[idx] : nullptr;
        }

        size_t IndexOf(Handle handle) const noexcept
        {
            if (!handle) return NodeCount;
            const AdaptivePackedCellContainer* first = Nodes_.data();
            const AdaptivePackedCellContainer* last = first + Nodes_.size();
            if (handle < first || handle >= last) return NodeCount;
            return static_cast<size_t>(handle - first);
        }

        bool Attach(
            size_t predecessor,
            size_t child,
            Axis axis,
            Inheritance inheritance,
            uint32_t max_tries = DEFAULT_MAX_TRIES) noexcept
        {
            return predecessor < NodeCount && child < NodeCount &&
                Nodes_[predecessor].AttachAnotherToMe(
                    Nodes_[child], axis, inheritance, max_tries);
        }

        bool Detach(
            size_t child,
            Axis axis,
            uint32_t max_tries = DEFAULT_MAX_TRIES) noexcept
        {
            return child < NodeCount &&
                Nodes_[child].DetachMeFromAnotherEdge(axis, max_tries);
        }

        bool DetachChild(
            size_t owner,
            size_t child,
            Axis axis,
            uint32_t max_tries = DEFAULT_MAX_TRIES) noexcept
        {
            return owner < NodeCount && child < NodeCount &&
                Nodes_[owner].DetachMyChild(Nodes_[child], axis, max_tries);
        }

        Handle FindNext(Handle from, Axis axis, Inheritance inheritance) noexcept
        {
            return from ? from->FindMyNext(axis, inheritance) : nullptr;
        }

        Handle FindPrevious(Handle from, Axis axis) noexcept
        {
            return from ? from->FindPrevious(axis) : nullptr;
        }

        bool StorePayload(size_t node, uint32_t word, uint64_t value, bool atomic) noexcept
        {
            if constexpr (PayloadWords == 0u)
            {
                (void)node; (void)word; (void)value; (void)atomic;
                return false;
            }
            else
            {
                if (node >= NodeCount || word >= PayloadWords) return false;
                const uint32_t local = PayloadBegin_[node] + word;
                if (atomic)
                {
                    Nodes_[node].AtomicallyWriteU64ToAPC(local, value);
                    return true;
                }
                return Nodes_[node].ForceCopyToAPCFromBuffer(local, 1u, &value);
            }
        }

        bool LoadPayload(Handle node, uint32_t word, uint64_t& value, bool atomic) noexcept
        {
            if constexpr (PayloadWords == 0u)
            {
                (void)node; (void)word; (void)value; (void)atomic;
                return false;
            }
            else
            {
                const size_t idx = IndexOf(node);
                if (idx >= NodeCount || word >= PayloadWords) return false;
                const uint32_t local = PayloadBegin_[idx] + word;
                return atomic
                    ? Nodes_[idx].AtomicallyReadLongLongAPCUnit(local, value)
                    : Nodes_[idx].CopyFromAPCToBuffer(local, 1u, &value);
            }
        }

        bool ReadGraphState(size_t node, IAB::GraphMutationValues& values) noexcept
        {
            return node < NodeCount && Fabric_.ReadGraphMutationFlags(Slots_[node], values);
        }

        bool ReadMeta(size_t node, HeaderIdentifierOfAPC id, uint64_t& value) noexcept
        {
            return node < NodeCount && Nodes_[node].ReadAPCMetaUnit(id, value, true);
        }

        uint32_t SlotOf(size_t node) const noexcept
        {
            return node < NodeCount ? Slots_[node] : APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        }

        bool LocksReleased() noexcept
        {
            for (size_t i = 0u; i < NodeCount; ++i)
            {
                IAB::GraphMutationValues values{};
                if (!Fabric_.ReadGraphMutationFlags(Slots_[i], values) ||
                    !values.IsValid ||
                    !IAB::IsIdentityGraphUnlocked(values.Flags))
                {
                    return false;
                }
            }
            return true;
        }

        size_t SegmentPoolLowerBoundBytes() const noexcept
        {
            return static_cast<size_t>(FABRIC_SLOT_COUNT) * SLOT_WORDS * sizeof(uint64_t);
        }

        size_t RuntimeViewBytesLowerBound() const noexcept
        {
            return sizeof(Nodes_) +
                static_cast<size_t>(FABRIC_SLOT_COUNT) *
                    sizeof(std::atomic<AdaptivePackedCellContainer*>);
        }

        VagueTemoraryPremativeFabric Fabric_{};

    private:
        std::array<AdaptivePackedCellContainer, NodeCount> Nodes_{};
        std::array<uint32_t, NodeCount> Slots_{};
        std::array<uint32_t, NodeCount> PayloadBegin_{};

        bool ResolvePayloadBegin_(size_t node) noexcept
        {
            uint64_t packed = FABRIC_CELL_SENTINAL;
            if (!Nodes_[node].ReadAPCMetaUnit(
                    HeaderIdentifierOfAPC::FEEDFORWARD_BOUNDS,
                    packed,
                    true))
            {
                return false;
            }

            const auto bounds = LayoutBoundsOrchestrator::GetLayoutCarrierFromValidLayoutCell(
                packed,
                MacroColumnOfAPC::FEEDFORWARD_MESSAGE);

            if (!bounds.IsValid || bounds.BeginIndex >= bounds.EndIndex ||
                bounds.EndIndex - bounds.BeginIndex < PayloadWords)
            {
                return false;
            }

            PayloadBegin_[node] = static_cast<uint32_t>(bounds.BeginIndex);
            return true;
        }
    };

    struct AxisVersion
    {
        bool Valid = false;
        bool Locked = true;
        uint32_t Sequence = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
    };

    template <typename APCBackend>
    AxisVersion ReadAxisVersion(APCBackend& backend, size_t node, Axis axis) noexcept
    {
        InstallAxisToBuffer::GraphMutationValues values{};
        AxisVersion out{};

        if (!backend.ReadGraphState(node, values) || !values.IsValid)
        {
            return out;
        }

        out.Valid = true;
        out.Locked = InstallAxisToBuffer::IsDesiredAxisLocked(values.Flags, axis);
        out.Sequence = axis == Axis::HORIZONTAL
            ? values.SeqLockHorizontal
            : values.SeqLockVertical;
        return out;
    }

    static bool SameStableVersion(const AxisVersion& before, const AxisVersion& after) noexcept
    {
        return before.Valid && after.Valid &&
            !before.Locked && !after.Locked &&
            before.Sequence == after.Sequence;
    }

} // namespace TestKit

// ============================================================================
// TEST 1
// Public traversal/data/control baseline.
// ============================================================================
namespace Test01_PublicTraversalBaseline
{
    using namespace TestKit;

    constexpr size_t CHAIN_LENGTH = 64u;
    constexpr size_t OWNER_INDEX = CHAIN_LENGTH;
    constexpr size_t NODE_COUNT = CHAIN_LENGTH + 1u;
    constexpr size_t PAYLOAD_WORDS = 32u;

    constexpr uint32_t TRAVERSAL_ROUNDS = 20'000u;
    constexpr uint32_t PAYLOAD_ROUNDS = 200u;
    constexpr uint32_t GRAPH_PAYLOAD_ROUNDS = 2'000u;
    constexpr uint32_t MUTATION_ROUNDS = 500u;
    constexpr uint32_t MEASURED_RUNS = 5u;
    constexpr uint32_t CONCURRENT_TRIALS = 20u;

    using VectorBackend = VectorGraphBackend<NODE_COUNT, PAYLOAD_WORDS>;
    using APCBackend = APCFabricBackend<NODE_COUNT, PAYLOAD_WORDS>;

    struct Timing
    {
        bool Ok = false;
        uint64_t Checksum = 0u;
        uint64_t Operations = 0u;
        int64_t ElapsedNs = 0;

        double NsPerOp() const noexcept
        {
            return Operations == 0u
                ? 0.0
                : static_cast<double>(ElapsedNs) / static_cast<double>(Operations);
        }
    };

    static constexpr size_t Reverse6Bits(size_t value) noexcept
    {
        size_t out = 0u;
        for (size_t bit = 0u; bit < 6u; ++bit)
        {
            out = (out << 1u) | ((value >> bit) & 1u);
        }
        return out;
    }

    static constexpr std::array<size_t, CHAIN_LENGTH> MakeVerticalOrder() noexcept
    {
        std::array<size_t, CHAIN_LENGTH> out{};
        for (size_t i = 0u; i < CHAIN_LENGTH; ++i) out[i] = Reverse6Bits(i);
        return out;
    }

    constexpr auto VERTICAL_ORDER = MakeVerticalOrder();

    static RootPlan<NODE_COUNT> MakeRootPlan() noexcept
    {
        RootPlan<NODE_COUNT> roots{};
        for (size_t i = 0u; i + 1u < CHAIN_LENGTH; ++i)
        {
            roots.Horizontal[i] = true;
        }
        roots.Vertical[OWNER_INDEX] = true;
        return roots;
    }

    template <typename Backend>
    bool BuildScenario(Backend& b)
    {
        if (!b.Initialize(MakeRootPlan())) return false;

        for (size_t i = 0u; i + 1u < CHAIN_LENGTH; ++i)
        {
            if (!b.Attach(i, i + 1u, Axis::HORIZONTAL, Inheritance::FIRST_CHILD))
            {
                return false;
            }
        }

        if (!b.Attach(OWNER_INDEX, VERTICAL_ORDER[0], Axis::VERTICAL, Inheritance::FIRST_CHILD))
        {
            return false;
        }

        for (size_t i = 1u; i < CHAIN_LENGTH; ++i)
        {
            if (!b.Attach(
                    VERTICAL_ORDER[i - 1u],
                    VERTICAL_ORDER[i],
                    Axis::VERTICAL,
                    Inheritance::LINKED_CHILD))
            {
                return false;
            }
        }

        for (size_t node = 0u; node < NODE_COUNT; ++node)
        {
            for (uint32_t word = 0u; word < PAYLOAD_WORDS; ++word)
            {
                const uint64_t value = (static_cast<uint64_t>(node + 1u) << 32u) | word;
                if (!b.StorePayload(node, word, value, false)) return false;
            }
        }
        return true;
    }

    template <typename Backend>
    bool ValidateHorizontal(Backend& b)
    {
        auto current = b.HandleAt(0u);
        if (!current) return false;

        for (size_t expected = 1u; expected < CHAIN_LENGTH; ++expected)
        {
            current = b.FindNext(current, Axis::HORIZONTAL, Inheritance::FIRST_CHILD);
            if (current != b.HandleAt(expected)) return false;
        }

        if (b.FindNext(current, Axis::HORIZONTAL, Inheritance::FIRST_CHILD) != nullptr)
        {
            return false;
        }

        for (size_t expected = CHAIN_LENGTH - 1u; expected > 0u; --expected)
        {
            current = b.FindPrevious(current, Axis::HORIZONTAL);
            if (current != b.HandleAt(expected - 1u)) return false;
        }

        return b.FindPrevious(current, Axis::HORIZONTAL) == nullptr;
    }

    template <typename Backend>
    bool ValidateVertical(Backend& b)
    {
        auto current = b.HandleAt(OWNER_INDEX);
        if (!current) return false;

        current = b.FindNext(current, Axis::VERTICAL, Inheritance::FIRST_CHILD);
        if (current != b.HandleAt(VERTICAL_ORDER[0])) return false;

        for (size_t i = 1u; i < CHAIN_LENGTH; ++i)
        {
            current = b.FindNext(current, Axis::VERTICAL, Inheritance::LINKED_CHILD);
            if (current != b.HandleAt(VERTICAL_ORDER[i])) return false;
        }

        if (b.FindNext(current, Axis::VERTICAL, Inheritance::LINKED_CHILD) != nullptr)
        {
            return false;
        }

        for (size_t i = CHAIN_LENGTH - 1u; i > 0u; --i)
        {
            current = b.FindPrevious(current, Axis::VERTICAL);
            if (current != b.HandleAt(VERTICAL_ORDER[i - 1u])) return false;
        }

        current = b.FindPrevious(current, Axis::VERTICAL);
        if (current != b.HandleAt(OWNER_INDEX)) return false;
        return b.FindPrevious(current, Axis::VERTICAL) == nullptr;
    }

    template <typename Backend>
    bool ValidateAll(Backend& b)
    {
        return ValidateHorizontal(b) && ValidateVertical(b) && b.LocksReleased();
    }

    template <typename Backend>
    Timing TraverseHorizontalForward(Backend& b)
    {
        uint64_t checksum = 0u;
        const auto begin = Clock::now();
        for (uint32_t round = 0u; round < TRAVERSAL_ROUNDS; ++round)
        {
            auto current = b.HandleAt(0u);
            for (size_t i = 1u; i < CHAIN_LENGTH; ++i)
            {
                current = b.FindNext(current, Axis::HORIZONTAL, Inheritance::FIRST_CHILD);
                if (!current) return {};
                checksum += static_cast<uint64_t>(b.IndexOf(current) + 1u);
            }
        }
        const auto end = Clock::now();
        return {true, checksum,
            static_cast<uint64_t>(TRAVERSAL_ROUNDS) * (CHAIN_LENGTH - 1u),
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()};
    }

    template <typename Backend>
    Timing TraverseHorizontalBackward(Backend& b)
    {
        uint64_t checksum = 0u;
        const auto begin = Clock::now();
        for (uint32_t round = 0u; round < TRAVERSAL_ROUNDS; ++round)
        {
            auto current = b.HandleAt(CHAIN_LENGTH - 1u);
            for (size_t i = CHAIN_LENGTH - 1u; i > 0u; --i)
            {
                current = b.FindPrevious(current, Axis::HORIZONTAL);
                if (!current) return {};
                checksum += static_cast<uint64_t>(b.IndexOf(current) + 1u);
            }
        }
        const auto end = Clock::now();
        return {true, checksum,
            static_cast<uint64_t>(TRAVERSAL_ROUNDS) * (CHAIN_LENGTH - 1u),
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()};
    }

    template <typename Backend>
    Timing TraverseVerticalForward(Backend& b)
    {
        uint64_t checksum = 0u;
        const auto begin = Clock::now();
        for (uint32_t round = 0u; round < TRAVERSAL_ROUNDS; ++round)
        {
            auto current = b.FindNext(
                b.HandleAt(OWNER_INDEX), Axis::VERTICAL, Inheritance::FIRST_CHILD);
            if (!current) return {};
            checksum += static_cast<uint64_t>(b.IndexOf(current) + 1u);

            for (size_t i = 1u; i < CHAIN_LENGTH; ++i)
            {
                current = b.FindNext(current, Axis::VERTICAL, Inheritance::LINKED_CHILD);
                if (!current) return {};
                checksum += static_cast<uint64_t>(b.IndexOf(current) + 1u);
            }
        }
        const auto end = Clock::now();
        return {true, checksum,
            static_cast<uint64_t>(TRAVERSAL_ROUNDS) * CHAIN_LENGTH,
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()};
    }

    template <typename Backend>
    Timing TraverseVerticalBackward(Backend& b)
    {
        uint64_t checksum = 0u;
        const auto begin = Clock::now();
        for (uint32_t round = 0u; round < TRAVERSAL_ROUNDS; ++round)
        {
            auto current = b.HandleAt(VERTICAL_ORDER.back());
            for (size_t i = CHAIN_LENGTH - 1u; i > 0u; --i)
            {
                current = b.FindPrevious(current, Axis::VERTICAL);
                if (!current) return {};
                checksum += static_cast<uint64_t>(b.IndexOf(current) + 1u);
            }
            current = b.FindPrevious(current, Axis::VERTICAL);
            if (current != b.HandleAt(OWNER_INDEX)) return {};
            checksum += static_cast<uint64_t>(OWNER_INDEX + 1u);
        }
        const auto end = Clock::now();
        return {true, checksum,
            static_cast<uint64_t>(TRAVERSAL_ROUNDS) * CHAIN_LENGTH,
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()};
    }

    template <typename Backend>
    Timing SequentialPayloadRead(Backend& b, bool atomic)
    {
        uint64_t checksum = 0u;
        const auto begin = Clock::now();
        for (uint32_t round = 0u; round < PAYLOAD_ROUNDS; ++round)
        {
            for (size_t node = 0u; node < CHAIN_LENGTH; ++node)
            {
                auto handle = b.HandleAt(node);
                for (uint32_t word = 0u; word < PAYLOAD_WORDS; ++word)
                {
                    uint64_t value = 0u;
                    if (!b.LoadPayload(handle, word, value, atomic)) return {};
                    checksum += value;
                }
            }
        }
        const auto end = Clock::now();
        return {true, checksum,
            static_cast<uint64_t>(PAYLOAD_ROUNDS) * CHAIN_LENGTH * PAYLOAD_WORDS,
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()};
    }

    template <typename Backend>
    Timing ScrambledGraphPlusPayload(Backend& b)
    {
        uint64_t checksum = 0u;
        const auto begin = Clock::now();
        for (uint32_t round = 0u; round < GRAPH_PAYLOAD_ROUNDS; ++round)
        {
            auto current = b.FindNext(
                b.HandleAt(OWNER_INDEX), Axis::VERTICAL, Inheritance::FIRST_CHILD);
            if (!current) return {};

            for (size_t i = 0u; i < CHAIN_LENGTH; ++i)
            {
                uint64_t value = 0u;
                if (!b.LoadPayload(
                        current,
                        static_cast<uint32_t>(i & (PAYLOAD_WORDS - 1u)),
                        value,
                        false))
                {
                    return {};
                }
                checksum += value;

                if (i + 1u < CHAIN_LENGTH)
                {
                    current = b.FindNext(current, Axis::VERTICAL, Inheritance::LINKED_CHILD);
                    if (!current) return {};
                }
            }
        }
        const auto end = Clock::now();
        return {true, checksum,
            static_cast<uint64_t>(GRAPH_PAYLOAD_ROUNDS) * CHAIN_LENGTH,
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()};
    }

    template <typename Backend>
    Timing SerialHorizontalTailMutation(Backend& b)
    {
        const size_t tail = CHAIN_LENGTH - 1u;
        const size_t parent = CHAIN_LENGTH - 2u;
        uint64_t completed = 0u;
        const auto begin = Clock::now();
        for (uint32_t i = 0u; i < MUTATION_ROUNDS; ++i)
        {
            if (!b.DetachChild(parent, tail, Axis::HORIZONTAL) ||
                !b.Attach(parent, tail, Axis::HORIZONTAL, Inheritance::FIRST_CHILD))
            {
                return {};
            }
            ++completed;
        }
        const auto end = Clock::now();
        return {ValidateHorizontal(b), completed, completed,
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()};
    }

    template <typename Backend>
    Timing SerialVerticalTailMutation(Backend& b)
    {
        const size_t tail = VERTICAL_ORDER.back();
        const size_t previous = VERTICAL_ORDER[CHAIN_LENGTH - 2u];
        uint64_t completed = 0u;
        const auto begin = Clock::now();
        for (uint32_t i = 0u; i < MUTATION_ROUNDS; ++i)
        {
            if (!b.Detach(tail, Axis::VERTICAL) ||
                !b.Attach(previous, tail, Axis::VERTICAL, Inheritance::LINKED_CHILD))
            {
                return {};
            }
            ++completed;
        }
        const auto end = Clock::now();
        return {ValidateVertical(b), completed, completed,
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()};
    }

    struct ConcurrentMutationResult
    {
        bool OperationsOk = false;
        bool IntegrityOk = false;
        uint64_t HorizontalCompleted = 0u;
        uint64_t VerticalCompleted = 0u;
        int64_t ElapsedNs = 0;
    };

    template <typename Backend>
    ConcurrentMutationResult ConcurrentSameNodeHVMutation(Backend& b)
    {
        const size_t tail = CHAIN_LENGTH - 1u;
        const size_t h_parent = CHAIN_LENGTH - 2u;
        const size_t v_previous = VERTICAL_ORDER[CHAIN_LENGTH - 2u];

        std::atomic<bool> failed{false};
        std::atomic<uint64_t> h_done{0u};
        std::atomic<uint64_t> v_done{0u};
        std::barrier start(3);

        const auto begin = Clock::now();

        std::thread h([&]
        {
            start.arrive_and_wait();
            for (uint32_t i = 0u; i < MUTATION_ROUNDS; ++i)
            {
                if (!b.DetachChild(h_parent, tail, Axis::HORIZONTAL) ||
                    !b.Attach(h_parent, tail, Axis::HORIZONTAL, Inheritance::FIRST_CHILD))
                {
                    failed.store(true, std::memory_order_release);
                    return;
                }
                h_done.fetch_add(1u, std::memory_order_relaxed);
                PerturbSchedule(i);
            }
        });

        std::thread v([&]
        {
            start.arrive_and_wait();
            for (uint32_t i = 0u; i < MUTATION_ROUNDS; ++i)
            {
                if (!b.Detach(tail, Axis::VERTICAL) ||
                    !b.Attach(v_previous, tail, Axis::VERTICAL, Inheritance::LINKED_CHILD))
                {
                    failed.store(true, std::memory_order_release);
                    return;
                }
                v_done.fetch_add(1u, std::memory_order_relaxed);
                PerturbSchedule(i + 1000u);
            }
        });

        start.arrive_and_wait();
        h.join();
        v.join();

        const auto end = Clock::now();
        const uint64_t hd = h_done.load(std::memory_order_acquire);
        const uint64_t vd = v_done.load(std::memory_order_acquire);
        const bool operations_ok =
            !failed.load(std::memory_order_acquire) &&
            hd == MUTATION_ROUNDS && vd == MUTATION_ROUNDS;

        return {
            operations_ok,
            ValidateAll(b),
            hd,
            vd,
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
        };
    }

    struct ConcurrentStressSummary
    {
        uint32_t Trials = 0u;
        uint32_t FullCompletionTrials = 0u;
        uint32_t IntegrityFailures = 0u;
        uint64_t HorizontalCycles = 0u;
        uint64_t VerticalCycles = 0u;

        double CompletionPercent() const noexcept
        {
            return Trials == 0u ? 0.0 :
                100.0 * static_cast<double>(FullCompletionTrials) / static_cast<double>(Trials);
        }
    };

    template <typename Backend>
    ConcurrentStressSummary RunConcurrentStress(Backend& b)
    {
        ConcurrentStressSummary out{};
        for (uint32_t trial = 0u; trial < CONCURRENT_TRIALS; ++trial)
        {
            const auto r = ConcurrentSameNodeHVMutation(b);
            ++out.Trials;
            out.HorizontalCycles += r.HorizontalCompleted;
            out.VerticalCycles += r.VerticalCompleted;
            if (r.OperationsOk) ++out.FullCompletionTrials;
            if (!r.IntegrityOk)
            {
                ++out.IntegrityFailures;
                break;
            }
        }
        return out;
    }

    enum class Metric : size_t
    {
        H_FORWARD = 0,
        H_BACKWARD,
        V_FORWARD,
        V_BACKWARD,
        PAYLOAD_DIRECT,
        PAYLOAD_ATOMIC,
        GRAPH_PAYLOAD,
        MUTATE_H,
        MUTATE_V,
        COUNT
    };

    constexpr size_t METRIC_COUNT = static_cast<size_t>(Metric::COUNT);

    static constexpr const char* MetricName(Metric m) noexcept
    {
        switch (m)
        {
        case Metric::H_FORWARD: return "H forward sequential";
        case Metric::H_BACKWARD: return "H backward sequential";
        case Metric::V_FORWARD: return "V forward scrambled";
        case Metric::V_BACKWARD: return "V backward scrambled";
        case Metric::PAYLOAD_DIRECT: return "payload direct read";
        case Metric::PAYLOAD_ATOMIC: return "payload atomic read";
        case Metric::GRAPH_PAYLOAD: return "scrambled graph+payload";
        case Metric::MUTATE_H: return "serial H tail mutate";
        case Metric::MUTATE_V: return "serial V tail mutate";
        default: return "unknown";
        }
    }

    template <typename Backend>
    Timing RunMetric(Backend& b, Metric m)
    {
        switch (m)
        {
        case Metric::H_FORWARD: return TraverseHorizontalForward(b);
        case Metric::H_BACKWARD: return TraverseHorizontalBackward(b);
        case Metric::V_FORWARD: return TraverseVerticalForward(b);
        case Metric::V_BACKWARD: return TraverseVerticalBackward(b);
        case Metric::PAYLOAD_DIRECT: return SequentialPayloadRead(b, false);
        case Metric::PAYLOAD_ATOMIC: return SequentialPayloadRead(b, true);
        case Metric::GRAPH_PAYLOAD: return ScrambledGraphPlusPayload(b);
        case Metric::MUTATE_H: return SerialHorizontalTailMutation(b);
        case Metric::MUTATE_V: return SerialVerticalTailMutation(b);
        default: return {};
        }
    }

    static void PrintMetric(const char* name, double baseline_ns, double apc_ns)
    {
        std::cout
            << std::left << std::setw(28) << name
            << " vector+ptr=" << std::right << std::setw(10) << std::fixed << std::setprecision(2)
            << baseline_ns << " ns/op"
            << "  APC=" << std::setw(10) << apc_ns << " ns/op"
            << "  ratio=" << std::setw(8) << std::setprecision(2)
            << Ratio(apc_ns, baseline_ns) << "x\n";
    }

    inline Result Run()
    {
        Banner("TEST 1 - PUBLIC TRAVERSAL / DATA / CONTROL BASELINE");
        std::cout
            << "APC navigation under test: FindMyNext() + FindPrevious() only.\n"
            << "H topology: 0 -> 1 -> ... -> 63 (FIRST_CHILD depth)\n"
            << "V topology: owner -> bit-reversed 0..63 sibling chain\n\n";

        const auto vector_build_begin = Clock::now();
        VectorBackend vector_backend{};
        if (!BuildScenario(vector_backend)) return Result::FAIL;
        const auto vector_build_end = Clock::now();

        const auto apc_build_begin = Clock::now();
        APCBackend apc_backend{};
        if (!BuildScenario(apc_backend)) return Result::FAIL;
        const auto apc_build_end = Clock::now();

        if (!ValidateAll(vector_backend) || !ValidateAll(apc_backend))
        {
            std::cout << "Initial topology validation: FAIL\n";
            return Result::FAIL;
        }

        const auto vector_build_us = std::chrono::duration_cast<std::chrono::microseconds>(
            vector_build_end - vector_build_begin).count();
        const auto apc_build_us = std::chrono::duration_cast<std::chrono::microseconds>(
            apc_build_end - apc_build_begin).count();

        std::cout
            << "CORRECTNESS\n"
            << "  H forward/backward traversal : PASS\n"
            << "  V sibling forward/backward   : PASS\n"
            << "  APC locks after build         : PASS\n\n"
            << "CONSTRUCTION\n"
            << "  vector+pointer : " << vector_build_us << " us\n"
            << "  APC+Fabric     : " << apc_build_us << " us\n"
            << "  ratio          : " << std::fixed << std::setprecision(2)
            << Ratio(static_cast<double>(apc_build_us), static_cast<double>(vector_build_us))
            << "x\n\n";

        std::array<std::array<double, MEASURED_RUNS>, METRIC_COUNT> vector_samples{};
        std::array<std::array<double, MEASURED_RUNS>, METRIC_COUNT> apc_samples{};

        std::cout << "MEASURED RUNS\n";
        for (uint32_t run = 0u; run < MEASURED_RUNS; ++run)
        {
            for (size_t mi = 0u; mi < METRIC_COUNT; ++mi)
            {
                const Metric metric = static_cast<Metric>(mi);
                Timing v{};
                Timing a{};

                if ((run & 1u) == 0u)
                {
                    v = RunMetric(vector_backend, metric);
                    a = RunMetric(apc_backend, metric);
                }
                else
                {
                    a = RunMetric(apc_backend, metric);
                    v = RunMetric(vector_backend, metric);
                }

                if (!v.Ok || !a.Ok)
                {
                    std::cout << "  failed metric: " << MetricName(metric)
                              << " vector=" << (v.Ok ? "PASS" : "FAIL")
                              << " APC=" << (a.Ok ? "PASS" : "FAIL") << '\n';
                    return Result::FAIL;
                }

                vector_samples[mi][run] = v.NsPerOp();
                apc_samples[mi][run] = a.NsPerOp();
            }

            if (!ValidateAll(vector_backend) || !ValidateAll(apc_backend))
            {
                std::cout << "  post-run topology/lock validation: FAIL\n";
                return Result::FAIL;
            }
            std::cout << "  run " << (run + 1u) << '/' << MEASURED_RUNS << " : PASS\n";
        }

        std::array<double, METRIC_COUNT> vector_median{};
        std::array<double, METRIC_COUNT> apc_median{};

        std::cout << "\nMEDIAN COST PER OPERATION\n";
        for (size_t mi = 0u; mi < METRIC_COUNT; ++mi)
        {
            vector_median[mi] = Median(vector_samples[mi]);
            apc_median[mi] = Median(apc_samples[mi]);
            PrintMetric(MetricName(static_cast<Metric>(mi)), vector_median[mi], apc_median[mi]);
        }

        std::cout << "\nSAME-NODE H/V CONCURRENT MUTATION STRESS\n";
        const auto vector_concurrent = RunConcurrentStress(vector_backend);
        const auto apc_concurrent = RunConcurrentStress(apc_backend);

        std::cout
            << "  vector+ptr full-completion trials : "
            << vector_concurrent.FullCompletionTrials << '/' << vector_concurrent.Trials
            << "  (" << std::fixed << std::setprecision(1)
            << vector_concurrent.CompletionPercent() << "%)\n"
            << "  vector+ptr integrity failures     : " << vector_concurrent.IntegrityFailures << '\n'
            << "  APC+Fabric full-completion trials : "
            << apc_concurrent.FullCompletionTrials << '/' << apc_concurrent.Trials
            << "  (" << apc_concurrent.CompletionPercent() << "%)\n"
            << "  APC+Fabric integrity failures     : " << apc_concurrent.IntegrityFailures << '\n'
            << "  APC completed H/V cycles          : "
            << apc_concurrent.HorizontalCycles << " / " << apc_concurrent.VerticalCycles << '\n';

        const bool vector_final = ValidateAll(vector_backend);
        const bool apc_h_final = ValidateHorizontal(apc_backend);
        const bool apc_v_final = ValidateVertical(apc_backend);
        const bool apc_locks_final = apc_backend.LocksReleased();

        std::cout
            << "\nMEMORY FOOTPRINT SIGNAL (lower bounds, not process RSS)\n"
            << "  vector node storage                 : "
            << vector_backend.ApproxStorageBytes() << " bytes\n"
            << "  APC configured segment-pool minimum : "
            << apc_backend.SegmentPoolLowerBoundBytes() << " bytes\n"
            << "  APC wrapper+runtime-ptr lower bound  : "
            << apc_backend.RuntimeViewBytesLowerBound() << " bytes\n";

        const bool final_ok =
            vector_final &&
            vector_concurrent.IntegrityFailures == 0u &&
            apc_concurrent.IntegrityFailures == 0u &&
            apc_h_final && apc_v_final && apc_locks_final;

        std::cout
            << "\nLACKING SIGNALS RELATIVE TO VECTOR\n"
            << "  sequential graph-hop tax : " << std::setprecision(2)
            << Ratio(apc_median[static_cast<size_t>(Metric::H_FORWARD)],
                     vector_median[static_cast<size_t>(Metric::H_FORWARD)]) << "x\n"
            << "  scrambled graph-hop tax  : "
            << Ratio(apc_median[static_cast<size_t>(Metric::V_FORWARD)],
                     vector_median[static_cast<size_t>(Metric::V_FORWARD)]) << "x\n"
            << "  direct payload-read tax  : "
            << Ratio(apc_median[static_cast<size_t>(Metric::PAYLOAD_DIRECT)],
                     vector_median[static_cast<size_t>(Metric::PAYLOAD_DIRECT)]) << "x\n"
            << "  serial H mutation tax    : "
            << Ratio(apc_median[static_cast<size_t>(Metric::MUTATE_H)],
                     vector_median[static_cast<size_t>(Metric::MUTATE_H)]) << "x\n"
            << "  serial V mutation tax    : "
            << Ratio(apc_median[static_cast<size_t>(Metric::MUTATE_V)],
                     vector_median[static_cast<size_t>(Metric::MUTATE_V)]) << "x\n";

        std::cout
            << "\nTEST 1 OVERALL: " << (final_ok ? "PASS" : "FAIL") << '\n';
        return final_ok ? Result::PASS : Result::FAIL;
    }
}

// ============================================================================
// TEST 2
// Global std::mutex vector vs APC internal synchronization contention sweep.
// ============================================================================
namespace Test02_ContentionSweep
{
    using namespace TestKit;

    constexpr uint32_t SHARD_COUNT = 4u;
    constexpr uint32_t MIN_CONTENTION_LEVEL = 0u;
    constexpr uint32_t MAX_CONTENTION_LEVEL = 19u;
    constexpr uint32_t MAX_WORKERS = MAX_CONTENTION_LEVEL + 1u;
    static_assert(MAX_WORKERS % SHARD_COUNT == 0u);

    constexpr size_t OWNER_BASE = 0u;
    constexpr size_t ANCHOR_BASE = OWNER_BASE + SHARD_COUNT;
    constexpr size_t FIRST_WORKER_CHILD_INDEX = ANCHOR_BASE + SHARD_COUNT;
    constexpr size_t NODE_COUNT = FIRST_WORKER_CHILD_INDEX + MAX_WORKERS;
    constexpr size_t WORKER_CHILDREN_PER_SHARD = MAX_WORKERS / SHARD_COUNT;
    constexpr size_t CHILDREN_PER_SHARD = 1u + WORKER_CHILDREN_PER_SHARD;

    constexpr uint32_t WARMUP_CYCLES_PER_WORKER = 1'000u;
    constexpr uint32_t MEASURED_CYCLES_PER_WORKER = 20'000u;
    constexpr uint32_t MEASURED_RUNS = 3u;
    constexpr uint32_t LATENCY_SAMPLE_STRIDE = 16u;
    constexpr uint32_t APC_INTERNAL_TRIES_PER_PUBLIC_CALL = 1u;
    constexpr uint64_t MAX_RETRY_EVENTS_PER_CYCLE = 1'000'000ull;
    constexpr size_t MAX_FAILED_ATTEMPT_SAMPLES_PER_THREAD = 100'000u;

    using VectorBackend = VectorGraphBackend<NODE_COUNT, 0u, false>;
    using APCBackend = APCFabricBackend<NODE_COUNT, 1u>;

    static_assert((LATENCY_SAMPLE_STRIDE & (LATENCY_SAMPLE_STRIDE - 1u)) == 0u);

    constexpr size_t OwnerForShard(uint32_t shard) noexcept { return OWNER_BASE + shard; }
    constexpr size_t AnchorForShard(uint32_t shard) noexcept { return ANCHOR_BASE + shard; }
    constexpr uint32_t ShardForWorker(uint32_t worker) noexcept { return worker % SHARD_COUNT; }
    constexpr size_t ChildForWorker(uint32_t worker) noexcept { return FIRST_WORKER_CHILD_INDEX + worker; }
    constexpr uint32_t WorkerForChild(size_t child) noexcept
    {
        return static_cast<uint32_t>(child - FIRST_WORKER_CHILD_INDEX);
    }
    constexpr size_t OwnerForChild(size_t child) noexcept
    {
        return OwnerForShard(ShardForWorker(WorkerForChild(child)));
    }

    static RootPlan<NODE_COUNT> MakeRootPlan() noexcept
    {
        RootPlan<NODE_COUNT> roots{};
        for (uint32_t shard = 0u; shard < SHARD_COUNT; ++shard)
        {
            roots.Horizontal[OwnerForShard(shard)] = true;
        }
        return roots;
    }

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
            CycleLatencyNs.reserve(MEASURED_CYCLES_PER_WORKER / LATENCY_SAMPLE_STRIDE + 8u);
            RetriedCycleLatencyNs.reserve(MEASURED_CYCLES_PER_WORKER / LATENCY_SAMPLE_STRIDE + 8u);
            RetryEventsPerCycle.reserve(MEASURED_CYCLES_PER_WORKER);
            MutexWaitLatencyNs.reserve(MEASURED_CYCLES_PER_WORKER / LATENCY_SAMPLE_STRIDE * 2u + 8u);
            FailedMutationAttemptLatencyNs.reserve(
                std::min<size_t>(MAX_FAILED_ATTEMPT_SAMPLES_PER_THREAD,
                                 MEASURED_CYCLES_PER_WORKER));
        }
    };

    struct RunResult
    {
        bool Ok = false;
        bool Aborted = false;
        bool TopologyOk = false;
        bool LocksReleased = false;
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

    inline void RetryBackoff(uint64_t retry_events) noexcept
    {
        if ((retry_events & 63ull) == 0ull) std::this_thread::yield();
    }

    template <typename Backend>
    bool BuildSharedSiblingTopology(Backend& b)
    {
        if (!b.Initialize(MakeRootPlan())) return false;

        for (uint32_t shard = 0u; shard < SHARD_COUNT; ++shard)
        {
            const size_t owner = OwnerForShard(shard);
            const size_t anchor = AnchorForShard(shard);
            if (!b.Attach(owner, anchor, Axis::HORIZONTAL, Inheritance::FIRST_CHILD))
            {
                return false;
            }

            size_t predecessor = anchor;
            for (uint32_t worker = shard; worker < MAX_WORKERS; worker += SHARD_COUNT)
            {
                const size_t child = ChildForWorker(worker);
                if (!b.Attach(predecessor, child, Axis::HORIZONTAL, Inheritance::LINKED_CHILD))
                {
                    return false;
                }
                predecessor = child;
            }
        }
        return true;
    }

    template <typename Backend>
    bool FindCurrentTailIndex(Backend& b, size_t owner_index, size_t& tail_index) noexcept
    {
        auto current = b.FindNext(
            b.HandleAt(owner_index), Axis::HORIZONTAL, Inheritance::FIRST_CHILD);
        if (!current) return false;

        for (size_t step = 0u; step < CHILDREN_PER_SHARD; ++step)
        {
            auto next = b.FindNext(current, Axis::HORIZONTAL, Inheritance::LINKED_CHILD);
            if (!next)
            {
                const size_t idx = b.IndexOf(current);
                if (idx >= NODE_COUNT) return false;
                tail_index = idx;
                return true;
            }
            current = next;
        }
        return false;
    }

    template <typename Backend>
    bool ValidateSharedSiblingTopology(Backend& b)
    {
        std::array<bool, NODE_COUNT> seen{};

        for (uint32_t shard = 0u; shard < SHARD_COUNT; ++shard)
        {
            auto owner = b.HandleAt(OwnerForShard(shard));
            auto current = b.FindNext(owner, Axis::HORIZONTAL, Inheritance::FIRST_CHILD);
            if (current != b.HandleAt(AnchorForShard(shard))) return false;

            typename Backend::Handle previous = owner;
            size_t count = 0u;
            while (current)
            {
                const size_t idx = b.IndexOf(current);
                if (idx < ANCHOR_BASE || idx >= NODE_COUNT || seen[idx]) return false;

                if (idx != AnchorForShard(shard))
                {
                    if (idx < FIRST_WORKER_CHILD_INDEX ||
                        ShardForWorker(WorkerForChild(idx)) != shard)
                    {
                        return false;
                    }
                }

                if (b.FindPrevious(current, Axis::HORIZONTAL) != previous) return false;

                seen[idx] = true;
                ++count;
                if (count > CHILDREN_PER_SHARD) return false;

                previous = current;
                current = b.FindNext(current, Axis::HORIZONTAL, Inheritance::LINKED_CHILD);
            }

            if (count != CHILDREN_PER_SHARD) return false;
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

    bool VectorCycle(
        VectorBackend& b,
        std::mutex& global_graph_mutex,
        size_t child,
        ThreadStats* measured,
        bool sample_latency) noexcept
    {
        Clock::time_point cycle_begin{};
        if (sample_latency) cycle_begin = Clock::now();

        Clock::time_point wait_begin{};
        if (sample_latency) wait_begin = Clock::now();
        global_graph_mutex.lock();
        if (sample_latency && measured)
        {
            measured->MutexWaitLatencyNs.push_back(static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    Clock::now() - wait_begin).count()));
        }

        const bool detached = b.Detach(child, Axis::HORIZONTAL);
        global_graph_mutex.unlock();
        if (!detached) return false;

        if (sample_latency) wait_begin = Clock::now();
        global_graph_mutex.lock();
        if (sample_latency && measured)
        {
            measured->MutexWaitLatencyNs.push_back(static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    Clock::now() - wait_begin).count()));
        }

        const size_t owner = OwnerForChild(child);
        size_t tail = NODE_COUNT;
        const bool tail_ok = FindCurrentTailIndex(b, owner, tail);
        const bool attached = tail_ok &&
            b.Attach(tail, child, Axis::HORIZONTAL, Inheritance::LINKED_CHILD);
        global_graph_mutex.unlock();
        if (!attached) return false;

        if (measured)
        {
            ++measured->CompletedCycles;
            measured->RetryEventsPerCycle.push_back(0u);
            if (sample_latency)
            {
                measured->CycleLatencyNs.push_back(static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        Clock::now() - cycle_begin).count()));
            }
        }
        return true;
    }

    bool APCCycle(
        APCBackend& b,
        size_t child,
        ThreadStats* measured,
        bool sample_latency) noexcept
    {
        Clock::time_point cycle_begin{};
        if (sample_latency) cycle_begin = Clock::now();

        uint64_t retry_events = 0u;
        uint64_t mutation_rejects = 0u;
        uint64_t traversal_restarts = 0u;

        for (;;)
        {
            Clock::time_point attempt_begin{};
            if (sample_latency) attempt_begin = Clock::now();

            const bool ok = b.Detach(
                child, Axis::HORIZONTAL, APC_INTERNAL_TRIES_PER_PUBLIC_CALL);
            if (ok) break;

            ++retry_events;
            ++mutation_rejects;
            if (sample_latency && measured &&
                measured->FailedMutationAttemptLatencyNs.size() <
                    MAX_FAILED_ATTEMPT_SAMPLES_PER_THREAD)
            {
                measured->FailedMutationAttemptLatencyNs.push_back(static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        Clock::now() - attempt_begin).count()));
            }

            if (retry_events >= MAX_RETRY_EVENTS_PER_CYCLE) return false;
            RetryBackoff(retry_events);
        }

        for (;;)
        {
            const size_t owner = OwnerForChild(child);
            size_t tail = NODE_COUNT;
            if (!FindCurrentTailIndex(b, owner, tail))
            {
                ++retry_events;
                ++traversal_restarts;
                if (retry_events >= MAX_RETRY_EVENTS_PER_CYCLE) return false;
                RetryBackoff(retry_events);
                continue;
            }

            Clock::time_point attempt_begin{};
            if (sample_latency) attempt_begin = Clock::now();

            const bool ok = b.Attach(
                tail,
                child,
                Axis::HORIZONTAL,
                Inheritance::LINKED_CHILD,
                APC_INTERNAL_TRIES_PER_PUBLIC_CALL);
            if (ok) break;

            ++retry_events;
            ++mutation_rejects;
            if (sample_latency && measured &&
                measured->FailedMutationAttemptLatencyNs.size() <
                    MAX_FAILED_ATTEMPT_SAMPLES_PER_THREAD)
            {
                measured->FailedMutationAttemptLatencyNs.push_back(static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        Clock::now() - attempt_begin).count()));
            }

            if (retry_events >= MAX_RETRY_EVENTS_PER_CYCLE) return false;
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
                const uint64_t cycle_ns = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        Clock::now() - cycle_begin).count());
                measured->CycleLatencyNs.push_back(cycle_ns);
                if (retry_events != 0u) measured->RetriedCycleLatencyNs.push_back(cycle_ns);
            }
        }
        return true;
    }

    RunResult RunVectorOnce(uint32_t workers)
    {
        RunResult out{};
        out.Workers = workers;

        VectorBackend backend{};
        if (!BuildSharedSiblingTopology(backend) || !ValidateSharedSiblingTopology(backend))
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
                    const bool sample = (i & (LATENCY_SAMPLE_STRIDE - 1u)) == 0u;
                    if (!VectorCycle(backend, global_graph_mutex, child, &stats[worker], sample))
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
            out.Aborted = true;
            return out;
        }

        const auto begin = Clock::now();
        go.store(true, std::memory_order_release);
        for (auto& t : threads) t.join();
        const auto end = Clock::now();

        std::vector<uint64_t> cycle_latencies{};
        std::vector<uint64_t> mutex_wait_latencies{};
        for (auto& s : stats)
        {
            out.CompletedCycles += s.CompletedCycles;
            cycle_latencies.insert(cycle_latencies.end(),
                s.CycleLatencyNs.begin(), s.CycleLatencyNs.end());
            mutex_wait_latencies.insert(mutex_wait_latencies.end(),
                s.MutexWaitLatencyNs.begin(), s.MutexWaitLatencyNs.end());
        }

        out.ElapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
        out.ThroughputCyclesPerSecond = out.ElapsedNs > 0
            ? 1.0e9 * static_cast<double>(out.CompletedCycles) / static_cast<double>(out.ElapsedNs)
            : 0.0;
        out.P99CycleLatencyNs = P99(cycle_latencies);
        out.P99MutexWaitLatencyNs = P99(mutex_wait_latencies);
        out.TopologyOk = ValidateSharedSiblingTopology(backend);
        out.LocksReleased = true;
        out.Aborted = abort.load(std::memory_order_acquire);
        out.Ok = !out.Aborted &&
            out.CompletedCycles == static_cast<uint64_t>(workers) * MEASURED_CYCLES_PER_WORKER &&
            out.TopologyOk;
        return out;
    }

    RunResult RunAPCOnce(uint32_t workers)
    {
        RunResult out{};
        out.Workers = workers;

        APCBackend backend{};
        if (!BuildSharedSiblingTopology(backend) || !ValidateSharedSiblingTopology(backend))
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
                    const bool sample = (i & (LATENCY_SAMPLE_STRIDE - 1u)) == 0u;
                    if (!APCCycle(backend, child, &stats[worker], sample))
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
            out.Aborted = true;
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

        uint64_t total_rejects = 0u;
        uint64_t total_traversal_restarts = 0u;
        uint64_t total_retry_events = 0u;

        for (auto& s : stats)
        {
            out.CompletedCycles += s.CompletedCycles;
            total_rejects += s.MutationRejects;
            total_traversal_restarts += s.TraversalRestarts;
            total_retry_events += s.RetryEvents;

            cycle_latencies.insert(cycle_latencies.end(),
                s.CycleLatencyNs.begin(), s.CycleLatencyNs.end());
            retried_cycle_latencies.insert(retried_cycle_latencies.end(),
                s.RetriedCycleLatencyNs.begin(), s.RetriedCycleLatencyNs.end());
            failed_attempt_latencies.insert(failed_attempt_latencies.end(),
                s.FailedMutationAttemptLatencyNs.begin(), s.FailedMutationAttemptLatencyNs.end());
            retry_events_per_cycle.insert(retry_events_per_cycle.end(),
                s.RetryEventsPerCycle.begin(), s.RetryEventsPerCycle.end());
        }

        out.ElapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
        out.ThroughputCyclesPerSecond = out.ElapsedNs > 0
            ? 1.0e9 * static_cast<double>(out.CompletedCycles) / static_cast<double>(out.ElapsedNs)
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
        out.TopologyOk = ValidateSharedSiblingTopology(backend);
        out.LocksReleased = backend.LocksReleased();
        out.Aborted = abort.load(std::memory_order_acquire);
        out.Ok = !out.Aborted &&
            out.CompletedCycles == static_cast<uint64_t>(workers) * MEASURED_CYCLES_PER_WORKER &&
            out.TopologyOk && out.LocksReleased;
        return out;
    }

    bool MeasureLevel(uint32_t level, LevelResult& out)
    {
        const uint32_t workers = level + 1u;
        out.ContentionLevel = level;
        out.Workers = workers;

        std::array<double, MEASURED_RUNS> vector_tput{};
        std::array<double, MEASURED_RUNS> vector_p99_cycle{};
        std::array<double, MEASURED_RUNS> vector_p99_wait{};
        std::array<double, MEASURED_RUNS> apc_tput{};
        std::array<double, MEASURED_RUNS> apc_p99_cycle{};
        std::array<double, MEASURED_RUNS> apc_p99_failed{};
        std::array<double, MEASURED_RUNS> apc_p99_retried{};
        std::array<double, MEASURED_RUNS> apc_rejects{};
        std::array<double, MEASURED_RUNS> apc_restarts{};
        std::array<double, MEASURED_RUNS> apc_retries{};
        std::array<double, MEASURED_RUNS> apc_p99_retries{};

        for (uint32_t run = 0u; run < MEASURED_RUNS; ++run)
        {
            RunResult v{};
            RunResult a{};
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
                    << "  level " << level << " run " << (run + 1u)
                    << " failed: vector=" << (v.Ok ? "PASS" : "FAIL")
                    << " APC=" << (a.Ok ? "PASS" : "FAIL") << '\n'
                    << "    APC aborted=" << (a.Aborted ? "yes" : "no")
                    << " completed=" << a.CompletedCycles << '/'
                    << static_cast<uint64_t>(workers) * MEASURED_CYCLES_PER_WORKER
                    << " topology=" << (a.TopologyOk ? "PASS" : "FAIL")
                    << " locks=" << (a.LocksReleased ? "PASS" : "FAIL") << '\n';
                return false;
            }

            vector_tput[run] = v.ThroughputCyclesPerSecond;
            vector_p99_cycle[run] = static_cast<double>(v.P99CycleLatencyNs);
            vector_p99_wait[run] = static_cast<double>(v.P99MutexWaitLatencyNs);
            apc_tput[run] = a.ThroughputCyclesPerSecond;
            apc_p99_cycle[run] = static_cast<double>(a.P99CycleLatencyNs);
            apc_p99_failed[run] = static_cast<double>(a.P99FailedMutationAttemptLatencyNs);
            apc_p99_retried[run] = static_cast<double>(a.P99RetriedCycleLatencyNs);
            apc_rejects[run] = a.MutationRejectsPer1000Cycles;
            apc_restarts[run] = a.TraversalRestartsPer1000Cycles;
            apc_retries[run] = a.RetryEventsPerCompletedCycle;
            apc_p99_retries[run] = static_cast<double>(a.P99RetryEventsPerCycle);
        }

        out.VectorThroughput = Median(vector_tput);
        out.VectorP99CycleNs = Median(vector_p99_cycle);
        out.VectorP99MutexWaitNs = Median(vector_p99_wait);
        out.APCThroughput = Median(apc_tput);
        out.APCP99CycleNs = Median(apc_p99_cycle);
        out.APCP99FailedAttemptNs = Median(apc_p99_failed);
        out.APCP99RetriedCycleNs = Median(apc_p99_retried);
        out.APCRejectsPer1000 = Median(apc_rejects);
        out.APCTraversalRestartsPer1000 = Median(apc_restarts);
        out.APCRetryEventsPerCycle = Median(apc_retries);
        out.APCP99RetryEventsPerCycle = Median(apc_p99_retries);
        out.Ok = true;
        return true;
    }

    inline Result Run()
    {
        Banner("TEST 2 - GLOBAL-MUTEX VECTOR vs APC/FABRIC CONTENTION SWEEP");
        std::cout
            << "4 independent H roots. Workers map round-robin to roots.\n"
            << "vector: every graph primitive/traversal under ONE global std::mutex\n"
            << "APC: no external graph lock; public-call max_tries="
            << APC_INTERNAL_TRIES_PER_PUBLIC_CALL << "\n"
            << "Workers 1-4 occupy separate roots; same-root contention starts at worker 5.\n"
            << "Sweep: " << (MIN_CONTENTION_LEVEL + 1u) << ".."
            << (MAX_CONTENTION_LEVEL + 1u) << " workers\n\n";

        std::array<LevelResult, MAX_CONTENTION_LEVEL + 1u> levels{};

        std::cout
            << "level workers | vector Mc/s p99cycle(us) p99mutex(us) | "
            << "APC Mc/s p99cycle(us) p99retry(us) rejects/1k retries/cycle APC/vector\n";
        Divider();

        for (uint32_t level = MIN_CONTENTION_LEVEL; level <= MAX_CONTENTION_LEVEL; ++level)
        {
            LevelResult r{};
            if (!MeasureLevel(level, r)) return Result::FAIL;
            levels[level] = r;

            const double ratio = Ratio(r.APCThroughput, r.VectorThroughput);
            std::cout
                << std::setw(5) << level << ' '
                << std::setw(7) << r.Workers << " | "
                << std::setw(10) << std::fixed << std::setprecision(3)
                << r.VectorThroughput / 1.0e6 << ' '
                << std::setw(12) << NsToUs(r.VectorP99CycleNs) << ' '
                << std::setw(12) << NsToUs(r.VectorP99MutexWaitNs) << " | "
                << std::setw(8) << r.APCThroughput / 1.0e6 << ' '
                << std::setw(12) << NsToUs(r.APCP99CycleNs) << ' '
                << std::setw(12) << NsToUs(r.APCP99RetriedCycleNs) << ' '
                << std::setw(11) << std::setprecision(2) << r.APCRejectsPer1000 << ' '
                << std::setw(13) << std::setprecision(3) << r.APCRetryEventsPerCycle << ' '
                << std::setw(9) << ratio << "x\n";
        }

        std::cout
            << "\nAPC RETRY DIAGNOSTICS\n"
            << "level workers | p99 failed attempt(us) | p99 retry events/cycle | traversal restarts/1k\n";
        Divider();
        for (const auto& r : levels)
        {
            std::cout
                << std::setw(5) << r.ContentionLevel << ' '
                << std::setw(7) << r.Workers << " | "
                << std::setw(22) << std::fixed << std::setprecision(3)
                << NsToUs(r.APCP99FailedAttemptNs) << " | "
                << std::setw(22) << std::setprecision(1)
                << r.APCP99RetryEventsPerCycle << " | "
                << std::setw(21) << std::setprecision(2)
                << r.APCTraversalRestartsPer1000 << '\n';
        }

        const auto& low = levels[MIN_CONTENTION_LEVEL];
        const auto& high = levels[MAX_CONTENTION_LEVEL];
        const double low_ratio = Ratio(low.APCThroughput, low.VectorThroughput);
        const double high_ratio = Ratio(high.APCThroughput, high.VectorThroughput);

        std::cout
            << "\nCORE-BET SIGNAL\n"
            << "  uncontended APC/vector throughput ratio : "
            << std::fixed << std::setprecision(3) << low_ratio << "x\n"
            << "  max-contention APC/vector ratio         : " << high_ratio << "x\n"
            << "  ratio recovery level " << MIN_CONTENTION_LEVEL << " -> "
            << MAX_CONTENTION_LEVEL << "           : "
            << Ratio(high_ratio, low_ratio) << "x\n"
            << "  APC max-contention p99 cycle            : "
            << NsToUs(high.APCP99CycleNs) << " us\n"
            << "  APC max-contention p99 retry-cycle      : "
            << NsToUs(high.APCP99RetriedCycleNs) << " us\n"
            << "  APC max-contention rejects/1000 cycles  : "
            << high.APCRejectsPer1000 << "\n"
            << "\nTEST 2 OVERALL: PASS\n";

        return Result::PASS;
    }
}

// ============================================================================
// TEST 3
// One writer continuously detach/attaches one child on one axis while N
// readers call the PUBLIC FindMyNext/FindPrevious APIs.
//
// Important methodology:
//   nullptr alone is NOT a failure because detached state is legal and the
//   current public reader also returns nullptr when the axis is temporarily
//   locked. We therefore accept only samples bracketed by the same unlocked
//   sequence on BOTH participating APCs. Raw metadata is read only as a test
//   oracle inside that stable bracket; it is never used for navigation.
// ============================================================================
namespace Test03_ReaderWriterTraversal
{
    using namespace TestKit;

    constexpr size_t OWNER = 0u;
    constexpr size_t CHILD = 1u;
    constexpr size_t NODE_COUNT = 2u;
    constexpr uint64_t WRITER_CYCLES = 100'000u;
    constexpr std::array<uint32_t, 5u> READER_COUNTS{1u, 2u, 4u, 8u, 16u};

    using APCBackend = APCFabricBackend<NODE_COUNT, 1u>;

    struct ReaderStats
    {
        uint64_t Iterations = 0u;
        uint64_t StableWindows = 0u;
        uint64_t UnstableWindows = 0u;
        uint64_t AttachedStable = 0u;
        uint64_t DetachedStable = 0u;
        uint64_t PublicReadFailures = 0u;
        uint64_t WrongPointerFailures = 0u;
        uint64_t OracleStructuralFailures = 0u;
    };

    struct RunStats
    {
        bool Ok = false;
        uint32_t Readers = 0u;
        uint64_t WriterCompleted = 0u;
        uint64_t WriterFailures = 0u;
        ReaderStats ReadersTotal{};
        bool FinalTopologyOk = false;
        bool LocksReleased = false;
    };

    static RootPlan<NODE_COUNT> MakeRootPlan() noexcept
    {
        RootPlan<NODE_COUNT> roots{};
        roots.Horizontal[OWNER] = true;
        return roots;
    }

    static void Add(ReaderStats& dst, const ReaderStats& src) noexcept
    {
        dst.Iterations += src.Iterations;
        dst.StableWindows += src.StableWindows;
        dst.UnstableWindows += src.UnstableWindows;
        dst.AttachedStable += src.AttachedStable;
        dst.DetachedStable += src.DetachedStable;
        dst.PublicReadFailures += src.PublicReadFailures;
        dst.WrongPointerFailures += src.WrongPointerFailures;
        dst.OracleStructuralFailures += src.OracleStructuralFailures;
    }

    RunStats RunOnce(uint32_t reader_count)
    {
        RunStats out{};
        out.Readers = reader_count;

        APCBackend backend{};
        if (!backend.Initialize(MakeRootPlan()) ||
            !backend.Attach(OWNER, CHILD, Axis::HORIZONTAL, Inheritance::FIRST_CHILD))
        {
            return out;
        }

        std::atomic<bool> start{false};
        std::atomic<bool> stop{false};
        std::atomic<uint64_t> writer_completed{0u};
        std::atomic<uint64_t> writer_failures{0u};
        std::vector<ReaderStats> reader_stats(reader_count);
        std::vector<std::thread> readers{};
        readers.reserve(reader_count);

        const auto owner_handle = backend.HandleAt(OWNER);
        const auto child_handle = backend.HandleAt(CHILD);
        const uint32_t owner_slot = backend.SlotOf(OWNER);
        const uint32_t child_slot = backend.SlotOf(CHILD);
        const auto map = InstallAxisToBuffer::ConstructAxisMap(Axis::HORIZONTAL);

        for (uint32_t reader = 0u; reader < reader_count; ++reader)
        {
            readers.emplace_back([&, reader]
            {
                ReaderStats local{};
                while (!start.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }

                while (!stop.load(std::memory_order_acquire))
                {
                    const AxisVersion owner_before = ReadAxisVersion(backend, OWNER, Axis::HORIZONTAL);
                    const AxisVersion child_before = ReadAxisVersion(backend, CHILD, Axis::HORIZONTAL);

                    // APIs under test.
                    auto public_next = backend.FindNext(
                        owner_handle, Axis::HORIZONTAL, Inheritance::FIRST_CHILD);
                    auto public_previous = backend.FindPrevious(
                        child_handle, Axis::HORIZONTAL);

                    // Oracle only. Never used to navigate.
                    uint64_t raw_owner_child = FABRIC_CELL_SENTINAL;
                    uint64_t raw_child_previous = FABRIC_CELL_SENTINAL;
                    const bool oracle_read_ok =
                        backend.ReadMeta(OWNER, map.RootOwnedChild, raw_owner_child) &&
                        backend.ReadMeta(CHILD, map.PreviousSibling, raw_child_previous);

                    const AxisVersion child_after = ReadAxisVersion(backend, CHILD, Axis::HORIZONTAL);
                    const AxisVersion owner_after = ReadAxisVersion(backend, OWNER, Axis::HORIZONTAL);

                    ++local.Iterations;

                    if (public_next && public_next != child_handle)
                    {
                        ++local.WrongPointerFailures;
                    }
                    if (public_previous && public_previous != owner_handle)
                    {
                        ++local.WrongPointerFailures;
                    }

                    const bool stable =
                        oracle_read_ok &&
                        SameStableVersion(owner_before, owner_after) &&
                        SameStableVersion(child_before, child_after);

                    if (!stable)
                    {
                        ++local.UnstableWindows;
                        continue;
                    }

                    ++local.StableWindows;

                    const bool oracle_attached =
                        raw_owner_child == child_slot &&
                        raw_child_previous == owner_slot;
                    const bool oracle_detached =
                        raw_owner_child == FABRIC_CELL_SENTINAL &&
                        raw_child_previous == FABRIC_CELL_SENTINAL;

                    if (!oracle_attached && !oracle_detached)
                    {
                        ++local.OracleStructuralFailures;
                        continue;
                    }

                    if (oracle_attached)
                    {
                        ++local.AttachedStable;
                        if (public_next != child_handle || public_previous != owner_handle)
                        {
                            ++local.PublicReadFailures;
                        }
                    }
                    else
                    {
                        ++local.DetachedStable;
                        if (public_next != nullptr || public_previous != nullptr)
                        {
                            ++local.PublicReadFailures;
                        }
                    }
                }

                reader_stats[reader] = local;
            });
        }

        std::thread writer([&]
        {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            for (uint64_t cycle = 0u; cycle < WRITER_CYCLES; ++cycle)
            {
                if (!backend.Detach(CHILD, Axis::HORIZONTAL) ||
                    !backend.Attach(OWNER, CHILD, Axis::HORIZONTAL, Inheritance::FIRST_CHILD))
                {
                    writer_failures.fetch_add(1u, std::memory_order_relaxed);
                    break;
                }
                writer_completed.fetch_add(1u, std::memory_order_relaxed);
                PerturbSchedule(cycle);
            }
            stop.store(true, std::memory_order_release);
        });

        start.store(true, std::memory_order_release);
        writer.join();
        for (auto& t : readers) t.join();

        for (const auto& s : reader_stats) Add(out.ReadersTotal, s);
        out.WriterCompleted = writer_completed.load(std::memory_order_acquire);
        out.WriterFailures = writer_failures.load(std::memory_order_acquire);
        out.FinalTopologyOk =
            backend.FindNext(backend.HandleAt(OWNER), Axis::HORIZONTAL, Inheritance::FIRST_CHILD) ==
                backend.HandleAt(CHILD) &&
            backend.FindPrevious(backend.HandleAt(CHILD), Axis::HORIZONTAL) ==
                backend.HandleAt(OWNER);
        out.LocksReleased = backend.LocksReleased();

        out.Ok =
            out.WriterFailures == 0u &&
            out.WriterCompleted == WRITER_CYCLES &&
            out.ReadersTotal.PublicReadFailures == 0u &&
            out.ReadersTotal.WrongPointerFailures == 0u &&
            out.ReadersTotal.OracleStructuralFailures == 0u &&
            out.FinalTopologyOk &&
            out.LocksReleased;
        return out;
    }

    inline Result Run()
    {
        Banner("TEST 3 - ONE WRITER + N FindMyNext/FindPrevious READERS");
        std::cout
            << "Writer: repeatedly detach/reattach CHILD as OWNER's H FIRST_CHILD.\n"
            << "Readers: public FindMyNext/FindPrevious only for navigation.\n"
            << "Stable-read validation: same unlocked H sequence before/after on owner+child.\n"
            << "Raw identity fields are used only as a validation oracle.\n\n";

        bool all_ok = true;
        std::cout
            << "readers | writer cycles | stable windows | unstable windows | public read fails | wrong ptr | oracle fail | final\n";
        Divider();

        for (uint32_t readers : READER_COUNTS)
        {
            const RunStats r = RunOnce(readers);
            all_ok = all_ok && r.Ok;
            std::cout
                << std::setw(7) << readers << " | "
                << std::setw(13) << r.WriterCompleted << " | "
                << std::setw(14) << r.ReadersTotal.StableWindows << " | "
                << std::setw(16) << r.ReadersTotal.UnstableWindows << " | "
                << std::setw(17) << r.ReadersTotal.PublicReadFailures << " | "
                << std::setw(9) << r.ReadersTotal.WrongPointerFailures << " | "
                << std::setw(11) << r.ReadersTotal.OracleStructuralFailures << " | "
                << (r.Ok ? "PASS" : "FAIL") << '\n';

            if (!r.Ok)
            {
                std::cout
                    << "    writer failures : " << r.WriterFailures << '\n'
                    << "    attached stable : " << r.ReadersTotal.AttachedStable << '\n'
                    << "    detached stable : " << r.ReadersTotal.DetachedStable << '\n'
                    << "    topology final  : " << (r.FinalTopologyOk ? "PASS" : "FAIL") << '\n'
                    << "    locks released  : " << (r.LocksReleased ? "PASS" : "FAIL") << '\n';
            }
        }

        std::cout << "\nTEST 3 OVERALL: " << (all_ok ? "PASS" : "FAIL") << '\n';
        return all_ok ? Result::PASS : Result::FAIL;
    }
}

inline int RunAll()
{
    using TestKit::Result;

    const std::array<std::pair<const char*, Result>, 4u> results{{
        {"Test 1 - public traversal baseline", Test01_PublicTraversalBaseline::Run()},
        {"Test 2 - contention sweep", Test02_ContentionSweep::Run()},
        {"Test 3 - writer/readers", Test03_ReaderWriterTraversal::Run()}
    }};

    TestKit::Banner("APC MODULAR TEST SUITE SUMMARY");
    uint32_t failures = 0u;
    uint32_t skips = 0u;

    for (const auto& [name, result] : results)
    {
        std::cout << "  " << std::left << std::setw(40) << name
                  << TestKit::ResultName(result) << '\n';
        if (result == Result::FAIL) ++failures;
        if (result == Result::SKIP) ++skips;
    }

    std::cout
        << "\n  failures: " << failures
        << "\n  skipped : " << skips
        << "\n================================================================================\n";

    if (failures != 0u) return 1;
    if (skips != 0u) return 2;
    return 0;
}

} // namespace APCModularTests

