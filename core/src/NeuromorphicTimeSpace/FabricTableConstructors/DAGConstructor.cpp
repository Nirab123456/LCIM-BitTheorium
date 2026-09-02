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

}