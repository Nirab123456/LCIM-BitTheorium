#pragma once
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{
    bool DAGMutationConf::AddRowParticipant_(
        DAGMutationTransaction& transaction,
        uint32_t slot,
        bool is_parent_anchor
    ) noexcept
    {
        if (slot >= CountOfAPC_)
        {
            return false;
        }
        
        uint8_t insert_at = transaction.RowCount;
        for (uint8_t i = 0; i < transaction.RowCount; i++)
        {
            DAGRowParticipant& current = transaction.Rows[i];
            if (current.Slot == slot)
            {
                current.IsParentAnchor = current.IsParentAnchor || is_parent_anchor;
                return true;
            }
            if (slot < current.Slot)
            {
                insert_at = i;
                break;
            }
        }

        if (transaction.RowCount >= DAG_MAX_ROW_PARTICIPANTS)
        {
            return false;
        }
        for (uint8_t i = transaction.RowCount; i > insert_at; --i)
        {
            transaction.Rows[i] = transaction.Rows[i - 1u];
        }
        transaction.Rows[insert_at] = DAGRowParticipant{};
        transaction.Rows[insert_at].Slot = slot;
        transaction.Rows[insert_at].IsParentAnchor = is_parent_anchor;
        ++transaction.RowCount;
        return true;
    }


    DAGMutationConf::DAGRowParticipant* DAGMutationConf::FindRowParticipant_(
        DAGMutationTransaction& transaction,
        uint32_t slot
    ) noexcept
    {
        for (size_t i = 0; i < transaction.RowCount; i++)
        {
            if (transaction.Rows[i].Slot == slot)
            {
                return &transaction.Rows[i];
            }
        }
        return nullptr;
    }

    bool DAGMutationConf::ReserveAllRows_(
        DAGMutationTransaction& transaction,
        EdgeBuilder::EdgeStatus required_status,
        uint32_t max_tries
    ) noexcept
    {
        for (uint8_t i = 0; i < transaction.RowCount; i++)
        {
            DAGRowParticipant& row = transaction.Rows[i];
            if (
                ReserveEdgeRow_(
                    transaction.EdgeTable,
                    row.Slot,
                    required_status,
                    row.Before,
                    max_tries
                ) != SeqLockedOperation::FOUND
            )
            {
                AbortRowTransaction_(transaction);
                return false;
            }
            row.WorkTail = row.Before.TailLocator;
            row.Reserved = true;
        }
        return true;
    }

    DAGMutationConf::DAGRelationDelta*
    DAGMutationConf::EditReservedRelation_(
        DAGMutationTransaction& transaction,
        uint32_t child_slot,
        uint8_t ordinal
    ) noexcept
    {
        DAGRowParticipant* row =
            FindRowParticipant_(transaction, child_slot);

        if (
            !row ||
            !row->Reserved ||
            !EdgeBuilder::IsValidRelationOrdinal(
                ordinal,
                MaxDirectParentsPerAxis_
            )
        )
        {
            return nullptr;
        }

        for (uint8_t i = 0u; i < transaction.RelationCount; ++i)
        {
            DAGRelationDelta& delta = transaction.Relations[i];
            if (
                delta.ChildSlot == child_slot &&
                delta.Ordinal == ordinal
            )
            {
                return &delta;
            }
        }

        if (transaction.RelationCount >= DAG_MAX_RELATION_DELTAS)
        {
            return nullptr;
        }

        std::span<EdgeBuilder::ParentRelation> relations =
            ParentRelations_(transaction.EdgeTable, child_slot);

        if (relations.size() != MaxDirectParentsPerAxis_)
        {
            return nullptr;
        }

        DAGRelationDelta& inserted =
            transaction.Relations[transaction.RelationCount++];
        inserted.ChildSlot = child_slot;
        inserted.Ordinal = ordinal;
        
        inserted.Before.ParentHandle = std::atomic_ref<uint64_t>(
            relations[ordinal].ParentHandle
        ).load(std::memory_order_relaxed);

        inserted.Before.SiblingLocators = std::atomic_ref<uint64_t>(
            relations[ordinal].SiblingLocators
        ).load(std::memory_order_relaxed);

        inserted.Work = inserted.Before;
        return &inserted;
    }



    void DAGMutationConf::CommitRowTransaction_(
        DAGMutationTransaction& transaction,
        EdgeBuilder::EdgeStatus final_status
    ) noexcept
    {
        for (uint8_t i =  transaction.RowCount; i > UNSIGNED_ZERO; i--)
        {
            const DAGRelationDelta& delta = transaction.Relations[i];
            StoreReservedParentRelation_(
                transaction.EdgeTable,
                delta.ChildSlot,
                delta.Ordinal,
                delta.Work
            );
        }

        auto Publish___ = [&](bool publish_anchor) noexcept -> void
        {
            for (uint8_t i = 0; i < transaction.RowCount; i++)
            {
                DAGRowParticipant& row = transaction.Rows[i];
                if (row.IsParentAnchor != publish_anchor)
                {
                    continue;
                }

                PublishReservedEdgeRow_(
                    transaction.EdgeTable,
                    row.Slot,
                    row.Before,
                    row.WorkTail,
                    final_status
                );
                row.Reserved = false;
            }
        };

        Publish___(false);
        Publish___(true);
    }

    void DAGMutationConf::AbortRowTransaction_(
        DAGMutationTransaction& transaction
    ) noexcept
    {
        for (uint8_t i = transaction.RowCount; i > UNSIGNED_ZERO; i--)
        {
            DAGRowParticipant& row = transaction.Rows[i - 1u];
            if (!row.Reserved)
            {
                continue;
            }
            
            PublishReservedEdgeRow_(
                transaction.EdgeTable,
                row.Slot,
                row.Before,
                row.Before.TailLocator,
                row.Before.Status
            );
            row.Reserved = false;
        }
    }

    ConstructDAGOnEachAxis::SeqLockedOperation ConstructDAGOnEachAxis::ScanParentRow_(
        FabricSegments edge_table,
        uint32_t child_slot,
        uint64_t wanted_parent_handle,
        uint64_t other_parent_handle,
        ParentRowScan& scan,
        uint32_t max_tries 
    ) noexcept
    {
        scan = ParentRowScan{};
        const EdgeTableRange range = ReadAnEdgeTableRange_(edge_table, child_slot);
        std::span<EdgeBuilder::ParentRelation> stored = ParentRelations_(edge_table, child_slot);

        if (
            !range.IsValid ||
            stored.size() != MaxDirectParentsPerAxis_
        )
        {
            return SeqLockedOperation::NONE;
        }

        for (size_t i = 0; i < max_tries; i++)
        {
            const uint64_t before_raw = std::atomic_ref<uint64_t>(SlabBasePtr_[range.BeginIndex]).load(std::memory_order_acquire);

            const EdgeBuilder::EdgeData before = EdgeBuilder::UnpackEdgeHeader(before_raw);

            if (!before.IsValid)
            {
                return SeqLockedOperation::NONE;
            }

            if (before.Status == EdgeBuilder::EdgeStatus::RESERVED)
            {
                continue;
            }
            
            if (before.Status != EdgeBuilder::EdgeStatus::LIVE)
            {
                return SeqLockedOperation::NONE;
            }
            
            ParentRowScan observed{};
            observed.Header = before;
            bool malformed = false;

            for (uint8_t ordinal = 0; ordinal < MaxDirectParentsPerAxis_; ordinal++)
            {
                EdgeBuilder::ParentRelation relation{};
                relation.ParentHandle = std::atomic_ref<uint64_t>(stored[ordinal].ParentHandle).load(std::memory_order_acquire);
                relation.SiblingLocators = std::atomic_ref<uint64_t>(stored[ordinal].SiblingLocators).load(std::memory_order_relaxed);  

                if (EdgeBuilder::IsPartiallyEmpty(relation))
                {
                    malformed = true;
                    continue;
                }

                if (EdgeBuilder::IsEmpty(relation))
                {
                    if (observed.EmptyOrdinal == UINT8_MAX)
                    {
                        observed.EmptyOrdinal = ordinal;
                    }
                    continue;
                }
                
                if (relation.ParentHandle == wanted_parent_handle)
                {
                    if (observed.MatchOrdinal != UINT8_MAX)
                    {
                        malformed = true;
                    }
                    else
                    {
                        observed.MatchOrdinal = ordinal;
                        observed.Match = relation;
                    }
                }

                if (
                    other_parent_handle != FABRIC_CELL_SENTINAL &&
                    relation.ParentHandle == other_parent_handle
                )
                {
                    if (observed.OtherOrdinal != UINT8_MAX)
                    {
                        malformed = true;
                    }
                    else
                    {
                        observed.OtherOrdinal = ordinal;
                    }
                }
            }
            
            const uint64_t after_raw = std::atomic_ref<uint64_t>(SlabBasePtr_[range.BeginIndex]).load(std::memory_order_acquire);
            if (before_raw != after_raw)
            {
                continue;
            }
            
            if (malformed)
            {
                return SeqLockedOperation::NONE;
            }

            scan = observed;
            return SeqLockedOperation::FOUND;
        }
        
        return SeqLockedOperation::RETRY;
    }

}