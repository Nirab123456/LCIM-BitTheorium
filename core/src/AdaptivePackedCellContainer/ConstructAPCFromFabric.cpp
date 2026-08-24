#pragma once
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{

    std::optional<uint64_t> ConstructForestOnEachAxis::AcquireGraphMutationFlag_(
        uint32_t apc_slot_idx,
        IAB::BidirectionalAxis axis,
        uint32_t max_tries 
    ) noexcept
    {
        const RangeOfAPC range_of_apc = GetSegmentPoolRange(apc_slot_idx);
        if (!range_of_apc.IsValid)
        {
            return std::nullopt;
        }

        const IAB::MemGraphFlag desired_state = IAB::GetMemGFlagFromAxis(axis);

        const size_t gmv_idx = range_of_apc.BeginIndex + static_cast<uint8_t>(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK);
        uint64_t gmv_raw = FABRIC_CELL_SENTINAL;
        IAB::GraphMutationValues gmv_values{};

        for (size_t i = 0; i < max_tries; i++)
        {
            if (
                !AtomicallyLoadReadAUnit(gmv_idx, gmv_raw) ||
                !APCDataStructure::IsValidFabricUnit(gmv_raw) ||
                !IAB::ExtractGraphMutationValues(gmv_raw, gmv_values)

            )
            {
                return std::nullopt;
            }

            if (
                IAB::HasThisGraphMutationFlag(gmv_values.Flags, desired_state) ||
                IAB::HasThisGraphMutationFlag(gmv_values.Flags, IAB::MemGraphFlag::READ_ONLY) ||
                (
                    desired_state == IAB::MemGraphFlag::READ_ONLY &&
                    !IAB::IsIdentityGraphUnlocked(gmv_values.Flags)
                )
            )
            {
                continue;
            }
            IAB::GraphMutationValues updated_gmv = IAB::UpdateSeqkBasedOnDesiredLock(gmv_values, desired_state);

            const uint64_t updated_gmv_raw = IAB::MakeGraphMutationRaw(updated_gmv);
            
            if (
                !IAB::IsValidGraphMutationState(updated_gmv) ||
                !APCDataStructure::IsValidFabricUnit(updated_gmv_raw)
            )
            {
                return std::nullopt;
            }
            
            if (!CompareExchangeStrongFromFabric(
                gmv_idx,
                gmv_raw,
                updated_gmv_raw
            ))
            {
                continue;
            }
            
            return gmv_raw;
        }
    
        return std::nullopt;
    }

    FabricToAPCLinker::SeqLockedOperation ConstructForestOnEachAxis::FindForestRootEdge_(
        uint32_t start_edge_idx,
        uint32_t forbidden_edge_idx,
        bool reject_forbidden,
        uint32_t& root_edge_idx,
        IAB::BidirectionalAxis axis,
        ForestMutationTransaction_& transaction
    ) noexcept
    {
        if (start_edge_idx >= CountOfAPC_)
        {
            return SeqLockedOperation::NONE;
        }

        uint32_t cursor = start_edge_idx;
        for (uint32_t i = 0u; i < CountOfAPC_; ++i)
        {
            if (reject_forbidden && cursor == forbidden_edge_idx)
            {
                return SeqLockedOperation::NONE;
            }

            EdgeBuilder::EdgeData edge{};
            const SeqLockedOperation outcome = ReadCommittedForestEdge_(
                cursor,
                axis,
                edge,
                &transaction
            );

            if (outcome != SeqLockedOperation::FOUND)
            {
                return outcome;
            }

            if (edge.ParentEdgeIndex == APCDataStructure::APC_INDEX_BOUND_SENTINAL)
            {
                root_edge_idx = cursor;
                return SeqLockedOperation::FOUND;
            }
            if (edge.ParentEdgeIndex >= CountOfAPC_ || edge.ParentEdgeIndex == cursor)
            {
                return SeqLockedOperation::NONE;
            }
            cursor = edge.ParentEdgeIndex;
        }
        return SeqLockedOperation::NONE;
    }

    ConstructForestOnEachAxis::SeqLockedOperation ConstructForestOnEachAxis::ReadIdentityBufferOfAPC(
        uint32_t apc_slot,
        IAB::BufferOfAPCIdentity& identity,
        std::optional<IAB::BidirectionalAxis> axis
    ) noexcept
    {
        const RangeOfAPC range_of_apc_sagmant_pool = GetSegmentPoolRange(apc_slot);

        const uint8_t internal_st_lock_idx = static_cast<uint8_t>(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK);
        const size_t st_lock_idx = range_of_apc_sagmant_pool.BeginIndex + internal_st_lock_idx;

        IAB::GraphMutationValues before_values{};
        IAB::GraphMutationValues after_values{};

        if (
            !ReadGraphMutationFlags(apc_slot, before_values) ||
            !ReadBufferwithSyncAtomicIndex(
                st_lock_idx,
                APCDataStructure::TotalIdentityUnitCount(),
                identity.data(),
                IAB::GetBufferIdxFromIdentityUnit(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK).value()
            ) ||
            !IAB::ExtractGraphMutationValues(
                IAB::ValueOfAnIdentityFromBuffer(identity, HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK), 
                after_values
            )
        )
        {
            return SeqLockedOperation::NONE;
        }

        // if (axis.has_value() && IAB::IsDesiredAxisLocked(before_values.Flags, axis.value()))
        // {
        //     return SeqLockedOperation::RETRY;
        // }
        

        if (!axis.has_value())
        {
            if (
                before_values.Flags == after_values.Flags &&
                before_values.SeqLockHorizontal == after_values.SeqLockHorizontal &&
                before_values.SeqLockVertical == after_values.SeqLockVertical
            )
            {
                return SeqLockedOperation::FOUND;
            }
            return SeqLockedOperation::RETRY;
        }

        switch (axis.value())
        {
        case IAB::BidirectionalAxis::HORIZONTAL:
            if (
                before_values.SeqLockHorizontal != after_values.SeqLockHorizontal ||
                IAB::HasThisGraphMutationFlag(before_values.Flags, IAB::MemGraphFlag::HORIZONTAL_LOCK) !=
                    IAB::HasThisGraphMutationFlag(after_values.Flags, IAB::MemGraphFlag::HORIZONTAL_LOCK)
            )
            {
                return SeqLockedOperation::RETRY;
            }
            break;

        case IAB::BidirectionalAxis::VERTICAL:
            if (
                before_values.SeqLockVertical != after_values.SeqLockVertical ||
                IAB::HasThisGraphMutationFlag(before_values.Flags, IAB::MemGraphFlag::VERTICAL_LOCK) !=
                    IAB::HasThisGraphMutationFlag(after_values.Flags, IAB::MemGraphFlag::VERTICAL_LOCK)
            )
            {
                return SeqLockedOperation::RETRY;
            }
            break;

        default:
            break;
        }
        

        if (
            IAB::ValidateIdentityBuffer(identity)
        )
        {
            return SeqLockedOperation::FOUND;
        }
        
        return SeqLockedOperation::NONE;
    }

    bool ConstructForestOnEachAxis::ReadGraphMutationFlags(
        uint32_t slot_idx,
        IAB::GraphMutationValues& values
    ) noexcept
    {
        const RangeOfAPC range_of_apc_sagmant_pool = GetSegmentPoolRange(slot_idx);

        const size_t identity_begin = range_of_apc_sagmant_pool.BeginIndex +
            static_cast<uint8_t>(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK);

        uint64_t mutation_lock = FABRIC_CELL_SENTINAL;

        if (
            !range_of_apc_sagmant_pool.IsValid ||
            !AtomicallyLoadReadAUnit(identity_begin, mutation_lock)
        )
        {
            return false;
        }
        return IAB::ExtractGraphMutationValues(mutation_lock, values);
    }




    bool ConstructForestOnEachAxis::OpenForestGateOnAxis(
        uint32_t apc_slot,
        IAB::BidirectionalAxis axis,
        uint32_t max_tries
    ) noexcept
    {
        if (apc_slot >= CountOfAPC_ )
        {
            return false;
        }

        IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);

        EdgeBuilder::EdgeLockValues edge_status{};

        if (
            !ReadEdgedataAtomically(map.EdgeTable, apc_slot, edge_status) ||
            edge_status.StateOfTheAPC != EdgeBuilder::EdgeStatus::RESERVED
        )
        {
            return false;
        }

        auto RevertEdgeToFree____ = [&]()
        {
            EdgeBuilder::EdgeData free_edge{};
            if (
                !EdgeBuilder::BuildFreeEdgeTable(map.EdgeTable, apc_slot, free_edge)
            )
            {
                return false;
            }
            return PublishReservedEdge_(free_edge, apc_slot);
        };

        if (
            !AcquireGraphMutationFlag_(apc_slot, axis, max_tries).has_value()
        )
        {
            RevertEdgeToFree____();
            return false;
        }

        bool axis_locked = true;

        auto ReleseAxis___ = [&]()
        {
            if (!axis_locked)
            {
                return true;
            }
            const bool relesed = ReleseGraphMutationFlag_(apc_slot, axis, max_tries);
            if (relesed)
            {
                axis_locked = false;
            }
            return relesed;
        };

        IAB::BufferOfAPCIdentity identity_buffer{};
        if (
            ReadIdentityBufferOfAPC(apc_slot, identity_buffer, axis) != SeqLockedOperation::FOUND
        )
        {
            ReleseAxis___();
            RevertEdgeToFree____();
            return false;
        }

        const IAB::BufferOfAPCIdentity before_identity = identity_buffer;

        EdgeBuilder::EdgeData desired_edge{};
        if (!EdgeBuilder::InstallOwnedRoot(
            identity_buffer,
            axis,
            apc_slot,
            desired_edge
        ))
        {
            ReleseAxis___();
            RevertEdgeToFree____();
            return false;
        }

        WriteAcquiredAxisDelta_(
            apc_slot,
            before_identity,
            identity_buffer,
            axis
        );

        if (!PublishReservedEdge_(desired_edge, apc_slot))
        {
            WriteAcquiredAxisDelta_(
                apc_slot,
                identity_buffer,
                before_identity,
                axis
            );
            RevertEdgeToFree____();
            ReleseAxis___();
            return false;
        }
        
        return ReleseAxis___();
    }

    bool ConstructForestOnEachAxis::AnchorADetachedChildToParent(
        uint32_t predessor_idx,
        uint32_t child_idx,
        IAB::BidirectionalAxis axis,
        IAB::DescOfInharitance inharitance,
        uint32_t internal_max_tries
    ) noexcept
    {
        if (
            predessor_idx >= CountOfAPC_ ||
            child_idx >= CountOfAPC_ ||
            predessor_idx == child_idx ||
            (
                inharitance != IAB::DescOfInharitance::FIRST_CHILD &&
                inharitance != IAB::DescOfInharitance::LINKED_CHILD
            )
        )
        {
            return false;
        }

        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);
        ForestMutationTransaction_ transaction{};
        transaction.Axis = axis;

        IAB::BufferOfAPCIdentity predessor_snapshot{};
        IAB::BufferOfAPCIdentity child_snapshot{};
        if (
            ReadIdentityBufferOfAPC(predessor_idx, predessor_snapshot, axis) != SeqLockedOperation::FOUND ||
            ReadIdentityBufferOfAPC(child_idx, child_snapshot, axis) != SeqLockedOperation::FOUND ||
            !IAB::IsInheritedAxisDisabled(child_snapshot, axis)
        )
        {
            return false;
        }

        const HeaderIdentifierOfAPC destination_unit =
            inharitance == IAB::DescOfInharitance::FIRST_CHILD
                ? map.OwnedEgdeTableIdx
                : map.InheritedEgdeTableIdx;

        const uint64_t destination_edge_raw = IAB::ValueOfAnIdentityFromBuffer(
            predessor_snapshot,
            destination_unit
        );
        const uint64_t child_owned_edge_raw = IAB::ValueOfAnIdentityFromBuffer(
            child_snapshot,
            map.OwnedEgdeTableIdx
        );
        const bool child_has_owned_edge = child_owned_edge_raw != FABRIC_CELL_SENTINAL;

        if (
            !APCDataStructure::IsValid32BitAPCUnit(destination_edge_raw) ||
            (
                child_has_owned_edge &&
                !APCDataStructure::IsValid32BitAPCUnit(child_owned_edge_raw)
            )
        )
        {
            return false;
        }

        const uint32_t destination_edge_idx = static_cast<uint32_t>(destination_edge_raw);
        const uint32_t child_owned_edge_idx = child_has_owned_edge
            ? static_cast<uint32_t>(child_owned_edge_raw)
            : APCDataStructure::APC_INDEX_BOUND_SENTINAL;

        if (
            destination_edge_idx == child_owned_edge_idx ||
            !AddForestEdgeParticipent_(transaction, destination_edge_idx) ||
            (
                child_has_owned_edge &&
                !AddForestEdgeParticipent_(transaction, child_owned_edge_idx)
            ) ||
            !ReserveLocalForestEdges_(transaction, internal_max_tries)
        )
        {
            RestoreForestEdges_(transaction, internal_max_tries);
            return false;
        }

        ForestEdgePerticipent_* destination_edge = FindForestEdgeParticipent_(
            transaction,
            destination_edge_idx
        );
        ForestEdgePerticipent_* child_owned_edge = child_has_owned_edge
            ? FindForestEdgeParticipent_(transaction, child_owned_edge_idx)
            : nullptr;

        if (
            !destination_edge ||
            !destination_edge->Before.IsValid ||
            destination_edge->Before.Status != EdgeBuilder::EdgeStatus::LIVE ||
            destination_edge->Before.EdgeTable != map.EdgeTable ||
            destination_edge->Before.OwnerAPCSlot != destination_edge_idx ||
            (
                child_has_owned_edge &&
                (
                    !child_owned_edge ||
                    !child_owned_edge->Before.IsValid ||
                    child_owned_edge->Before.Status != EdgeBuilder::EdgeStatus::LIVE ||
                    child_owned_edge->Before.EdgeTable != map.EdgeTable ||
                    child_owned_edge->Before.OwnerAPCSlot != child_idx ||
                    child_owned_edge->Before.ParentEdgeIndex !=
                        APCDataStructure::APC_INDEX_BOUND_SENTINAL
                )
            )
        )
        {
            AbroatForestMutation_(transaction);
            return false;
        }

        const bool needs_forest_guard =
            child_owned_edge && child_owned_edge->Before.OwnLinkCount != UNSIGNED_ZERO;

        uint32_t destination_root = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t child_root = APCDataStructure::APC_INDEX_BOUND_SENTINAL;

        if (needs_forest_guard)
        {
            if (
                FindForestRootEdge_(
                    destination_edge_idx,
                    child_owned_edge_idx,
                    true,
                    destination_root,
                    axis,
                    transaction
                ) != SeqLockedOperation::FOUND ||
                FindForestRootEdge_(
                    child_owned_edge_idx,
                    APCDataStructure::APC_INDEX_BOUND_SENTINAL,
                    false,
                    child_root,
                    axis,
                    transaction
                ) != SeqLockedOperation::FOUND ||
                !AddForestEdgeParticipent_(transaction, destination_root, true, true) ||
                !AddForestEdgeParticipent_(transaction, child_root, true, true) ||
                !ReserveLocalForestEdges_(transaction, internal_max_tries)
            )
            {
                AbroatForestMutation_(transaction);
                return false;
            }
        }

        destination_edge = FindForestEdgeParticipent_(transaction, destination_edge_idx);
        child_owned_edge = child_has_owned_edge
            ? FindForestEdgeParticipent_(transaction, child_owned_edge_idx)
            : nullptr;
        if (!destination_edge || (child_has_owned_edge && !child_owned_edge))
        {
            AbroatForestMutation_(transaction);
            return false;
        }

        AllRequiresApcList_ lock_slots{};
        lock_slots[0u] = predessor_idx;
        lock_slots[1u] = child_idx;
        if (!AcquiteAllIdentitiesForTransaction_(lock_slots, 2u, axis, transaction, internal_max_tries))
        {
            AbroatForestMutation_(transaction);
            return false;
        }

        ForestAPCPerticipent_* predessor = FindForestAPCParticipent_(
            transaction,
            predessor_idx
        );
        ForestAPCPerticipent_* child = FindForestAPCParticipent_(
            transaction,
            child_idx
        );

        if (
            !predessor ||
            !child ||
            IAB::ValueOfAnIdentityFromBuffer(predessor->Work, destination_unit) !=
                destination_edge_idx ||
            !IAB::IsInheritedAxisDisabled(child->Work, axis) ||
            IAB::ValueOfAnIdentityFromBuffer(child->Work, map.OwnedEgdeTableIdx) !=
                child_owned_edge_raw
        )
        {
            AbroatForestMutation_(transaction);
            return false;
        }

        if (needs_forest_guard)
        {
            uint32_t verified_destination_root = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
            uint32_t verified_child_root = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
            if (
                FindForestRootEdge_(
                    destination_edge_idx,
                    child_owned_edge_idx,
                    true,
                    verified_destination_root,
                    axis,
                    transaction
                ) != SeqLockedOperation::FOUND ||
                FindForestRootEdge_(
                    child_owned_edge_idx,
                    APCDataStructure::APC_INDEX_BOUND_SENTINAL,
                    false,
                    verified_child_root,
                    axis,
                    transaction
                ) != SeqLockedOperation::FOUND ||
                verified_destination_root != destination_root ||
                verified_child_root != child_root
            )
            {
                AbroatForestMutation_(transaction);
                return false;
            }
        }

        if (!EdgeBuilder::PrepareInharitedAxis(
            predessor->Work,
            child->Work,
            axis,
            inharitance,
            destination_edge_idx,
            destination_edge->Work,
            child_owned_edge ? &child_owned_edge->Work : nullptr
        ))
        {
            AbroatForestMutation_(transaction);
            return false;
        }

        return CommitForestMutation_(transaction);
    }



    bool ConstructForestOnEachAxis::UnlinkTwoAPC(
        uint32_t child_idx,
        IAB::BidirectionalAxis axis,
        uint32_t internal_max_tries
    ) noexcept
    {
        if (child_idx >= CountOfAPC_)
        {
            return false;
        }

        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);
        ForestMutationTransaction_ transaction{};
        transaction.Axis = axis;

        IAB::BufferOfAPCIdentity child_snapshot{};
        if (
            ReadIdentityBufferOfAPC(child_idx, child_snapshot, axis) != SeqLockedOperation::FOUND ||
            !IAB::IsInharitedChild(child_snapshot, axis)
        )
        {
            return false;
        }

        const uint64_t source_edge_raw = IAB::ValueOfAnIdentityFromBuffer(
            child_snapshot,
            map.InheritedEgdeTableIdx
        );
        const uint64_t predessor_raw = IAB::ValueOfAnIdentityFromBuffer(
            child_snapshot,
            map.PreviousSibling
        );
        const uint64_t next_raw = IAB::ValueOfAnIdentityFromBuffer(
            child_snapshot,
            map.NextSibling
        );
        const uint64_t child_owned_edge_raw = IAB::ValueOfAnIdentityFromBuffer(
            child_snapshot,
            map.OwnedEgdeTableIdx
        );

        const bool has_next = next_raw != FABRIC_CELL_SENTINAL;
        const bool has_owned_edge = child_owned_edge_raw != FABRIC_CELL_SENTINAL;

        if (
            !APCDataStructure::IsValid32BitAPCUnit(source_edge_raw) ||
            !APCDataStructure::IsValid32BitAPCUnit(predessor_raw) ||
            (
                has_next &&
                !APCDataStructure::IsValid32BitAPCUnit(next_raw)
            ) ||
            (
                has_owned_edge &&
                !APCDataStructure::IsValid32BitAPCUnit(child_owned_edge_raw)
            )
        )
        {
            return false;
        }

        const uint32_t source_edge_idx = static_cast<uint32_t>(source_edge_raw);
        const uint32_t predessor_idx = static_cast<uint32_t>(predessor_raw);
        const uint32_t next_idx = has_next
            ? static_cast<uint32_t>(next_raw)
            : APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        const uint32_t child_owned_edge_idx = has_owned_edge
            ? static_cast<uint32_t>(child_owned_edge_raw)
            : APCDataStructure::APC_INDEX_BOUND_SENTINAL;

        if (
            predessor_idx >= CountOfAPC_ ||
            predessor_idx == child_idx ||
            (
                has_next &&
                (
                    next_idx >= CountOfAPC_ ||
                    next_idx == child_idx ||
                    next_idx == predessor_idx
                )
            ) ||
            source_edge_idx == child_owned_edge_idx ||
            !AddForestEdgeParticipent_(transaction, source_edge_idx) ||
            (
                has_owned_edge &&
                !AddForestEdgeParticipent_(transaction, child_owned_edge_idx)
            ) ||
            !ReserveLocalForestEdges_(transaction, internal_max_tries)
        )
        {
            RestoreForestEdges_(transaction, internal_max_tries);
            return false;
        }

        ForestEdgePerticipent_* source_edge = FindForestEdgeParticipent_(
            transaction,
            source_edge_idx
        );
        ForestEdgePerticipent_* child_owned_edge = has_owned_edge
            ? FindForestEdgeParticipent_(transaction, child_owned_edge_idx)
            : nullptr;

        if (
            !source_edge ||
            !source_edge->Before.IsValid ||
            source_edge->Before.Status != EdgeBuilder::EdgeStatus::LIVE ||
            source_edge->Before.EdgeTable != map.EdgeTable ||
            source_edge->Before.OwnerAPCSlot != source_edge_idx ||
            source_edge->Before.OwnLinkCount == UNSIGNED_ZERO ||
            (
                has_owned_edge &&
                (
                    !child_owned_edge ||
                    !child_owned_edge->Before.IsValid ||
                    child_owned_edge->Before.Status != EdgeBuilder::EdgeStatus::LIVE ||
                    child_owned_edge->Before.EdgeTable != map.EdgeTable ||
                    child_owned_edge->Before.OwnerAPCSlot != child_idx ||
                    child_owned_edge->Before.ParentEdgeIndex != source_edge_idx
                )
            )
        )
        {
            AbroatForestMutation_(transaction);
            return false;
        }

        AllRequiresApcList_ lock_slots{};
        uint8_t lock_count = 2u;
        lock_slots[0u] = predessor_idx;
        lock_slots[1u] = child_idx;
        if (has_next)
        {
            lock_slots[lock_count++] = next_idx;
        }

        if (!AcquiteAllIdentitiesForTransaction_(lock_slots, lock_count, axis, transaction, internal_max_tries))
        {
            AbroatForestMutation_(transaction);
            return false;
        }

        ForestAPCPerticipent_* predessor = FindForestAPCParticipent_(
            transaction,
            predessor_idx
        );
        ForestAPCPerticipent_* child = FindForestAPCParticipent_(
            transaction,
            child_idx
        );
        ForestAPCPerticipent_* next = has_next
            ? FindForestAPCParticipent_(transaction, next_idx)
            : nullptr;

        if (
            !predessor ||
            !child ||
            (has_next && !next) ||
            IAB::ValueOfAnIdentityFromBuffer(child->Work, map.InheritedEgdeTableIdx) !=
                source_edge_idx ||
            IAB::ValueOfAnIdentityFromBuffer(child->Work, map.PreviousSibling) !=
                predessor_idx ||
            IAB::ValueOfAnIdentityFromBuffer(child->Work, map.NextSibling) != next_raw ||
            IAB::ValueOfAnIdentityFromBuffer(child->Work, map.OwnedEgdeTableIdx) !=
                child_owned_edge_raw
        )
        {
            AbroatForestMutation_(transaction);
            return false;
        }

        if (!EdgeBuilder::PrepareForDetachmentOfInharitedAxis(
            predessor->Work,
            child->Work,
            has_next ? &next->Work : nullptr,
            axis,
            source_edge_idx,
            source_edge->Work,
            child_owned_edge ? &child_owned_edge->Work : nullptr
        ))
        {
            AbroatForestMutation_(transaction);
            return false;
        }

        return CommitForestMutation_(transaction);
    }

    bool ConstructForestOnEachAxis::UnlinkAndRelinkToTail(
        uint32_t apc_slot_idx,
        uint32_t unlink_edge_idx,
        uint32_t relink_edge_idx,
        IAB::BidirectionalAxis axis,
        uint32_t internal_max_tries
    ) noexcept
    {
        if (
            apc_slot_idx >= CountOfAPC_ ||
            unlink_edge_idx >= CountOfAPC_ ||
            relink_edge_idx >= CountOfAPC_
        )
        {
            return false;
        }

        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);
        const bool same_edge = unlink_edge_idx == relink_edge_idx;

        ForestMutationTransaction_ transaction{};
        transaction.Axis = axis;

        IAB::BufferOfAPCIdentity child_snapshot{};
        if (
            ReadIdentityBufferOfAPC(apc_slot_idx, child_snapshot, axis) != SeqLockedOperation::FOUND ||
            !IAB::IsInharitedChild(child_snapshot, axis)
        )
        {
            return false;
        }

        const uint64_t observed_unlink_edge = IAB::ValueOfAnIdentityFromBuffer(
            child_snapshot,
            map.InheritedEgdeTableIdx
        );
        const uint64_t previous_raw = IAB::ValueOfAnIdentityFromBuffer(
            child_snapshot,
            map.PreviousSibling
        );
        const uint64_t next_raw = IAB::ValueOfAnIdentityFromBuffer(
            child_snapshot,
            map.NextSibling
        );
        const uint64_t child_owned_edge_raw = IAB::ValueOfAnIdentityFromBuffer(
            child_snapshot,
            map.OwnedEgdeTableIdx
        );

        const bool has_next = next_raw != FABRIC_CELL_SENTINAL;
        const bool has_owned_edge = child_owned_edge_raw != FABRIC_CELL_SENTINAL;

        if (
            observed_unlink_edge != unlink_edge_idx ||
            !APCDataStructure::IsValid32BitAPCUnit(previous_raw) ||
            (has_next && !APCDataStructure::IsValid32BitAPCUnit(next_raw)) ||
            (
                has_owned_edge &&
                !APCDataStructure::IsValid32BitAPCUnit(child_owned_edge_raw)
            )
        )
        {
            return false;
        }

        const uint32_t previous_idx = static_cast<uint32_t>(previous_raw);
        const uint32_t next_idx = has_next
            ? static_cast<uint32_t>(next_raw)
            : APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        const uint32_t child_owned_edge_idx = has_owned_edge
            ? static_cast<uint32_t>(child_owned_edge_raw)
            : APCDataStructure::APC_INDEX_BOUND_SENTINAL;

        if (
            previous_idx >= CountOfAPC_ ||
            previous_idx == apc_slot_idx ||
            (
                has_next &&
                (
                    next_idx >= CountOfAPC_ ||
                    next_idx == apc_slot_idx ||
                    next_idx == previous_idx
                )
            ) ||
            (!same_edge && relink_edge_idx == child_owned_edge_idx) ||
            !AddForestEdgeParticipent_(transaction, unlink_edge_idx) ||
            (!same_edge && !AddForestEdgeParticipent_(transaction, relink_edge_idx)) ||
            (
                has_owned_edge &&
                !same_edge &&
                !AddForestEdgeParticipent_(transaction, child_owned_edge_idx)
            ) ||
            !ReserveLocalForestEdges_(transaction, internal_max_tries)
        )
        {
            RestoreForestEdges_(transaction, internal_max_tries);
            return false;
        }

        ForestEdgePerticipent_* source_edge = FindForestEdgeParticipent_(
            transaction,
            unlink_edge_idx
        );
        ForestEdgePerticipent_* destination_edge = FindForestEdgeParticipent_(
            transaction,
            relink_edge_idx
        );
        ForestEdgePerticipent_* child_owned_edge = has_owned_edge && !same_edge
            ? FindForestEdgeParticipent_(transaction, child_owned_edge_idx)
            : nullptr;

        auto EdgeValid___ = [&](ForestEdgePerticipent_* part) noexcept -> bool
        {
            return
                part &&
                part->Before.IsValid &&
                part->Before.Status == EdgeBuilder::EdgeStatus::LIVE &&
                part->Before.EdgeTable == map.EdgeTable &&
                part->Before.OwnerAPCSlot == part->Index;
        };

        if (
            !EdgeValid___(source_edge) ||
            source_edge->Before.OwnLinkCount == UNSIGNED_ZERO ||
            !EdgeValid___(destination_edge) ||
            (
                has_owned_edge &&
                !same_edge &&
                (
                    !EdgeValid___(child_owned_edge) ||
                    child_owned_edge->Before.OwnerAPCSlot != apc_slot_idx ||
                    child_owned_edge->Before.ParentEdgeIndex != unlink_edge_idx
                )
            )
        )
        {
            AbroatForestMutation_(transaction);
            return false;
        }

        const bool no_op_candidate =
            same_edge &&
            !has_next &&
            source_edge->Before.Tail == apc_slot_idx;

        uint32_t relink_predessor_idx = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        IAB::DescOfInharitance relink_inharitance =
            IAB::DescOfInharitance::LINKED_CHILD;

        if (!no_op_candidate)
        {
            if (same_edge)
            {
                relink_predessor_idx = source_edge->Before.Tail;
            }
            else if (destination_edge->Before.OwnLinkCount == UNSIGNED_ZERO)
            {
                relink_predessor_idx = destination_edge->Before.OwnerAPCSlot;
                relink_inharitance = IAB::DescOfInharitance::FIRST_CHILD;
            }
            else
            {
                relink_predessor_idx = destination_edge->Before.Tail;
            }

            if (
                relink_predessor_idx >= CountOfAPC_ ||
                relink_predessor_idx == apc_slot_idx
            )
            {
                AbroatForestMutation_(transaction);
                return false;
            }
        }

        const bool needs_forest_guard =
            !same_edge &&
            child_owned_edge &&
            child_owned_edge->Before.OwnLinkCount != UNSIGNED_ZERO;

        uint32_t source_root = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t destination_root = APCDataStructure::APC_INDEX_BOUND_SENTINAL;

        if (needs_forest_guard)
        {
            if (
                FindForestRootEdge_(
                    unlink_edge_idx,
                    APCDataStructure::APC_INDEX_BOUND_SENTINAL,
                    false,
                    source_root,
                    axis,
                    transaction
                ) != SeqLockedOperation::FOUND ||
                FindForestRootEdge_(
                    relink_edge_idx,
                    child_owned_edge_idx,
                    true,
                    destination_root,
                    axis,
                    transaction
                ) != SeqLockedOperation::FOUND ||
                !AddForestEdgeParticipent_(transaction, source_root, true, true) ||
                !AddForestEdgeParticipent_(transaction, destination_root, true, true) ||
                !ReserveLocalForestEdges_(transaction, internal_max_tries)
            )
            {
                AbroatForestMutation_(transaction);
                return false;
            }
        }

        source_edge = FindForestEdgeParticipent_(transaction, unlink_edge_idx);
        destination_edge = FindForestEdgeParticipent_(transaction, relink_edge_idx);
        child_owned_edge = has_owned_edge && !same_edge
            ? FindForestEdgeParticipent_(transaction, child_owned_edge_idx)
            : nullptr;
        if (
            !source_edge ||
            !destination_edge ||
            (has_owned_edge && !same_edge && !child_owned_edge)
        )
        {
            AbroatForestMutation_(transaction);
            return false;
        }

        AllRequiresApcList_ lock_slots{};
        uint8_t lock_count = 2u;
        lock_slots[0u] = previous_idx;
        lock_slots[1u] = apc_slot_idx;
        if (has_next)
        {
            lock_slots[lock_count++] = next_idx;
        }
        if (!no_op_candidate)
        {
            lock_slots[lock_count++] = relink_predessor_idx;
        }

        if (!AcquiteAllIdentitiesForTransaction_(lock_slots, lock_count, axis, transaction, internal_max_tries))
        {
            AbroatForestMutation_(transaction);
            return false;
        }

        ForestAPCPerticipent_* previous = FindForestAPCParticipent_(
            transaction,
            previous_idx
        );
        ForestAPCPerticipent_* child = FindForestAPCParticipent_(
            transaction,
            apc_slot_idx
        );
        ForestAPCPerticipent_* next = has_next
            ? FindForestAPCParticipent_(transaction, next_idx)
            : nullptr;
        ForestAPCPerticipent_* relink_predessor = !no_op_candidate
            ? FindForestAPCParticipent_(transaction, relink_predessor_idx)
            : nullptr;

        if (
            !previous ||
            !child ||
            (has_next && !next) ||
            (!no_op_candidate && !relink_predessor) ||
            IAB::ValueOfAnIdentityFromBuffer(child->Work, map.InheritedEgdeTableIdx) !=
                unlink_edge_idx ||
            IAB::ValueOfAnIdentityFromBuffer(child->Work, map.PreviousSibling) !=
                previous_idx ||
            IAB::ValueOfAnIdentityFromBuffer(child->Work, map.NextSibling) != next_raw ||
            IAB::ValueOfAnIdentityFromBuffer(child->Work, map.OwnedEgdeTableIdx) !=
                child_owned_edge_raw
        )
        {
            AbroatForestMutation_(transaction);
            return false;
        }

        if (no_op_candidate)
        {
            RestoreForestEdges_(transaction, internal_max_tries);
            ReleseAxisReservation_(transaction);
            return true;
        }

        if (needs_forest_guard)
        {
            uint32_t verified_source_root = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
            uint32_t verified_destination_root = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
            if (
                FindForestRootEdge_(
                    unlink_edge_idx,
                    APCDataStructure::APC_INDEX_BOUND_SENTINAL,
                    false,
                    verified_source_root,
                    axis,
                    transaction
                ) != SeqLockedOperation::FOUND ||
                FindForestRootEdge_(
                    relink_edge_idx,
                    child_owned_edge_idx,
                    true,
                    verified_destination_root,
                    axis,
                    transaction
                ) != SeqLockedOperation::FOUND ||
                verified_source_root != source_root ||
                verified_destination_root != destination_root
            )
            {
                AbroatForestMutation_(transaction);
                return false;
            }
        }

        EdgeBuilder::EdgeData same_edge_owned_work{};
        EdgeBuilder::EdgeData* owned_work = nullptr;
        if (has_owned_edge)
        {
            if (same_edge)
            {
                if (
                    ReadCommittedForestEdge_(
                        child_owned_edge_idx,
                        axis,
                        same_edge_owned_work,
                        &transaction
                    ) != SeqLockedOperation::FOUND ||
                    same_edge_owned_work.OwnerAPCSlot != apc_slot_idx ||
                    same_edge_owned_work.ParentEdgeIndex != unlink_edge_idx
                )
                {
                    AbroatForestMutation_(transaction);
                    return false;
                }
                owned_work = &same_edge_owned_work;
            }
            else
            {
                owned_work = &child_owned_edge->Work;
            }
        }

        if (
            !EdgeBuilder::PrepareForDetachmentOfInharitedAxis(
                previous->Work,
                child->Work,
                has_next ? &next->Work : nullptr,
                axis,
                unlink_edge_idx,
                source_edge->Work,
                owned_work
            ) ||
            !EdgeBuilder::PrepareInharitedAxis(
                relink_predessor->Work,
                child->Work,
                axis,
                relink_inharitance,
                relink_edge_idx,
                destination_edge->Work,
                owned_work
            )
        )
        {
            AbroatForestMutation_(transaction);
            return false;
        }

        if (
            same_edge &&
            owned_work &&
            owned_work->ParentEdgeIndex != unlink_edge_idx
        )
        {
            AbroatForestMutation_(transaction);
            return false;
        }

        return CommitForestMutation_(transaction, internal_max_tries);
    }

}