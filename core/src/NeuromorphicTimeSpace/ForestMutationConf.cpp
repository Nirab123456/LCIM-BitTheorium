#pragma once
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{


    void ForestMutationConf::WriteAcquiredAxisDelta_(
        uint32_t apc_slot,
        const IAB::BufferOfAPCIdentity& before_idintity,
        const IAB::BufferOfAPCIdentity& desired_identity,
        IAB::BidirectionalAxis axis
    ) noexcept
    {
        const RangeOfAPC range = GetSegmentPoolRange(apc_slot);
        if (
            !range.IsValid ||
            IAB::ValueOfAnIdentityFromBuffer(before_idintity, HeaderIdentifierOfAPC::APC_SLOT_IDX) != apc_slot ||
            IAB::ValueOfAnIdentityFromBuffer(desired_identity, HeaderIdentifierOfAPC::APC_SLOT_IDX) != apc_slot 
        )
        {
            return;
        }

        const IAB::EasyAxisMapArray axis_map_array = IAB::GetMutableAxisArray(axis);

        for (const HeaderIdentifierOfAPC unit : axis_map_array)
        {
            if (
                before_idintity[IAB::GetBufferIdxFromIdentityUnit(unit).value()] == desired_identity[IAB::GetBufferIdxFromIdentityUnit(unit).value()]
            )
            {
                continue;
            }
            
            AtomicallyStoreU64Fab(
                range.BeginIndex + static_cast<uint8_t>(unit),
                desired_identity[IAB::GetBufferIdxFromIdentityUnit(unit).value()],
                std::memory_order_relaxed
            );
        }
    }

    bool ForestMutationConf::ReleseGraphMutationFlag_(
        uint32_t apc_slot,
        IAB::BidirectionalAxis axis,
        uint32_t max_tries
    ) noexcept
    {
        const RangeOfAPC range_of_apc_sagmant_pool = GetSegmentPoolRange(apc_slot);
        const IAB::MemGraphFlag axis_flag = IAB::GetMemGFlagFromAxis(axis);

        if (
            !range_of_apc_sagmant_pool.IsValid
        )
        {
            return false;
        }

        const size_t lock_idx = range_of_apc_sagmant_pool.BeginIndex + static_cast<uint8_t>(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK);

        uint64_t observed = FABRIC_CELL_SENTINAL;
        IAB::GraphMutationValues current_values{};

        for (size_t i = 0; i < max_tries; i++)
        {
            if (
                !AtomicallyLoadReadAUnit(lock_idx, observed) ||
                !IAB::ExtractGraphMutationValues(observed, current_values) ||
                !IAB::HasThisGraphMutationFlag(current_values.Flags, axis_flag)
            )
            {
                return false;
            }

            IAB:: GraphMutationValues desired = current_values;
            desired.Flags = IAB::ClearThisFlag(current_values.Flags, axis_flag);

            IAB::UpdateCounterOnDesiredState(desired, axis_flag);
            
            const uint64_t desired_raw = IAB::MakeGraphMutationRaw(desired);
            if (
                !IAB::IsValidGraphMutationState(desired) ||
                !APCDataStructure::IsValidFabricUnit(desired_raw)
            )
            {
                return false;
            }

            uint64_t expected = observed;

            if (
                !CompareExchangeStrongFromFabric(
                    lock_idx,
                    expected,
                    desired_raw
                )
            )
            {
                continue;
            }
            
            return true;
        }
        return false;
    }


    bool ForestMutationConf::AddForestEdgeParticipent_(
        ForestMutationTransaction_& transaction,
        uint32_t edge_idx,
        bool is_local_perticipent,
        bool is_forest_gate,
        EdgeBuilder::EdgeStatus expacted_state
    ) noexcept
    {
        if (edge_idx >= CountOfAPC_)
        {
            return false;
        }

        uint8_t insert_at = transaction.EdgeCount;

        for (uint8_t i = 0; i < transaction.EdgeCount; i++)
        {
            ForestEdgePerticipent_& current_edge_t = transaction.Edges[i];
            if (current_edge_t.Index == edge_idx)
            {
                if (current_edge_t.ExpectedStatus != expacted_state)
                {
                    return false;
                }

                current_edge_t.IsLocalParticipent = current_edge_t.IsLocalParticipent || is_local_perticipent;
                current_edge_t.IsForestGate = current_edge_t.IsForestGate || is_forest_gate;
                return true;
            }
            
            if (edge_idx < current_edge_t.Index)
            {
                insert_at = i;
                break;
            }
        }

        if (transaction.EdgeCount >= FOREST_MAX_EDGE_PERTICIPENT_)
        {
            return false;
        }

        for (uint8_t i = transaction.EdgeCount; i > insert_at; --i)
        {
            transaction.Edges[i] = transaction.Edges[i - 1u];
        }

        ForestEdgePerticipent_& inserted = transaction.Edges[insert_at];

        inserted = ForestEdgePerticipent_{};
        inserted.Index = edge_idx;
        inserted.ExpectedStatus = expacted_state;
        inserted.IsLocalParticipent = is_local_perticipent;
        inserted.IsForestGate = is_forest_gate;

        ++transaction.EdgeCount;
        return true;
    }

    ForestMutationConf::ForestEdgePerticipent_* ForestMutationConf::FindForestEdgeParticipent_(
        ForestMutationTransaction_& transaction,
        uint32_t edge_idx 
    ) noexcept
    {
        for (uint8_t i = 0; i < transaction.EdgeCount; i++)
        {
            if (transaction.Edges[i].Index == edge_idx)
            {
                return &transaction.Edges[i];
            }
        }
        return nullptr;
    }

    ForestMutationConf::ForestAPCPerticipent_* ForestMutationConf::FindForestAPCParticipent_(
        ForestMutationTransaction_& transaction,
        uint32_t apc_slot 
    ) noexcept
    {
        for (uint8_t i = 0; i < transaction.APCCount; i++)
        {
            if (transaction.Identities[i].Slot == apc_slot)
            {
                return &transaction.Identities[i];
            }
        }
        return nullptr;
    }

    bool ForestMutationConf::ReserveLocalForestEdges_(
        ForestMutationTransaction_& transaction,
        uint32_t max_tries
    ) noexcept
    {
        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(transaction.Axis);

        for (uint8_t i = 0; i < transaction.EdgeCount; i++)
        {
            ForestEdgePerticipent_& part = transaction.Edges[i];
            if (
                !part.IsLocalParticipent ||
                part.Reserved
            )
            {
                continue;
            }
            
            if (
                !ReserveAnEdge_(
                    map.EdgeTable,
                    part.Index,
                    &part.Before,
                    part.ExpectedStatus,
                    max_tries
                )
            )
            {
                RestoreForestEdges_(transaction);
                return false;
            }

            part.Work = part.Before;
            part.Reserved = true;
        }
        
        return true;
    }


    void ForestMutationConf::RestoreForestEdges_(
        ForestMutationTransaction_& transaction,
        uint32_t max_tries
    ) noexcept
    {
        const IAB::AxisConstructionMap map =IAB::ConstructAxisMap(transaction.Axis);
        auto RestorePass___ = [&](bool gates) noexcept -> void
        {
            for (
                uint8_t i = 0u;
                i < transaction.EdgeCount;
                ++i
            )
            {
                ForestEdgePerticipent_& part =
                    transaction.Edges[i];

                if (
                    part.IsForestGate != gates ||
                    (
                        !part.Reserved &&
                        !part.Published
                    )
                )
                {
                    continue;
                }

                bool restored = false;

                if (part.Published)
                {
                    if (
                        ReserveAnEdge_(
                            map.EdgeTable,
                            part.Index,
                            nullptr,
                            part.Work.Status,
                            max_tries
                        )
                    )
                    {
                        restored =
                            PublishReservedEdge_(
                                part.Before,
                                part.Index
                            );
                    }
                }
                else if (part.Reserved)
                {
                    restored =
                        PublishReservedEdge_(
                            part.Before,
                            part.Index
                        );
                }

                if (restored)
                {
                    part.Reserved = false;
                    part.Published = false;
                    part.Work = part.Before;
                }
            }
        };

        RestorePass___(false);
        RestorePass___(true);
    }

    void ForestMutationConf::RestoreForestIdentities_(
        ForestMutationTransaction_& transaction
    ) noexcept
    {
        for (uint8_t i = 0; i < transaction.APCCount; i++)
        {
            ForestAPCPerticipent_& part = transaction.Identities[i];
            if (!part.Published)
            {
                continue;
            }
            WriteAcquiredAxisDelta_(
                part.Slot,
                part.Work,
                part.Before,
                transaction.Axis
            );
            part.Published = false;
        }
    }

    void ForestMutationConf::ReleseAxisReservation_(
        ForestMutationTransaction_& transaction,
        uint32_t max_tries
    ) noexcept
    {
        for (uint8_t i = transaction.APCCount; i > 0u; --i)
        {
            ForestAPCPerticipent_& part = transaction.Identities[i - 1u];
            if (!part.Locked)
            {
                continue;
            }

            const bool relesed = ReleseGraphMutationFlag_(
                part.Slot,
                transaction.Axis,
                max_tries
            );

            if (relesed)
            {
                part.Locked = false;
            }
        }
    }

    void ForestMutationConf::AbroatForestMutation_(
        ForestMutationTransaction_ transaction,
        uint32_t max_tries 
    ) noexcept
    {
        RestoreForestIdentities_(transaction);
        RestoreForestEdges_(transaction, max_tries);
        ReleseAxisReservation_(transaction, max_tries);
    }

    bool ForestMutationConf::CommitForestMutation_(
        ForestMutationTransaction_& transaction,
        uint32_t max_tries
    ) noexcept
    {
        for (uint8_t i = 0; i < transaction.APCCount; i++)
        {
            if (!IAB::SealIdentityBuffer(transaction.Identities[i].Work))
            {
                AbroatForestMutation_(transaction, max_tries);
                return false;
            }
            ForestAPCPerticipent_& part = transaction.Identities[i];

            WriteAcquiredAxisDelta_(
                part.Slot,
                part.Before,
                part.Work,
                transaction.Axis
            );

            part.Published = true;

        }

        auto PublishPass___ = [&](bool gates) noexcept -> bool
        {
            for (uint8_t i = 0; i < transaction.EdgeCount; i++)
            {
                ForestEdgePerticipent_& part = transaction.Edges[i];
                if (part.IsForestGate != gates)
                {
                    continue;
                }
                if (!PublishReservedEdge_(
                    part.Work,
                    part.Index
                ))
                {
                    return false;
                }
                part.Reserved = false;
                part.Published = true;
            }
            return true;
        };

        if (
            !PublishPass___(false) ||
            !PublishPass___(true)
        )
        {
            AbroatForestMutation_(transaction);
            return false;
        }

        ReleseAxisReservation_(transaction);
        return true;
    }


    ForestMutationConf::SeqLockedOperation ForestMutationConf::ReadCommittedForestEdge_(
        uint32_t edge_idx,
        IAB::BidirectionalAxis axis,
        EdgeBuilder::EdgeData& edge_data,
        ForestMutationTransaction_* transaction
    ) noexcept
    {
        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);
        if (transaction)
        {
            ForestEdgePerticipent_* part = FindForestEdgeParticipent_(
                *transaction,
                edge_idx
            );

            if (part)
            {
                edge_data = part->Before;
                if (
                    edge_data.IsValid &&
                    edge_data.Status == EdgeBuilder::EdgeStatus::LIVE &&
                    edge_data.EdgeTable == map.EdgeTable &&
                    edge_data.OwnerAPCSlot == edge_idx
                )
                {
                    return SeqLockedOperation::FOUND;
                }
                return SeqLockedOperation::NONE;
            }
        }
        SeqLockedOperation outcome = ReadEdgeData_(
            map.EdgeTable,
            edge_idx,
            edge_data
        );

        if (outcome != SeqLockedOperation::FOUND)
        {
            return outcome;
        }
        
        if (
            !edge_data.IsValid ||
            edge_data.EdgeTable != map.EdgeTable ||
            edge_data.OwnerAPCSlot != edge_idx
        )
        {
            return SeqLockedOperation::NONE;
        }
        if (edge_data.Status == EdgeBuilder::EdgeStatus::RESERVED)
        {
            return SeqLockedOperation::RETRY;
        }
        
        return 
            edge_data.Status == EdgeBuilder::EdgeStatus::LIVE ?
                SeqLockedOperation::FOUND : SeqLockedOperation::NONE;
    }

}