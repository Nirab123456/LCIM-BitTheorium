#pragma once

#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

namespace APCFabricVsVectorPointerChaseTest
{
    using namespace BidirectionalInMemGraph;

    using Axis = InstallAxisToBuffer::BidirectionalAxis;
    using Inheritance = InstallAxisToBuffer::DescOfInharitance;
    using Clock = std::chrono::steady_clock;

    // Keep this a power of two: the vertical order uses bit reversal.
    constexpr size_t CHAIN_LENGTH = 64u;
    constexpr size_t OWNER_INDEX = CHAIN_LENGTH;
    constexpr size_t NODE_COUNT = CHAIN_LENGTH + 1u;

    constexpr uint32_t SLOT_WORDS = MINIMUM_APC_CELL_COUNT;
    constexpr uint32_t FABRIC_SLOT_COUNT = static_cast<uint32_t>(NODE_COUNT + 8u);
    constexpr uint32_t PAYLOAD_WORDS = 32u;

    constexpr uint32_t TRAVERSAL_ROUNDS = 20'000u;
    constexpr uint32_t PAYLOAD_ROUNDS = 200u;
    constexpr uint32_t GRAPH_PAYLOAD_ROUNDS = 2'000u;
    constexpr uint32_t MUTATION_ROUNDS = 500u;
    constexpr uint32_t MEASURED_RUNS = 5u;
    constexpr uint32_t CONCURRENT_TRIALS = 20u;

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

    template <size_t N>
    static double Median(std::array<double, N> values)
    {
        std::sort(values.begin(), values.end());
        return values[N / 2u];
    }

    static double Ratio(double apc, double baseline) noexcept
    {
        return baseline > 0.0 ? apc / baseline : 0.0;
    }

    static void PerturbSchedule(uint32_t i) noexcept
    {
        if ((i & 31u) == 0u)
        {
            std::this_thread::yield();
        }
    }

    // =====================================================================
    // Deterministic locality pattern.
    // HORIZONTAL: 0 -> 1 -> 2 -> ... -> 63 (sequential)
    // VERTICAL  : owner -> bit_reverse(0) -> bit_reverse(1) -> ... -> 63
    // =====================================================================

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
        for (size_t i = 0u; i < CHAIN_LENGTH; ++i)
        {
            out[i] = Reverse6Bits(i);
        }
        return out;
    }

    constexpr auto VERTICAL_ORDER = MakeVerticalOrder();
    static_assert(VERTICAL_ORDER.front() == 0u);
    static_assert(VERTICAL_ORDER.back() == CHAIN_LENGTH - 1u);

    // =====================================================================
    // Baseline: std::vector owns the nodes; graph navigation is ordinary
    // pointer chasing. Storage is fixed before any pointers are installed.
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
        PointerAxis V{};
        std::array<uint64_t, PAYLOAD_WORDS> Payload{};
    };

    class VectorPointerBackend
    {
    public:
        using Handle = PointerNode*;

        bool Initialize()
        {
            Nodes_.resize(NODE_COUNT);

            for (size_t i = 0u; i + 1u < CHAIN_LENGTH; ++i)
            {
                Nodes_[i].H.OwnsRoot = true;
            }
            Nodes_[OWNER_INDEX].V.OwnsRoot = true;
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

        bool Attach(size_t predecessor, size_t child, Axis axis, Inheritance inheritance) noexcept
        {
            if (predecessor >= NODE_COUNT || child >= NODE_COUNT || predecessor == child)
            {
                return false;
            }
            return Attach_(HandleAt(predecessor), HandleAt(child), axis, inheritance);
        }

        bool Detach(size_t child, Axis axis) noexcept
        {
            if (child >= NODE_COUNT) return false;
            return Detach_(HandleAt(child), axis);
        }

        bool DetachChild(size_t owner, size_t child, Axis axis) noexcept
        {
            if (owner >= NODE_COUNT || child >= NODE_COUNT) return false;
            PointerAxis& child_axis = Axis_(HandleAt(child), axis);
            if (child_axis.Owner != HandleAt(owner)) return false;
            return Detach_(HandleAt(child), axis);
        }

        Handle FindNext(Handle from, Axis axis, Inheritance inheritance) noexcept
        {
            if (!from) return nullptr;
            PointerAxis& state = Axis_(from, axis);
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
            if (node >= NODE_COUNT || word >= PAYLOAD_WORDS) return false;
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
            if (idx >= NODE_COUNT || word >= PAYLOAD_WORDS) return false;
            uint64_t& cell = Nodes_[idx].Payload[word];
            value = atomic
                ? std::atomic_ref<uint64_t>(cell).load(std::memory_order_acquire)
                : cell;
            return true;
        }

        bool LocksReleased() noexcept { return true; }

        size_t ApproxStorageBytes() const noexcept
        {
            return Nodes_.capacity() * sizeof(PointerNode);
        }

    private:
        std::vector<PointerNode> Nodes_{};

        static PointerAxis& Axis_(Handle node, Axis axis) noexcept
        {
            return axis == Axis::HORIZONTAL ? node->H : node->V;
        }

        static bool Attach_(Handle predecessor, Handle child, Axis axis, Inheritance inheritance) noexcept
        {
            if (!predecessor || !child || predecessor == child) return false;

            PointerAxis& child_axis = Axis_(child, axis);
            if (child_axis.Owner || child_axis.Previous || child_axis.Next)
            {
                return false;
            }

            if (inheritance == Inheritance::FIRST_CHILD)
            {
                PointerAxis& owner_axis = Axis_(predecessor, axis);
                if (!owner_axis.OwnsRoot || owner_axis.FirstChild || owner_axis.LastChild || owner_axis.ChildCount != 0u)
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
                PointerAxis& predecessor_axis = Axis_(predecessor, axis);
                if (!predecessor_axis.Owner || predecessor_axis.Next)
                {
                    return false;
                }

                PointerAxis& owner_axis = Axis_(predecessor_axis.Owner, axis);
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

            PointerAxis& child_axis = Axis_(child, axis);
            Handle owner = child_axis.Owner;
            Handle previous = child_axis.Previous;
            Handle next = child_axis.Next;
            if (!owner || !previous) return false;

            PointerAxis& owner_axis = Axis_(owner, axis);
            if (!owner_axis.OwnsRoot || owner_axis.ChildCount == 0u) return false;

            if (previous == owner)
            {
                if (owner_axis.FirstChild != child) return false;
                owner_axis.FirstChild = next;
            }
            else
            {
                PointerAxis& prev_axis = Axis_(previous, axis);
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

    // =====================================================================
    // APC/Fabric backend.
    // IMPORTANT: every graph traversal below goes through ONLY:
    //   - AdaptivePackedCellContainer::FindMyNext
    //   - AdaptivePackedCellContainer::FindPrevious
    // No identity-buffer/root-table/slot lookup is used for navigation.
    // =====================================================================

    class APCBackend
    {
    public:
        using Handle = AdaptivePackedCellContainer*;

        bool Initialize()
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

            for (size_t i = 0u; i < NODE_COUNT; ++i)
            {
                const bool wants_h_root = i + 1u < CHAIN_LENGTH;
                const bool wants_v_root = i == OWNER_INDEX;

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

                uint64_t slot = FABRIC_CELL_SENTINAL;
                if (!Nodes_[i].GetThisSlotIdx(slot) ||
                    !APCDataStructure::IsValid32BitAPCUnit(slot) ||
                    slot >= FABRIC_SLOT_COUNT)
                {
                    return false;
                }
                Slots_[i] = static_cast<uint32_t>(slot);

                if (!ResolvePayloadBegin_(i))
                {
                    return false;
                }
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

        bool Attach(size_t predecessor, size_t child, Axis axis, Inheritance inheritance) noexcept
        {
            return predecessor < NODE_COUNT && child < NODE_COUNT &&
                Nodes_[predecessor].AttachAnotherToMe(Nodes_[child], axis, inheritance);
        }

        bool Detach(size_t child, Axis axis) noexcept
        {
            return child < NODE_COUNT && Nodes_[child].DetachMeFromAnotherEdge(axis);
        }

        bool DetachChild(size_t owner, size_t child, Axis axis) noexcept
        {
            return owner < NODE_COUNT && child < NODE_COUNT &&
                Nodes_[owner].DetachMyChild(Nodes_[child], axis);
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
            if (node >= NODE_COUNT || word >= PAYLOAD_WORDS) return false;
            const uint32_t local = PayloadBegin_[node] + word;
            if (atomic)
            {
                Nodes_[node].AtomicallyWriteU64ToAPC(local, value);
                return true;
            }
            return Nodes_[node].ForceCopyToAPCFromBuffer(local, 1u, &value);
        }

        bool LoadPayload(Handle node, uint32_t word, uint64_t& value, bool atomic) noexcept
        {
            const size_t idx = IndexOf(node);
            if (idx >= NODE_COUNT || word >= PAYLOAD_WORDS) return false;
            const uint32_t local = PayloadBegin_[idx] + word;
            return atomic
                ? Nodes_[idx].AtomicallyReadLongLongAPCUnit(local, value)
                : Nodes_[idx].CopyFromAPCToBuffer(local, 1u, &value, false);
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

        size_t SegmentPoolLowerBoundBytes() const noexcept
        {
            return static_cast<size_t>(FABRIC_SLOT_COUNT) * SLOT_WORDS * sizeof(uint64_t);
        }

        size_t RuntimeViewBytesLowerBound() const noexcept
        {
            return sizeof(Nodes_) +
                static_cast<size_t>(FABRIC_SLOT_COUNT) * sizeof(std::atomic<AdaptivePackedCellContainer*>);
        }

    private:
        VagueTemoraryPremativeFabric Fabric_{};
        std::array<AdaptivePackedCellContainer, NODE_COUNT> Nodes_{};
        std::array<uint32_t, NODE_COUNT> Slots_{};
        std::array<uint32_t, NODE_COUNT> PayloadBegin_{};

        bool ResolvePayloadBegin_(size_t node) noexcept
        {
            uint64_t packed = FABRIC_CELL_SENTINAL;
            if (!Nodes_[node].ReadAPCMetaUnit(HeaderIdentifierOfAPC::FEEDFORWARD_BOUNDS, packed, true))
            {
                return false;
            }

            const auto bounds = LayoutBoundsOrchestrator::GetLayoutCarrierFromValidLayoutCell(
                packed,
                MacroColumnOfAPC::FEEDFORWARD_MESSAGE);

            if (!bounds.IsValid || bounds.BeginIndex >= bounds.EndIndex ||
                bounds.EndIndex - bounds.BeginIndex < PAYLOAD_WORDS)
            {
                return false;
            }

            PayloadBegin_[node] = static_cast<uint32_t>(bounds.BeginIndex);
            return true;
        }
    };

    // =====================================================================
    // Common scenario and traversal-only validation.
    // =====================================================================

    template <typename Backend>
    static bool BuildScenario(Backend& b)
    {
        if (!b.Initialize()) return false;

        // Sequential depth chain on H.
        for (size_t i = 0u; i + 1u < CHAIN_LENGTH; ++i)
        {
            if (!b.Attach(i, i + 1u, Axis::HORIZONTAL, Inheritance::FIRST_CHILD))
            {
                return false;
            }
        }

        // Scrambled sibling chain on V.
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

        // Payload identity makes wrong-node traversal visible in checksums.
        for (size_t node = 0u; node < NODE_COUNT; ++node)
        {
            for (uint32_t word = 0u; word < PAYLOAD_WORDS; ++word)
            {
                const uint64_t value = (static_cast<uint64_t>(node + 1u) << 32u) | word;
                if (!b.StorePayload(node, word, value, false))
                {
                    return false;
                }
            }
        }
        return true;
    }

    template <typename Backend>
    static bool ValidateHorizontal(Backend& b)
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
    static bool ValidateVertical(Backend& b)
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
    static bool ValidateAll(Backend& b)
    {
        return ValidateHorizontal(b) && ValidateVertical(b) && b.LocksReleased();
    }

    // =====================================================================
    // Traversal benchmarks.
    // Each operation count is the number of graph hops, not outer loops.
    // =====================================================================

    template <typename Backend>
    static Timing TraverseHorizontalForward(Backend& b)
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
        return {
            true,
            checksum,
            static_cast<uint64_t>(TRAVERSAL_ROUNDS) * (CHAIN_LENGTH - 1u),
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
        };
    }

    template <typename Backend>
    static Timing TraverseHorizontalBackward(Backend& b)
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
        return {
            true,
            checksum,
            static_cast<uint64_t>(TRAVERSAL_ROUNDS) * (CHAIN_LENGTH - 1u),
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
        };
    }

    template <typename Backend>
    static Timing TraverseVerticalForward(Backend& b)
    {
        uint64_t checksum = 0u;
        const auto begin = Clock::now();

        for (uint32_t round = 0u; round < TRAVERSAL_ROUNDS; ++round)
        {
            auto current = b.FindNext(b.HandleAt(OWNER_INDEX), Axis::VERTICAL, Inheritance::FIRST_CHILD);
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
        return {
            true,
            checksum,
            static_cast<uint64_t>(TRAVERSAL_ROUNDS) * CHAIN_LENGTH,
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
        };
    }

    template <typename Backend>
    static Timing TraverseVerticalBackward(Backend& b)
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
        return {
            true,
            checksum,
            static_cast<uint64_t>(TRAVERSAL_ROUNDS) * CHAIN_LENGTH,
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
        };
    }

    // =====================================================================
    // Payload benchmarks.
    // =====================================================================

    template <typename Backend>
    static Timing SequentialPayloadRead(Backend& b, bool atomic)
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
            static_cast<uint64_t>(PAYLOAD_ROUNDS) * CHAIN_LENGTH * PAYLOAD_WORDS,
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
        };
    }

    template <typename Backend>
    static Timing ScrambledGraphPlusPayload(Backend& b)
    {
        uint64_t checksum = 0u;
        const auto begin = Clock::now();

        for (uint32_t round = 0u; round < GRAPH_PAYLOAD_ROUNDS; ++round)
        {
            auto current = b.FindNext(b.HandleAt(OWNER_INDEX), Axis::VERTICAL, Inheritance::FIRST_CHILD);
            if (!current) return {};

            for (size_t i = 0u; i < CHAIN_LENGTH; ++i)
            {
                uint64_t value = 0u;
                if (!b.LoadPayload(current, static_cast<uint32_t>(i & (PAYLOAD_WORDS - 1u)), value, false))
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
        return {
            true,
            checksum,
            static_cast<uint64_t>(GRAPH_PAYLOAD_ROUNDS) * CHAIN_LENGTH,
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
        };
    }

    // =====================================================================
    // Mutation benchmarks.
    // =====================================================================

    template <typename Backend>
    static Timing SerialHorizontalTailMutation(Backend& b)
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
        return {
            ValidateHorizontal(b),
            completed,
            completed,
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
        };
    }

    template <typename Backend>
    static Timing SerialVerticalTailMutation(Backend& b)
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
        return {
            ValidateVertical(b),
            completed,
            completed,
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
        };
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
    static ConcurrentMutationResult ConcurrentSameNodeHVMutation(Backend& b)
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
        const bool integrity_ok = ValidateAll(b);
        const uint64_t hd = h_done.load(std::memory_order_acquire);
        const uint64_t vd = v_done.load(std::memory_order_acquire);
        const bool operations_ok =
            !failed.load(std::memory_order_acquire) &&
            hd == MUTATION_ROUNDS &&
            vd == MUTATION_ROUNDS;

        return {
            operations_ok,
            integrity_ok,
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
        int64_t ElapsedNs = 0;

        double CompletionPercent() const noexcept
        {
            return Trials == 0u
                ? 0.0
                : 100.0 * static_cast<double>(FullCompletionTrials) / static_cast<double>(Trials);
        }
    };

    template <typename Backend>
    static ConcurrentStressSummary RunConcurrentStress(Backend& b)
    {
        ConcurrentStressSummary out{};
        for (uint32_t trial = 0u; trial < CONCURRENT_TRIALS; ++trial)
        {
            const ConcurrentMutationResult r = ConcurrentSameNodeHVMutation(b);
            ++out.Trials;
            out.HorizontalCycles += r.HorizontalCompleted;
            out.VerticalCycles += r.VerticalCompleted;
            out.ElapsedNs += r.ElapsedNs;

            if (r.OperationsOk)
            {
                ++out.FullCompletionTrials;
            }
            if (!r.IntegrityOk)
            {
                ++out.IntegrityFailures;
                break;
            }
        }
        return out;
    }

    // =====================================================================
    // Reporting.
    // =====================================================================

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
    static Timing RunMetric(Backend& b, Metric m)
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
            << "  ratio=" << std::setw(8) << std::setprecision(2) << Ratio(apc_ns, baseline_ns) << "x\n";
    }

    static int Fail(const char* where)
    {
        std::cout << "\nFAIL: " << where << '\n';
        return 1;
    }

    inline int RunAPCFabricVsVectorPointerChaseTest()
    {
        const auto vector_build_begin = Clock::now();
        VectorPointerBackend vector_backend{};
        if (!BuildScenario(vector_backend)) return Fail("vector+pointer construction");
        const auto vector_build_end = Clock::now();

        const auto apc_build_begin = Clock::now();
        APCBackend apc_backend{};
        if (!BuildScenario(apc_backend)) return Fail("APC/Fabric construction");
        const auto apc_build_end = Clock::now();

        if (!ValidateAll(vector_backend)) return Fail("initial vector traversal topology");
        if (!ValidateAll(apc_backend)) return Fail("initial APC traversal topology/locks");

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
            << Ratio(static_cast<double>(apc_build_us), static_cast<double>(vector_build_us)) << "x\n\n";

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

                // Alternate order to reduce systematic first/second-run bias.
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

                if (!v.Ok)
                {
                    std::cout << "  vector failed metric: " << MetricName(metric) << '\n';
                    return Fail("measured vector metric");
                }
                if (!a.Ok)
                {
                    std::cout << "  APC failed metric: " << MetricName(metric) << '\n'
                              << "  APC final H topology : " << (ValidateHorizontal(apc_backend) ? "PASS" : "FAIL") << '\n'
                              << "  APC final V topology : " << (ValidateVertical(apc_backend) ? "PASS" : "FAIL") << '\n'
                              << "  APC locks released   : " << (apc_backend.LocksReleased() ? "PASS" : "FAIL") << '\n';
                    return Fail("measured APC metric");
                }

                vector_samples[mi][run] = v.NsPerOp();
                apc_samples[mi][run] = a.NsPerOp();
            }

            if (!ValidateAll(vector_backend) || !ValidateAll(apc_backend))
            {
                return Fail("post-run traversal topology/lock invariant");
            }

            std::cout << "  run " << (run + 1u) << '/' << MEASURED_RUNS << " : PASS\n";
        }

        std::cout << "\nMEDIAN COST PER OPERATION\n";
        std::array<double, METRIC_COUNT> vector_median{};
        std::array<double, METRIC_COUNT> apc_median{};

        for (size_t mi = 0u; mi < METRIC_COUNT; ++mi)
        {
            vector_median[mi] = Median(vector_samples[mi]);
            apc_median[mi] = Median(apc_samples[mi]);
            PrintMetric(MetricName(static_cast<Metric>(mi)), vector_median[mi], apc_median[mi]);
        }

        // Same terminal object is mutated concurrently on H and V.
        // Bounded-retry rejection is reported separately from structural corruption.
        std::cout << "\nSAME-NODE H/V CONCURRENT MUTATION STRESS\n";
        const ConcurrentStressSummary vector_concurrent = RunConcurrentStress(vector_backend);
        const ConcurrentStressSummary apc_concurrent = RunConcurrentStress(apc_backend);

        std::cout
            << "  vector+ptr full-completion trials : "
            << vector_concurrent.FullCompletionTrials << '/' << vector_concurrent.Trials
            << "  (" << std::fixed << std::setprecision(1) << vector_concurrent.CompletionPercent() << "%)\n"
            << "  vector+ptr integrity failures     : " << vector_concurrent.IntegrityFailures << '\n'
            << "  APC+Fabric full-completion trials : "
            << apc_concurrent.FullCompletionTrials << '/' << apc_concurrent.Trials
            << "  (" << apc_concurrent.CompletionPercent() << "%)\n"
            << "  APC+Fabric integrity failures     : " << apc_concurrent.IntegrityFailures << '\n'
            << "  APC completed H/V cycles          : "
            << apc_concurrent.HorizontalCycles << " / " << apc_concurrent.VerticalCycles << '\n'
            << "  NOTE: a rejected bounded-retry operation with intact final topology is\n"
            << "        an availability/contention signal, not structural corruption.\n";

        const bool vector_final = ValidateAll(vector_backend);
        const bool apc_h_final = ValidateHorizontal(apc_backend);
        const bool apc_v_final = ValidateVertical(apc_backend);
        const bool apc_locks_final = apc_backend.LocksReleased();

        std::cout
            << "\nMEMORY FOOTPRINT SIGNAL (NOT exact total-process memory)\n"
            << "  vector node storage                 : " << vector_backend.ApproxStorageBytes() << " bytes\n"
            << "  APC configured segment-pool minimum : " << apc_backend.SegmentPoolLowerBoundBytes() << " bytes\n"
            << "  APC wrapper+runtime-ptr lower bound  : " << apc_backend.RuntimeViewBytesLowerBound() << " bytes\n"
            << "  NOTE: APC numbers exclude additional Fabric control tables, so they are a lower bound.\n\n";

        const double h_forward_ratio = Ratio(
            apc_median[static_cast<size_t>(Metric::H_FORWARD)],
            vector_median[static_cast<size_t>(Metric::H_FORWARD)]);
        const double v_forward_ratio = Ratio(
            apc_median[static_cast<size_t>(Metric::V_FORWARD)],
            vector_median[static_cast<size_t>(Metric::V_FORWARD)]);
        const double direct_payload_ratio = Ratio(
            apc_median[static_cast<size_t>(Metric::PAYLOAD_DIRECT)],
            vector_median[static_cast<size_t>(Metric::PAYLOAD_DIRECT)]);
        const double mutation_ratio = Ratio(
            apc_median[static_cast<size_t>(Metric::MUTATE_H)],
            vector_median[static_cast<size_t>(Metric::MUTATE_H)]);

        std::cout
            << "LACKING SIGNALS RELATIVE TO THE STANDARD BASELINE\n"
            << "  sequential graph-hop tax : " << std::fixed << std::setprecision(2) << h_forward_ratio << "x\n"
            << "  scrambled graph-hop tax  : " << v_forward_ratio << "x\n"
            << "  direct payload-read tax  : " << direct_payload_ratio << "x\n"
            << "  serial mutation tax      : " << mutation_ratio << "x\n"
            << "  concurrent H/V full trials: " << apc_concurrent.FullCompletionTrials << "/" << apc_concurrent.Trials << "\n"
            << "  concurrent integrity fails: " << apc_concurrent.IntegrityFailures << "\n\n";

        const bool final_ok =
            vector_final &&
            vector_concurrent.IntegrityFailures == 0u &&
            apc_concurrent.IntegrityFailures == 0u &&
            apc_h_final &&
            apc_v_final &&
            apc_locks_final;

        std::cout
            << "FINAL DIAGNOSTICS\n"
            << "  vector topology       : " << (vector_final ? "PASS" : "FAIL") << '\n'
            << "  APC H topology        : " << (apc_h_final ? "PASS" : "FAIL") << '\n'
            << "  APC V topology        : " << (apc_v_final ? "PASS" : "FAIL") << '\n'
            << "  APC graph locks       : " << (apc_locks_final ? "PASS" : "FAIL") << '\n'
            << "  APC concurrent integrity: " << (apc_concurrent.IntegrityFailures == 0u ? "PASS" : "FAIL") << '\n'
            << "======================================================================\n"
            << "OVERALL: " << (final_ok ? "PASS" : "FAIL") << '\n'
            << "======================================================================\n";

        return final_ok ? 0 : 1;
    }
}
