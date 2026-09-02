#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{
    using EdgeTableRange = RangeOfAPC;

    std::span<EdgeTableConf::ParentRelation> EdgeTableConstructor::ParentRelation_(
        FabricSegments edge_table,
        uint32_t edge_idx
    ) noexcept
    {
        const EdgeTableRange range = ReadAnEdgeTableRange_(edge_table, edge_idx);
        if (!range.IsValid)
        {
            return {};
        }

        auto* first = std::launder(reinterpret_cast<EdgeBuilder::ParentRelation*>(
            SlabBasePtr_ + range.BeginIndex + 1u
        ));

        return {
            first,
            static_cast<size_t>(MaxDirectParentsPerAxis_)
        };
    }

    bool EdgeTableConstructor::ConstructParentRelationObject_(
        FabricSegments edge_table,
        uint32_t edge_idx
    ) noexcept
    {
        const EdgeTableRange range = ReadAnEdgeTableRange_(edge_table, edge_idx);
        if (!range.IsValid)
        {
            return false;
        }

        auto* first = reinterpret_cast<EdgeBuilder::ParentRelation*>(SlabBasePtr_ + range.BeginIndex + 1u);

        for (uint8_t i = 0; i < MaxDirectParentsPerAxis_; i++)
        {
            std::construct_at(first + i);
        }
        
        return true;
    }

    EdgeTableRange EdgeTableConstructor::ReadAnEdgeTableRange_(
        FabricSegments edge_table,
        uint32_t edge_idx
    ) noexcept
    {
        EdgeTableRange range{};
        if (
            !CoreOfFabricCoordinator::IsValidEdgeTable(edge_table) ||
            edge_idx >= CountOfAPC_ ||
            EdgeTableRecordWidth_ != EdgeBuilder::EdgeTableRecordWidth(MaxDirectParentsPerAxis_)
        )
        {
            return range;
        }
        
        const uint64_t range_begin = edge_table == FabricSegments::HORIZONTAL_EDGE_TABLE ?
            HorizontalEdgeBeginIdx_ : VerticalEdgeBeginIdx_;

        range.BeginIndex = range_begin + static_cast<uint64_t>(edge_idx) * EdgeTableRecordWidth_;
        range.EndIndex = range.BeginIndex + EdgeTableRecordWidth_;
        range.IsValid = range.BeginIndex >= range_begin &&
            range.EndIndex <= SlabCellCount_;

        return range;
    }

    void EdgeTableConstructor::InitializeEdgeTable_(FabricSegments edge_table) noexcept
    {
        if (!CoreOfFabricCoordinator::IsValidEdgeTable(edge_table))
        {
            return;
        }
        
        for (uint32_t edge_idx = 0; edge_idx < CountOfAPC_; edge_idx++)
        {
            const EdgeTableRange range = ReadAnEdgeTableRange_(edge_table, edge_idx);

            if (
                !range.IsValid || 
                !ConstructParentRelationObject_(edge_table, edge_idx)
            )
            {
                return;
            }

            SlabBasePtr_[range.BeginIndex] = EdgeBuilder::PackEdgeHeader(
                UNSIGNED_ZERO,
                EdgeBuilder::EdgeStatus::FREE
            );
        }
    }

    FabricToAPCLinker::SeqLockedOperation EdgeTableConstructor::ReadParentRelation_(
        FabricSegments edge_table,
        uint32_t edge_idx,
        uint8_t relation_ordinal,
        EdgeBuilder::ParentRelation& relation,
        uint32_t max_tries
    ) noexcept
    {
        const EdgeTableRange range = ReadAnEdgeTableRange_(edge_table, edge_idx);

        std::span<EdgeBuilder::ParentRelation> stored = ParentRelation_(edge_table, edge_idx);

        if (
            !range.IsValid ||
            stored.size() != MaxDirectParentsPerAxis_
        )
        {
            return SeqLockedOperation::NONE;
        }

        for (uint32_t i = 0; i < max_tries; i++)
        {
            std::atomic_ref<uint64_t>sequense_lock(SlabBasePtr_[range.BeginIndex]);
            const uint64_t before = sequense_lock.load(std::memory_order_acquire);

            EdgeBuilder::EdgeData header{};
            EdgeBuilder::UnpackEdgeHader(before, header);

            if (
                header.Status == EdgeBuilder::EdgeStatus::RESERVED ||
                !IAB::IsValidEven64(header.SeqLock)
            )
            {
                continue;
            }
            
            if (header.Status != EdgeBuilder::EdgeStatus::LIVE)
            {
                return SeqLockedOperation::NONE;
            }

            EdgeBuilder::ParentRelation observed{};

            observed.ParentHandle = std::atomic_ref<uint64_t>(stored[relation_ordinal].ParentHandle).load(std::memory_order_relaxed);
            observed.SiblingLocators = std::atomic_ref<uint64_t>(stored[relation_ordinal].SiblingLocators).load(std::memory_order_relaxed);

            const uint64_t after = std::atomic_ref<uint64_t>(SlabBasePtr_[range.BeginIndex]).load(std::memory_order_acquire);

            if (before != after)
            {
                relation = observed;
                return EdgeBuilder::IsEmpty(relation) ?
                    SeqLockedOperation::NONE : SeqLockedOperation::FOUND;

            }
        }
        
        return SeqLockedOperation::RETRY;
    }


    bool EdgeTableConstructor::ReadEdgeHeader_(
        FabricSegments edge_table,
        uint32_t edge_idx,
        EdgeBuilder::EdgeLockValues& values
    ) noexcept
    {
        const EdgeTableRange range = ReadAnEdgeTableRange_(edge_table, edge_idx);
        if (!range.IsValid)
        {
            return false;
        }
        const uint64_t raw =  std::atomic_ref<uint64_t>(SlabBasePtr_[range.BeginIndex]).load(std::memory_order_acquire);
        EdgeBuilder::UnpackEdgeLock(raw, values);
        return DescriptionOfAPC::ValidateStateAgainstSeqLock(values);
    }


    EdgeTableConstructor::SeqLockedOperation EdgeTableConstructor::SwitchEdgeState__(
        FabricSegments edge_table,
        uint32_t edge_idx,
        EdgeBuilder::EdgeLockValues& before,
        EdgeBuilder::EdgeStatus desired_state,
        std::optional<EdgeBuilder::EdgeStatus> required_st,
        uint32_t max_tries
    ) noexcept
    {
        const EdgeTableRange range = ReadAnEdgeTableRange_(edge_table, edge_idx);

        std::span<EdgeBuilder::ParentRelation> stored = ParentRelation_(edge_table, edge_idx);
        if (
            !range.IsValid ||
            stored.size() != MaxDirectParentsPerAxis_
        )
        {
            return SeqLockedOperation::NONE;
        }
        
        before = EdgeBuilder::EdgeLockValues{};

        for (size_t i = 0; i < max_tries; i++)
        {
            if (
                !ReadEdgeHeader_(edge_table, edge_idx, before) ||
                !before.IsValid ||
                (
                    before.StateOfTheAPC == EdgeBuilder::EdgeStatus::HAULTED &&
                    desired_state != EdgeBuilder::EdgeStatus::LIVE
                )
            )
            {
                return SeqLockedOperation::NONE;
            }
            
            if (
                required_st.has_value() &&
                required_st != before.StateOfTheAPC
            )
            {
                continue;
            }
            
            const bool caller_holds_reservation = before.StateOfTheAPC == EdgeBuilder::EdgeStatus::RESERVED;
            const bool false_owner_claim = !caller_holds_reservation && desired_state != EdgeBuilder::EdgeStatus::RESERVED;
            const bool non_ower_touching_reserved = caller_holds_reservation && desired_state == EdgeBuilder::EdgeStatus::RESERVED;

            if ( 
                false_owner_claim ||
                non_ower_touching_reserved ||
                !EdgeBuilder::IsTransitionStateLeagal(before.StateOfTheAPC, desired_state)
            )
            {
                continue;
            }

            uint64_t observed = EdgeBuilder::PackEdgeHeader(before.SeqLock , before.StateOfTheAPC);

            const uint64_t desired = EdgeBuilder::PackEdgeHeader(before.SeqLock + 1u, desired_state);

            if (CompareExchangeStrongFromFabric(range.BeginIndex, observed, desired))
            {
                return SeqLockedOperation::FOUND;
            }
        }
        
        return SeqLockedOperation::RETRY;
    }

}