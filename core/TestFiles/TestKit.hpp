
#pragma once

// ============================================================================
// APC / Fabric modular comprehensive test suite
//
// Tests
//   1. Traversal/data baseline + real compound parent/sibling mutation
//   2. Global-mutex vector compound move vs APC/Fabric compound move contention
//   3. Public reader snapshot race against one compound cross-parent writer
//   4. Primitive two-call mutation APIs vs compound one-call mutation APIs
//   5. Per-axis acyclicity with a legal cyclic H-union-V projection
//   6. Public RegionView coverage for every supported primitive dtype
//
// Design rule:
//   Test-specific workloads live in Test01..Test06.
//   Graph storage, APC fixture construction, timing/statistics and common
//   validation live in TestKit and are shared.
//
// The suite intentionally avoids repeatedly moving an already-tail child to
// the same edge, because UnlinkAndRelinkToTail has a validated same-edge-tail
// fast path. Timed compound workloads therefore perform real topology changes.
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
    using ReadOperation = AdaptivePackedCellContainer::SeqLockedOperation;

    template <typename Handle>
    struct NavigationRead
    {
        Handle Ptr = nullptr;
        ReadOperation Outcome = ReadOperation::NONE;

        bool IsFound() const noexcept
        {
            return Outcome == ReadOperation::FOUND && Ptr != nullptr;
        }

        bool IsNone() const noexcept
        {
            return Outcome == ReadOperation::NONE && Ptr == nullptr;
        }

        bool IsRetry() const noexcept
        {
            return Outcome == ReadOperation::RETRY && Ptr == nullptr;
        }

        bool ContractValid() const noexcept
        {
            switch (Outcome)
            {
            case ReadOperation::FOUND: return Ptr != nullptr;
            case ReadOperation::NONE:
            case ReadOperation::RETRY: return Ptr == nullptr;
            default: return false;
            }
        }
    };

    struct NavigationCounts
    {
        uint64_t Found = 0u;
        uint64_t Retry = 0u;
        uint64_t None = 0u;
        uint64_t ContractFailures = 0u;

        uint64_t Calls() const noexcept
        {
            return Found + Retry + None;
        }

        template <typename Handle>
        void Observe(const NavigationRead<Handle>& read) noexcept
        {
            switch (read.Outcome)
            {
            case ReadOperation::FOUND: ++Found; break;
            case ReadOperation::RETRY: ++Retry; break;
            case ReadOperation::NONE: ++None; break;
            default: ++ContractFailures; return;
            }

            if (!read.ContractValid()) ++ContractFailures;
        }

        void Add(const NavigationCounts& other) noexcept
        {
            Found += other.Found;
            Retry += other.Retry;
            None += other.None;
            ContractFailures += other.ContractFailures;
        }
    };

    inline void PrintNavigationCounts(
        const char* label,
        const NavigationCounts& counts)
    {
        std::cout
            << "  " << std::left << std::setw(34) << label
            << " calls=" << std::right << std::setw(10) << counts.Calls()
            << "  FOUND=" << std::setw(10) << counts.Found
            << "  RETRY=" << std::setw(10) << counts.Retry
            << "  NONE=" << std::setw(10) << counts.None
            << "  bad-contract=" << counts.ContractFailures
            << '\n';
    }

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

        // Compound-operation baseline equivalent to
        // AdaptivePackedCellContainer::DetachAndReAttachMeToThisParent().
        //
        // In concurrent vector tests the caller deliberately holds the one
        // global graph mutex across this complete function, so the detached
        // intermediate state is never visible to another vector worker.
        bool DetachAndReAttachToParent(
            size_t child,
            size_t root_parent,
            Axis axis,
            uint32_t /*max_tries*/ = DEFAULT_MAX_TRIES) noexcept
        {
            if (child >= NodeCount || root_parent >= NodeCount || child == root_parent)
            {
                return false;
            }

            Handle child_handle = HandleAt(child);
            Handle parent_handle = HandleAt(root_parent);
            auto& child_axis = Axis_(child_handle, axis);

            // Match the APC wrapper's detached-child fallback.
            if (!child_axis.Owner && !child_axis.Previous && !child_axis.Next)
            {
                return Attach_(parent_handle, child_handle, axis, Inheritance::FIRST_CHILD);
            }

            return MoveAttachedChildToOwnerTail_(child_handle, parent_handle, axis);
        }

        // Compound-operation baseline equivalent to
        // AdaptivePackedCellContainer::DetachAndReattachMeAsEquivelentSibbling().
        bool DetachAndReattachAsEquivalentSibling(
            size_t child,
            size_t sibling,
            Axis axis,
            uint32_t /*max_tries*/ = DEFAULT_MAX_TRIES) noexcept
        {
            if (child >= NodeCount || sibling >= NodeCount || child == sibling)
            {
                return false;
            }

            Handle child_handle = HandleAt(child);
            Handle sibling_handle = HandleAt(sibling);
            auto& child_axis = Axis_(child_handle, axis);
            auto& sibling_axis = Axis_(sibling_handle, axis);

            // Match the APC wrapper's detached-child fallback: attach after
            // the supplied sibling, which therefore must currently be tail.
            if (!child_axis.Owner && !child_axis.Previous && !child_axis.Next)
            {
                return Attach_(sibling_handle, child_handle, axis, Inheritance::LINKED_CHILD);
            }

            if (!sibling_axis.Owner)
            {
                return false;
            }

            return MoveAttachedChildToOwnerTail_(
                child_handle,
                sibling_axis.Owner,
                axis
            );
        }

        NavigationRead<Handle> FindNextRead(
            Handle from,
            Axis axis,
            Inheritance inheritance) noexcept
        {
            if (!from) return {};

            PointerAxis<PayloadWords, EnableVertical>& state = Axis_(from, axis);
            Handle next = inheritance == Inheritance::FIRST_CHILD
                ? state.FirstChild
                : state.Next;

            return {
                next,
                next ? ReadOperation::FOUND : ReadOperation::NONE
            };
        }

        NavigationRead<Handle> FindPreviousRead(
            Handle from,
            Axis axis) noexcept
        {
            if (!from) return {};
            Handle previous = Axis_(from, axis).Previous;
            return {
                previous,
                previous ? ReadOperation::FOUND : ReadOperation::NONE
            };
        }

        Handle FindNext(Handle from, Axis axis, Inheritance inheritance) noexcept
        {
            const auto read = FindNextRead(from, axis, inheritance);
            return read.IsFound() ? read.Ptr : nullptr;
        }

        Handle FindPrevious(Handle from, Axis axis) noexcept
        {
            const auto read = FindPreviousRead(from, axis);
            return read.IsFound() ? read.Ptr : nullptr;
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

        static bool MoveAttachedChildToOwnerTail_(
            Handle child,
            Handle destination_owner,
            Axis axis) noexcept
        {
            if (!child || !destination_owner || child == destination_owner)
            {
                return false;
            }

            auto& child_axis = Axis_(child, axis);
            auto& destination_axis = Axis_(destination_owner, axis);

            if (!child_axis.Owner || !child_axis.Previous || !destination_axis.OwnsRoot)
            {
                return false;
            }

            // Match UnlinkAndRelinkToTail's same-edge tail fast path.
            if (
                child_axis.Owner == destination_owner &&
                destination_axis.LastChild == child &&
                child_axis.Next == nullptr
            )
            {
                return true;
            }

            if (!Detach_(child, axis))
            {
                return false;
            }

            if (destination_axis.ChildCount == 0u)
            {
                return Attach_(
                    destination_owner,
                    child,
                    axis,
                    Inheritance::FIRST_CHILD
                );
            }

            return Attach_(
                destination_axis.LastChild,
                child,
                axis,
                Inheritance::LINKED_CHILD
            );
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
    // Payload methods intentionally route through APC's public RegionView API.
    // ========================================================================

    template <size_t NodeCount, size_t PayloadWords>
    class TestBackedndFabric
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

            if (!Fabric_.InitializeFabricWithPtrTable(
                    FABRIC_SLOT_COUNT,
                    SLOT_WORDS
            ))
            {
                return false;
            }

            LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier layout{};
            layout.FeedForward = PayloadWords > 0u ? 1u : 0u;
            layout.FeedBackward = PayloadWords > 0u ? 1u : 0u;
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
            dtype.FEEDBACKWARD_MESSAGE = SchemaDefinition::DataTypeOfMacroColumn::UINT64_T;

            SchemaDefinition::InitialRegionalProtocol protocol{};
            protocol.FEEDFORWARD_MESSAGE = SchemaDefinition::SchemaProtocols::PRIVATE_REGION;
            protocol.FEEDBACKWARD_MESSAGE = SchemaDefinition::SchemaProtocols::ATOMIC_WORD_ARRAY;

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

                const uint64_t slot = Nodes_[i].GetThisSlotIdx();
                if (
                    !APCDataStructure::IsValid32BitAPCUnit(slot) ||
                    slot >= FABRIC_SLOT_COUNT)
                {
                    return false;
                }
                Slots_[i] = static_cast<uint32_t>(slot);

                if constexpr (PayloadWords > 0u)
                {
                    auto direct_view = Nodes_[i].template BuildAViewOverRegion<uint64_t>(
                        MacroColumnOfAPC::FEEDFORWARD_MESSAGE);
                    auto atomic_view = Nodes_[i].template BuildAViewOverRegion<uint64_t>(
                        MacroColumnOfAPC::FEEDBACKWARD_MESSAGE);

                    if (
                        !direct_view.has_value() ||
                        !atomic_view.has_value() ||
                        direct_view->Size() < PayloadWords ||
                        atomic_view->Size() < PayloadWords ||
                        !direct_view->RawMutableSpan().has_value() ||
                        direct_view->GetProtocol() !=
                            SchemaDefinition::SchemaProtocols::PRIVATE_REGION ||
                        atomic_view->GetProtocol() !=
                            SchemaDefinition::SchemaProtocols::ATOMIC_WORD_ARRAY
                    )
                    {
                        return false;
                    }

                    DirectPayloadViews_[i] = direct_view.value();
                    AtomicPayloadViews_[i] = atomic_view.value();
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
                Nodes_[predecessor].AttachSiblingOrChild(
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

        bool DetachAndReAttachToParent(
            size_t child,
            size_t root_parent,
            Axis axis,
            uint32_t max_tries = DEFAULT_MAX_TRIES) noexcept
        {
            return child < NodeCount && root_parent < NodeCount &&
                Nodes_[child].DetachAndReAttachMeToThisParent(
                    Nodes_[root_parent],
                    axis,
                    max_tries
                );
        }

        bool DetachAndReattachAsEquivalentSibling(
            size_t child,
            size_t sibling,
            Axis axis,
            uint32_t max_tries = DEFAULT_MAX_TRIES) noexcept
        {
            return child < NodeCount && sibling < NodeCount &&
                Nodes_[child].DetachAndReattachMeAsEquivelentSibbling(
                    Nodes_[sibling],
                    axis,
                    max_tries
                );
        }

        NavigationRead<Handle> FindNextRead(
            Handle from,
            Axis axis,
            Inheritance inheritance,
            uint32_t max_tries = AdaptivePackedCellContainer::REALTION_FIND_TRIES) noexcept
        {
            if (!from) return {};

            const auto read = from->FindMyNext(axis, inheritance, max_tries);
            return {read.APCPtr_, read.MutationOP_};
        }

        NavigationRead<Handle> FindPreviousRead(
            Handle from,
            Axis axis,
            uint32_t max_tries = AdaptivePackedCellContainer::REALTION_FIND_TRIES) noexcept
        {
            if (!from) return {};

            const auto read = from->FindPrevious(axis, max_tries);
            return {read.APCPtr_, read.MutationOP_};
        }

        Handle FindNext(Handle from, Axis axis, Inheritance inheritance) noexcept
        {
            const auto read = FindNextRead(from, axis, inheritance);
            return read.IsFound() ? read.Ptr : nullptr;
        }

        Handle FindPrevious(Handle from, Axis axis) noexcept
        {
            const auto read = FindPreviousRead(from, axis);
            return read.IsFound() ? read.Ptr : nullptr;
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
                if (atomic)
                {
                    return AtomicPayloadViews_[node].AtomicStore(
                        word,
                        value,
                        std::memory_order_release
                    );
                }

                auto mutable_span = DirectPayloadViews_[node].RawMutableSpan();
                if (!mutable_span.has_value()) return false;
                mutable_span.value()[word] = value;
                return true;
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

                if (atomic)
                {
                    if (word >= AtomicPayloadViews_[idx].Size()) return false;
                    value = AtomicPayloadViews_[idx].AtomicLoad(
                        word,
                        std::memory_order_acquire
                    );
                    return true;
                }

                auto mutable_span = DirectPayloadViews_[idx].RawMutableSpan();
                if (!mutable_span.has_value()) return false;
                value = mutable_span.value()[word];
                return true;
            }
        }

        bool ReadGraphState(size_t node, IAB::GraphMutationValues& values) noexcept
        {
            return node < NodeCount && Fabric_.ReadGraphMutationFlags(Slots_[node], values);
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
        std::array<RegionView<uint64_t>, NodeCount> DirectPayloadViews_{};
        std::array<RegionView<uint64_t>, NodeCount> AtomicPayloadViews_{};
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
// Public traversal/data/control baseline + real compound mutation.
// ============================================================================
namespace Test01_PublicTraversalBaseline
{
    using namespace TestKit;

    constexpr size_t CHAIN_LENGTH = 64u;
    constexpr size_t OWNER_INDEX = CHAIN_LENGTH;
    constexpr size_t V_AUX_ROOT = CHAIN_LENGTH + 1u;
    constexpr size_t V_AUX_ANCHOR = CHAIN_LENGTH + 2u;
    constexpr size_t NODE_COUNT = CHAIN_LENGTH + 3u;
    constexpr size_t PAYLOAD_WORDS = 32u;

    constexpr uint32_t TRAVERSAL_ROUNDS = 20'000u;
    constexpr uint32_t PAYLOAD_ROUNDS = 200u;
    constexpr uint32_t GRAPH_PAYLOAD_ROUNDS = 2'000u;
    constexpr uint32_t MUTATION_ROUNDS = 512u; // even and divisible by 64
    constexpr uint32_t MEASURED_RUNS = 5u;
    constexpr uint32_t CONCURRENT_TRIALS = 20u;

    using VectorBackend = VectorGraphBackend<NODE_COUNT, PAYLOAD_WORDS>;
    using APCBackend = TestBackedndFabric<NODE_COUNT, PAYLOAD_WORDS>;

    struct Timing
    {
        bool Ok = false;
        uint64_t Checksum = 0u;
        uint64_t Operations = 0u;
        int64_t ElapsedNs = 0;
        NavigationCounts Reads{};

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

        // H: 0 -> 1 -> ... -> 63, therefore 0..62 own H roots.
        for (size_t i = 0u; i + 1u < CHAIN_LENGTH; ++i)
        {
            roots.Horizontal[i] = true;
        }

        // Main V sibling edge.
        roots.Vertical[OWNER_INDEX] = true;

        // Independent auxiliary V edge. Keeping this outside the 0..63
        // measured sibling chain lets sibling compound mutation perform a
        // REAL cross-edge move instead of relying on the currently unsupported
        // same-edge non-tail relink case.
        roots.Vertical[V_AUX_ROOT] = true;
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

        if (!b.Attach(
                OWNER_INDEX,
                VERTICAL_ORDER[0],
                Axis::VERTICAL,
                Inheritance::FIRST_CHILD))
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

        if (!b.Attach(
                V_AUX_ROOT,
                V_AUX_ANCHOR,
                Axis::VERTICAL,
                Inheritance::FIRST_CHILD))
        {
            return false;
        }

        for (size_t node = 0u; node < NODE_COUNT; ++node)
        {
            for (uint32_t word = 0u; word < PAYLOAD_WORDS; ++word)
            {
                const uint64_t value =
                    (static_cast<uint64_t>(node + 1u) << 32u) | word;
                if (
                    !b.StorePayload(node, word, value, false) ||
                    !b.StorePayload(node, word, value, true)
                )
                {
                    return false;
                }
            }
        }

        return true;
    }

    template <typename Backend>
    bool ValidateHorizontal(
        Backend& b,
        NavigationCounts* reads = nullptr)
    {
        auto current = b.HandleAt(0u);
        if (!current) return false;

        for (size_t expected = 1u; expected < CHAIN_LENGTH; ++expected)
        {
            const auto read = b.FindNextRead(
                current,
                Axis::HORIZONTAL,
                Inheritance::FIRST_CHILD
            );
            if (reads) reads->Observe(read);
            if (!read.IsFound() || read.Ptr != b.HandleAt(expected)) return false;
            current = read.Ptr;
        }

        const auto forward_end = b.FindNextRead(
            current,
            Axis::HORIZONTAL,
            Inheritance::FIRST_CHILD
        );
        if (reads) reads->Observe(forward_end);
        if (!forward_end.IsNone()) return false;

        for (size_t expected = CHAIN_LENGTH - 1u; expected > 0u; --expected)
        {
            const auto read = b.FindPreviousRead(current, Axis::HORIZONTAL);
            if (reads) reads->Observe(read);
            if (!read.IsFound() || read.Ptr != b.HandleAt(expected - 1u)) return false;
            current = read.Ptr;
        }

        const auto backward_end = b.FindPreviousRead(current, Axis::HORIZONTAL);
        if (reads) reads->Observe(backward_end);
        return backward_end.IsNone();
    }

    template <typename Backend>
    bool ValidateVertical(
        Backend& b,
        NavigationCounts* reads = nullptr)
    {
        const auto first = b.FindNextRead(
            b.HandleAt(OWNER_INDEX),
            Axis::VERTICAL,
            Inheritance::FIRST_CHILD
        );
        if (reads) reads->Observe(first);

        if (!first.IsFound() || first.Ptr != b.HandleAt(VERTICAL_ORDER[0])) return false;
        auto current = first.Ptr;

        for (size_t i = 1u; i < CHAIN_LENGTH; ++i)
        {
            const auto read = b.FindNextRead(
                current,
                Axis::VERTICAL,
                Inheritance::LINKED_CHILD
            );
            if (reads) reads->Observe(read);
            if (!read.IsFound() || read.Ptr != b.HandleAt(VERTICAL_ORDER[i])) return false;
            current = read.Ptr;
        }

        const auto forward_end = b.FindNextRead(
            current,
            Axis::VERTICAL,
            Inheritance::LINKED_CHILD
        );
        if (reads) reads->Observe(forward_end);
        if (!forward_end.IsNone()) return false;

        for (size_t i = CHAIN_LENGTH - 1u; i > 0u; --i)
        {
            const auto read = b.FindPreviousRead(current, Axis::VERTICAL);
            if (reads) reads->Observe(read);
            if (!read.IsFound() || read.Ptr != b.HandleAt(VERTICAL_ORDER[i - 1u])) return false;
            current = read.Ptr;
        }

        const auto owner_read = b.FindPreviousRead(current, Axis::VERTICAL);
        if (reads) reads->Observe(owner_read);
        if (!owner_read.IsFound() || owner_read.Ptr != b.HandleAt(OWNER_INDEX)) return false;
        current = owner_read.Ptr;

        const auto backward_end = b.FindPreviousRead(current, Axis::VERTICAL);
        if (reads) reads->Observe(backward_end);
        if (!backward_end.IsNone()) return false;

        // Auxiliary V edge must return to its one permanent anchor.
        const auto aux_anchor_read = b.FindNextRead(
            b.HandleAt(V_AUX_ROOT),
            Axis::VERTICAL,
            Inheritance::FIRST_CHILD
        );
        if (reads) reads->Observe(aux_anchor_read);
        if (!aux_anchor_read.IsFound()) return false;
        auto aux_anchor = aux_anchor_read.Ptr;

        const auto aux_previous = b.FindPreviousRead(
            aux_anchor,
            Axis::VERTICAL
        );
        if (reads) reads->Observe(aux_previous);

        const auto aux_end = b.FindNextRead(
            aux_anchor,
            Axis::VERTICAL,
            Inheritance::LINKED_CHILD
        );
        if (reads) reads->Observe(aux_end);

        return
            aux_anchor == b.HandleAt(V_AUX_ANCHOR) &&
            aux_previous.IsFound() &&
            aux_previous.Ptr == b.HandleAt(V_AUX_ROOT) &&
            aux_end.IsNone();
    }

    template <typename Backend>
    bool ValidateAll(
        Backend& b,
        NavigationCounts* reads = nullptr)
    {
        return
            ValidateHorizontal(b, reads) &&
            ValidateVertical(b, reads) &&
            b.LocksReleased();
    }

    template <typename Backend>
    Timing TraverseHorizontalForward(Backend& b)
    {
        uint64_t checksum = 0u;
        NavigationCounts reads{};
        const auto begin = Clock::now();

        for (uint32_t round = 0u; round < TRAVERSAL_ROUNDS; ++round)
        {
            auto current = b.HandleAt(0u);
            for (size_t i = 1u; i < CHAIN_LENGTH; ++i)
            {
                const auto read = b.FindNextRead(
                    current,
                    Axis::HORIZONTAL,
                    Inheritance::FIRST_CHILD
                );
                reads.Observe(read);
                if (!read.IsFound()) return {};
                current = read.Ptr;
                checksum += static_cast<uint64_t>(b.IndexOf(current) + 1u);
            }
        }

        const auto end = Clock::now();
        return {
            true,
            checksum,
            static_cast<uint64_t>(TRAVERSAL_ROUNDS) * (CHAIN_LENGTH - 1u),
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                end - begin
            ).count(),
            reads
        };
    }

    template <typename Backend>
    Timing TraverseHorizontalBackward(Backend& b)
    {
        uint64_t checksum = 0u;
        NavigationCounts reads{};
        const auto begin = Clock::now();

        for (uint32_t round = 0u; round < TRAVERSAL_ROUNDS; ++round)
        {
            auto current = b.HandleAt(CHAIN_LENGTH - 1u);
            for (size_t i = CHAIN_LENGTH - 1u; i > 0u; --i)
            {
                const auto read = b.FindPreviousRead(current, Axis::HORIZONTAL);
                reads.Observe(read);
                if (!read.IsFound()) return {};
                current = read.Ptr;
                checksum += static_cast<uint64_t>(b.IndexOf(current) + 1u);
            }
        }

        const auto end = Clock::now();
        return {
            true,
            checksum,
            static_cast<uint64_t>(TRAVERSAL_ROUNDS) * (CHAIN_LENGTH - 1u),
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                end - begin
            ).count(),
            reads
        };
    }

    template <typename Backend>
    Timing TraverseVerticalForward(Backend& b)
    {
        uint64_t checksum = 0u;
        NavigationCounts reads{};
        const auto begin = Clock::now();

        for (uint32_t round = 0u; round < TRAVERSAL_ROUNDS; ++round)
        {
            const auto first = b.FindNextRead(
                b.HandleAt(OWNER_INDEX),
                Axis::VERTICAL,
                Inheritance::FIRST_CHILD
            );
            reads.Observe(first);
            if (!first.IsFound()) return {};
            auto current = first.Ptr;
            checksum += static_cast<uint64_t>(b.IndexOf(current) + 1u);

            for (size_t i = 1u; i < CHAIN_LENGTH; ++i)
            {
                const auto read = b.FindNextRead(
                    current,
                    Axis::VERTICAL,
                    Inheritance::LINKED_CHILD
                );
                reads.Observe(read);
                if (!read.IsFound()) return {};
                current = read.Ptr;
                checksum += static_cast<uint64_t>(b.IndexOf(current) + 1u);
            }
        }

        const auto end = Clock::now();
        return {
            true,
            checksum,
            static_cast<uint64_t>(TRAVERSAL_ROUNDS) * CHAIN_LENGTH,
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                end - begin
            ).count(),
            reads
        };
    }

    template <typename Backend>
    Timing TraverseVerticalBackward(Backend& b)
    {
        uint64_t checksum = 0u;
        NavigationCounts reads{};
        const auto begin = Clock::now();

        for (uint32_t round = 0u; round < TRAVERSAL_ROUNDS; ++round)
        {
            auto current = b.HandleAt(VERTICAL_ORDER.back());
            for (size_t i = CHAIN_LENGTH - 1u; i > 0u; --i)
            {
                const auto read = b.FindPreviousRead(current, Axis::VERTICAL);
                reads.Observe(read);
                if (!read.IsFound()) return {};
                current = read.Ptr;
                checksum += static_cast<uint64_t>(b.IndexOf(current) + 1u);
            }

            const auto owner_read = b.FindPreviousRead(current, Axis::VERTICAL);
            reads.Observe(owner_read);
            if (!owner_read.IsFound() || owner_read.Ptr != b.HandleAt(OWNER_INDEX)) return {};
            current = owner_read.Ptr;
            checksum += static_cast<uint64_t>(OWNER_INDEX + 1u);
        }

        const auto end = Clock::now();
        return {
            true,
            checksum,
            static_cast<uint64_t>(TRAVERSAL_ROUNDS) * CHAIN_LENGTH,
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                end - begin
            ).count(),
            reads
        };
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
        return {
            true,
            checksum,
            static_cast<uint64_t>(PAYLOAD_ROUNDS) *
                CHAIN_LENGTH * PAYLOAD_WORDS,
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                end - begin
            ).count()
        };
    }

    template <typename Backend>
    Timing ScrambledGraphPlusPayload(Backend& b)
    {
        uint64_t checksum = 0u;
        NavigationCounts reads{};
        const auto begin = Clock::now();

        for (uint32_t round = 0u; round < GRAPH_PAYLOAD_ROUNDS; ++round)
        {
            const auto first = b.FindNextRead(
                b.HandleAt(OWNER_INDEX),
                Axis::VERTICAL,
                Inheritance::FIRST_CHILD
            );
            reads.Observe(first);
            if (!first.IsFound()) return {};
            auto current = first.Ptr;

            for (size_t i = 0u; i < CHAIN_LENGTH; ++i)
            {
                uint64_t value = 0u;
                if (!b.LoadPayload(
                        current,
                        static_cast<uint32_t>(i % PAYLOAD_WORDS),
                        value,
                        false))
                {
                    return {};
                }

                checksum += value;

                if (i + 1u < CHAIN_LENGTH)
                {
                    const auto read = b.FindNextRead(
                        current,
                        Axis::VERTICAL,
                        Inheritance::LINKED_CHILD
                    );
                    reads.Observe(read);
                    if (!read.IsFound()) return {};
                    current = read.Ptr;
                }
            }
        }

        const auto end = Clock::now();
        return {
            true,
            checksum,
            static_cast<uint64_t>(GRAPH_PAYLOAD_ROUNDS) * CHAIN_LENGTH,
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                end - begin
            ).count(),
            reads
        };
    }

    // Real cross-edge move every call:
    //
    //   ... 61 -> 62 -> 63
    //
    // 63 alternates between root edge 61 and root edge 62.
    // The even round count restores the original H chain.
    template <typename Backend>
    Timing SerialHorizontalCompoundParentMove(Backend& b)
    {
        const size_t child = CHAIN_LENGTH - 1u;
        const size_t parent_a = CHAIN_LENGTH - 2u; // initial parent: 62
        const size_t parent_b = CHAIN_LENGTH - 3u; // alternate parent: 61

        uint64_t completed = 0u;
        const auto begin = Clock::now();

        for (uint32_t i = 0u; i < MUTATION_ROUNDS; ++i)
        {
            const size_t destination = (i & 1u) == 0u ? parent_b : parent_a;
            if (!b.DetachAndReAttachToParent(
                    child,
                    destination,
                    Axis::HORIZONTAL))
            {
                return {};
            }
            ++completed;
        }

        const auto end = Clock::now();
        return {
            ValidateHorizontal(b),
            completed,
            completed,
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                end - begin
            ).count()
        };
    }

    // Real cross-edge sibling move every call:
    // child 63 alternates between the main V edge and an auxiliary V edge.
    // The destination object supplied to the sibling API is always a real
    // sibling already attached to the destination edge.
    template <typename Backend>
    Timing SerialVerticalCompoundSiblingMove(Backend& b)
    {
        const size_t child = VERTICAL_ORDER.back();
        const size_t main_destination_sibling =
            VERTICAL_ORDER[CHAIN_LENGTH - 2u];

        uint64_t completed = 0u;
        const auto begin = Clock::now();

        for (uint32_t i = 0u; i < MUTATION_ROUNDS; ++i)
        {
            const size_t destination_sibling =
                (i & 1u) == 0u ?
                V_AUX_ANCHOR :
                main_destination_sibling;

            if (!b.DetachAndReattachAsEquivalentSibling(
                    child,
                    destination_sibling,
                    Axis::VERTICAL))
            {
                return {};
            }

            ++completed;
        }

        const auto end = Clock::now();
        return {
            ValidateVertical(b),
            completed,
            completed,
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                end - begin
            ).count()
        };
    }

    struct ConcurrentMutationResult
    {
        bool OperationsOk = false;
        bool IntegrityOk = false;
        uint64_t HorizontalCompleted = 0u;
        uint64_t VerticalCompleted = 0u;
    };

    // Same APC (63), two axes, two independent compound cross-edge moves.
    // H alternates 61 <-> 62. V alternates V_AUX_ROOT <-> main root 64.
    template <typename Backend>
    ConcurrentMutationResult ConcurrentSameNodeHVCompoundMutation(Backend& b)
    {
        const size_t child = CHAIN_LENGTH - 1u;

        std::atomic<bool> failed{false};
        std::atomic<uint64_t> h_done{0u};
        std::atomic<uint64_t> v_done{0u};
        std::barrier start(3);

        std::thread h([&]
        {
            start.arrive_and_wait();

            for (uint32_t i = 0u; i < MUTATION_ROUNDS; ++i)
            {
                const size_t destination =
                    (i & 1u) == 0u ?
                    CHAIN_LENGTH - 3u :
                    CHAIN_LENGTH - 2u;

                if (!b.DetachAndReAttachToParent(
                        child,
                        destination,
                        Axis::HORIZONTAL))
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
                const size_t destination =
                    (i & 1u) == 0u ?
                    V_AUX_ROOT :
                    OWNER_INDEX;

                if (!b.DetachAndReAttachToParent(
                        child,
                        destination,
                        Axis::VERTICAL))
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

        const uint64_t hd = h_done.load(std::memory_order_acquire);
        const uint64_t vd = v_done.load(std::memory_order_acquire);

        return {
            !failed.load(std::memory_order_acquire) &&
                hd == MUTATION_ROUNDS &&
                vd == MUTATION_ROUNDS,
            ValidateAll(b),
            hd,
            vd
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
                100.0 * static_cast<double>(FullCompletionTrials) /
                static_cast<double>(Trials);
        }
    };

    template <typename Backend>
    ConcurrentStressSummary RunConcurrentStress(Backend& b)
    {
        ConcurrentStressSummary out{};

        for (uint32_t trial = 0u; trial < CONCURRENT_TRIALS; ++trial)
        {
            const auto r = ConcurrentSameNodeHVCompoundMutation(b);
            ++out.Trials;
            out.HorizontalCycles += r.HorizontalCompleted;
            out.VerticalCycles += r.VerticalCompleted;

            if (r.OperationsOk) ++out.FullCompletionTrials;

            // An incomplete trial is already a hard failure.  Stop here so
            // its possibly odd-but-legal endpoint is not reused as the
            // starting phase of another scheduled trial.
            if (!r.OperationsOk) break;

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
        COMPOUND_PARENT_H,
        COMPOUND_SIBLING_V,
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
        case Metric::COMPOUND_PARENT_H: return "compound H parent move";
        case Metric::COMPOUND_SIBLING_V: return "compound V sibling move";
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
        case Metric::COMPOUND_PARENT_H:
            return SerialHorizontalCompoundParentMove(b);
        case Metric::COMPOUND_SIBLING_V:
            return SerialVerticalCompoundSiblingMove(b);
        default:
            return {};
        }
    }

    // Vector mutation rows use the same logical compound move, but the entire
    // move is protected by one global mutex. Traversal/data rows remain the
    // raw vector+pointer lower bound so Test 1 still answers the original
    // public traversal/data question.
    Timing RunVectorMetricLocked(
        VectorBackend& b,
        Metric m,
        std::mutex& global_graph_mutex)
    {
        if (m == Metric::COMPOUND_PARENT_H)
        {
            const size_t child = CHAIN_LENGTH - 1u;
            const size_t parent_a = CHAIN_LENGTH - 2u;
            const size_t parent_b = CHAIN_LENGTH - 3u;

            uint64_t completed = 0u;
            const auto begin = Clock::now();

            for (uint32_t i = 0u; i < MUTATION_ROUNDS; ++i)
            {
                const size_t destination =
                    (i & 1u) == 0u ? parent_b : parent_a;

                {
                    std::lock_guard<std::mutex> lock(global_graph_mutex);
                    if (!b.DetachAndReAttachToParent(
                            child,
                            destination,
                            Axis::HORIZONTAL))
                    {
                        return {};
                    }
                }

                ++completed;
            }

            const auto end = Clock::now();

            return {
                ValidateHorizontal(b),
                completed,
                completed,
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end - begin
                ).count()
            };
        }

        if (m == Metric::COMPOUND_SIBLING_V)
        {
            const size_t child = VERTICAL_ORDER.back();
            const size_t main_destination_sibling =
                VERTICAL_ORDER[CHAIN_LENGTH - 2u];

            uint64_t completed = 0u;
            const auto begin = Clock::now();

            for (uint32_t i = 0u; i < MUTATION_ROUNDS; ++i)
            {
                const size_t destination_sibling =
                    (i & 1u) == 0u ?
                    V_AUX_ANCHOR :
                    main_destination_sibling;

                {
                    std::lock_guard<std::mutex> lock(global_graph_mutex);
                    if (!b.DetachAndReattachAsEquivalentSibling(
                            child,
                            destination_sibling,
                            Axis::VERTICAL))
                    {
                        return {};
                    }
                }

                ++completed;
            }

            const auto end = Clock::now();

            return {
                ValidateVertical(b),
                completed,
                completed,
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end - begin
                ).count()
            };
        }

        return RunMetric(b, m);
    }

    ConcurrentMutationResult ConcurrentSameNodeHVCompoundMutationLocked(
        VectorBackend& b,
        std::mutex& global_graph_mutex)
    {
        const size_t child = CHAIN_LENGTH - 1u;

        std::atomic<bool> failed{false};
        std::atomic<uint64_t> h_done{0u};
        std::atomic<uint64_t> v_done{0u};
        std::barrier start(3);

        std::thread h([&]
        {
            start.arrive_and_wait();

            for (uint32_t i = 0u; i < MUTATION_ROUNDS; ++i)
            {
                const size_t destination =
                    (i & 1u) == 0u ?
                    CHAIN_LENGTH - 3u :
                    CHAIN_LENGTH - 2u;

                {
                    std::lock_guard<std::mutex> lock(global_graph_mutex);
                    if (!b.DetachAndReAttachToParent(
                            child,
                            destination,
                            Axis::HORIZONTAL))
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }
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
                const size_t destination =
                    (i & 1u) == 0u ?
                    V_AUX_ROOT :
                    OWNER_INDEX;

                {
                    std::lock_guard<std::mutex> lock(global_graph_mutex);
                    if (!b.DetachAndReAttachToParent(
                            child,
                            destination,
                            Axis::VERTICAL))
                    {
                        failed.store(true, std::memory_order_release);
                        return;
                    }
                }

                v_done.fetch_add(1u, std::memory_order_relaxed);
                PerturbSchedule(i + 1000u);
            }
        });

        start.arrive_and_wait();
        h.join();
        v.join();

        const uint64_t hd = h_done.load(std::memory_order_acquire);
        const uint64_t vd = v_done.load(std::memory_order_acquire);

        return {
            !failed.load(std::memory_order_acquire) &&
                hd == MUTATION_ROUNDS &&
                vd == MUTATION_ROUNDS,
            ValidateAll(b),
            hd,
            vd
        };
    }

    ConcurrentStressSummary RunVectorConcurrentStressLocked(
        VectorBackend& b)
    {
        ConcurrentStressSummary out{};
        std::mutex global_graph_mutex{};

        for (uint32_t trial = 0u; trial < CONCURRENT_TRIALS; ++trial)
        {
            const auto r =
                ConcurrentSameNodeHVCompoundMutationLocked(
                    b,
                    global_graph_mutex
                );

            ++out.Trials;
            out.HorizontalCycles += r.HorizontalCompleted;
            out.VerticalCycles += r.VerticalCompleted;

            if (r.OperationsOk) ++out.FullCompletionTrials;

            if (!r.OperationsOk) break;

            if (!r.IntegrityOk)
            {
                ++out.IntegrityFailures;
                break;
            }
        }

        return out;
    }

    static void PrintMetric(
        const char* name,
        double baseline_ns,
        double apc_ns)
    {
        std::cout
            << std::left << std::setw(30) << name
            << " vector-base=" << std::right << std::setw(10)
            << std::fixed << std::setprecision(2)
            << baseline_ns << " ns/op"
            << "  APC=" << std::setw(10) << apc_ns << " ns/op"
            << "  ratio=" << std::setw(8)
            << Ratio(apc_ns, baseline_ns) << "x\n";
    }

    inline Result Run()
    {
        Banner("TEST 1 - PUBLIC TRAVERSAL / DATA + COMPOUND MUTATION BASELINE");

        std::cout
            << "Navigation: FindMyNext() + FindPrevious() only.\n"
            << "H topology: 0 -> 1 -> ... -> 63.\n"
            << "V topology: owner -> bit-reversed 0..63 sibling chain + isolated auxiliary V edge.\n"
            << "Mutation rows use real DetachAndReAttachMeToThisParent / "
               "DetachAndReattachMeAsEquivelentSibbling-equivalent moves.\n"
            << "No repeated same-edge-tail no-op is timed.\n"
            << "Vector mutation rows/stress hold ONE global std::mutex across each whole move; "
               "traversal/data rows remain raw vector+pointer lower bounds.\n\n";

        const auto vector_build_begin = Clock::now();
        VectorBackend vector_backend{};
        if (!BuildScenario(vector_backend)) return Result::FAIL;
        const auto vector_build_end = Clock::now();

        const auto apc_build_begin = Clock::now();
        APCBackend apc_backend{};
        if (!BuildScenario(apc_backend)) return Result::FAIL;
        const auto apc_build_end = Clock::now();

        std::mutex vector_graph_mutex{};
        NavigationCounts vector_public_reads{};
        NavigationCounts apc_public_reads{};

        const bool vector_initial_ok =
            ValidateAll(vector_backend, &vector_public_reads);

        const bool apc_initial_ok =
            ValidateAll(apc_backend, &apc_public_reads);

        if (!vector_initial_ok || !apc_initial_ok)
        {
            std::cout
                << "Initial topology validation\n"
                << "  vector : " << (vector_initial_ok ? "PASS" : "FAIL") << '\n'
                << "  APC    : " << (apc_initial_ok ? "PASS" : "FAIL") << '\n';

            PrintNavigationCounts(
                "vector FindMyNext/FindPrevious",
                vector_public_reads
            );
            PrintNavigationCounts(
                "APC FindMyNext/FindPrevious",
                apc_public_reads
            );

            return Result::FAIL;
        }

        const auto vector_build_us =
            std::chrono::duration_cast<std::chrono::microseconds>(
                vector_build_end - vector_build_begin
            ).count();

        const auto apc_build_us =
            std::chrono::duration_cast<std::chrono::microseconds>(
                apc_build_end - apc_build_begin
            ).count();

        std::cout
            << "CORRECTNESS\n"
            << "  H forward/backward traversal : PASS\n"
            << "  V sibling forward/backward   : PASS\n"
            << "  APC locks after build         : PASS\n\n"
            << "CONSTRUCTION\n"
            << "  vector+pointer : " << vector_build_us << " us\n"
            << "  APC+Fabric     : " << apc_build_us << " us\n"
            << "  ratio          : " << std::fixed << std::setprecision(2)
            << Ratio(
                static_cast<double>(apc_build_us),
                static_cast<double>(vector_build_us)
            )
            << "x\n\n";

        std::array<std::array<double, MEASURED_RUNS>, METRIC_COUNT>
            vector_samples{};
        std::array<std::array<double, MEASURED_RUNS>, METRIC_COUNT>
            apc_samples{};

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
                    v = RunVectorMetricLocked(vector_backend, metric, vector_graph_mutex);
                    a = RunMetric(apc_backend, metric);
                }
                else
                {
                    a = RunMetric(apc_backend, metric);
                    v = RunVectorMetricLocked(vector_backend, metric, vector_graph_mutex);
                }

                if (!v.Ok || !a.Ok)
                {
                    std::cout
                        << "  failed metric: " << MetricName(metric)
                        << " vector=" << (v.Ok ? "PASS" : "FAIL")
                        << " APC=" << (a.Ok ? "PASS" : "FAIL")
                        << '\n';
                    return Result::FAIL;
                }

                vector_samples[mi][run] = v.NsPerOp();
                apc_samples[mi][run] = a.NsPerOp();
                vector_public_reads.Add(v.Reads);
                apc_public_reads.Add(a.Reads);
            }

            if (
                !ValidateAll(vector_backend, &vector_public_reads) ||
                !ValidateAll(apc_backend, &apc_public_reads)
            )
            {
                std::cout << "  post-run topology/lock validation: FAIL\n";
                return Result::FAIL;
            }

            std::cout
                << "  run " << (run + 1u)
                << '/' << MEASURED_RUNS
                << " : PASS\n";
        }

        std::array<double, METRIC_COUNT> vector_median{};
        std::array<double, METRIC_COUNT> apc_median{};

        std::cout << "\nMEDIAN COST PER OPERATION\n";

        for (size_t mi = 0u; mi < METRIC_COUNT; ++mi)
        {
            vector_median[mi] = Median(vector_samples[mi]);
            apc_median[mi] = Median(apc_samples[mi]);

            PrintMetric(
                MetricName(static_cast<Metric>(mi)),
                vector_median[mi],
                apc_median[mi]
            );
        }

        std::cout << "\nSAME-NODE H/V COMPOUND MUTATION STRESS\n";

        const auto vector_concurrent = RunVectorConcurrentStressLocked(vector_backend);
        const auto apc_concurrent = RunConcurrentStress(apc_backend);

        std::cout
            << "  vector+ptr+lock full-completion trials : "
            << vector_concurrent.FullCompletionTrials << '/'
            << vector_concurrent.Trials << "  ("
            << std::fixed << std::setprecision(1)
            << vector_concurrent.CompletionPercent() << "%)\n"
            << "  vector+ptr+lock integrity failures     : "
            << vector_concurrent.IntegrityFailures << '\n'
            << "  APC+Fabric full-completion trials : "
            << apc_concurrent.FullCompletionTrials << '/'
            << apc_concurrent.Trials << "  ("
            << apc_concurrent.CompletionPercent() << "%)\n"
            << "  APC+Fabric integrity failures     : "
            << apc_concurrent.IntegrityFailures << '\n'
            << "  APC completed H/V compound moves  : "
            << apc_concurrent.HorizontalCycles << " / "
            << apc_concurrent.VerticalCycles << '\n';

        const bool vector_final = ValidateAll(vector_backend);
        const bool apc_final = ValidateAll(apc_backend);

        std::cout << "\nPUBLIC FIND OUTCOMES (TIMED READS + QUIESCENT VALIDATION)\n";
        PrintNavigationCounts("vector FindMyNext/FindPrevious", vector_public_reads);
        PrintNavigationCounts("APC FindMyNext/FindPrevious", apc_public_reads);

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
            apc_final &&
            vector_concurrent.FullCompletionTrials == vector_concurrent.Trials &&
            apc_concurrent.FullCompletionTrials == apc_concurrent.Trials &&
            vector_concurrent.IntegrityFailures == 0u &&
            apc_concurrent.IntegrityFailures == 0u &&
            vector_public_reads.Retry == 0u &&
            apc_public_reads.Retry == 0u &&
            vector_public_reads.ContractFailures == 0u &&
            apc_public_reads.ContractFailures == 0u;

        std::cout
            << "\nLACKING SIGNALS RELATIVE TO VECTOR\n"
            << "  sequential graph-hop tax : "
            << Ratio(
                apc_median[static_cast<size_t>(Metric::H_FORWARD)],
                vector_median[static_cast<size_t>(Metric::H_FORWARD)]
            ) << "x\n"
            << "  scrambled graph-hop tax  : "
            << Ratio(
                apc_median[static_cast<size_t>(Metric::V_FORWARD)],
                vector_median[static_cast<size_t>(Metric::V_FORWARD)]
            ) << "x\n"
            << "  direct payload-read tax  : "
            << Ratio(
                apc_median[static_cast<size_t>(Metric::PAYLOAD_DIRECT)],
                vector_median[static_cast<size_t>(Metric::PAYLOAD_DIRECT)]
            ) << "x\n"
            << "  compound parent-move tax : "
            << Ratio(
                apc_median[static_cast<size_t>(Metric::COMPOUND_PARENT_H)],
                vector_median[static_cast<size_t>(Metric::COMPOUND_PARENT_H)]
            ) << "x\n"
            << "  compound sibling-move tax: "
            << Ratio(
                apc_median[static_cast<size_t>(Metric::COMPOUND_SIBLING_V)],
                vector_median[static_cast<size_t>(Metric::COMPOUND_SIBLING_V)]
            ) << "x\n";

        std::cout
            << "\nTEST 1 OVERALL: "
            << (final_ok ? "PASS" : "FAIL")
            << '\n';

        return final_ok ? Result::PASS : Result::FAIL;
    }

} // namespace Test01_PublicTraversalBaseline

// ============================================================================
// TEST 2
// Global-mutex vector compound move vs APC/Fabric compound move contention.
//
// Fairness rule:
//   - every worker owns one child; no worker can consume another worker's
//     detached intermediate state;
//   - each contention group owns TWO H roots;
//   - a worker alternates its child between those two roots;
//   - vector holds ONE global mutex across the whole logical move;
//   - APC performs one DetachAndReAttachMeToThisParent-equivalent call and
//     retries that complete transaction when max_tries=1 rejects contention.
// ============================================================================
namespace Test02_ContentionSweep
{
    using namespace TestKit;

    constexpr uint32_t GROUP_COUNT = 4u;
    constexpr uint32_t ROOTS_PER_GROUP = 2u;
    constexpr uint32_t ROOT_COUNT = GROUP_COUNT * ROOTS_PER_GROUP;

    constexpr uint32_t MIN_CONTENTION_LEVEL = 0u;
    constexpr uint32_t MAX_CONTENTION_LEVEL = 19u;
    constexpr uint32_t MAX_WORKERS = MAX_CONTENTION_LEVEL + 1u;

    constexpr size_t FIRST_CHILD_INDEX = ROOT_COUNT;
    constexpr size_t NODE_COUNT = FIRST_CHILD_INDEX + MAX_WORKERS;

    constexpr uint32_t WARMUP_CYCLES_PER_WORKER = 1'000u;
    constexpr uint32_t MEASURED_CYCLES_PER_WORKER = 20'000u;
    constexpr uint32_t MEASURED_RUNS = 3u;
    constexpr uint32_t LATENCY_SAMPLE_STRIDE = 16u;

    constexpr uint32_t APC_INTERNAL_TRIES_PER_COMPOUND_CALL = 1u;
    constexpr uint64_t MAX_RETRY_EVENTS_PER_CYCLE = 1'000'000ull;
    constexpr size_t MAX_FAILED_ATTEMPT_SAMPLES_PER_THREAD = 100'000u;

    using VectorBackend = VectorGraphBackend<NODE_COUNT, 0u, false>;
    using APCBackend = TestBackedndFabric<NODE_COUNT, 1u>;

    static_assert(
        (LATENCY_SAMPLE_STRIDE & (LATENCY_SAMPLE_STRIDE - 1u)) == 0u
    );

    constexpr uint32_t GroupForWorker(uint32_t worker) noexcept
    {
        return worker % GROUP_COUNT;
    }

    constexpr size_t RootAForGroup(uint32_t group) noexcept
    {
        return static_cast<size_t>(group * ROOTS_PER_GROUP);
    }

    constexpr size_t RootBForGroup(uint32_t group) noexcept
    {
        return RootAForGroup(group) + 1u;
    }

    constexpr size_t ChildForWorker(uint32_t worker) noexcept
    {
        return FIRST_CHILD_INDEX + worker;
    }

    constexpr uint32_t WorkerForChild(size_t child) noexcept
    {
        return static_cast<uint32_t>(child - FIRST_CHILD_INDEX);
    }

    static RootPlan<NODE_COUNT> MakeRootPlan() noexcept
    {
        RootPlan<NODE_COUNT> roots{};

        for (uint32_t group = 0u; group < GROUP_COUNT; ++group)
        {
            roots.Horizontal[RootAForGroup(group)] = true;
            roots.Horizontal[RootBForGroup(group)] = true;
        }

        return roots;
    }

    struct ThreadStats
    {
        uint64_t CompletedCycles = 0u;
        uint64_t MutationRejects = 0u;
        uint64_t RetryEvents = 0u;

        std::vector<uint64_t> CycleLatencyNs{};
        std::vector<uint64_t> RetriedCycleLatencyNs{};
        std::vector<uint64_t> FailedMutationAttemptLatencyNs{};
        std::vector<uint64_t> RetryEventsPerCycle{};
        std::vector<uint64_t> MutexWaitLatencyNs{};

        void ReserveForMeasuredRun()
        {
            CycleLatencyNs.reserve(
                MEASURED_CYCLES_PER_WORKER / LATENCY_SAMPLE_STRIDE + 8u
            );
            RetriedCycleLatencyNs.reserve(
                MEASURED_CYCLES_PER_WORKER / LATENCY_SAMPLE_STRIDE + 8u
            );
            RetryEventsPerCycle.reserve(MEASURED_CYCLES_PER_WORKER);
            MutexWaitLatencyNs.reserve(
                MEASURED_CYCLES_PER_WORKER / LATENCY_SAMPLE_STRIDE + 8u
            );
            FailedMutationAttemptLatencyNs.reserve(
                std::min<size_t>(
                    MAX_FAILED_ATTEMPT_SAMPLES_PER_THREAD,
                    MEASURED_CYCLES_PER_WORKER
                )
            );
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
        double RetryEventsPerCompletedCycle = 0.0;

        uint64_t P99CycleLatencyNs = 0u;
        uint64_t P99MutexWaitLatencyNs = 0u;
        uint64_t P99FailedMutationAttemptLatencyNs = 0u;
        uint64_t P99RetriedCycleLatencyNs = 0u;
        uint64_t P99RetryEventsPerCycle = 0u;
        NavigationCounts PublicReads{};
    };

    struct LevelResult
    {
        bool Ok = false;
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
        double APCRetryEventsPerCycle = 0.0;
        double APCP99RetryEventsPerCycle = 0.0;
        NavigationCounts VectorPublicReads{};
        NavigationCounts APCPublicReads{};
    };

    inline void RetryBackoff(uint64_t retry_events) noexcept
    {
        if ((retry_events & 63ull) == 0ull)
        {
            std::this_thread::yield();
        }
    }

    template <typename Backend>
    bool BuildContentionTopology(Backend& b, uint32_t active_workers)
    {
        if (!b.Initialize(MakeRootPlan())) return false;

        // Every worker starts on OwnerAPCSlot A of its group.
        // Multiple workers in one group form a sibling chain there.
        std::array<size_t, GROUP_COUNT> tails{};
        for (uint32_t group = 0u; group < GROUP_COUNT; ++group)
        {
            tails[group] = RootAForGroup(group);
        }

        for (uint32_t worker = 0u; worker < active_workers; ++worker)
        {
            const uint32_t group = GroupForWorker(worker);
            const size_t child = ChildForWorker(worker);

            if (tails[group] == RootAForGroup(group))
            {
                if (!b.Attach(
                        RootAForGroup(group),
                        child,
                        Axis::HORIZONTAL,
                        Inheritance::FIRST_CHILD))
                {
                    return false;
                }
            }
            else
            {
                if (!b.Attach(
                        tails[group],
                        child,
                        Axis::HORIZONTAL,
                        Inheritance::LINKED_CHILD))
                {
                    return false;
                }
            }

            tails[group] = child;
        }

        return true;
    }

    template <typename Backend>
    bool ValidateContentionTopology(
        Backend& b,
        uint32_t active_workers,
        NavigationCounts* reads = nullptr)
    {
        std::array<bool, NODE_COUNT> seen{};

        for (uint32_t group = 0u; group < GROUP_COUNT; ++group)
        {
            for (uint32_t which = 0u; which < ROOTS_PER_GROUP; ++which)
            {
                const size_t root =
                    which == 0u ?
                    RootAForGroup(group) :
                    RootBForGroup(group);

                auto previous = b.HandleAt(root);
                const auto first = b.FindNextRead(
                    previous,
                    Axis::HORIZONTAL,
                    Inheritance::FIRST_CHILD
                );
                if (reads) reads->Observe(first);
                if (first.IsRetry() || !first.ContractValid()) return false;

                auto current = first.IsFound() ? first.Ptr : nullptr;

                size_t guard = 0u;

                while (current)
                {
                    const size_t idx = b.IndexOf(current);

                    if (
                        idx < FIRST_CHILD_INDEX ||
                        idx >= FIRST_CHILD_INDEX + active_workers ||
                        seen[idx]
                    )
                    {
                        return false;
                    }

                    const uint32_t worker = WorkerForChild(idx);
                    if (GroupForWorker(worker) != group)
                    {
                        return false;
                    }

                    const auto previous_read = b.FindPreviousRead(
                        current,
                        Axis::HORIZONTAL
                    );
                    if (reads) reads->Observe(previous_read);

                    if (
                        !previous_read.IsFound() ||
                        previous_read.Ptr != previous
                    )
                    {
                        return false;
                    }

                    seen[idx] = true;
                    previous = current;
                    const auto next_read = b.FindNextRead(
                        current,
                        Axis::HORIZONTAL,
                        Inheritance::LINKED_CHILD
                    );
                    if (reads) reads->Observe(next_read);
                    if (next_read.IsRetry() || !next_read.ContractValid()) return false;
                    current = next_read.IsFound() ? next_read.Ptr : nullptr;

                    if (++guard > active_workers) return false;
                }
            }
        }

        for (uint32_t worker = 0u; worker < active_workers; ++worker)
        {
            if (!seen[ChildForWorker(worker)]) return false;
        }

        return b.LocksReleased();
    }

    bool VectorCycle(
        VectorBackend& b,
        std::mutex& global_graph_mutex,
        size_t child,
        size_t destination_root,
        ThreadStats* measured,
        bool sample_latency) noexcept
    {
        Clock::time_point cycle_begin{};
        Clock::time_point wait_begin{};

        if (sample_latency)
        {
            cycle_begin = Clock::now();
            wait_begin = cycle_begin;
        }

        std::unique_lock<std::mutex> lock(
            global_graph_mutex,
            std::defer_lock
        );

        lock.lock();

        if (sample_latency && measured)
        {
            measured->MutexWaitLatencyNs.push_back(
                static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        Clock::now() - wait_begin
                    ).count()
                )
            );
        }

        // One critical section = one logical move.
        // No detached state is globally visible in the vector baseline.
        const bool ok = b.DetachAndReAttachToParent(
            child,
            destination_root,
            Axis::HORIZONTAL
        );

        lock.unlock();

        if (!ok) return false;

        if (measured)
        {
            ++measured->CompletedCycles;
            measured->RetryEventsPerCycle.push_back(0u);

            if (sample_latency)
            {
                measured->CycleLatencyNs.push_back(
                    static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            Clock::now() - cycle_begin
                        ).count()
                    )
                );
            }
        }

        return true;
    }

    bool APCCycle(
        APCBackend& b,
        size_t child,
        size_t destination_root,
        ThreadStats* measured,
        bool sample_latency) noexcept
    {
        Clock::time_point cycle_begin{};
        if (sample_latency) cycle_begin = Clock::now();

        uint64_t retry_events = 0u;
        uint64_t mutation_rejects = 0u;

        for (;;)
        {
            Clock::time_point attempt_begin{};
            if (sample_latency) attempt_begin = Clock::now();

            const bool ok = b.DetachAndReAttachToParent(
                child,
                destination_root,
                Axis::HORIZONTAL,
                APC_INTERNAL_TRIES_PER_COMPOUND_CALL
            );

            if (ok) break;

            ++retry_events;
            ++mutation_rejects;

            if (
                sample_latency &&
                measured &&
                measured->FailedMutationAttemptLatencyNs.size() <
                    MAX_FAILED_ATTEMPT_SAMPLES_PER_THREAD
            )
            {
                measured->FailedMutationAttemptLatencyNs.push_back(
                    static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            Clock::now() - attempt_begin
                        ).count()
                    )
                );
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
            measured->RetryEvents += retry_events;

            if (sample_latency)
            {
                const uint64_t cycle_ns = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        Clock::now() - cycle_begin
                    ).count()
                );

                measured->CycleLatencyNs.push_back(cycle_ns);

                if (retry_events != 0u)
                {
                    measured->RetriedCycleLatencyNs.push_back(cycle_ns);
                }
            }
        }

        return true;
    }

    template <typename BackendCycle>
    bool RunWorkerCycles(
        uint32_t worker,
        uint32_t count,
        BackendCycle&& cycle)
    {
        bool on_a = true;

        for (uint32_t i = 0u; i < count; ++i)
        {
            const uint32_t group = GroupForWorker(worker);
            const size_t destination =
                on_a ?
                RootBForGroup(group) :
                RootAForGroup(group);

            if (!cycle(destination, i))
            {
                return false;
            }

            on_a = !on_a;
        }

        return true;
    }

    RunResult RunVectorOnce(uint32_t workers)
    {
        RunResult out{};
        out.Workers = workers;

        VectorBackend backend{};
        if (
            !BuildContentionTopology(backend, workers) ||
            !ValidateContentionTopology(backend, workers)
        )
        {
            return out;
        }

        std::mutex global_graph_mutex{};
        std::vector<ThreadStats> stats(workers);
        for (auto& s : stats) s.ReserveForMeasuredRun();

        std::atomic<bool> abort{false};
        std::atomic<uint32_t> warmup_ready{0u};
        std::atomic<bool> go{false};

        std::barrier launch(
            static_cast<std::ptrdiff_t>(workers + 1u)
        );

        std::vector<std::thread> threads{};
        threads.reserve(workers);

        for (uint32_t worker = 0u; worker < workers; ++worker)
        {
            threads.emplace_back([&, worker]
            {
                const size_t child = ChildForWorker(worker);
                const uint32_t group = GroupForWorker(worker);
                bool on_a = true;

                launch.arrive_and_wait();

                for (uint32_t i = 0u;
                     i < WARMUP_CYCLES_PER_WORKER;
                     ++i)
                {
                    const size_t destination =
                        on_a ?
                        RootBForGroup(group) :
                        RootAForGroup(group);

                    if (!VectorCycle(
                            backend,
                            global_graph_mutex,
                            child,
                            destination,
                            nullptr,
                            false))
                    {
                        abort.store(true, std::memory_order_release);
                        break;
                    }

                    on_a = !on_a;
                }

                warmup_ready.fetch_add(1u, std::memory_order_acq_rel);

                while (
                    !go.load(std::memory_order_acquire) &&
                    !abort.load(std::memory_order_acquire)
                )
                {
                    std::this_thread::yield();
                }

                if (abort.load(std::memory_order_acquire)) return;

                for (uint32_t i = 0u;
                     i < MEASURED_CYCLES_PER_WORKER;
                     ++i)
                {
                    const size_t destination =
                        on_a ?
                        RootBForGroup(group) :
                        RootAForGroup(group);

                    const bool sample =
                        (i & (LATENCY_SAMPLE_STRIDE - 1u)) == 0u;

                    if (!VectorCycle(
                            backend,
                            global_graph_mutex,
                            child,
                            destination,
                            &stats[worker],
                            sample))
                    {
                        abort.store(true, std::memory_order_release);
                        return;
                    }

                    on_a = !on_a;
                }
            });
        }

        launch.arrive_and_wait();

        while (
            warmup_ready.load(std::memory_order_acquire) != workers &&
            !abort.load(std::memory_order_acquire)
        )
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

            cycle_latencies.insert(
                cycle_latencies.end(),
                s.CycleLatencyNs.begin(),
                s.CycleLatencyNs.end()
            );

            mutex_wait_latencies.insert(
                mutex_wait_latencies.end(),
                s.MutexWaitLatencyNs.begin(),
                s.MutexWaitLatencyNs.end()
            );
        }

        out.ElapsedNs =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                end - begin
            ).count();

        out.ThroughputCyclesPerSecond =
            out.ElapsedNs > 0 ?
            1.0e9 * static_cast<double>(out.CompletedCycles) /
                static_cast<double>(out.ElapsedNs) :
            0.0;

        out.P99CycleLatencyNs = P99(cycle_latencies);
        out.P99MutexWaitLatencyNs = P99(mutex_wait_latencies);
        out.TopologyOk = ValidateContentionTopology(
            backend,
            workers,
            &out.PublicReads
        );
        out.LocksReleased = true;
        out.Aborted = abort.load(std::memory_order_acquire);

        out.Ok =
            !out.Aborted &&
            out.CompletedCycles ==
                static_cast<uint64_t>(workers) *
                MEASURED_CYCLES_PER_WORKER &&
            out.TopologyOk &&
            out.PublicReads.Retry == 0u &&
            out.PublicReads.ContractFailures == 0u;

        return out;
    }

    RunResult RunAPCOnce(uint32_t workers)
    {
        RunResult out{};
        out.Workers = workers;

        APCBackend backend{};
        if (
            !BuildContentionTopology(backend, workers) ||
            !ValidateContentionTopology(backend, workers)
        )
        {
            return out;
        }

        std::vector<ThreadStats> stats(workers);
        for (auto& s : stats) s.ReserveForMeasuredRun();

        std::atomic<bool> abort{false};
        std::atomic<uint32_t> warmup_ready{0u};
        std::atomic<bool> go{false};

        std::barrier launch(
            static_cast<std::ptrdiff_t>(workers + 1u)
        );

        std::vector<std::thread> threads{};
        threads.reserve(workers);

        for (uint32_t worker = 0u; worker < workers; ++worker)
        {
            threads.emplace_back([&, worker]
            {
                const size_t child = ChildForWorker(worker);
                const uint32_t group = GroupForWorker(worker);
                bool on_a = true;

                launch.arrive_and_wait();

                for (uint32_t i = 0u;
                     i < WARMUP_CYCLES_PER_WORKER;
                     ++i)
                {
                    const size_t destination =
                        on_a ?
                        RootBForGroup(group) :
                        RootAForGroup(group);

                    if (!APCCycle(
                            backend,
                            child,
                            destination,
                            nullptr,
                            false))
                    {
                        abort.store(true, std::memory_order_release);
                        break;
                    }

                    on_a = !on_a;
                }

                warmup_ready.fetch_add(1u, std::memory_order_acq_rel);

                while (
                    !go.load(std::memory_order_acquire) &&
                    !abort.load(std::memory_order_acquire)
                )
                {
                    std::this_thread::yield();
                }

                if (abort.load(std::memory_order_acquire)) return;

                for (uint32_t i = 0u;
                     i < MEASURED_CYCLES_PER_WORKER;
                     ++i)
                {
                    const size_t destination =
                        on_a ?
                        RootBForGroup(group) :
                        RootAForGroup(group);

                    const bool sample =
                        (i & (LATENCY_SAMPLE_STRIDE - 1u)) == 0u;

                    if (!APCCycle(
                            backend,
                            child,
                            destination,
                            &stats[worker],
                            sample))
                    {
                        abort.store(true, std::memory_order_release);
                        return;
                    }

                    on_a = !on_a;
                }
            });
        }

        launch.arrive_and_wait();

        while (
            warmup_ready.load(std::memory_order_acquire) != workers &&
            !abort.load(std::memory_order_acquire)
        )
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
        uint64_t total_retry_events = 0u;

        for (auto& s : stats)
        {
            out.CompletedCycles += s.CompletedCycles;
            total_rejects += s.MutationRejects;
            total_retry_events += s.RetryEvents;

            cycle_latencies.insert(
                cycle_latencies.end(),
                s.CycleLatencyNs.begin(),
                s.CycleLatencyNs.end()
            );

            retried_cycle_latencies.insert(
                retried_cycle_latencies.end(),
                s.RetriedCycleLatencyNs.begin(),
                s.RetriedCycleLatencyNs.end()
            );

            failed_attempt_latencies.insert(
                failed_attempt_latencies.end(),
                s.FailedMutationAttemptLatencyNs.begin(),
                s.FailedMutationAttemptLatencyNs.end()
            );

            retry_events_per_cycle.insert(
                retry_events_per_cycle.end(),
                s.RetryEventsPerCycle.begin(),
                s.RetryEventsPerCycle.end()
            );
        }

        out.ElapsedNs =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                end - begin
            ).count();

        out.ThroughputCyclesPerSecond =
            out.ElapsedNs > 0 ?
            1.0e9 * static_cast<double>(out.CompletedCycles) /
                static_cast<double>(out.ElapsedNs) :
            0.0;

        if (out.CompletedCycles != 0u)
        {
            out.MutationRejectsPer1000Cycles =
                1000.0 * static_cast<double>(total_rejects) /
                static_cast<double>(out.CompletedCycles);

            out.RetryEventsPerCompletedCycle =
                static_cast<double>(total_retry_events) /
                static_cast<double>(out.CompletedCycles);
        }

        out.P99CycleLatencyNs = P99(cycle_latencies);
        out.P99FailedMutationAttemptLatencyNs =
            P99(failed_attempt_latencies);
        out.P99RetriedCycleLatencyNs =
            P99(retried_cycle_latencies);
        out.P99RetryEventsPerCycle =
            P99(retry_events_per_cycle);

        out.TopologyOk = ValidateContentionTopology(
            backend,
            workers,
            &out.PublicReads
        );
        out.LocksReleased = backend.LocksReleased();
        out.Aborted = abort.load(std::memory_order_acquire);

        out.Ok =
            !out.Aborted &&
            out.CompletedCycles ==
                static_cast<uint64_t>(workers) *
                MEASURED_CYCLES_PER_WORKER &&
            out.TopologyOk &&
            out.LocksReleased &&
            out.PublicReads.Retry == 0u &&
            out.PublicReads.ContractFailures == 0u;

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
                    << "  level " << level
                    << " run " << run
                    << " failed. vector="
                    << (v.Ok ? "PASS" : "FAIL")
                    << " APC="
                    << (a.Ok ? "PASS" : "FAIL")
                    << '\n';
                return false;
            }

            out.VectorPublicReads.Add(v.PublicReads);
            out.APCPublicReads.Add(a.PublicReads);

            vector_tput[run] = v.ThroughputCyclesPerSecond;
            vector_p99_cycle[run] =
                static_cast<double>(v.P99CycleLatencyNs);
            vector_p99_wait[run] =
                static_cast<double>(v.P99MutexWaitLatencyNs);

            apc_tput[run] = a.ThroughputCyclesPerSecond;
            apc_p99_cycle[run] =
                static_cast<double>(a.P99CycleLatencyNs);
            apc_p99_failed[run] =
                static_cast<double>(a.P99FailedMutationAttemptLatencyNs);
            apc_p99_retried[run] =
                static_cast<double>(a.P99RetriedCycleLatencyNs);
            apc_rejects[run] = a.MutationRejectsPer1000Cycles;
            apc_retries[run] = a.RetryEventsPerCompletedCycle;
            apc_p99_retries[run] =
                static_cast<double>(a.P99RetryEventsPerCycle);
        }

        out.VectorThroughput = Median(vector_tput);
        out.VectorP99CycleNs = Median(vector_p99_cycle);
        out.VectorP99MutexWaitNs = Median(vector_p99_wait);

        out.APCThroughput = Median(apc_tput);
        out.APCP99CycleNs = Median(apc_p99_cycle);
        out.APCP99FailedAttemptNs = Median(apc_p99_failed);
        out.APCP99RetriedCycleNs = Median(apc_p99_retried);
        out.APCRejectsPer1000 = Median(apc_rejects);
        out.APCRetryEventsPerCycle = Median(apc_retries);
        out.APCP99RetryEventsPerCycle = Median(apc_p99_retries);

        out.Ok = true;
        return true;
    }

    inline Result Run()
    {
        Banner(
            "TEST 2 - GLOBAL-MUTEX VECTOR vs APC/FABRIC COMPOUND CONTENTION SWEEP"
        );

        std::cout
            << "4 independent contention groups; each group has TWO H roots.\n"
            << "Each worker owns one child and alternates it root-A <-> root-B.\n"
            << "vector: one global std::mutex covers the COMPLETE logical move.\n"
            << "APC: one compound DetachAndReAttachMeToThisParent-equivalent call; "
               "no external graph lock.\n"
            << "APC compound-call max_tries="
            << APC_INTERNAL_TRIES_PER_COMPOUND_CALL << ".\n"
            << "Workers 1-4 occupy separate root-pairs; same-group contention "
               "starts at worker 5.\n"
            << "Sweep: " << (MIN_CONTENTION_LEVEL + 1u)
            << ".." << (MAX_CONTENTION_LEVEL + 1u)
            << " workers.\n\n";

        std::array<LevelResult, MAX_CONTENTION_LEVEL + 1u> levels{};

        std::cout
            << "level workers | vector Mc/s p99cycle(us) p99mutex(us) | "
            << "APC Mc/s p99cycle(us) p99retry(us) rejects/1k retries/cycle APC/vector\n";

        Divider();

        for (
            uint32_t level = MIN_CONTENTION_LEVEL;
            level <= MAX_CONTENTION_LEVEL;
            ++level
        )
        {
            LevelResult r{};
            if (!MeasureLevel(level, r)) return Result::FAIL;

            levels[level] = r;

            const double ratio =
                Ratio(r.APCThroughput, r.VectorThroughput);

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
                << std::setw(11) << std::setprecision(2)
                << r.APCRejectsPer1000 << ' '
                << std::setw(13) << std::setprecision(3)
                << r.APCRetryEventsPerCycle << ' '
                << std::setw(9) << ratio << "x\n";
        }

        std::cout
            << "\nAPC WHOLE-TRANSACTION RETRY DIAGNOSTICS\n"
            << "level workers | p99 failed compound(us) | "
               "p99 retry events/cycle\n";

        Divider();

        for (const auto& r : levels)
        {
            std::cout
                << std::setw(5) << r.ContentionLevel << ' '
                << std::setw(7) << r.Workers << " | "
                << std::setw(23) << std::fixed << std::setprecision(3)
                << NsToUs(r.APCP99FailedAttemptNs) << " | "
                << std::setw(22) << std::setprecision(1)
                << r.APCP99RetryEventsPerCycle << '\n';
        }

        const LevelResult& low = levels[MIN_CONTENTION_LEVEL];
        const LevelResult& high = levels[MAX_CONTENTION_LEVEL];

        NavigationCounts vector_public_reads{};
        NavigationCounts apc_public_reads{};
        for (const auto& level : levels)
        {
            vector_public_reads.Add(level.VectorPublicReads);
            apc_public_reads.Add(level.APCPublicReads);
        }

        const double low_ratio =
            Ratio(low.APCThroughput, low.VectorThroughput);
        const double high_ratio =
            Ratio(high.APCThroughput, high.VectorThroughput);

        const bool final_ok =
            low.Ok &&
            high.Ok &&
            low.VectorThroughput > 0.0 &&
            high.VectorThroughput > 0.0 &&
            low.APCThroughput > 0.0 &&
            high.APCThroughput > 0.0 &&
            vector_public_reads.Retry == 0u &&
            apc_public_reads.Retry == 0u &&
            vector_public_reads.ContractFailures == 0u &&
            apc_public_reads.ContractFailures == 0u;

        std::cout
            << "\nCORE-BET SIGNAL\n"
            << "  uncontended APC/vector throughput ratio : "
            << low_ratio << "x\n"
            << "  max-contention APC/vector ratio         : "
            << high_ratio << "x\n"
            << "  ratio recovery level 0 -> 19           : "
            << Ratio(high_ratio, low_ratio) << "x\n"
            << "  APC max-contention p99 cycle            : "
            << NsToUs(high.APCP99CycleNs) << " us\n"
            << "  APC max-contention p99 retry-cycle      : "
            << NsToUs(high.APCP99RetriedCycleNs) << " us\n"
            << "  APC max-contention rejects/1000 moves   : "
            << high.APCRejectsPer1000 << '\n';

        std::cout << "\nQUIESCENT PUBLIC FIND OUTCOMES AFTER CONTENTION RUNS\n";
        PrintNavigationCounts("vector FindMyNext/FindPrevious", vector_public_reads);
        PrintNavigationCounts("APC FindMyNext/FindPrevious", apc_public_reads);

        std::cout
            << "\nTEST 2 OVERALL: "
            << (final_ok ? "PASS" : "FAIL")
            << '\n';

        return final_ok ? Result::PASS : Result::FAIL;
    }

} // namespace Test02_ContentionSweep

// ============================================================================
// TEST 3
// Public reader snapshot / compound writer race.
//
// The writer never performs public Detach() followed later by public Attach().
// It alternates one child between two H roots with ONE compound move.
// This removes the old intentionally-visible detached phase from the workload.
// ============================================================================
namespace Test03_ReaderWriterTraversal
{
    using namespace TestKit;

    constexpr size_t PARENT_A = 0u;
    constexpr size_t PARENT_B = 1u;
    constexpr size_t CHILD = 2u;
    constexpr size_t NODE_COUNT = 3u;

    constexpr uint32_t CALL_PAIRS_PER_READER = 50'000u;
    constexpr uint64_t MIN_PUBLIC_CALLS_PER_READER =
        static_cast<uint64_t>(CALL_PAIRS_PER_READER) * 2u;

    constexpr std::array<uint32_t, 5u> READER_COUNTS{
        1u, 2u, 4u, 8u, 16u
    };

    using VectorBackend = VectorGraphBackend<NODE_COUNT, 0u, false>;
    using APCBackend = TestBackedndFabric<NODE_COUNT, 1u>;

    static RootPlan<NODE_COUNT> MakeRootPlan() noexcept
    {
        RootPlan<NODE_COUNT> roots{};
        roots.Horizontal[PARENT_A] = true;
        roots.Horizontal[PARENT_B] = true;
        return roots;
    }

    template <typename Backend>
    bool BuildScenario(Backend& backend)
    {
        return
            backend.Initialize(MakeRootPlan()) &&
            backend.Attach(
                PARENT_A,
                CHILD,
                Axis::HORIZONTAL,
                Inheritance::FIRST_CHILD
            );
    }

    template <typename Backend>
    bool ValidateTwoParentTopology(Backend& backend)
    {
        auto parent_a = backend.HandleAt(PARENT_A);
        auto parent_b = backend.HandleAt(PARENT_B);
        auto child = backend.HandleAt(CHILD);

        if (!parent_a || !parent_b || !child) return false;

        const auto a_child = backend.FindNextRead(
            parent_a,
            Axis::HORIZONTAL,
            Inheritance::FIRST_CHILD
        );

        const auto b_child = backend.FindNextRead(
            parent_b,
            Axis::HORIZONTAL,
            Inheritance::FIRST_CHILD
        );

        if (
            !a_child.ContractValid() ||
            !b_child.ContractValid() ||
            a_child.IsRetry() ||
            b_child.IsRetry()
        )
        {
            return false;
        }

        const bool on_a =
            a_child.IsFound() && a_child.Ptr == child &&
            b_child.IsNone();

        const bool on_b =
            b_child.IsFound() && b_child.Ptr == child &&
            a_child.IsNone();

        if (!on_a && !on_b) return false;

        const auto child_next = backend.FindNextRead(
            child,
            Axis::HORIZONTAL,
            Inheritance::LINKED_CHILD
        );
        if (!child_next.IsNone())
        {
            return false;
        }

        const auto previous =
            backend.FindPreviousRead(child, Axis::HORIZONTAL);

        if (!previous.IsFound()) return false;
        if (on_a && previous.Ptr != parent_a) return false;
        if (on_b && previous.Ptr != parent_b) return false;

        return backend.LocksReleased();
    }

    struct ApiStats
    {
        uint64_t Calls = 0u;
        uint64_t Found = 0u;
        uint64_t Retry = 0u;
        uint64_t None = 0u;
        uint64_t StableValidated = 0u;
        uint64_t StableNull = 0u;
        uint64_t StableNonNull = 0u;
        uint64_t CompletedMutationWindows = 0u;
        uint64_t BoundaryUnstableWindows = 0u;

        uint64_t WrongPointerFailures = 0u;
        uint64_t VersionReadFailures = 0u;
        uint64_t OutcomeContractFailures = 0u;
        uint64_t UnexpectedNoneFailures = 0u;
    };

    struct ReaderStats
    {
        ApiStats FindNextParentA{};
        ApiStats FindPreviousChild{};
    };

    static void Add(ApiStats& dst, const ApiStats& src) noexcept
    {
        dst.Calls += src.Calls;
        dst.Found += src.Found;
        dst.Retry += src.Retry;
        dst.None += src.None;
        dst.StableValidated += src.StableValidated;
        dst.StableNull += src.StableNull;
        dst.StableNonNull += src.StableNonNull;
        dst.CompletedMutationWindows += src.CompletedMutationWindows;
        dst.BoundaryUnstableWindows += src.BoundaryUnstableWindows;

        dst.WrongPointerFailures += src.WrongPointerFailures;
        dst.VersionReadFailures += src.VersionReadFailures;
        dst.OutcomeContractFailures += src.OutcomeContractFailures;
        dst.UnexpectedNoneFailures += src.UnexpectedNoneFailures;
    }

    static void Add(ReaderStats& dst, const ReaderStats& src) noexcept
    {
        Add(dst.FindNextParentA, src.FindNextParentA);
        Add(dst.FindPreviousChild, src.FindPreviousChild);
    }

    static uint64_t HardFailures(const ApiStats& s) noexcept
    {
        return
            s.WrongPointerFailures +
            s.VersionReadFailures +
            s.OutcomeContractFailures +
            s.UnexpectedNoneFailures;
    }

    static uint64_t HardFailures(const ReaderStats& s) noexcept
    {
        return
            HardFailures(s.FindNextParentA) +
            HardFailures(s.FindPreviousChild);
    }

    static uint64_t TotalCalls(const ReaderStats& s) noexcept
    {
        return
            s.FindNextParentA.Calls +
            s.FindPreviousChild.Calls;
    }

    static uint64_t MutationWindows(const ReaderStats& s) noexcept
    {
        return
            s.FindNextParentA.CompletedMutationWindows +
            s.FindPreviousChild.CompletedMutationWindows;
    }

    template <typename Handle>
    static void ObservePublicRead(
        const NavigationRead<Handle>& read,
        Handle legal_handle_1,
        Handle legal_handle_2,
        bool none_is_legal,
        ApiStats& stats) noexcept
    {
        ++stats.Calls;

        switch (read.Outcome)
        {
        case ReadOperation::FOUND: ++stats.Found; break;
        case ReadOperation::RETRY: ++stats.Retry; break;
        case ReadOperation::NONE: ++stats.None; break;
        default: ++stats.OutcomeContractFailures; return;
        }

        if (!read.ContractValid())
        {
            ++stats.OutcomeContractFailures;
            return;
        }

        if (
            read.IsFound() &&
            read.Ptr != legal_handle_1 &&
            read.Ptr != legal_handle_2
        )
        {
            ++stats.WrongPointerFailures;
        }

        if (read.IsNone() && !none_is_legal)
        {
            ++stats.UnexpectedNoneFailures;
        }
    }

    template <typename PublicCall>
    static void ExerciseAPCRead(
        APCBackend& backend,
        size_t source_node,
        APCBackend::Handle legal_handle_1,
        APCBackend::Handle legal_handle_2,
        bool none_is_legal,
        PublicCall&& public_call,
        ApiStats& stats) noexcept
    {
        const AxisVersion before =
            ReadAxisVersion(
                backend,
                source_node,
                Axis::HORIZONTAL
            );

        const NavigationRead<APCBackend::Handle> public_read = public_call();
        ObservePublicRead(
            public_read,
            legal_handle_1,
            legal_handle_2,
            none_is_legal,
            stats
        );
        if (!public_read.ContractValid()) return;

        const AxisVersion after =
            ReadAxisVersion(
                backend,
                source_node,
                Axis::HORIZONTAL
            );

        if (!before.Valid || !after.Valid)
        {
            ++stats.VersionReadFailures;
            return;
        }

        if (
            !before.Locked &&
            !after.Locked &&
            before.Sequence != after.Sequence
        )
        {
            ++stats.CompletedMutationWindows;
        }

        // RETRY is the public API's explicit statement that this call did not
        // observe a stable relationship, so it is not counted as a stable
        // null/non-null observation.
        if (public_read.IsRetry()) return;

        if (!SameStableVersion(before, after))
        {
            ++stats.BoundaryUnstableWindows;
            return;
        }

        ++stats.StableValidated;

        if (public_read.IsFound())
        {
            ++stats.StableNonNull;
        }
        else if (public_read.IsNone())
        {
            ++stats.StableNull;
        }
    }

    struct RunStats
    {
        bool Ok = false;
        bool EnoughCalls = false;
        bool HadMutationCoverage = false;
        bool FinalTopologyOk = false;
        bool LocksReleased = false;

        uint32_t Readers = 0u;
        uint64_t WriterCycles = 0u;
        uint64_t WriterFailures = 0u;
        uint64_t TotalPublicCalls = 0u;
        int64_t ElapsedNs = 0;

        ReaderStats ReadersTotal{};

        double MillionCallsPerSecond() const noexcept
        {
            return ElapsedNs <= 0 ?
                0.0 :
                (1.0e3 * static_cast<double>(TotalPublicCalls)) /
                    static_cast<double>(ElapsedNs);
        }
    };

    RunStats RunVectorOnce(uint32_t reader_count)
    {
        RunStats out{};
        out.Readers = reader_count;

        VectorBackend backend{};
        if (!BuildScenario(backend)) return out;

        std::mutex graph_mutex{};
        std::atomic<bool> start{false};
        std::atomic<uint32_t> readers_done{0u};
        std::atomic<uint64_t> writer_cycles{0u};
        std::atomic<uint64_t> writer_failures{0u};
        std::atomic<uint64_t> reader_failures{0u};

        std::vector<ReaderStats> reader_stats(reader_count);
        std::vector<std::thread> readers{};
        readers.reserve(reader_count);

        const auto begin = Clock::now();

        for (uint32_t reader = 0u; reader < reader_count; ++reader)
        {
            readers.emplace_back([&, reader]
            {
                while (!start.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }

                ReaderStats local{};

                for (uint32_t i = 0u; i < CALL_PAIRS_PER_READER; ++i)
                {
                    {
                        std::lock_guard<std::mutex> lock(graph_mutex);

                        const auto observed = backend.FindNextRead(
                            backend.HandleAt(PARENT_A),
                            Axis::HORIZONTAL,
                            Inheritance::FIRST_CHILD
                        );

                        ObservePublicRead(
                            observed,
                            backend.HandleAt(CHILD),
                            static_cast<VectorBackend::Handle>(nullptr),
                            true,
                            local.FindNextParentA
                        );

                        if (
                            !observed.ContractValid() ||
                            (observed.IsFound() &&
                             observed.Ptr != backend.HandleAt(CHILD)) ||
                            observed.IsRetry()
                        )
                        {
                            reader_failures.fetch_add(
                                1u,
                                std::memory_order_relaxed
                            );
                        }
                    }

                    {
                        std::lock_guard<std::mutex> lock(graph_mutex);

                        const auto observed = backend.FindPreviousRead(
                            backend.HandleAt(CHILD),
                            Axis::HORIZONTAL
                        );

                        ObservePublicRead(
                            observed,
                            backend.HandleAt(PARENT_A),
                            backend.HandleAt(PARENT_B),
                            false,
                            local.FindPreviousChild
                        );

                        if (
                            !observed.IsFound() ||
                            (observed.Ptr != backend.HandleAt(PARENT_A) &&
                             observed.Ptr != backend.HandleAt(PARENT_B))
                        )
                        {
                            reader_failures.fetch_add(
                                1u,
                                std::memory_order_relaxed
                            );
                        }
                    }

                    PerturbSchedule(
                        static_cast<uint64_t>(i) + reader
                    );
                }

                reader_stats[reader] = local;

                readers_done.fetch_add(1u, std::memory_order_release);
            });
        }

        std::thread writer([&]
        {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            bool on_a = true;
            uint64_t cycle = 0u;

            while (
                readers_done.load(std::memory_order_acquire) != reader_count
            )
            {
                const size_t destination =
                    on_a ? PARENT_B : PARENT_A;

                bool ok = false;

                {
                    std::lock_guard<std::mutex> lock(graph_mutex);

                    ok = backend.DetachAndReAttachToParent(
                        CHILD,
                        destination,
                        Axis::HORIZONTAL
                    );
                }

                if (!ok)
                {
                    writer_failures.fetch_add(
                        1u,
                        std::memory_order_relaxed
                    );
                    break;
                }

                on_a = !on_a;

                writer_cycles.fetch_add(
                    1u,
                    std::memory_order_relaxed
                );

                PerturbSchedule(cycle++);
            }
        });

        start.store(true, std::memory_order_release);

        for (auto& reader : readers) reader.join();
        writer.join();

        const auto end = Clock::now();

        out.WriterCycles =
            writer_cycles.load(std::memory_order_acquire);

        out.WriterFailures =
            writer_failures.load(std::memory_order_acquire);

        out.ElapsedNs =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                end - begin
            ).count();

        for (const ReaderStats& stats : reader_stats)
        {
            Add(out.ReadersTotal, stats);
        }

        out.TotalPublicCalls = TotalCalls(out.ReadersTotal);

        {
            std::lock_guard<std::mutex> lock(graph_mutex);
            out.FinalTopologyOk = ValidateTwoParentTopology(backend);
        }

        out.EnoughCalls =
            out.TotalPublicCalls >=
            static_cast<uint64_t>(reader_count) *
                MIN_PUBLIC_CALLS_PER_READER;

        out.HadMutationCoverage = out.WriterCycles != 0u;
        out.LocksReleased = true;

        out.Ok =
            out.EnoughCalls &&
            out.HadMutationCoverage &&
            out.WriterFailures == 0u &&
            reader_failures.load(std::memory_order_acquire) == 0u &&
            HardFailures(out.ReadersTotal) == 0u &&
            out.FinalTopologyOk;

        return out;
    }

    RunStats RunAPCOnce(uint32_t reader_count)
    {
        RunStats out{};
        out.Readers = reader_count;

        APCBackend backend{};
        if (!BuildScenario(backend)) return out;

        std::atomic<bool> start{false};
        std::atomic<uint32_t> readers_done{0u};
        std::atomic<uint64_t> writer_cycles{0u};
        std::atomic<uint64_t> writer_failures{0u};

        std::vector<ReaderStats> reader_stats(reader_count);
        std::vector<std::thread> readers{};
        readers.reserve(reader_count);

        const auto begin = Clock::now();

        for (uint32_t reader = 0u; reader < reader_count; ++reader)
        {
            readers.emplace_back([&, reader]
            {
                while (!start.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }

                ReaderStats local{};

                for (
                    uint32_t call_pair = 0u;
                    call_pair < CALL_PAIRS_PER_READER;
                    ++call_pair
                )
                {
                    ExerciseAPCRead(
                        backend,
                        PARENT_A,
                        backend.HandleAt(CHILD),
                        nullptr,
                        true,
                        [&]() noexcept
                        {
                            return backend.FindNextRead(
                                backend.HandleAt(PARENT_A),
                                Axis::HORIZONTAL,
                                Inheritance::FIRST_CHILD
                            );
                        },
                        local.FindNextParentA
                    );

                    ExerciseAPCRead(
                        backend,
                        CHILD,
                        backend.HandleAt(PARENT_A),
                        backend.HandleAt(PARENT_B),
                        false,
                        [&]() noexcept
                        {
                            return backend.FindPreviousRead(
                                backend.HandleAt(CHILD),
                                Axis::HORIZONTAL
                            );
                        },
                        local.FindPreviousChild
                    );

                    PerturbSchedule(
                        static_cast<uint64_t>(call_pair) + reader
                    );
                }

                reader_stats[reader] = local;

                readers_done.fetch_add(
                    1u,
                    std::memory_order_release
                );
            });
        }

        std::thread writer([&]
        {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            bool on_a = true;
            uint64_t cycle = 0u;

            while (
                readers_done.load(std::memory_order_acquire) != reader_count
            )
            {
                const size_t destination =
                    on_a ? PARENT_B : PARENT_A;

                if (!backend.DetachAndReAttachToParent(
                        CHILD,
                        destination,
                        Axis::HORIZONTAL))
                {
                    writer_failures.fetch_add(
                        1u,
                        std::memory_order_relaxed
                    );
                    break;
                }

                on_a = !on_a;

                writer_cycles.fetch_add(
                    1u,
                    std::memory_order_relaxed
                );

                PerturbSchedule(cycle++);
            }
        });

        start.store(true, std::memory_order_release);

        for (auto& reader : readers) reader.join();
        writer.join();

        const auto end = Clock::now();

        for (const ReaderStats& stats : reader_stats)
        {
            Add(out.ReadersTotal, stats);
        }

        out.TotalPublicCalls = TotalCalls(out.ReadersTotal);

        out.WriterCycles =
            writer_cycles.load(std::memory_order_acquire);

        out.WriterFailures =
            writer_failures.load(std::memory_order_acquire);

        out.ElapsedNs =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                end - begin
            ).count();

        out.EnoughCalls =
            out.TotalPublicCalls >=
            static_cast<uint64_t>(reader_count) *
                MIN_PUBLIC_CALLS_PER_READER;

        // Scheduling coverage is telemetry, not a correctness oracle.  A run
        // is covered when the compound writer actually completed mutations;
        // public RETRY counts are reported independently below.
        out.HadMutationCoverage = out.WriterCycles != 0u;

        out.FinalTopologyOk =
            ValidateTwoParentTopology(backend);

        out.LocksReleased =
            backend.LocksReleased();

        out.Ok =
            out.EnoughCalls &&
            out.HadMutationCoverage &&
            out.WriterFailures == 0u &&
            HardFailures(out.ReadersTotal) == 0u &&
            out.FinalTopologyOk &&
            out.LocksReleased;

        return out;
    }

    static void PrintApiStats(
        const char* name,
        const ApiStats& s)
    {
        std::cout
            << "    " << name << '\n'
            << "      calls                       : "
            << s.Calls << '\n'
            << "      FOUND                       : "
            << s.Found << '\n'
            << "      RETRY                       : "
            << s.Retry << '\n'
            << "      NONE                        : "
            << s.None << '\n'
            << "      stable validated            : "
            << s.StableValidated << '\n'
            << "      stable null                 : "
            << s.StableNull << '\n'
            << "      stable non-null             : "
            << s.StableNonNull << '\n'
            << "      completed mutation windows  : "
            << s.CompletedMutationWindows << '\n'
            << "      boundary unstable windows   : "
            << s.BoundaryUnstableWindows << '\n'
            << "      wrong pointer failures      : "
            << s.WrongPointerFailures << '\n'
            << "      version read failures       : "
            << s.VersionReadFailures << '\n'
            << "      outcome contract failures   : "
            << s.OutcomeContractFailures << '\n'
            << "      unexpected NONE failures    : "
            << s.UnexpectedNoneFailures << '\n';
    }

    inline Result Run()
    {
        Banner("TEST 3 - PUBLIC READERS vs COMPOUND CROSS-PARENT WRITER");

        std::cout
            << "Writer: CHILD alternates PARENT_A <-> PARENT_B with ONE compound call.\n"
            << "Vector reference: each reader call and whole writer move use one "
               "global std::mutex.\n"
            << "APC readers: FindMyNext() / FindPrevious() only, no external lock.\n"
            << "Reader outcomes are counted directly as FOUND / RETRY / NONE.\n"
            << "FindMyNext(PARENT_A): FOUND(child), RETRY, or NONE are legal.\n"
            << "FindPrevious(CHILD): FOUND(parent A/B) or RETRY are legal; NONE is a failure.\n"
            << "There is no intentional stable detached phase in the writer workload.\n"
            << "Each reader performs " << CALL_PAIRS_PER_READER
            << " call pairs = " << MIN_PUBLIC_CALLS_PER_READER
            << " public calls.\n\n";

        bool all_ok = true;

        for (uint32_t reader_count : READER_COUNTS)
        {
            const RunStats vector_run =
                RunVectorOnce(reader_count);

            const RunStats apc_run =
                RunAPCOnce(reader_count);

            const uint64_t apc_hard_failures =
                HardFailures(apc_run.ReadersTotal);

            const bool pair_ok =
                vector_run.Ok &&
                apc_run.Ok;

            all_ok = all_ok && pair_ok;

            std::cout
                << "READERS = " << reader_count << '\n'
                << "  vector writer cycles : "
                << vector_run.WriterCycles << '\n'
                << "  APC writer cycles    : "
                << apc_run.WriterCycles << '\n'
                << "  vector Mcalls/s      : "
                << std::fixed << std::setprecision(3)
                << vector_run.MillionCallsPerSecond() << '\n'
                << "  APC Mcalls/s         : "
                << apc_run.MillionCallsPerSecond() << '\n'
                << "  APC/vector read ratio: "
                << Ratio(
                    apc_run.MillionCallsPerSecond(),
                    vector_run.MillionCallsPerSecond()
                ) << "x\n"
                << "  APC mutation crossings: "
                << MutationWindows(apc_run.ReadersTotal) << '\n'
                << "  APC hard failures      : "
                << apc_hard_failures << '\n';

            PrintApiStats(
                "vector FindMyNext(PARENT_A, FIRST_CHILD)",
                vector_run.ReadersTotal.FindNextParentA
            );

            PrintApiStats(
                "vector FindPrevious(CHILD)",
                vector_run.ReadersTotal.FindPreviousChild
            );

            PrintApiStats(
                "FindMyNext(PARENT_A, FIRST_CHILD)",
                apc_run.ReadersTotal.FindNextParentA
            );

            PrintApiStats(
                "FindPrevious(CHILD)",
                apc_run.ReadersTotal.FindPreviousChild
            );

            std::cout
                << "    enough calls       : "
                << (apc_run.EnoughCalls ? "PASS" : "FAIL")
                << '\n'
                << "    mutation coverage  : "
                << (apc_run.HadMutationCoverage ? "PASS" : "FAIL")
                << '\n'
                << "    final topology     : "
                << (apc_run.FinalTopologyOk ? "PASS" : "FAIL")
                << '\n'
                << "    locks released     : "
                << (apc_run.LocksReleased ? "PASS" : "FAIL")
                << '\n'
                << "    VECTOR RESULT      : "
                << (vector_run.Ok ? "PASS" : "FAIL")
                << '\n'
                << "    APC RESULT         : "
                << (apc_run.Ok ? "PASS" : "FAIL")
                << "\n\n";
        }

        std::cout
            << "TEST 3 OVERALL: "
            << (all_ok ? "PASS" : "FAIL")
            << '\n';

        return all_ok ? Result::PASS : Result::FAIL;
    }

} // namespace Test03_ReaderWriterTraversal

// ============================================================================
// TEST 4
// Primitive public mutation pairs vs compound one-call mutation APIs.
//
// This test is intentionally APC-only. It answers a different question from
// Test 2: "what do the new compound APIs buy over composing the old surface
// APIs yourself?"
// ============================================================================
namespace Test04_PrimitiveVsCompoundMutation
{
    using namespace TestKit;

    constexpr size_t PARENT_A = 0u;
    constexpr size_t PARENT_B = 1u;
    constexpr size_t ANCHOR_A = 2u;
    constexpr size_t ANCHOR_B = 3u;
    constexpr size_t CHILD = 4u;
    constexpr size_t NODE_COUNT = 5u;

    constexpr uint32_t LOGICAL_MOVES = 20'000u;
    constexpr uint32_t COMPONENT_MOVES = 2'000u;
    constexpr uint32_t MEASURED_RUNS = 5u;

    static_assert((LOGICAL_MOVES & 1u) == 0u);
    static_assert((COMPONENT_MOVES & 1u) == 0u);

    using APCBackend = TestBackedndFabric<NODE_COUNT, 1u>;

    static RootPlan<NODE_COUNT> MakeRootPlan() noexcept
    {
        RootPlan<NODE_COUNT> roots{};
        roots.Horizontal[PARENT_A] = true;
        roots.Horizontal[PARENT_B] = true;
        return roots;
    }

    bool BuildParentFixture(APCBackend& backend)
    {
        return
            backend.Initialize(MakeRootPlan()) &&
            backend.Attach(
                PARENT_A,
                CHILD,
                Axis::HORIZONTAL,
                Inheritance::FIRST_CHILD
            );
    }

    bool ValidateParentFixture(
        APCBackend& backend,
        NavigationCounts* reads = nullptr)
    {
        auto parent_a = backend.HandleAt(PARENT_A);
        auto parent_b = backend.HandleAt(PARENT_B);
        auto child = backend.HandleAt(CHILD);

        const auto a_child = backend.FindNextRead(
            parent_a,
            Axis::HORIZONTAL,
            Inheritance::FIRST_CHILD
        );
        if (reads) reads->Observe(a_child);

        const auto b_child = backend.FindNextRead(
            parent_b,
            Axis::HORIZONTAL,
            Inheritance::FIRST_CHILD
        );
        if (reads) reads->Observe(b_child);

        const auto previous = backend.FindPreviousRead(
            child,
            Axis::HORIZONTAL
        );
        if (reads) reads->Observe(previous);

        const bool on_a =
            a_child.IsFound() && a_child.Ptr == child &&
            b_child.IsNone() &&
            previous.IsFound() && previous.Ptr == parent_a;

        const bool on_b =
            b_child.IsFound() && b_child.Ptr == child &&
            a_child.IsNone() &&
            previous.IsFound() && previous.Ptr == parent_b;

        const auto child_end = backend.FindNextRead(
            child,
            Axis::HORIZONTAL,
            Inheritance::LINKED_CHILD
        );
        if (reads) reads->Observe(child_end);

        return
            (on_a || on_b) &&
            child_end.IsNone() &&
            backend.LocksReleased();
    }

    bool BuildSiblingFixture(APCBackend& backend)
    {
        return
            backend.Initialize(MakeRootPlan()) &&
            backend.Attach(
                PARENT_A,
                ANCHOR_A,
                Axis::HORIZONTAL,
                Inheritance::FIRST_CHILD
            ) &&
            backend.Attach(
                ANCHOR_A,
                CHILD,
                Axis::HORIZONTAL,
                Inheritance::LINKED_CHILD
            ) &&
            backend.Attach(
                PARENT_B,
                ANCHOR_B,
                Axis::HORIZONTAL,
                Inheritance::FIRST_CHILD
            );
    }

    bool ValidateSiblingFixture(
        APCBackend& backend,
        NavigationCounts* reads = nullptr)
    {
        auto parent_a = backend.HandleAt(PARENT_A);
        auto parent_b = backend.HandleAt(PARENT_B);
        auto anchor_a = backend.HandleAt(ANCHOR_A);
        auto anchor_b = backend.HandleAt(ANCHOR_B);
        auto child = backend.HandleAt(CHILD);

        const auto first_a = backend.FindNextRead(
            parent_a,
            Axis::HORIZONTAL,
            Inheritance::FIRST_CHILD
        );
        const auto first_b = backend.FindNextRead(
            parent_b,
            Axis::HORIZONTAL,
            Inheritance::FIRST_CHILD
        );
        if (reads)
        {
            reads->Observe(first_a);
            reads->Observe(first_b);
        }

        if (
            !first_a.IsFound() || first_a.Ptr != anchor_a ||
            !first_b.IsFound() || first_b.Ptr != anchor_b
        )
        {
            return false;
        }

        const auto next_a = backend.FindNextRead(
            anchor_a,
            Axis::HORIZONTAL,
            Inheritance::LINKED_CHILD
        );

        const auto next_b = backend.FindNextRead(
            anchor_b,
            Axis::HORIZONTAL,
            Inheritance::LINKED_CHILD
        );
        const auto previous = backend.FindPreviousRead(
            child,
            Axis::HORIZONTAL
        );
        if (reads)
        {
            reads->Observe(next_a);
            reads->Observe(next_b);
            reads->Observe(previous);
        }

        const bool on_a =
            next_a.IsFound() && next_a.Ptr == child &&
            next_b.IsNone() &&
            previous.IsFound() && previous.Ptr == anchor_a;

        const bool on_b =
            next_b.IsFound() && next_b.Ptr == child &&
            next_a.IsNone() &&
            previous.IsFound() && previous.Ptr == anchor_b;

        const auto child_end = backend.FindNextRead(
            child,
            Axis::HORIZONTAL,
            Inheritance::LINKED_CHILD
        );
        if (reads) reads->Observe(child_end);

        return
            (on_a || on_b) &&
            child_end.IsNone() &&
            backend.LocksReleased();
    }

    struct WholeTiming
    {
        bool Ok = false;
        double NsPerLogicalMove = 0.0;
    };

    WholeTiming MeasureParentPrimitivePair()
    {
        APCBackend backend{};
        if (!BuildParentFixture(backend)) return {};

        auto* child = backend.HandleAt(CHILD);
        bool on_a = true;

        const auto begin = Clock::now();

        for (uint32_t i = 0u; i < LOGICAL_MOVES; ++i)
        {
            auto* source =
                backend.HandleAt(on_a ? PARENT_A : PARENT_B);

            auto* destination =
                backend.HandleAt(on_a ? PARENT_B : PARENT_A);

            if (
                !source->DetachMyChild(
                    *child,
                    Axis::HORIZONTAL
                ) ||
                !child->AttachMeToAnother(
                    *destination,
                    Axis::HORIZONTAL,
                    Inheritance::FIRST_CHILD
                )
            )
            {
                return {};
            }

            on_a = !on_a;
        }

        const auto end = Clock::now();

        if (!ValidateParentFixture(backend)) return {};

        return {
            true,
            static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end - begin
                ).count()
            ) / static_cast<double>(LOGICAL_MOVES)
        };
    }

    WholeTiming MeasureParentCompound()
    {
        APCBackend backend{};
        if (!BuildParentFixture(backend)) return {};

        auto* child = backend.HandleAt(CHILD);
        bool on_a = true;

        const auto begin = Clock::now();

        for (uint32_t i = 0u; i < LOGICAL_MOVES; ++i)
        {
            auto* destination =
                backend.HandleAt(on_a ? PARENT_B : PARENT_A);

            if (!child->DetachAndReAttachMeToThisParent(
                    *destination,
                    Axis::HORIZONTAL))
            {
                return {};
            }

            on_a = !on_a;
        }

        const auto end = Clock::now();

        if (!ValidateParentFixture(backend)) return {};

        return {
            true,
            static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end - begin
                ).count()
            ) / static_cast<double>(LOGICAL_MOVES)
        };
    }

    WholeTiming MeasureSiblingPrimitivePair()
    {
        APCBackend backend{};
        if (!BuildSiblingFixture(backend)) return {};

        auto* child = backend.HandleAt(CHILD);
        bool on_a = true;

        const auto begin = Clock::now();

        for (uint32_t i = 0u; i < LOGICAL_MOVES; ++i)
        {
            auto* destination_anchor =
                backend.HandleAt(on_a ? ANCHOR_B : ANCHOR_A);

            if (
                !child->DetachMeFromAnotherEdge(
                    Axis::HORIZONTAL
                ) ||
                !destination_anchor->AttachSiblingOrChild(
                    *child,
                    Axis::HORIZONTAL,
                    Inheritance::LINKED_CHILD
                )
            )
            {
                return {};
            }

            on_a = !on_a;
        }

        const auto end = Clock::now();

        if (!ValidateSiblingFixture(backend)) return {};

        return {
            true,
            static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end - begin
                ).count()
            ) / static_cast<double>(LOGICAL_MOVES)
        };
    }

    WholeTiming MeasureSiblingCompound()
    {
        APCBackend backend{};
        if (!BuildSiblingFixture(backend)) return {};

        auto* child = backend.HandleAt(CHILD);
        bool on_a = true;

        const auto begin = Clock::now();

        for (uint32_t i = 0u; i < LOGICAL_MOVES; ++i)
        {
            auto* destination_anchor =
                backend.HandleAt(on_a ? ANCHOR_B : ANCHOR_A);

            if (!child->DetachAndReattachMeAsEquivelentSibbling(
                    *destination_anchor,
                    Axis::HORIZONTAL))
            {
                return {};
            }

            on_a = !on_a;
        }

        const auto end = Clock::now();

        if (!ValidateSiblingFixture(backend)) return {};

        return {
            true,
            static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end - begin
                ).count()
            ) / static_cast<double>(LOGICAL_MOVES)
        };
    }

    struct ComponentTiming
    {
        bool Ok = false;
        double DetachNs = 0.0;
        double AttachNs = 0.0;
    };

    ComponentTiming MeasureParentPrimitiveComponents()
    {
        APCBackend backend{};
        if (!BuildParentFixture(backend)) return {};

        auto* child = backend.HandleAt(CHILD);
        bool on_a = true;

        uint64_t detach_ns = 0u;
        uint64_t attach_ns = 0u;

        for (uint32_t i = 0u; i < COMPONENT_MOVES; ++i)
        {
            auto* source =
                backend.HandleAt(on_a ? PARENT_A : PARENT_B);

            auto* destination =
                backend.HandleAt(on_a ? PARENT_B : PARENT_A);

            const auto d0 = Clock::now();
            const bool detached =
                source->DetachMyChild(
                    *child,
                    Axis::HORIZONTAL
                );
            const auto d1 = Clock::now();

            if (!detached) return {};

            const auto a0 = Clock::now();
            const bool attached =
                child->AttachMeToAnother(
                    *destination,
                    Axis::HORIZONTAL,
                    Inheritance::FIRST_CHILD
                );
            const auto a1 = Clock::now();

            if (!attached) return {};

            detach_ns += static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    d1 - d0
                ).count()
            );

            attach_ns += static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    a1 - a0
                ).count()
            );

            on_a = !on_a;
        }

        if (!ValidateParentFixture(backend)) return {};

        return {
            true,
            static_cast<double>(detach_ns) /
                static_cast<double>(COMPONENT_MOVES),
            static_cast<double>(attach_ns) /
                static_cast<double>(COMPONENT_MOVES)
        };
    }

    ComponentTiming MeasureSiblingPrimitiveComponents()
    {
        APCBackend backend{};
        if (!BuildSiblingFixture(backend)) return {};

        auto* child = backend.HandleAt(CHILD);
        bool on_a = true;

        uint64_t detach_ns = 0u;
        uint64_t attach_ns = 0u;

        for (uint32_t i = 0u; i < COMPONENT_MOVES; ++i)
        {
            auto* destination_anchor =
                backend.HandleAt(on_a ? ANCHOR_B : ANCHOR_A);

            const auto d0 = Clock::now();
            const bool detached =
                child->DetachMeFromAnotherEdge(
                    Axis::HORIZONTAL
                );
            const auto d1 = Clock::now();

            if (!detached) return {};

            const auto a0 = Clock::now();
            const bool attached =
                destination_anchor->AttachSiblingOrChild(
                    *child,
                    Axis::HORIZONTAL,
                    Inheritance::LINKED_CHILD
                );
            const auto a1 = Clock::now();

            if (!attached) return {};

            detach_ns += static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    d1 - d0
                ).count()
            );

            attach_ns += static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    a1 - a0
                ).count()
            );

            on_a = !on_a;
        }

        if (!ValidateSiblingFixture(backend)) return {};

        return {
            true,
            static_cast<double>(detach_ns) /
                static_cast<double>(COMPONENT_MOVES),
            static_cast<double>(attach_ns) /
                static_cast<double>(COMPONENT_MOVES)
        };
    }

    inline Result Run()
    {
        Banner("TEST 4 - PRIMITIVE API PAIRS vs COMPOUND MUTATION APIs");

        std::cout
            << "SITUATION A - move one FIRST_CHILD between two root edges\n"
            << "  primitive: source.DetachMyChild(child) + "
               "child.AttachMeToAnother(destination, FIRST_CHILD)\n"
            << "  compound : child.DetachAndReAttachMeToThisParent(destination)\n"
            << "  semantic difference: primitive pair exposes a detached state "
               "between calls; compound does not.\n\n"
            << "SITUATION B - move one child between two sibling edges\n"
            << "  primitive: child.DetachMeFromAnotherEdge() + "
               "destinationTail.AttachSiblingOrChild(child, LINKED_CHILD)\n"
            << "  compound : child.DetachAndReattachMeAsEquivelentSibbling("
               "destinationSibling)\n"
            << "  semantic difference: primitive pair exposes a detached state "
               "between calls; compound does not.\n\n";

        std::array<double, MEASURED_RUNS> parent_primitive{};
        std::array<double, MEASURED_RUNS> parent_compound{};
        std::array<double, MEASURED_RUNS> sibling_primitive{};
        std::array<double, MEASURED_RUNS> sibling_compound{};

        for (uint32_t run = 0u; run < MEASURED_RUNS; ++run)
        {
            WholeTiming pp{};
            WholeTiming pc{};
            WholeTiming sp{};
            WholeTiming sc{};

            if ((run & 1u) == 0u)
            {
                pp = MeasureParentPrimitivePair();
                pc = MeasureParentCompound();
                sp = MeasureSiblingPrimitivePair();
                sc = MeasureSiblingCompound();
            }
            else
            {
                sc = MeasureSiblingCompound();
                sp = MeasureSiblingPrimitivePair();
                pc = MeasureParentCompound();
                pp = MeasureParentPrimitivePair();
            }

            if (!pp.Ok || !pc.Ok || !sp.Ok || !sc.Ok)
            {
                std::cout
                    << "  timing run " << (run + 1u)
                    << " : FAIL\n";
                return Result::FAIL;
            }

            parent_primitive[run] = pp.NsPerLogicalMove;
            parent_compound[run] = pc.NsPerLogicalMove;
            sibling_primitive[run] = sp.NsPerLogicalMove;
            sibling_compound[run] = sc.NsPerLogicalMove;

            std::cout
                << "  timing run " << (run + 1u)
                << '/' << MEASURED_RUNS
                << " : PASS\n";
        }

        const double parent_primitive_med =
            Median(parent_primitive);
        const double parent_compound_med =
            Median(parent_compound);
        const double sibling_primitive_med =
            Median(sibling_primitive);
        const double sibling_compound_med =
            Median(sibling_compound);

        const ComponentTiming parent_components =
            MeasureParentPrimitiveComponents();

        const ComponentTiming sibling_components =
            MeasureSiblingPrimitiveComponents();

        if (!parent_components.Ok || !sibling_components.Ok)
        {
            std::cout << "Component timing pass: FAIL\n";
            return Result::FAIL;
        }

        NavigationCounts public_reads{};
        APCBackend parent_probe{};
        APCBackend sibling_probe{};

        const bool public_contract_ok =
            BuildParentFixture(parent_probe) &&
            ValidateParentFixture(parent_probe, &public_reads) &&
            BuildSiblingFixture(sibling_probe) &&
            ValidateSiblingFixture(sibling_probe, &public_reads) &&
            public_reads.Retry == 0u &&
            public_reads.ContractFailures == 0u;

        std::cout
            << "\nMEDIAN WHOLE LOGICAL-MOVE COST\n"
            << std::left << std::setw(52)
            << "parent: DetachMyChild + AttachMeToAnother"
            << std::right << std::setw(10)
            << std::fixed << std::setprecision(2)
            << parent_primitive_med << " ns/move\n"
            << std::left << std::setw(52)
            << "parent: DetachAndReAttachMeToThisParent"
            << std::right << std::setw(10)
            << parent_compound_med << " ns/move"
            << "  compound/primitive="
            << Ratio(parent_compound_med, parent_primitive_med)
            << "x\n\n"
            << std::left << std::setw(52)
            << "sibling: DetachMeFromAnotherEdge + AttachSiblingOrChild"
            << std::right << std::setw(10)
            << sibling_primitive_med << " ns/move\n"
            << std::left << std::setw(52)
            << "sibling: DetachAndReattachMeAsEquivelentSibbling"
            << std::right << std::setw(10)
            << sibling_compound_med << " ns/move"
            << "  compound/primitive="
            << Ratio(sibling_compound_med, sibling_primitive_med)
            << "x\n";

        std::cout
            << "\nINSTRUMENTED PRIMITIVE COMPONENT COST"
               " (includes per-call clock overhead)\n"
            << "  DetachMyChild             : "
            << parent_components.DetachNs << " ns/call\n"
            << "  AttachMeToAnother         : "
            << parent_components.AttachNs << " ns/call\n"
            << "  DetachMeFromAnotherEdge   : "
            << sibling_components.DetachNs << " ns/call\n"
            << "  AttachSiblingOrChild      : "
            << sibling_components.AttachNs << " ns/call\n";

        std::cout << "\nQUIESCENT PUBLIC FIND OUTCOMES AFTER API FIXTURES\n";
        PrintNavigationCounts("APC FindMyNext/FindPrevious", public_reads);

        const bool final_ok =
            parent_primitive_med > 0.0 &&
            parent_compound_med > 0.0 &&
            sibling_primitive_med > 0.0 &&
            sibling_compound_med > 0.0 &&
            public_contract_ok;

        std::cout
            << "\nINTERPRETATION\n"
            << "  primitive parent path API calls/logical move : 2\n"
            << "  compound parent path API calls/logical move  : 1\n"
            << "  primitive sibling path API calls/logical move: 2\n"
            << "  compound sibling path API calls/logical move : 1\n"
            << "  timing alone is NOT the whole result: the compound path also "
               "owns the detach+relink state transition as one mutation.\n";

        std::cout
            << "\nTEST 4 OVERALL: "
            << (final_ok ? "PASS" : "FAIL")
            << '\n';

        return final_ok ? Result::PASS : Result::FAIL;
    }

} // namespace Test04_PrimitiveVsCompoundMutation

// ============================================================================
// TEST 5
// Each axis is independently acyclic.  The H-union-V projection may be
// cyclic because H and V encode different relations.
// ============================================================================
namespace Test05_PerAxisAcyclicity
{
    using namespace TestKit;

    constexpr size_t A = 0u;
    constexpr size_t B = 1u;
    constexpr size_t C = 2u;
    constexpr size_t NODE_COUNT = 3u;

    using APCBackend = TestBackedndFabric<NODE_COUNT, 1u>;
    using Adjacency = std::array<std::array<bool, NODE_COUNT>, NODE_COUNT>;

    static RootPlan<NODE_COUNT> MakeRootPlan() noexcept
    {
        RootPlan<NODE_COUNT> roots{};

        // Empty C(H) and A(V) roots are intentional: they are the
        // destinations used by the two cycle-closing rejection probes.
        roots.Horizontal[A] = true;
        roots.Horizontal[B] = true;
        roots.Horizontal[C] = true;
        roots.Vertical[A] = true;
        roots.Vertical[C] = true;
        return roots;
    }

    static bool HasDirectedCycle(const Adjacency& adjacency) noexcept
    {
        std::array<uint8_t, NODE_COUNT> colour{};

        auto Visit___ = [&](auto&& self, size_t node) noexcept -> bool
        {
            colour[node] = 1u;

            for (size_t next = 0u; next < NODE_COUNT; ++next)
            {
                if (!adjacency[node][next]) continue;
                if (colour[next] == 1u) return true;
                if (colour[next] == 0u && self(self, next)) return true;
            }

            colour[node] = 2u;
            return false;
        };

        for (size_t node = 0u; node < NODE_COUNT; ++node)
        {
            if (colour[node] == 0u && Visit___(Visit___, node)) return true;
        }

        return false;
    }

    static size_t EdgeCount(const Adjacency& adjacency) noexcept
    {
        size_t count = 0u;
        for (const auto& row : adjacency)
        {
            for (bool edge : row)
            {
                if (edge) ++count;
            }
        }
        return count;
    }

    static bool AuditAxis(
        APCBackend& backend,
        Axis axis,
        Adjacency& adjacency,
        NavigationCounts& reads) noexcept
    {
        adjacency = {};

        constexpr std::array<Inheritance, 2u> relationships{
            Inheritance::FIRST_CHILD,
            Inheritance::LINKED_CHILD
        };

        for (size_t from = 0u; from < NODE_COUNT; ++from)
        {
            for (Inheritance relationship : relationships)
            {
                const auto read = backend.FindNextRead(
                    backend.HandleAt(from),
                    axis,
                    relationship
                );
                reads.Observe(read);

                if (!read.ContractValid() || read.IsRetry()) return false;
                if (read.IsNone()) continue;

                const size_t to = backend.IndexOf(read.Ptr);
                if (to >= NODE_COUNT || to == from) return false;
                adjacency[from][to] = true;

                const auto previous = backend.FindPreviousRead(
                    read.Ptr,
                    axis
                );
                reads.Observe(previous);

                if (
                    !previous.IsFound() ||
                    previous.Ptr != backend.HandleAt(from)
                )
                {
                    return false;
                }
            }
        }

        return !HasDirectedCycle(adjacency);
    }

    inline Result Run()
    {
        Banner("TEST 5 - PER-AXIS ACYCLICITY + LEGAL CROSS-AXIS UNION CYCLE");

        std::cout
            << "H relation: A -> B -> C.\n"
            << "V relation: C -> A.\n"
            << "Each axis must remain acyclic; H union V intentionally contains "
               "A -> B -> C -> A.\n"
            << "The union cycle is legal because H and V represent different relations.\n\n";

        APCBackend backend{};
        if (
            !backend.Initialize(MakeRootPlan()) ||
            !backend.Attach(A, B, Axis::HORIZONTAL, Inheritance::FIRST_CHILD) ||
            !backend.Attach(B, C, Axis::HORIZONTAL, Inheritance::FIRST_CHILD) ||
            !backend.Attach(C, A, Axis::VERTICAL, Inheritance::FIRST_CHILD)
        )
        {
            std::cout << "  independent-axis construction : FAIL\n";
            return Result::FAIL;
        }

        NavigationCounts reads{};
        Adjacency horizontal{};
        Adjacency vertical{};

        const bool horizontal_acyclic =
            AuditAxis(backend, Axis::HORIZONTAL, horizontal, reads);

        const bool vertical_acyclic =
            AuditAxis(backend, Axis::VERTICAL, vertical, reads);

        const bool expected_horizontal =
            horizontal[A][B] &&
            horizontal[B][C] &&
            EdgeCount(horizontal) == 2u;

        const bool expected_vertical =
            vertical[C][A] &&
            EdgeCount(vertical) == 1u;

        Adjacency combined = horizontal;
        for (size_t from = 0u; from < NODE_COUNT; ++from)
        {
            for (size_t to = 0u; to < NODE_COUNT; ++to)
            {
                combined[from][to] =
                    combined[from][to] || vertical[from][to];
            }
        }

        const bool combined_cycle_observed = HasDirectedCycle(combined);

        // These operations would close a cycle inside one axis and therefore
        // must be rejected even though a cross-axis union cycle is permitted.
        const bool horizontal_cycle_rejected =
            !backend.Attach(C, A, Axis::HORIZONTAL, Inheritance::FIRST_CHILD);

        const bool vertical_cycle_rejected =
            !backend.Attach(A, C, Axis::VERTICAL, Inheritance::FIRST_CHILD);

        Adjacency horizontal_after{};
        Adjacency vertical_after{};

        const bool unchanged_after_rejection =
            AuditAxis(backend, Axis::HORIZONTAL, horizontal_after, reads) &&
            AuditAxis(backend, Axis::VERTICAL, vertical_after, reads) &&
            horizontal_after == horizontal &&
            vertical_after == vertical &&
            backend.LocksReleased();

        const bool read_contract_ok =
            reads.Retry == 0u &&
            reads.ContractFailures == 0u;

        const bool final_ok =
            horizontal_acyclic &&
            vertical_acyclic &&
            expected_horizontal &&
            expected_vertical &&
            combined_cycle_observed &&
            horizontal_cycle_rejected &&
            vertical_cycle_rejected &&
            unchanged_after_rejection &&
            read_contract_ok;

        std::cout
            << "  H audit acyclic                    : "
            << (horizontal_acyclic ? "PASS" : "FAIL") << '\n'
            << "  V audit acyclic                    : "
            << (vertical_acyclic ? "PASS" : "FAIL") << '\n'
            << "  H union V directed cycle observed  : "
            << (combined_cycle_observed ? "PASS" : "FAIL") << '\n'
            << "  reject same-axis H cycle C -> A    : "
            << (horizontal_cycle_rejected ? "PASS" : "FAIL") << '\n'
            << "  reject same-axis V cycle A -> C    : "
            << (vertical_cycle_rejected ? "PASS" : "FAIL") << '\n'
            << "  topology unchanged / locks released: "
            << (unchanged_after_rejection ? "PASS" : "FAIL") << '\n';

        std::cout << "\nPUBLIC FIND OUTCOMES DURING BOTH AXIS AUDITS\n";
        PrintNavigationCounts("APC FindMyNext/FindPrevious", reads);

        std::cout
            << "\nINTERPRETATION\n"
            << "  acyclicity is an invariant of each relation axis, not of their union.\n"
            << "  a cycle that alternates H and V edges does not make either forest cyclic.\n"
            << "\nTEST 5 OVERALL: "
            << (final_ok ? "PASS" : "FAIL")
            << '\n';

        return final_ok ? Result::PASS : Result::FAIL;
    }

} // namespace Test05_PerAxisAcyclicity

namespace Test06_PrimitiveRegionViews
{
    using namespace TestKit;
    using SD = SchemaDefinition;

    static constexpr uint32_t FABRIC_SLOT_COUNT = 4u;
    static constexpr uint32_t SLOT_WORDS = MINIMUM_APC_CELL_COUNT;

    template <typename T>
    constexpr const char* PrimitiveName() noexcept
    {
        if constexpr (std::is_same_v<T, uint8_t>) return "uint8_t";
        else if constexpr (std::is_same_v<T, uint16_t>) return "uint16_t";
        else if constexpr (std::is_same_v<T, uint32_t>) return "uint32_t";
        else if constexpr (std::is_same_v<T, uint64_t>) return "uint64_t";
        else if constexpr (std::is_same_v<T, int8_t>) return "int8_t";
        else if constexpr (std::is_same_v<T, int16_t>) return "int16_t";
        else if constexpr (std::is_same_v<T, int32_t>) return "int32_t";
        else if constexpr (std::is_same_v<T, int64_t>) return "int64_t";
        else if constexpr (std::is_same_v<T, float>) return "float";
        else if constexpr (std::is_same_v<T, double>) return "double";
        else if constexpr (std::is_same_v<T, char>) return "char";
        else return "unsupported";
    }

    template <typename T>
    constexpr T FirstValue() noexcept
    {
        if constexpr (std::is_same_v<T, char>) return 'A';
        else if constexpr (std::is_floating_point_v<T>) return static_cast<T>(1.25);
        else if constexpr (std::is_signed_v<T>) return static_cast<T>(-7);
        else return static_cast<T>(7u);
    }

    template <typename T>
    constexpr T SecondValue() noexcept
    {
        if constexpr (std::is_same_v<T, char>) return 'Z';
        else if constexpr (std::is_floating_point_v<T>) return static_cast<T>(3.5);
        else return static_cast<T>(42);
    }

    template <typename T>
    using WrongType = std::conditional_t<std::is_same_v<T, float>, uint32_t, float>;

    static LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier
    OneRegionLayout() noexcept
    {
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
        return layout;
    }

    template <typename T>
    bool CreateTypedAPC(
        VagueTemoraryPremativeFabric& fabric,
        AdaptivePackedCellContainer& apc,
        SD::SchemaProtocols protocol) noexcept
    {
        constexpr auto maybe_dtype = SD::CppTypeToRegionDType<T>();
        static_assert(maybe_dtype.has_value());

        SD::InitialRegionalDtypeConf dtype{};
        dtype.FEEDFORWARD_MESSAGE = maybe_dtype.value();

        SD::InitialRegionalProtocol protocols{};
        protocols.FEEDFORWARD_MESSAGE = protocol;

        return fabric.CreateAPC(
            apc,
            false,
            false,
            OneRegionLayout(),
            dtype,
            protocols,
            APCDataStructure::BRANCH_VERSION
        );
    }

    template <typename T>
    bool TestPrivateView() noexcept
    {
        VagueTemoraryPremativeFabric fabric{};
        AdaptivePackedCellContainer apc{};

        if (
            !fabric.InitializeFabricWithPtrTable(FABRIC_SLOT_COUNT, SLOT_WORDS) ||
            !CreateTypedAPC<T>(fabric, apc, SD::SchemaProtocols::PRIVATE_REGION)
        )
        {
            return false;
        }

        auto view = apc.BuildAViewOverRegion<T>(
            MacroColumnOfAPC::FEEDFORWARD_MESSAGE);
        const auto wrong_view = apc.BuildAViewOverRegion<WrongType<T>>(
            MacroColumnOfAPC::FEEDFORWARD_MESSAGE);

        if (
            !view.has_value() ||
            !view->IsValid() ||
            view->Size() < 3u ||
            view->GetProtocol() != SD::SchemaProtocols::PRIVATE_REGION ||
            wrong_view.has_value() ||
            view->AtomicStore(0u, FirstValue<T>())
        )
        {
            return false;
        }

        auto mutable_span = view->RawMutableSpan();
        if (!mutable_span.has_value()) return false;

        const size_t middle = mutable_span->size() / 2u;
        const size_t last = mutable_span->size() - 1u;
        mutable_span.value()[0u] = FirstValue<T>();
        mutable_span.value()[middle] = SecondValue<T>();
        mutable_span.value()[last] = FirstValue<T>();

        if (
            mutable_span.value()[0u] != FirstValue<T>() ||
            mutable_span.value()[middle] != SecondValue<T>() ||
            mutable_span.value()[last] != FirstValue<T>() ||
            !apc.ZeroARegion<T>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE)
        )
        {
            return false;
        }

        for (const T& value : mutable_span.value())
        {
            if (value != T{}) return false;
        }

        return true;
    }

    template <typename T>
    bool TestAtomicView() noexcept
    {
        VagueTemoraryPremativeFabric fabric{};
        AdaptivePackedCellContainer apc{};

        if (
            !fabric.InitializeFabricWithPtrTable(FABRIC_SLOT_COUNT, SLOT_WORDS) ||
            !CreateTypedAPC<T>(fabric, apc, SD::SchemaProtocols::ATOMIC_WORD_ARRAY)
        )
        {
            return false;
        }

        auto view = apc.BuildAViewOverRegion<T>(
            MacroColumnOfAPC::FEEDFORWARD_MESSAGE);
        const auto wrong_view = apc.BuildAViewOverRegion<WrongType<T>>(
            MacroColumnOfAPC::FEEDFORWARD_MESSAGE);

        if (
            !view.has_value() ||
            !view->IsValid() ||
            view->Size() < 3u ||
            view->GetProtocol() != SD::SchemaProtocols::ATOMIC_WORD_ARRAY ||
            view->RawMutableSpan().has_value() ||
            wrong_view.has_value()
        )
        {
            return false;
        }

        const size_t middle = view->Size() / 2u;
        const size_t last = view->Size() - 1u;

        if (
            !view->AtomicStore(0u, FirstValue<T>(), std::memory_order_relaxed) ||
            !view->AtomicStore(middle, SecondValue<T>(), std::memory_order_release) ||
            !view->AtomicStore(last, FirstValue<T>(), std::memory_order_release) ||
            view->AtomicStore(view->Size(), SecondValue<T>()) ||
            view->AtomicLoad(0u, std::memory_order_relaxed) != FirstValue<T>() ||
            view->AtomicLoad(middle, std::memory_order_acquire) != SecondValue<T>() ||
            view->AtomicLoad(last, std::memory_order_acquire) != FirstValue<T>()
        )
        {
            return false;
        }

        T expected = FirstValue<T>();
        if (
            !view->AtomicCompareExchangeStrong(
                0u,
                expected,
                SecondValue<T>(),
                std::memory_order_acq_rel,
                std::memory_order_acquire
            ) ||
            view->AtomicLoad(0u, std::memory_order_acquire) != SecondValue<T>() ||
            !apc.ZeroARegion<T>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE)
        )
        {
            return false;
        }

        for (size_t i = 0u; i < view->Size(); ++i)
        {
            if (view->AtomicLoad(i, std::memory_order_relaxed) != T{})
            {
                return false;
            }
        }

        return true;
    }

    template <typename T>
    bool RunPrimitiveCase()
    {
        const bool private_ok = TestPrivateView<T>();
        const bool atomic_ok = TestAtomicView<T>();

        std::cout
            << "  " << std::left << std::setw(10) << PrimitiveName<T>()
            << " private-span=" << (private_ok ? "PASS" : "FAIL")
            << "  atomic-ref=" << (atomic_ok ? "PASS" : "FAIL")
            << '\n';

        return private_ok && atomic_ok;
    }

    inline Result Run()
    {
        Banner("TEST 6 - PUBLIC REGION VIEW / ALL PRIMITIVE DTYPES");

        std::cout
            << "Every case constructs views through AdaptivePackedCellContainer.\n"
            << "PRIVATE_REGION uses RawMutableSpan; ATOMIC_WORD_ARRAY uses atomic_ref operations.\n"
            << "Each case also proves exact schema-dtype rejection and ZeroARegion<T>().\n\n";

        bool all_ok = true;
        all_ok = RunPrimitiveCase<uint8_t>() && all_ok;
        all_ok = RunPrimitiveCase<uint16_t>() && all_ok;
        all_ok = RunPrimitiveCase<uint32_t>() && all_ok;
        all_ok = RunPrimitiveCase<uint64_t>() && all_ok;
        all_ok = RunPrimitiveCase<int8_t>() && all_ok;
        all_ok = RunPrimitiveCase<int16_t>() && all_ok;
        all_ok = RunPrimitiveCase<int32_t>() && all_ok;
        all_ok = RunPrimitiveCase<int64_t>() && all_ok;
        all_ok = RunPrimitiveCase<float>() && all_ok;
        all_ok = RunPrimitiveCase<double>() && all_ok;
        all_ok = RunPrimitiveCase<char>() && all_ok;

        std::cout
            << "\nTEST 6 OVERALL: "
            << (all_ok ? "PASS" : "FAIL")
            << '\n';

        return all_ok ? Result::PASS : Result::FAIL;
    }
}

inline int RunAll()
{
    using TestKit::Result;

    const std::array<std::pair<const char*, Result>, 6u> results{{
        {"Test 1 - traversal + compound baseline", Test01_PublicTraversalBaseline::Run()},
        {"Test 2 - compound contention sweep", Test02_ContentionSweep::Run()},
        {"Test 3 - compound writer/readers", Test03_ReaderWriterTraversal::Run()},
        {"Test 4 - primitive vs compound APIs", Test04_PrimitiveVsCompoundMutation::Run()},
        {"Test 5 - per-axis acyclicity", Test05_PerAxisAcyclicity::Run()},
        {"Test 6 - primitive region views", Test06_PrimitiveRegionViews::Run()}
    }};

    TestKit::Banner("APC MODULAR TEST SUITE SUMMARY");

    uint32_t failures = 0u;
    uint32_t skips = 0u;

    for (const auto& [name, result] : results)
    {
        std::cout
            << "  " << std::left << std::setw(42) << name
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
