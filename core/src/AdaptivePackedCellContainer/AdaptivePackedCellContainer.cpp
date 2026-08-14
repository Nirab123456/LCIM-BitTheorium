#pragma once
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{
    bool AdaptivePackedCellContainer::AttachAnotherToMe(
        AdaptivePackedCellContainer& sibbling,
        IAB::BidirectionalAxis axis,
        IAB::DescOfInharitance inharitance,
        uint32_t max_tries
    ) noexcept
    {
        uint64_t my_slot_idx = FABRIC_CELL_SENTINAL;
        uint64_t childs_slot_idx = FABRIC_CELL_SENTINAL;

        if (
            !IsThisAPCValid() ||
            !sibbling.IsThisAPCValid() ||
            !GetThisSlotIdx(my_slot_idx) ||
            !sibbling.GetThisSlotIdx(childs_slot_idx) ||
            !FabricOwnerPtr_
        )
        {
            return false;
        }
        
        return FabricOwnerPtr_->LinkTwoAPC(
            static_cast<uint32_t>(my_slot_idx),
            static_cast<uint32_t>(childs_slot_idx),
            axis,
            inharitance,
            max_tries
        );
    }

    bool AdaptivePackedCellContainer::AttachMeToAnother(
        AdaptivePackedCellContainer& sibbling,
        IAB::BidirectionalAxis axis,
        IAB::DescOfInharitance inharitance,
        uint32_t max_tries
    ) noexcept
    {
        uint64_t my_slot_idx = FABRIC_CELL_SENTINAL;
        uint64_t parents_slot_idx = FABRIC_CELL_SENTINAL;

        if (
            !IsThisAPCValid() ||
            !sibbling.IsThisAPCValid() ||
            !GetThisSlotIdx(my_slot_idx) ||
            !sibbling.GetThisSlotIdx(parents_slot_idx) ||
            !FabricOwnerPtr_
        )
        {
            return false;
        }
        
        return FabricOwnerPtr_->LinkTwoAPC(
            static_cast<uint32_t>(parents_slot_idx),
            static_cast<uint32_t>(my_slot_idx),
            axis,
            inharitance,
            max_tries
        );
    }


}