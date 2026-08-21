#pragma once
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{

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
        ForestMutationTransaction_ transaction,
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
        ForestMutationTransaction_ transaction,
        uint32_t apc_slot 
    ) noexcept
    {
        for (uint8_t i = 0; i < transaction.APCCount; i++)
        {
            if (transaction.Identities[i].slot == apc_slot)
            {
                return &transaction.Identities[i];
            }
        }
        return nullptr;
    }

    bool ForestMutationConf::ReserveLocalForestEdges_(
        ForestMutationTransaction_ transaction,
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

}