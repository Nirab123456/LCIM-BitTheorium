#pragma once

// APC/Fabric dual-edge DAG test kit (C++20)
//
// Put this file in core/TestFiles and compile a tiny runner:
//
//   #include "TestKit.hpp"
//   int main() { return APCDAGTests::RunAll(); }
//
// Required completed production API: APC_Dual_Edge_DAG_Remaining_Functions.md.
// The tests intentionally use only public APC/Fabric functions.

#ifndef APC_DAG_TEST_EXTERNAL_TYPES
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#endif

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
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace APCDAGTests
{
using namespace BidirectionalInMemGraph;

using Clock = std::chrono::steady_clock;
using ReadOperation = FabricToAPCLinker::SeqLockedOperation;

enum class Axis : std::uint8_t
{
    HORIZONTAL,
    VERTICAL
};

constexpr FabricSegments EdgeTableForAxis(Axis axis) noexcept
{
    return axis == Axis::HORIZONTAL
        ? FabricSegments::HORIZONTAL_EDGE_TABLE
        : FabricSegments::VERTICAL_EDGE_TABLE;
}

enum class Result : std::uint8_t
{
    PASS,
    FAIL
};

constexpr const char* ResultName(Result result) noexcept
{
    return result == Result::PASS ? "PASS" : "FAIL";
}

inline void Banner(const char* title)
{
    std::cout
        << "\n================================================================================\n"
        << title
        << "\n================================================================================\n";
}

inline double Ratio(double numerator, double denominator) noexcept
{
    return denominator > 0.0 ? numerator / denominator : 0.0;
}

template <std::size_t N>
double Median(std::array<double, N> samples)
{
    static_assert(N > 0u);
    std::sort(samples.begin(), samples.end());
    return samples[N / 2u];
}

inline void PerturbSchedule(std::uint64_t value) noexcept
{
    if ((value & 63u) == 0u)
    {
        std::this_thread::yield();
    }
}

struct ReadResult
{
    static constexpr std::size_t NO_NODE = std::numeric_limits<std::size_t>::max();
    static constexpr std::uint32_t NO_LOCATOR = UINT32_MAX;

    std::size_t Node = NO_NODE;
    std::uint32_t Locator = NO_LOCATOR;
    ReadOperation Outcome = ReadOperation::NONE;
    bool PointerPresent = false;

    bool IsFound() const noexcept
    {
        return Outcome == ReadOperation::FOUND &&
            PointerPresent &&
            Node != NO_NODE &&
            Locator != NO_LOCATOR;
    }

    bool IsNone() const noexcept
    {
        return Outcome == ReadOperation::NONE &&
            !PointerPresent &&
            Node == NO_NODE &&
            Locator == NO_LOCATOR;
    }

    bool IsRetry() const noexcept
    {
        return Outcome == ReadOperation::RETRY &&
            !PointerPresent &&
            Node == NO_NODE &&
            Locator == NO_LOCATOR;
    }

    bool ContractValid() const noexcept
    {
        return IsFound() || IsNone() || IsRetry();
    }
};

struct ReadCounts
{
    std::uint64_t Found = 0u;
    std::uint64_t None = 0u;
    std::uint64_t Retry = 0u;
    std::uint64_t BadContract = 0u;

    void Observe(const ReadResult& read) noexcept
    {
        if (!read.ContractValid())
        {
            ++BadContract;
            return;
        }

        if (read.IsFound()) ++Found;
        else if (read.IsRetry()) ++Retry;
        else ++None;
    }

    void Add(const ReadCounts& other) noexcept
    {
        Found += other.Found;
        None += other.None;
        Retry += other.Retry;
        BadContract += other.BadContract;
    }

    std::uint64_t Calls() const noexcept
    {
        return Found + None + Retry + BadContract;
    }
};

inline void PrintReadCounts(const char* label, const ReadCounts& counts)
{
    std::cout
        << "  " << std::left << std::setw(31) << label
        << " calls=" << std::right << std::setw(10) << counts.Calls()
        << " FOUND=" << std::setw(10) << counts.Found
        << " RETRY=" << std::setw(10) << counts.Retry
        << " NONE=" << std::setw(10) << counts.None
        << " bad=" << counts.BadContract << '\n';
}

// -----------------------------------------------------------------------------
// Global-mutex vector forest baseline.
// It deliberately has one parent per axis; that is the old forest lower bound.
// Quiescent reads are raw. Every mutation holds one mutex across the whole move.
// -----------------------------------------------------------------------------

template <std::size_t NodeCount, std::size_t PayloadWords>
class VectorLockedForest
{
    static constexpr std::uint32_t NIL = UINT32_MAX;

    struct AxisState
    {
        std::uint32_t Parent = NIL;
        std::uint32_t Previous = NIL;
        std::uint32_t Next = NIL;
        std::uint32_t First = NIL;
        std::uint32_t Last = NIL;
    };

    struct Node
    {
        AxisState H{};
        AxisState V{};
        std::array<std::uint64_t, PayloadWords> Payload{};
    };

public:
    bool Initialize() noexcept
    {
        Nodes_ = {};
        return true;
    }

    bool AddParent(
        std::size_t parent,
        std::size_t child,
        Axis axis,
        std::uint32_t = DEFAULT_MAX_TRIES) noexcept
    {
        std::lock_guard<std::mutex> lock(GraphMutex_);
        return AddUnlocked_(parent, child, axis);
    }

    bool RemoveParent(
        std::size_t parent,
        std::size_t child,
        Axis axis,
        std::uint32_t = DEFAULT_MAX_TRIES) noexcept
    {
        std::lock_guard<std::mutex> lock(GraphMutex_);
        return RemoveUnlocked_(parent, child, axis);
    }

    bool ReplaceParent(
        std::size_t old_parent,
        std::size_t new_parent,
        std::size_t child,
        Axis axis,
        std::uint32_t = DEFAULT_MAX_TRIES) noexcept
    {
        std::lock_guard<std::mutex> lock(GraphMutex_);

        if (
            old_parent >= NodeCount ||
            new_parent >= NodeCount ||
            child >= NodeCount ||
            old_parent == new_parent ||
            new_parent >= child ||
            Axis_(child, axis).Parent != old_parent
        )
        {
            return false;
        }

        if (!RemoveUnlocked_(old_parent, child, axis))
        {
            return false;
        }
        if (AddUnlocked_(new_parent, child, axis))
        {
            return true;
        }

        (void)AddUnlocked_(old_parent, child, axis);
        return false;
    }

    ReadResult FindParent(
        std::size_t child,
        Axis axis,
        std::uint8_t ordinal,
        std::uint32_t = 1u) noexcept
    {
        if (child >= NodeCount || ordinal != 0u)
        {
            return {};
        }

        const std::uint32_t parent = Axis_(child, axis).Parent;
        return parent == NIL
            ? ReadResult{}
            : ReadResult{parent, static_cast<std::uint32_t>(child), ReadOperation::FOUND, true};
    }

    ReadResult FindFirstChild(
        std::size_t parent,
        Axis axis,
        std::uint32_t = 1u) noexcept
    {
        return ChildResult_(parent < NodeCount ? Axis_(parent, axis).First : NIL);
    }

    ReadResult FindLastChild(
        std::size_t parent,
        Axis axis,
        std::uint32_t = 1u) noexcept
    {
        return ChildResult_(parent < NodeCount ? Axis_(parent, axis).Last : NIL);
    }

    ReadResult FindNextChild(
        std::size_t parent,
        Axis axis,
        std::uint32_t locator,
        std::uint32_t = 1u) noexcept
    {
        if (parent >= NodeCount || locator >= NodeCount)
        {
            return {};
        }
        const AxisState& child = Axis_(locator, axis);
        if (child.Parent != parent)
        {
            return {};
        }
        return ChildResult_(child.Next);
    }

    ReadResult FindPreviousChild(
        std::size_t parent,
        Axis axis,
        std::uint32_t locator,
        std::uint32_t = 1u) noexcept
    {
        if (parent >= NodeCount || locator >= NodeCount)
        {
            return {};
        }
        const AxisState& child = Axis_(locator, axis);
        if (child.Parent != parent)
        {
            return {};
        }
        return ChildResult_(child.Previous);
    }

    bool StorePayload(
        std::size_t node,
        std::uint32_t word,
        std::uint64_t value,
        bool atomic) noexcept
    {
        if (node >= NodeCount || word >= PayloadWords)
        {
            return false;
        }
        if (atomic)
        {
            std::atomic_ref<std::uint64_t>(Nodes_[node].Payload[word]).store(
                value,
                std::memory_order_release
            );
        }
        else
        {
            Nodes_[node].Payload[word] = value;
        }
        return true;
    }

    bool LoadPayload(
        std::size_t node,
        std::uint32_t word,
        std::uint64_t& value,
        bool atomic) noexcept
    {
        if (node >= NodeCount || word >= PayloadWords)
        {
            return false;
        }
        value = atomic
            ? std::atomic_ref<std::uint64_t>(Nodes_[node].Payload[word]).load(
                std::memory_order_acquire
            )
            : Nodes_[node].Payload[word];
        return true;
    }

    std::size_t ApproxStorageBytes() const noexcept
    {
        return sizeof(Nodes_);
    }

private:
    std::array<Node, NodeCount> Nodes_{};
    std::mutex GraphMutex_{};

    AxisState& Axis_(std::size_t node, Axis axis) noexcept
    {
        return axis == Axis::HORIZONTAL ? Nodes_[node].H : Nodes_[node].V;
    }

    const AxisState& Axis_(std::size_t node, Axis axis) const noexcept
    {
        return axis == Axis::HORIZONTAL ? Nodes_[node].H : Nodes_[node].V;
    }

    static ReadResult ChildResult_(std::uint32_t child) noexcept
    {
        return child == NIL
            ? ReadResult{}
            : ReadResult{child, child, ReadOperation::FOUND, true};
    }

    bool AddUnlocked_(std::size_t parent, std::size_t child, Axis axis) noexcept
    {
        if (
            parent >= NodeCount ||
            child >= NodeCount ||
            parent >= child
        )
        {
            return false;
        }

        AxisState& child_axis = Axis_(child, axis);
        AxisState& parent_axis = Axis_(parent, axis);
        if (child_axis.Parent != NIL)
        {
            return false;
        }

        child_axis.Parent = static_cast<std::uint32_t>(parent);
        child_axis.Previous = parent_axis.Last;
        child_axis.Next = NIL;

        if (parent_axis.Last == NIL)
        {
            parent_axis.First = static_cast<std::uint32_t>(child);
        }
        else
        {
            Axis_(parent_axis.Last, axis).Next = static_cast<std::uint32_t>(child);
        }
        parent_axis.Last = static_cast<std::uint32_t>(child);
        return true;
    }

    bool RemoveUnlocked_(std::size_t parent, std::size_t child, Axis axis) noexcept
    {
        if (parent >= NodeCount || child >= NodeCount)
        {
            return false;
        }

        AxisState& child_axis = Axis_(child, axis);
        AxisState& parent_axis = Axis_(parent, axis);
        if (child_axis.Parent != parent)
        {
            return false;
        }

        if (child_axis.Previous == NIL)
        {
            parent_axis.First = child_axis.Next;
        }
        else
        {
            Axis_(child_axis.Previous, axis).Next = child_axis.Next;
        }

        if (child_axis.Next == NIL)
        {
            parent_axis.Last = child_axis.Previous;
        }
        else
        {
            Axis_(child_axis.Next, axis).Previous = child_axis.Previous;
        }

        child_axis = {};
        return true;
    }
};

// -----------------------------------------------------------------------------
// Public APC/Fabric adapter for the completed DAG API.
// -----------------------------------------------------------------------------

template <std::size_t NodeCount, std::size_t PayloadWords, std::uint8_t ParentCapacity>
class APCFabricBackend
{
public:
    using SD = SchemaDefinition;
    static constexpr std::uint32_t SLOT_WORDS = MINIMUM_APC_CELL_COUNT;
    static constexpr std::uint32_t FABRIC_SLOT_COUNT =
        static_cast<std::uint32_t>(NodeCount + 8u);
    static constexpr std::uint8_t PARENT_CAPACITY = ParentCapacity;

    bool Initialize() noexcept
    {
        Slots_.fill(APCDataStructure::APC_INDEX_BOUND_SENTINAL);

        if (!Fabric_.InitializeFabricWithPtrTable(
            FABRIC_SLOT_COUNT,
            SLOT_WORDS,
            ParentCapacity
        ))
        {
            return false;
        }

        LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier layout{};
        layout.FeedForward = 1u;
        layout.FeedBackward = 1u;
        layout.Lateral = 0u;
        layout.StateSlot = 0u;
        layout.ErrorSlot = 0u;
        layout.Weightless = 0u;
        layout.WeightSlot = 0u;
        layout.AUXSlot = 0u;
        layout.HeterogenousPtr = 0u;
        layout.FreeSlot = 0u;

        SD::InitialRegionalDtypeConf dtype{};
        dtype.FEEDFORWARD_MESSAGE = SD::DataTypeOfMacroColumn::UINT64_T;
        dtype.FEEDBACKWARD_MESSAGE = SD::DataTypeOfMacroColumn::UINT64_T;

        SD::InitialRegionalProtocol protocol{};
        protocol.FEEDFORWARD_MESSAGE = SD::SchemaProtocols::PRIVATE_REGION;
        protocol.FEEDBACKWARD_MESSAGE = SD::SchemaProtocols::ATOMIC_WORD_ARRAY;

        for (std::size_t i = 0u; i < NodeCount; ++i)
        {
            if (!Fabric_.CreateAPC(
                Nodes_[i],
                layout,
                dtype,
                protocol,
                APCDataStructure::BRANCH_VERSION
            ))
            {
                return false;
            }

            const std::uint32_t slot = Nodes_[i].GetThisSlotIdx();
            if (slot != i)
            {
                return false;
            }
            Slots_[i] = slot;

            if constexpr (PayloadWords > 0u)
            {
                auto direct = Nodes_[i].template BuildAViewOverRegion<std::uint64_t>(
                    MacroColumnOfAPC::FEEDFORWARD_MESSAGE
                );
                auto atomic = Nodes_[i].template BuildAViewOverRegion<std::uint64_t>(
                    MacroColumnOfAPC::FEEDBACKWARD_MESSAGE
                );

                if (
                    !direct.has_value() ||
                    !atomic.has_value() ||
                    direct->Size() < PayloadWords ||
                    atomic->Size() < PayloadWords ||
                    !direct->RawMutableSpan().has_value()
                )
                {
                    return false;
                }

                DirectViews_[i] = std::move(direct.value());
                AtomicViews_[i] = std::move(atomic.value());
            }
        }
        return true;
    }

    bool AddParent(
        std::size_t parent,
        std::size_t child,
        Axis axis,
        std::uint32_t max_tries = DEFAULT_MAX_TRIES) noexcept
    {
        return parent < NodeCount && child < NodeCount &&
            Nodes_[child].AddParent(
                Nodes_[parent],
                EdgeTableForAxis(axis),
                max_tries
            );
    }

    bool RemoveParent(
        std::size_t parent,
        std::size_t child,
        Axis axis,
        std::uint32_t max_tries = DEFAULT_MAX_TRIES) noexcept
    {
        return parent < NodeCount && child < NodeCount &&
            Nodes_[child].RemoveParent(
                Nodes_[parent],
                EdgeTableForAxis(axis),
                max_tries
            );
    }

    bool ReplaceParent(
        std::size_t old_parent,
        std::size_t new_parent,
        std::size_t child,
        Axis axis,
        std::uint32_t max_tries = DEFAULT_MAX_TRIES) noexcept
    {
        return old_parent < NodeCount && new_parent < NodeCount && child < NodeCount &&
            Nodes_[child].ReplaceParent(
                Nodes_[old_parent],
                Nodes_[new_parent],
                EdgeTableForAxis(axis),
                max_tries
            );
    }

    ReadResult FindParent(
        std::size_t child,
        Axis axis,
        std::uint8_t ordinal,
        std::uint32_t max_tries = 1u) noexcept
    {
        if (child >= NodeCount)
        {
            return {};
        }
        return Convert_(Nodes_[child].FindParent(
            EdgeTableForAxis(axis),
            ordinal,
            max_tries
        ));
    }

    ReadResult FindFirstChild(
        std::size_t parent,
        Axis axis,
        std::uint32_t max_tries = 1u) noexcept
    {
        return parent < NodeCount
            ? Convert_(Nodes_[parent].FindFirstChild(EdgeTableForAxis(axis), max_tries))
            : ReadResult{};
    }

    ReadResult FindLastChild(
        std::size_t parent,
        Axis axis,
        std::uint32_t max_tries = 1u) noexcept
    {
        return parent < NodeCount
            ? Convert_(Nodes_[parent].FindLastChild(EdgeTableForAxis(axis), max_tries))
            : ReadResult{};
    }

    ReadResult FindNextChild(
        std::size_t parent,
        Axis axis,
        std::uint32_t locator,
        std::uint32_t max_tries = 1u) noexcept
    {
        return parent < NodeCount
            ? Convert_(Nodes_[parent].FindNextChild(
                EdgeTableForAxis(axis),
                locator,
                max_tries
            ))
            : ReadResult{};
    }

    ReadResult FindPreviousChild(
        std::size_t parent,
        Axis axis,
        std::uint32_t locator,
        std::uint32_t max_tries = 1u) noexcept
    {
        return parent < NodeCount
            ? Convert_(Nodes_[parent].FindPreviousChild(
                EdgeTableForAxis(axis),
                locator,
                max_tries
            ))
            : ReadResult{};
    }

    bool StorePayload(
        std::size_t node,
        std::uint32_t word,
        std::uint64_t value,
        bool atomic) noexcept
    {
        if constexpr (PayloadWords == 0u)
        {
            (void)node; (void)word; (void)value; (void)atomic;
            return false;
        }
        else
        {
            if (node >= NodeCount || word >= PayloadWords)
            {
                return false;
            }
            if (atomic)
            {
                return AtomicViews_[node].AtomicStore(
                    word,
                    value,
                    std::memory_order_release
                );
            }
            auto span = DirectViews_[node].RawMutableSpan();
            if (!span.has_value())
            {
                return false;
            }
            span.value()[word] = value;
            return true;
        }
    }

    bool LoadPayload(
        std::size_t node,
        std::uint32_t word,
        std::uint64_t& value,
        bool atomic) noexcept
    {
        if constexpr (PayloadWords == 0u)
        {
            (void)node; (void)word; (void)value; (void)atomic;
            return false;
        }
        else
        {
            if (node >= NodeCount || word >= PayloadWords)
            {
                return false;
            }
            if (atomic)
            {
                value = AtomicViews_[node].AtomicLoad(
                    word,
                    std::memory_order_acquire
                );
                return true;
            }
            auto span = DirectViews_[node].RawMutableSpan();
            if (!span.has_value())
            {
                return false;
            }
            value = span.value()[word];
            return true;
        }
    }

    AdaptivePackedCellContainer& Node(std::size_t index) noexcept
    {
        return Nodes_[index];
    }

    VagueTemoraryPremativeFabric Fabric_{};

private:
    std::array<AdaptivePackedCellContainer, NodeCount> Nodes_{};
    std::array<std::uint32_t, NodeCount> Slots_{};
    std::array<RegionView<std::uint64_t>, NodeCount> DirectViews_{};
    std::array<RegionView<std::uint64_t>, NodeCount> AtomicViews_{};

    std::size_t IndexOf_(AdaptivePackedCellContainer* ptr) const noexcept
    {
        if (!ptr)
        {
            return ReadResult::NO_NODE;
        }
        const AdaptivePackedCellContainer* first = Nodes_.data();
        const AdaptivePackedCellContainer* last = first + Nodes_.size();
        return ptr >= first && ptr < last
            ? static_cast<std::size_t>(ptr - first)
            : ReadResult::NO_NODE;
    }

    template <typename Operation>
    ReadResult Convert_(const Operation& operation) const noexcept
    {
        return ReadResult{
            IndexOf_(operation.APCPtr_),
            operation.RelationLocator_,
            operation.MutationOP_,
            operation.APCPtr_ != nullptr
        };
    }
};

// -----------------------------------------------------------------------------
// Exhaustive quiescent validator. It rebuilds H, V and H-union-V only through
// public reads, verifies both directions, then performs a full topological sort.
// -----------------------------------------------------------------------------

struct GraphProof
{
    bool ReadContracts = true;
    bool ParentOrder = true;
    bool NoDuplicates = true;
    bool ReverseLists = true;
    bool CombinedAcyclic = true;

    bool Passed() const noexcept
    {
        return ReadContracts && ParentOrder && NoDuplicates &&
            ReverseLists && CombinedAcyclic;
    }
};

template <std::size_t NodeCount, std::uint8_t ParentCapacity, typename Backend>
GraphProof ProveQuiescentCombinedDAG(Backend& backend)
{
    GraphProof proof{};
    std::array<std::array<std::array<bool, NodeCount>, NodeCount>, 2u> edges{};

    for (std::size_t axis_index = 0u; axis_index < 2u; ++axis_index)
    {
        const Axis axis = axis_index == 0u ? Axis::HORIZONTAL : Axis::VERTICAL;

        for (std::size_t child = 0u; child < NodeCount; ++child)
        {
            for (std::uint8_t ordinal = 0u; ordinal < ParentCapacity; ++ordinal)
            {
                const ReadResult read = backend.FindParent(child, axis, ordinal, DEFAULT_MAX_TRIES);
                proof.ReadContracts = proof.ReadContracts && read.ContractValid();
                if (read.IsRetry())
                {
                    proof.ReadContracts = false;
                    continue;
                }
                if (!read.IsFound())
                {
                    continue;
                }

                if (read.Node >= child)
                {
                    proof.ParentOrder = false;
                }
                if (read.Node >= NodeCount || edges[axis_index][read.Node][child])
                {
                    proof.NoDuplicates = false;
                    continue;
                }
                edges[axis_index][read.Node][child] = true;
            }
        }

        for (std::size_t parent = 0u; parent < NodeCount; ++parent)
        {
            std::array<bool, NodeCount> enumerated{};
            ReadResult read = backend.FindFirstChild(parent, axis, DEFAULT_MAX_TRIES);
            proof.ReadContracts = proof.ReadContracts && read.ContractValid();

            std::size_t steps = 0u;
            while (read.IsFound())
            {
                if (
                    read.Node >= NodeCount ||
                    enumerated[read.Node] ||
                    !edges[axis_index][parent][read.Node]
                )
                {
                    proof.ReverseLists = false;
                    break;
                }

                enumerated[read.Node] = true;
                if (++steps > NodeCount)
                {
                    proof.ReverseLists = false;
                    break;
                }

                read = backend.FindNextChild(
                    parent,
                    axis,
                    read.Locator,
                    DEFAULT_MAX_TRIES
                );
                proof.ReadContracts = proof.ReadContracts && read.ContractValid();
            }

            if (read.IsRetry())
            {
                proof.ReadContracts = false;
            }

            for (std::size_t child = 0u; child < NodeCount; ++child)
            {
                if (edges[axis_index][parent][child] != enumerated[child])
                {
                    proof.ReverseLists = false;
                }
            }
        }
    }

    std::array<std::uint32_t, NodeCount> indegree{};
    for (std::size_t parent = 0u; parent < NodeCount; ++parent)
    {
        for (std::size_t child = 0u; child < NodeCount; ++child)
        {
            if (edges[0u][parent][child] || edges[1u][parent][child])
            {
                ++indegree[child];
            }
        }
    }

    std::array<std::size_t, NodeCount> queue{};
    std::size_t head = 0u;
    std::size_t tail = 0u;
    for (std::size_t node = 0u; node < NodeCount; ++node)
    {
        if (indegree[node] == 0u)
        {
            queue[tail++] = node;
        }
    }

    std::size_t visited = 0u;
    while (head < tail)
    {
        const std::size_t parent = queue[head++];
        ++visited;
        for (std::size_t child = 0u; child < NodeCount; ++child)
        {
            if (
                (edges[0u][parent][child] || edges[1u][parent][child]) &&
                --indegree[child] == 0u
            )
            {
                queue[tail++] = child;
            }
        }
    }
    proof.CombinedAcyclic = visited == NodeCount;
    return proof;
}

template <typename Backend>
bool RetryReplace(
    Backend& backend,
    std::size_t old_parent,
    std::size_t new_parent,
    std::size_t child,
    Axis axis,
    std::uint64_t& retry_count,
    std::uint32_t attempt_limit = 100'000u) noexcept
{
    for (std::uint32_t attempt = 0u; attempt < attempt_limit; ++attempt)
    {
        if (backend.ReplaceParent(old_parent, new_parent, child, axis, 1u))
        {
            return true;
        }
        ++retry_count;
        PerturbSchedule(attempt);
    }
    return false;
}

// -----------------------------------------------------------------------------
// Test 1: original nine-row baseline, translated to explicit DAG operations.
// -----------------------------------------------------------------------------

namespace Test01_Baseline
{
constexpr std::size_t MAIN_V_PARENT = 0u;
constexpr std::size_t AUX_V_PARENT = 1u;
constexpr std::size_t CHAIN_BEGIN = 2u;
constexpr std::size_t CHAIN_LENGTH = 64u;
constexpr std::size_t CHAIN_END = CHAIN_BEGIN + CHAIN_LENGTH - 1u;
constexpr std::size_t AUX_ANCHOR = CHAIN_END + 1u;
constexpr std::size_t NODE_COUNT = AUX_ANCHOR + 1u;
constexpr std::size_t PAYLOAD_WORDS = 32u;
constexpr std::uint8_t PARENT_CAPACITY = 4u;
constexpr std::uint32_t TRAVERSAL_ROUNDS = 4'000u;
constexpr std::uint32_t PAYLOAD_ROUNDS = 100u;
constexpr std::uint32_t GRAPH_PAYLOAD_ROUNDS = 1'000u;
constexpr std::uint32_t MUTATION_ROUNDS = 512u;
constexpr std::uint32_t MEASURED_RUNS = 5u;

using VectorBackend = VectorLockedForest<NODE_COUNT, PAYLOAD_WORDS>;
using APCBackend = APCFabricBackend<NODE_COUNT, PAYLOAD_WORDS, PARENT_CAPACITY>;

constexpr std::size_t Reverse6(std::size_t value) noexcept
{
    std::size_t result = 0u;
    for (std::size_t bit = 0u; bit < 6u; ++bit)
    {
        result = (result << 1u) | ((value >> bit) & 1u);
    }
    return result;
}

constexpr std::array<std::size_t, CHAIN_LENGTH> MakeVerticalOrder() noexcept
{
    std::array<std::size_t, CHAIN_LENGTH> order{};
    for (std::size_t i = 0u; i < CHAIN_LENGTH; ++i)
    {
        order[i] = CHAIN_BEGIN + Reverse6(i);
    }
    return order;
}

constexpr auto VERTICAL_ORDER = MakeVerticalOrder();

template <typename Backend>
bool Build(Backend& backend)
{
    if (!backend.Initialize())
    {
        return false;
    }

    for (std::size_t child = CHAIN_BEGIN + 1u; child <= CHAIN_END; ++child)
    {
        if (!backend.AddParent(child - 1u, child, Axis::HORIZONTAL))
        {
            return false;
        }
    }
    for (std::size_t child : VERTICAL_ORDER)
    {
        if (!backend.AddParent(MAIN_V_PARENT, child, Axis::VERTICAL))
        {
            return false;
        }
    }
    if (!backend.AddParent(AUX_V_PARENT, AUX_ANCHOR, Axis::VERTICAL))
    {
        return false;
    }

    for (std::size_t node = 0u; node < NODE_COUNT; ++node)
    {
        for (std::uint32_t word = 0u; word < PAYLOAD_WORDS; ++word)
        {
            const std::uint64_t value =
                (static_cast<std::uint64_t>(node + 1u) << 32u) | word;
            if (
                !backend.StorePayload(node, word, value, false) ||
                !backend.StorePayload(node, word, value, true)
            )
            {
                return false;
            }
        }
    }
    return true;
}

struct Timing
{
    bool Ok = false;
    std::uint64_t Checksum = 0u;
    std::uint64_t Operations = 0u;
    std::int64_t ElapsedNs = 0;
    ReadCounts Reads{};

    double NsPerOperation() const noexcept
    {
        return Operations == 0u
            ? 0.0
            : static_cast<double>(ElapsedNs) / static_cast<double>(Operations);
    }
};

template <typename Backend>
Timing HorizontalForward(Backend& backend)
{
    Timing timing{};
    const auto begin = Clock::now();
    for (std::uint32_t round = 0u; round < TRAVERSAL_ROUNDS; ++round)
    {
        for (std::size_t parent = CHAIN_BEGIN; parent < CHAIN_END; ++parent)
        {
            const ReadResult read = backend.FindFirstChild(parent, Axis::HORIZONTAL);
            timing.Reads.Observe(read);
            if (!read.IsFound() || read.Node != parent + 1u)
            {
                return {};
            }
            timing.Checksum += read.Node + 1u;
        }
    }
    timing.ElapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now() - begin
    ).count();
    timing.Operations = static_cast<std::uint64_t>(TRAVERSAL_ROUNDS) *
        (CHAIN_LENGTH - 1u);
    timing.Ok = true;
    return timing;
}

template <typename Backend>
Timing HorizontalBackward(Backend& backend)
{
    Timing timing{};
    const auto begin = Clock::now();
    for (std::uint32_t round = 0u; round < TRAVERSAL_ROUNDS; ++round)
    {
        for (std::size_t child = CHAIN_END; child > CHAIN_BEGIN; --child)
        {
            const ReadResult read = backend.FindParent(child, Axis::HORIZONTAL, 0u);
            timing.Reads.Observe(read);
            if (!read.IsFound() || read.Node != child - 1u)
            {
                return {};
            }
            timing.Checksum += read.Node + 1u;
        }
    }
    timing.ElapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now() - begin
    ).count();
    timing.Operations = static_cast<std::uint64_t>(TRAVERSAL_ROUNDS) *
        (CHAIN_LENGTH - 1u);
    timing.Ok = true;
    return timing;
}

template <typename Backend>
Timing VerticalForward(Backend& backend, bool with_payload)
{
    Timing timing{};
    const std::uint32_t rounds = with_payload
        ? GRAPH_PAYLOAD_ROUNDS
        : TRAVERSAL_ROUNDS;
    const auto begin = Clock::now();

    for (std::uint32_t round = 0u; round < rounds; ++round)
    {
        ReadResult read = backend.FindFirstChild(MAIN_V_PARENT, Axis::VERTICAL);
        timing.Reads.Observe(read);
        for (std::size_t i = 0u; i < CHAIN_LENGTH; ++i)
        {
            if (!read.IsFound() || read.Node != VERTICAL_ORDER[i])
            {
                return {};
            }

            if (with_payload)
            {
                std::uint64_t value = 0u;
                if (!backend.LoadPayload(
                    read.Node,
                    static_cast<std::uint32_t>(i % PAYLOAD_WORDS),
                    value,
                    false
                ))
                {
                    return {};
                }
                timing.Checksum += value;
            }
            else
            {
                timing.Checksum += read.Node + 1u;
            }

            const std::uint32_t cursor = read.Locator;
            read = backend.FindNextChild(MAIN_V_PARENT, Axis::VERTICAL, cursor);
            timing.Reads.Observe(read);
        }
        if (!read.IsNone())
        {
            return {};
        }
    }

    timing.ElapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now() - begin
    ).count();
    timing.Operations = static_cast<std::uint64_t>(rounds) * CHAIN_LENGTH;
    timing.Ok = true;
    return timing;
}

template <typename Backend>
Timing VerticalBackward(Backend& backend)
{
    Timing timing{};
    const auto begin = Clock::now();
    for (std::uint32_t round = 0u; round < TRAVERSAL_ROUNDS; ++round)
    {
        ReadResult read = backend.FindLastChild(MAIN_V_PARENT, Axis::VERTICAL);
        timing.Reads.Observe(read);
        for (std::size_t i = CHAIN_LENGTH; i-- > 0u;)
        {
            if (!read.IsFound() || read.Node != VERTICAL_ORDER[i])
            {
                return {};
            }
            timing.Checksum += read.Node + 1u;
            const std::uint32_t cursor = read.Locator;
            read = backend.FindPreviousChild(MAIN_V_PARENT, Axis::VERTICAL, cursor);
            timing.Reads.Observe(read);
        }
        if (!read.IsNone())
        {
            return {};
        }
    }
    timing.ElapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now() - begin
    ).count();
    timing.Operations = static_cast<std::uint64_t>(TRAVERSAL_ROUNDS) * CHAIN_LENGTH;
    timing.Ok = true;
    return timing;
}

template <typename Backend>
Timing PayloadRead(Backend& backend, bool atomic)
{
    Timing timing{};
    const auto begin = Clock::now();
    for (std::uint32_t round = 0u; round < PAYLOAD_ROUNDS; ++round)
    {
        for (std::size_t node = CHAIN_BEGIN; node <= CHAIN_END; ++node)
        {
            for (std::uint32_t word = 0u; word < PAYLOAD_WORDS; ++word)
            {
                std::uint64_t value = 0u;
                if (!backend.LoadPayload(node, word, value, atomic))
                {
                    return {};
                }
                timing.Checksum += value;
            }
        }
    }
    timing.ElapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now() - begin
    ).count();
    timing.Operations = static_cast<std::uint64_t>(PAYLOAD_ROUNDS) *
        CHAIN_LENGTH * PAYLOAD_WORDS;
    timing.Ok = true;
    return timing;
}

template <typename Backend>
Timing ParentReplacement(Backend& backend, Axis axis)
{
    const std::size_t child = CHAIN_END;
    const std::size_t parent_a = axis == Axis::HORIZONTAL
        ? CHAIN_END - 1u
        : MAIN_V_PARENT;
    const std::size_t parent_b = axis == Axis::HORIZONTAL
        ? CHAIN_END - 2u
        : AUX_V_PARENT;

    Timing timing{};
    std::size_t current = parent_a;
    const auto begin = Clock::now();
    for (std::uint32_t i = 0u; i < MUTATION_ROUNDS; ++i)
    {
        const std::size_t next = current == parent_a ? parent_b : parent_a;
        if (!backend.ReplaceParent(current, next, child, axis))
        {
            return {};
        }
        current = next;
        ++timing.Operations;
    }
    timing.ElapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now() - begin
    ).count();
    timing.Checksum = timing.Operations;
    timing.Ok = current == parent_a;
    return timing;
}

enum class Metric : std::uint8_t
{
    H_FORWARD,
    H_BACKWARD,
    V_FORWARD,
    V_BACKWARD,
    PAYLOAD_DIRECT,
    PAYLOAD_ATOMIC,
    GRAPH_PAYLOAD,
    REPLACE_H_PARENT,
    REPLACE_V_PARENT,
    COUNT
};

constexpr std::size_t METRIC_COUNT = static_cast<std::size_t>(Metric::COUNT);

constexpr const char* MetricName(Metric metric) noexcept
{
    switch (metric)
    {
    case Metric::H_FORWARD: return "H forward sequential";
    case Metric::H_BACKWARD: return "H backward sequential";
    case Metric::V_FORWARD: return "V forward scrambled";
    case Metric::V_BACKWARD: return "V backward scrambled";
    case Metric::PAYLOAD_DIRECT: return "payload direct read";
    case Metric::PAYLOAD_ATOMIC: return "payload atomic read";
    case Metric::GRAPH_PAYLOAD: return "scrambled graph+payload";
    case Metric::REPLACE_H_PARENT: return "atomic H parent replace";
    case Metric::REPLACE_V_PARENT: return "atomic V parent replace";
    default: return "unknown";
    }
}

template <typename Backend>
Timing RunMetric(Backend& backend, Metric metric)
{
    switch (metric)
    {
    case Metric::H_FORWARD: return HorizontalForward(backend);
    case Metric::H_BACKWARD: return HorizontalBackward(backend);
    case Metric::V_FORWARD: return VerticalForward(backend, false);
    case Metric::V_BACKWARD: return VerticalBackward(backend);
    case Metric::PAYLOAD_DIRECT: return PayloadRead(backend, false);
    case Metric::PAYLOAD_ATOMIC: return PayloadRead(backend, true);
    case Metric::GRAPH_PAYLOAD: return VerticalForward(backend, true);
    case Metric::REPLACE_H_PARENT: return ParentReplacement(backend, Axis::HORIZONTAL);
    case Metric::REPLACE_V_PARENT: return ParentReplacement(backend, Axis::VERTICAL);
    default: return {};
    }
}

inline Result Run()
{
    Banner("TEST 1 - DAG TRAVERSAL / PAYLOAD / ATOMIC-PARENT-REPLACE BASELINE");

    const auto vector_build_begin = Clock::now();
    VectorBackend vector_backend{};
    if (!Build(vector_backend)) return Result::FAIL;
    const auto vector_build_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now() - vector_build_begin
    ).count();

    const auto apc_build_begin = Clock::now();
    APCBackend apc_backend{};
    if (!Build(apc_backend)) return Result::FAIL;
    const auto apc_build_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now() - apc_build_begin
    ).count();

    const GraphProof initial_proof =
        ProveQuiescentCombinedDAG<NODE_COUNT, PARENT_CAPACITY>(apc_backend);
    if (!initial_proof.Passed())
    {
        return Result::FAIL;
    }

    std::array<std::array<double, MEASURED_RUNS>, METRIC_COUNT> vector_samples{};
    std::array<std::array<double, MEASURED_RUNS>, METRIC_COUNT> apc_samples{};
    ReadCounts vector_reads{};
    ReadCounts apc_reads{};

    for (std::uint32_t run = 0u; run < MEASURED_RUNS; ++run)
    {
        for (std::size_t index = 0u; index < METRIC_COUNT; ++index)
        {
            const Metric metric = static_cast<Metric>(index);
            Timing vector_timing{};
            Timing apc_timing{};

            if ((run & 1u) == 0u)
            {
                vector_timing = RunMetric(vector_backend, metric);
                apc_timing = RunMetric(apc_backend, metric);
            }
            else
            {
                apc_timing = RunMetric(apc_backend, metric);
                vector_timing = RunMetric(vector_backend, metric);
            }

            if (
                !vector_timing.Ok ||
                !apc_timing.Ok ||
                vector_timing.Checksum != apc_timing.Checksum
            )
            {
                std::cout << "  failed metric: " << MetricName(metric) << '\n';
                return Result::FAIL;
            }

            vector_samples[index][run] = vector_timing.NsPerOperation();
            apc_samples[index][run] = apc_timing.NsPerOperation();
            vector_reads.Add(vector_timing.Reads);
            apc_reads.Add(apc_timing.Reads);
        }
    }

    const GraphProof final_proof =
        ProveQuiescentCombinedDAG<NODE_COUNT, PARENT_CAPACITY>(apc_backend);

    std::cout
        << "Correctness: explicit parent reads, cursor child traversal, and H-union-V proof: "
        << (final_proof.Passed() ? "PASS" : "FAIL") << "\n\n"
        << "CONSTRUCTION\n"
        << "  vector+locked forest: " << vector_build_ns / 1000 << " us\n"
        << "  APC+Fabric DAG       : " << apc_build_ns / 1000 << " us\n\n"
        << "MEDIAN COST PER OPERATION\n";

    for (std::size_t index = 0u; index < METRIC_COUNT; ++index)
    {
        const double vector_ns = Median(vector_samples[index]);
        const double apc_ns = Median(apc_samples[index]);
        std::cout
            << std::left << std::setw(29) << MetricName(static_cast<Metric>(index))
            << " vector-base=" << std::right << std::setw(10)
            << std::fixed << std::setprecision(2) << vector_ns << " ns/op"
            << "  APC=" << std::setw(10) << apc_ns << " ns/op"
            << "  ratio=" << std::setw(8) << Ratio(apc_ns, vector_ns) << "x\n";
    }

    std::cout << "\nPUBLIC READ OUTCOMES\n";
    PrintReadCounts("vector locked forest", vector_reads);
    PrintReadCounts("APC/Fabric DAG", apc_reads);

    const bool ok = final_proof.Passed() &&
        vector_reads.BadContract == 0u &&
        apc_reads.BadContract == 0u &&
        apc_reads.Retry == 0u;

    std::cout << "\nTEST 1 OVERALL: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok ? Result::PASS : Result::FAIL;
}
} // namespace Test01_Baseline

// -----------------------------------------------------------------------------
// Test 2: shared-parent contention sweep against the global-mutex forest.
// -----------------------------------------------------------------------------

namespace Test02_Contention
{
constexpr std::size_t NODE_COUNT = 40u;
constexpr std::uint8_t K = 4u;
constexpr std::size_t FIRST_CHILD = 8u;
constexpr std::uint32_t OPS_PER_THREAD = 2'000u;

template <typename Backend>
bool Build(Backend& backend, std::size_t workers)
{
    if (!backend.Initialize()) return false;
    for (std::size_t i = 0u; i < workers; ++i)
    {
        const std::size_t child = FIRST_CHILD + i;
        if (
            !backend.AddParent(0u, child, Axis::HORIZONTAL) ||
            !backend.AddParent(2u, child, Axis::VERTICAL)
        )
        {
            return false;
        }
    }
    return true;
}

struct SweepResult
{
    bool Ok = false;
    double NsPerSuccess = 0.0;
    std::uint64_t Success = 0u;
    std::uint64_t Retries = 0u;
};

template <typename Backend>
SweepResult RunWorkers(Backend& backend, std::size_t workers)
{
    std::barrier start(static_cast<std::ptrdiff_t>(workers + 1u));
    std::atomic<bool> failed{false};
    std::atomic<std::uint64_t> success{0u};
    std::atomic<std::uint64_t> retries{0u};
    std::vector<std::thread> threads;
    threads.reserve(workers);

    for (std::size_t worker = 0u; worker < workers; ++worker)
    {
        threads.emplace_back([&, worker]() noexcept
        {
            const std::size_t child = FIRST_CHILD + worker;
            std::size_t h_current = 0u;
            std::size_t v_current = 2u;
            std::uint64_t local_retries = 0u;
            std::uint64_t local_success = 0u;
            start.arrive_and_wait();

            for (std::uint32_t i = 0u; i < OPS_PER_THREAD; ++i)
            {
                const std::size_t h_next = h_current == 0u ? 1u : 0u;
                const std::size_t v_next = v_current == 2u ? 3u : 2u;
                if (
                    !RetryReplace(
                        backend, h_current, h_next, child,
                        Axis::HORIZONTAL, local_retries
                    ) ||
                    !RetryReplace(
                        backend, v_current, v_next, child,
                        Axis::VERTICAL, local_retries
                    )
                )
                {
                    failed.store(true, std::memory_order_release);
                    break;
                }
                h_current = h_next;
                v_current = v_next;
                local_success += 2u;
            }

            success.fetch_add(local_success, std::memory_order_relaxed);
            retries.fetch_add(local_retries, std::memory_order_relaxed);
        });
    }

    const auto begin = Clock::now();
    start.arrive_and_wait();
    for (std::thread& thread : threads) thread.join();
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now() - begin
    ).count();

    const std::uint64_t completed = success.load(std::memory_order_acquire);
    return {
        !failed.load(std::memory_order_acquire) &&
            completed == workers * OPS_PER_THREAD * 2u,
        completed == 0u ? 0.0 : static_cast<double>(elapsed) / completed,
        completed,
        retries.load(std::memory_order_acquire)
    };
}

inline Result Run()
{
    Banner("TEST 2 - GLOBAL-MUTEX VECTOR FOREST vs APC/FABRIC DAG CONTENTION");
    constexpr std::array<std::size_t, 4u> WORKERS{1u, 2u, 4u, 8u};
    bool all_ok = true;

    std::cout
        << "Each worker owns one child; all workers contend on the same two H and two V parents.\n"
        << "APC retries one-attempt transactions at workload level; vector serializes each move.\n\n";

    for (std::size_t workers : WORKERS)
    {
        VectorLockedForest<NODE_COUNT, 1u> vector_backend{};
        APCFabricBackend<NODE_COUNT, 1u, K> apc_backend{};
        if (!Build(vector_backend, workers) || !Build(apc_backend, workers))
        {
            return Result::FAIL;
        }

        const SweepResult vector_result = RunWorkers(vector_backend, workers);
        const SweepResult apc_result = RunWorkers(apc_backend, workers);
        const GraphProof proof = ProveQuiescentCombinedDAG<NODE_COUNT, K>(apc_backend);
        const bool row_ok = vector_result.Ok && apc_result.Ok && proof.Passed();
        all_ok = all_ok && row_ok;

        std::cout
            << "  threads=" << std::setw(2) << workers
            << " vector=" << std::setw(10) << std::fixed << std::setprecision(2)
            << vector_result.NsPerSuccess << " ns/op"
            << " APC=" << std::setw(10) << apc_result.NsPerSuccess << " ns/op"
            << " APC-retries=" << std::setw(10) << apc_result.Retries
            << " integrity=" << (row_ok ? "PASS" : "FAIL") << '\n';
    }

    std::cout << "\nTEST 2 OVERALL: " << (all_ok ? "PASS" : "FAIL") << '\n';
    return all_ok ? Result::PASS : Result::FAIL;
}
} // namespace Test02_Contention

// -----------------------------------------------------------------------------
// Test 3: public parent reader versus atomic ReplaceParent writer.
// -----------------------------------------------------------------------------

namespace Test03_ReaderWriter
{
inline Result Run()
{
    Banner("TEST 3 - PUBLIC PARENT READERS vs ATOMIC CROSS-PARENT WRITER");

    constexpr std::size_t N = 4u;
    constexpr std::uint8_t K = 2u;
    constexpr std::size_t CHILD = 3u;
    constexpr std::uint32_t WRITES = 50'000u;
    constexpr std::uint32_t READS = 80'000u;
    constexpr std::size_t READER_COUNT = 4u;

    APCFabricBackend<N, 1u, K> backend{};
    if (!backend.Initialize() || !backend.AddParent(0u, CHILD, Axis::HORIZONTAL))
    {
        return Result::FAIL;
    }

    std::barrier start(static_cast<std::ptrdiff_t>(READER_COUNT + 2u));
    std::atomic<bool> failed{false};
    std::atomic<std::uint64_t> found_a{0u};
    std::atomic<std::uint64_t> found_b{0u};
    std::atomic<std::uint64_t> retry{0u};
    std::atomic<std::uint64_t> none{0u};
    std::atomic<std::uint64_t> writer_retries{0u};

    std::thread writer([&]() noexcept
    {
        std::size_t current = 0u;
        std::uint64_t local_retries = 0u;
        start.arrive_and_wait();
        for (std::uint32_t i = 0u; i < WRITES; ++i)
        {
            const std::size_t next = current == 0u ? 1u : 0u;
            if (!RetryReplace(
                backend, current, next, CHILD,
                Axis::HORIZONTAL, local_retries
            ))
            {
                failed.store(true, std::memory_order_release);
                break;
            }
            current = next;
        }
        writer_retries.store(local_retries, std::memory_order_release);
    });

    std::vector<std::thread> readers;
    readers.reserve(READER_COUNT);
    for (std::size_t reader_index = 0u; reader_index < READER_COUNT; ++reader_index)
    {
        readers.emplace_back([&, reader_index]() noexcept
        {
            std::uint64_t local_a = 0u;
            std::uint64_t local_b = 0u;
            std::uint64_t local_retry = 0u;
            std::uint64_t local_none = 0u;
            start.arrive_and_wait();
            for (std::uint32_t i = 0u; i < READS; ++i)
            {
                const ReadResult read = backend.FindParent(
                    CHILD,
                    Axis::HORIZONTAL,
                    0u,
                    1u
                );
                if (!read.ContractValid())
                {
                    failed.store(true, std::memory_order_release);
                    break;
                }
                if (read.IsRetry()) ++local_retry;
                else if (read.IsNone()) ++local_none;
                else if (read.Node == 0u) ++local_a;
                else if (read.Node == 1u) ++local_b;
                else
                {
                    failed.store(true, std::memory_order_release);
                    break;
                }
                PerturbSchedule(i + reader_index);
            }
            found_a.fetch_add(local_a, std::memory_order_relaxed);
            found_b.fetch_add(local_b, std::memory_order_relaxed);
            retry.fetch_add(local_retry, std::memory_order_relaxed);
            none.fetch_add(local_none, std::memory_order_relaxed);
        });
    }

    start.arrive_and_wait();
    writer.join();
    for (std::thread& reader : readers) reader.join();

    const GraphProof proof = ProveQuiescentCombinedDAG<N, K>(backend);
    const bool ok = !failed.load(std::memory_order_acquire) &&
        none.load(std::memory_order_acquire) == 0u &&
        proof.Passed();

    std::cout
        << "  parent A observations : " << found_a.load() << '\n'
        << "  parent B observations : " << found_b.load() << '\n'
        << "  reader RETRY          : " << retry.load() << '\n'
        << "  reader NONE (illegal) : " << none.load() << '\n'
        << "  writer retries        : " << writer_retries.load() << '\n'
        << "\nTEST 3 OVERALL: " << (ok ? "PASS" : "FAIL") << '\n';

    return ok ? Result::PASS : Result::FAIL;
}
} // namespace Test03_ReaderWriter

// -----------------------------------------------------------------------------
// Test 4: API symmetry, multi-parent isolation, duplicate/full-row rejection.
// -----------------------------------------------------------------------------

namespace Test04_PublicMutationAPI
{
inline bool ParentSetEquals(
    APCFabricBackend<8u, 1u, 2u>& backend,
    std::size_t child,
    Axis axis,
    std::array<std::size_t, 2u> expected,
    std::size_t expected_count)
{
    std::array<bool, 8u> found{};
    std::size_t count = 0u;
    for (std::uint8_t ordinal = 0u; ordinal < 2u; ++ordinal)
    {
        const ReadResult read = backend.FindParent(child, axis, ordinal, DEFAULT_MAX_TRIES);
        if (!read.ContractValid() || read.IsRetry()) return false;
        if (read.IsFound())
        {
            if (read.Node >= found.size() || found[read.Node]) return false;
            found[read.Node] = true;
            ++count;
        }
    }
    if (count != expected_count) return false;
    for (std::size_t i = 0u; i < expected_count; ++i)
    {
        if (!found[expected[i]]) return false;
    }
    return true;
}

inline Result Run()
{
    Banner("TEST 4 - PUBLIC DAG MUTATION API PAIRS AND MULTI-PARENT ISOLATION");
    APCFabricBackend<8u, 1u, 2u> backend{};
    if (!backend.Initialize()) return Result::FAIL;

    bool ok = true;
    ok = backend.AddParent(0u, 5u, Axis::HORIZONTAL) && ok;
    ok = backend.Node(1u).AttachMyChild(
        backend.Node(5u),
        FabricSegments::HORIZONTAL_EDGE_TABLE
    ) && ok;
    ok = ParentSetEquals(backend, 5u, Axis::HORIZONTAL, {0u, 1u}, 2u) && ok;

    const bool duplicate_rejected = !backend.AddParent(0u, 5u, Axis::HORIZONTAL);
    const bool third_rejected = !backend.AddParent(2u, 5u, Axis::HORIZONTAL);
    ok = duplicate_rejected && third_rejected && ok;

    ok = backend.RemoveParent(0u, 5u, Axis::HORIZONTAL) && ok;
    ok = ParentSetEquals(backend, 5u, Axis::HORIZONTAL, {1u, 0u}, 1u) && ok;
    ok = backend.AddParent(2u, 5u, Axis::HORIZONTAL) && ok;
    ok = backend.Node(1u).DetachMyChild(
        backend.Node(5u),
        FabricSegments::HORIZONTAL_EDGE_TABLE
    ) && ok;
    ok = ParentSetEquals(backend, 5u, Axis::HORIZONTAL, {2u, 0u}, 1u) && ok;

    ok = backend.AddParent(0u, 5u, Axis::VERTICAL) && ok;
    ok = backend.AddParent(1u, 5u, Axis::VERTICAL) && ok;
    ok = backend.ReplaceParent(0u, 2u, 5u, Axis::VERTICAL) && ok;
    ok = ParentSetEquals(backend, 5u, Axis::VERTICAL, {1u, 2u}, 2u) && ok;

    const bool same_parent_replace_rejected =
        !backend.Node(5u).ReplaceParent(
            backend.Node(1u),
            backend.Node(1u),
            FabricSegments::VERTICAL_EDGE_TABLE
        );
    const bool invalid_table_rejected =
        !backend.Node(5u).AddParent(
            backend.Node(0u),
            FabricSegments::SEGMENT_POOL
        );

    const GraphProof proof = ProveQuiescentCombinedDAG<8u, 2u>(backend);
    ok = ok && same_parent_replace_rejected && invalid_table_rejected && proof.Passed();

    std::cout
        << "  child-side and parent-side API symmetry : " << (ok ? "PASS" : "FAIL") << '\n'
        << "  duplicate relation rejected             : " << (duplicate_rejected ? "PASS" : "FAIL") << '\n'
        << "  third parent at K=2 rejected            : " << (third_rejected ? "PASS" : "FAIL") << '\n'
        << "  H/V rows remain independent             : " << (proof.Passed() ? "PASS" : "FAIL") << '\n'
        << "\nTEST 4 OVERALL: " << (ok ? "PASS" : "FAIL") << '\n';

    return ok ? Result::PASS : Result::FAIL;
}
} // namespace Test04_PublicMutationAPI

// -----------------------------------------------------------------------------
// Test 5: direct proof of the fixed-order H-union-V DAG rule.
// -----------------------------------------------------------------------------

namespace Test05_CombinedAcyclicity
{
inline Result Run()
{
    Banner("TEST 5 - H-UNION-V ACYCLIC DAG, MULTI-PARENT CAPACITY, CYCLE REJECTION");
    constexpr std::size_t N = 7u;
    constexpr std::uint8_t K = 2u;
    APCFabricBackend<N, 1u, K> backend{};
    if (!backend.Initialize()) return Result::FAIL;

    bool valid_diamond =
        backend.AddParent(0u, 2u, Axis::HORIZONTAL) &&
        backend.AddParent(1u, 2u, Axis::VERTICAL) &&
        backend.AddParent(0u, 3u, Axis::VERTICAL) &&
        backend.AddParent(1u, 3u, Axis::HORIZONTAL) &&
        backend.AddParent(2u, 4u, Axis::HORIZONTAL) &&
        backend.AddParent(3u, 4u, Axis::VERTICAL);

    const bool backward_h_rejected = !backend.AddParent(4u, 0u, Axis::HORIZONTAL);
    const bool backward_v_rejected = !backend.AddParent(4u, 1u, Axis::VERTICAL);
    const bool self_h_rejected = !backend.AddParent(4u, 4u, Axis::HORIZONTAL);
    const bool self_v_rejected = !backend.AddParent(4u, 4u, Axis::VERTICAL);

    // If accepted, this would close 0 --H--> 2 --V--> 0.
    const bool cross_axis_cycle_rejected =
        !backend.AddParent(2u, 0u, Axis::VERTICAL);

    const bool h_capacity =
        backend.AddParent(0u, 6u, Axis::HORIZONTAL) &&
        backend.AddParent(1u, 6u, Axis::HORIZONTAL) &&
        !backend.AddParent(2u, 6u, Axis::HORIZONTAL);
    const bool v_capacity =
        backend.AddParent(0u, 6u, Axis::VERTICAL) &&
        backend.AddParent(1u, 6u, Axis::VERTICAL) &&
        !backend.AddParent(2u, 6u, Axis::VERTICAL);

    const bool bad_replace_rejected =
        !backend.ReplaceParent(0u, 6u, 2u, Axis::HORIZONTAL);
    const ReadResult preserved = backend.FindParent(2u, Axis::HORIZONTAL, 0u);
    const bool old_relation_preserved = preserved.IsFound() && preserved.Node == 0u;

    const GraphProof proof = ProveQuiescentCombinedDAG<N, K>(backend);
    const bool ok = valid_diamond &&
        backward_h_rejected && backward_v_rejected &&
        self_h_rejected && self_v_rejected &&
        cross_axis_cycle_rejected && h_capacity && v_capacity &&
        bad_replace_rejected && old_relation_preserved && proof.Passed();

    std::cout
        << "  legal mixed-axis diamond              : " << (valid_diamond ? "PASS" : "FAIL") << '\n'
        << "  every backward/self insertion rejected: "
        << ((backward_h_rejected && backward_v_rejected && self_h_rejected && self_v_rejected)
            ? "PASS" : "FAIL") << '\n'
        << "  attempted H-union-V cycle rejected    : "
        << (cross_axis_cycle_rejected ? "PASS" : "FAIL") << '\n'
        << "  K=2 independently enforced on H and V : "
        << ((h_capacity && v_capacity) ? "PASS" : "FAIL") << '\n'
        << "  failed replacement preserves old edge : "
        << ((bad_replace_rejected && old_relation_preserved) ? "PASS" : "FAIL") << '\n'
        << "  exhaustive public-read topological sort: "
        << (proof.Passed() ? "PASS" : "FAIL") << '\n'
        << "\nTEST 5 OVERALL: " << (ok ? "PASS" : "FAIL") << '\n';

    return ok ? Result::PASS : Result::FAIL;
}
} // namespace Test05_CombinedAcyclicity

// -----------------------------------------------------------------------------
// Test 6: existing RegionView coverage, only creation signature is updated.
// -----------------------------------------------------------------------------

namespace Test06_RegionViews
{
using SD = SchemaDefinition;

inline LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier OneRegionLayout() noexcept
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
using WrongType = std::conditional_t<std::is_same_v<T, float>, std::uint32_t, float>;

template <typename T>
bool CreateTyped(
    VagueTemoraryPremativeFabric& fabric,
    AdaptivePackedCellContainer& apc,
    SD::SchemaProtocols region_protocol) noexcept
{
    constexpr auto dtype_value = SD::CppTypeToRegionDType<T>();
    static_assert(dtype_value.has_value());

    SD::InitialRegionalDtypeConf dtype{};
    dtype.FEEDFORWARD_MESSAGE = dtype_value.value();
    SD::InitialRegionalProtocol protocol{};
    protocol.FEEDFORWARD_MESSAGE = region_protocol;

    return fabric.CreateAPC(
        apc,
        OneRegionLayout(),
        dtype,
        protocol,
        APCDataStructure::BRANCH_VERSION
    );
}

template <typename T>
bool PrivateCase() noexcept
{
    VagueTemoraryPremativeFabric fabric{};
    AdaptivePackedCellContainer apc{};
    if (
        !fabric.InitializeFabricWithPtrTable(2u, MINIMUM_APC_CELL_COUNT, 2u) ||
        !CreateTyped<T>(fabric, apc, SD::SchemaProtocols::PRIVATE_REGION)
    )
    {
        return false;
    }

    auto view = apc.BuildAViewOverRegion<T>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE);
    auto wrong = apc.BuildAViewOverRegion<WrongType<T>>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE);
    if (
        !view.has_value() || !view->IsValid() || view->Size() < 3u ||
        view->GetProtocol() != SD::SchemaProtocols::PRIVATE_REGION ||
        wrong.has_value() || view->RawMutableSpan().has_value() == false ||
        view->AtomicStore(0u, FirstValue<T>())
    )
    {
        return false;
    }

    auto span = view->RawMutableSpan();
    span.value()[0u] = FirstValue<T>();
    span.value()[span->size() / 2u] = SecondValue<T>();
    span.value().back() = FirstValue<T>();
    if (!apc.ZeroARegion<T>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE)) return false;
    return std::all_of(span->begin(), span->end(), [](T value) { return value == T{}; });
}

template <typename T>
bool AtomicCase() noexcept
{
    VagueTemoraryPremativeFabric fabric{};
    AdaptivePackedCellContainer apc{};
    if (
        !fabric.InitializeFabricWithPtrTable(2u, MINIMUM_APC_CELL_COUNT, 2u) ||
        !CreateTyped<T>(fabric, apc, SD::SchemaProtocols::ATOMIC_WORD_ARRAY)
    )
    {
        return false;
    }

    auto view = apc.BuildAViewOverRegion<T>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE);
    auto wrong = apc.BuildAViewOverRegion<WrongType<T>>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE);
    if (
        !view.has_value() || !view->IsValid() || view->Size() < 3u ||
        view->GetProtocol() != SD::SchemaProtocols::ATOMIC_WORD_ARRAY ||
        view->RawMutableSpan().has_value() || wrong.has_value()
    )
    {
        return false;
    }

    const std::size_t middle = view->Size() / 2u;
    if (
        !view->AtomicStore(0u, FirstValue<T>(), std::memory_order_relaxed) ||
        !view->AtomicStore(middle, SecondValue<T>(), std::memory_order_release) ||
        view->AtomicLoad(0u, std::memory_order_relaxed) != FirstValue<T>() ||
        view->AtomicLoad(middle, std::memory_order_acquire) != SecondValue<T>()
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
        !apc.ZeroARegion<T>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE)
    )
    {
        return false;
    }

    for (std::size_t i = 0u; i < view->Size(); ++i)
    {
        if (view->AtomicLoad(i, std::memory_order_relaxed) != T{}) return false;
    }
    return true;
}

template <typename T>
bool RunType(const char* name)
{
    const bool private_ok = PrivateCase<T>();
    const bool atomic_ok = AtomicCase<T>();
    std::cout
        << "  " << std::left << std::setw(10) << name
        << " private=" << (private_ok ? "PASS" : "FAIL")
        << " atomic=" << (atomic_ok ? "PASS" : "FAIL") << '\n';
    return private_ok && atomic_ok;
}

inline Result Run()
{
    Banner("TEST 6 - PUBLIC REGION VIEW / ALL PRIMITIVE DTYPES");
    bool ok = true;
    ok = RunType<std::uint8_t>("uint8_t") && ok;
    ok = RunType<std::uint16_t>("uint16_t") && ok;
    ok = RunType<std::uint32_t>("uint32_t") && ok;
    ok = RunType<std::uint64_t>("uint64_t") && ok;
    ok = RunType<std::int8_t>("int8_t") && ok;
    ok = RunType<std::int16_t>("int16_t") && ok;
    ok = RunType<std::int32_t>("int32_t") && ok;
    ok = RunType<std::int64_t>("int64_t") && ok;
    ok = RunType<float>("float") && ok;
    ok = RunType<double>("double") && ok;
    ok = RunType<char>("char") && ok;

    std::cout << "\nTEST 6 OVERALL: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok ? Result::PASS : Result::FAIL;
}
} // namespace Test06_RegionViews

// -----------------------------------------------------------------------------
// Test 7: concurrent mixed-axis mutation proof plus retirement/ABA lifecycle.
// -----------------------------------------------------------------------------

namespace Test07_ConcurrentDAGAndRetirement
{
inline bool FixedOrderRace()
{
    constexpr std::uint32_t ROUNDS = 10'000u;
    APCFabricBackend<2u, 1u, 2u> backend{};
    if (!backend.Initialize()) return false;

    std::barrier phase(3);
    std::atomic<std::uint32_t> valid_success{0u};
    std::atomic<std::uint32_t> invalid_success{0u};

    std::thread valid([&]() noexcept
    {
        for (std::uint32_t i = 0u; i < ROUNDS; ++i)
        {
            phase.arrive_and_wait();
            if (backend.AddParent(0u, 1u, Axis::HORIZONTAL))
            {
                valid_success.fetch_add(1u, std::memory_order_relaxed);
            }
            phase.arrive_and_wait();
        }
    });

    std::thread invalid([&]() noexcept
    {
        for (std::uint32_t i = 0u; i < ROUNDS; ++i)
        {
            phase.arrive_and_wait();
            if (backend.AddParent(1u, 0u, Axis::VERTICAL))
            {
                invalid_success.fetch_add(1u, std::memory_order_relaxed);
            }
            phase.arrive_and_wait();
        }
    });

    bool main_ok = true;
    for (std::uint32_t i = 0u; i < ROUNDS; ++i)
    {
        phase.arrive_and_wait();
        phase.arrive_and_wait();
        if (!backend.RemoveParent(0u, 1u, Axis::HORIZONTAL))
        {
            main_ok = false;
        }
    }

    valid.join();
    invalid.join();
    const GraphProof proof = ProveQuiescentCombinedDAG<2u, 2u>(backend);
    return main_ok &&
        valid_success.load() == ROUNDS &&
        invalid_success.load() == 0u &&
        proof.Passed();
}

inline bool MixedAxisStress(std::uint64_t& retries_out)
{
    constexpr std::size_t N = 32u;
    constexpr std::uint8_t K = 4u;
    constexpr std::size_t WORKERS = 8u;
    constexpr std::size_t FIRST_CHILD = 8u;
    constexpr std::uint32_t ROUNDS = 5'000u;

    APCFabricBackend<N, 1u, K> backend{};
    if (!backend.Initialize()) return false;
    for (std::size_t i = 0u; i < WORKERS; ++i)
    {
        if (
            !backend.AddParent(0u, FIRST_CHILD + i, Axis::HORIZONTAL) ||
            !backend.AddParent(2u, FIRST_CHILD + i, Axis::VERTICAL)
        )
        {
            return false;
        }
    }

    std::barrier start(static_cast<std::ptrdiff_t>(WORKERS + 1u));
    std::atomic<bool> failed{false};
    std::atomic<std::uint64_t> retries{0u};
    std::vector<std::thread> workers;
    workers.reserve(WORKERS);

    for (std::size_t worker = 0u; worker < WORKERS; ++worker)
    {
        workers.emplace_back([&, worker]() noexcept
        {
            const std::size_t child = FIRST_CHILD + worker;
            std::size_t h_current = 0u;
            std::size_t v_current = 2u;
            std::uint64_t local_retries = 0u;
            start.arrive_and_wait();

            for (std::uint32_t i = 0u; i < ROUNDS; ++i)
            {
                const std::size_t h_next = h_current == 0u ? 1u : 0u;
                const std::size_t v_next = v_current == 2u ? 3u : 2u;
                if (
                    !RetryReplace(
                        backend, h_current, h_next, child,
                        Axis::HORIZONTAL, local_retries
                    ) ||
                    !RetryReplace(
                        backend, v_current, v_next, child,
                        Axis::VERTICAL, local_retries
                    )
                )
                {
                    failed.store(true, std::memory_order_release);
                    break;
                }
                h_current = h_next;
                v_current = v_next;
            }
            retries.fetch_add(local_retries, std::memory_order_relaxed);
        });
    }

    start.arrive_and_wait();
    for (std::thread& worker : workers) worker.join();
    retries_out = retries.load(std::memory_order_acquire);

    const GraphProof proof = ProveQuiescentCombinedDAG<N, K>(backend);
    return !failed.load(std::memory_order_acquire) && proof.Passed();
}

inline LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier AtomicLayout() noexcept
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

inline bool CreateAtomic(
    VagueTemoraryPremativeFabric& fabric,
    AdaptivePackedCellContainer& apc) noexcept
{
    SchemaDefinition::InitialRegionalDtypeConf dtype{};
    dtype.FEEDFORWARD_MESSAGE = SchemaDefinition::DataTypeOfMacroColumn::UINT64_T;
    SchemaDefinition::InitialRegionalProtocol protocol{};
    protocol.FEEDFORWARD_MESSAGE = SchemaDefinition::SchemaProtocols::ATOMIC_WORD_ARRAY;
    return fabric.CreateAPC(
        apc,
        AtomicLayout(),
        dtype,
        protocol,
        APCDataStructure::BRANCH_VERSION
    );
}

inline bool RetirementAndABA()
{
    VagueTemoraryPremativeFabric fabric{};
    AdaptivePackedCellContainer parent{};
    AdaptivePackedCellContainer child{};
    AdaptivePackedCellContainer replacement{};

    if (
        !fabric.InitializeFabricWithPtrTable(2u, MINIMUM_APC_CELL_COUNT, 2u) ||
        !CreateAtomic(fabric, parent) ||
        !CreateAtomic(fabric, child)
    )
    {
        return false;
    }

    const std::uint32_t child_slot = child.GetThisSlotIdx();
    if (
        !child.AddParent(parent, FabricSegments::HORIZONTAL_EDGE_TABLE) ||
        !child.AddParent(parent, FabricSegments::VERTICAL_EDGE_TABLE) ||
        parent.Retire() ||
        child.Retire() ||
        !child.RemoveParent(parent, FabricSegments::HORIZONTAL_EDGE_TABLE) ||
        child.Retire() ||
        !child.RemoveParent(parent, FabricSegments::VERTICAL_EDGE_TABLE)
    )
    {
        return false;
    }

    auto held_view = child.BuildAViewOverRegion<std::uint64_t>(
        MacroColumnOfAPC::FEEDFORWARD_MESSAGE
    );
    if (!held_view.has_value() || child.Retire(1u))
    {
        return false;
    }
    held_view.reset();

    if (
        !child.Retire() ||
        child.IsActiveAPC() ||
        child.BuildAViewOverRegion<std::uint64_t>(
            MacroColumnOfAPC::FEEDFORWARD_MESSAGE
        ).has_value() ||
        !CreateAtomic(fabric, replacement) ||
        replacement.GetThisSlotIdx() != child_slot ||
        !replacement.IsActiveAPC() ||
        child.IsActiveAPC()
    )
    {
        return false;
    }

    return parent.Retire() && replacement.Retire();
}

inline Result Run()
{
    Banner("TEST 7 - CONCURRENT H/V DAG MUTATION + RETIREMENT / ABA");
    std::uint64_t mixed_retries = 0u;
    const bool race_ok = FixedOrderRace();
    const bool stress_ok = MixedAxisStress(mixed_retries);
    const bool retirement_ok = RetirementAndABA();
    const bool ok = race_ok && stress_ok && retirement_ok;

    std::cout
        << "  A--H-->B raced with illegal B--V-->A : " << (race_ok ? "PASS" : "FAIL") << '\n'
        << "  shared-parent mixed H/V stress       : " << (stress_ok ? "PASS" : "FAIL") << '\n'
        << "  transaction retries observed         : " << mixed_retries << '\n'
        << "  linked/pinned retirement + ABA reuse : " << (retirement_ok ? "PASS" : "FAIL") << '\n'
        << "\nTEST 7 OVERALL: " << (ok ? "PASS" : "FAIL") << '\n';

    return ok ? Result::PASS : Result::FAIL;
}
} // namespace Test07_ConcurrentDAGAndRetirement

inline int RunAll()
{
    const std::array<std::pair<const char*, Result>, 7u> results{{
        {"Test 1 - baseline and benchmark", Test01_Baseline::Run()},
        {"Test 2 - contention sweep", Test02_Contention::Run()},
        {"Test 3 - reader/writer atomicity", Test03_ReaderWriter::Run()},
        {"Test 4 - public mutation API", Test04_PublicMutationAPI::Run()},
        {"Test 5 - combined DAG proof", Test05_CombinedAcyclicity::Run()},
        {"Test 6 - primitive region views", Test06_RegionViews::Run()},
        {"Test 7 - concurrency and retirement", Test07_ConcurrentDAGAndRetirement::Run()}
    }};

    Banner("APC DUAL-EDGE DAG TEST SUITE SUMMARY");
    std::uint32_t failures = 0u;
    for (const auto& [name, result] : results)
    {
        std::cout
            << "  " << std::left << std::setw(42) << name
            << ResultName(result) << '\n';
        if (result == Result::FAIL) ++failures;
    }
    std::cout
        << "\n  failures: " << failures
        << "\n================================================================================\n";
    return failures == 0u ? 0 : 1;
}

} // namespace APCDAGTests
