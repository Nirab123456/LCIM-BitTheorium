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

        if (
            !IsThisAPCValid() ||
            !sibbling.IsThisAPCValid()
        )
        {
            return false;
        }
        
        return FabricOwnerPtr_->LinkTwoAPC(
            APCSlotIdx_,
            sibbling.APCSlotIdx_,
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
        if (
            !IsThisAPCValid() ||
            !sibbling.IsThisAPCValid()
        )
        {
            return false;
        }
        
        return FabricOwnerPtr_->LinkTwoAPC(
            sibbling.APCSlotIdx_,
            APCSlotIdx_,
            axis,
            inharitance,
            max_tries
        );
    }

    bool AdaptivePackedCellContainer::DetachMeFromAnotherEdge(
        IAB::BidirectionalAxis axis,
        uint32_t max_tries
    ) noexcept
    {
        return 
            IsThisAPCValid() &&
            FabricOwnerPtr_->UnlinkTwoAPC(
                static_cast<uint32_t>(APCSlotIdx_),
                axis,
                max_tries
            );
    }

    bool AdaptivePackedCellContainer::DetachMyChild(
        AdaptivePackedCellContainer& sibbling,
        IAB::BidirectionalAxis axis,
        uint32_t max_tries 
    ) noexcept
    {
        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);

        uint64_t childs_current_axis_inharitance = FABRIC_CELL_SENTINAL;

        if (
            !IsThisAPCValid() ||
            !sibbling.ReadAPCMetaUnit(map.InheritedEgdeTableIdx, childs_current_axis_inharitance) ||
            APCSlotIdx_ != childs_current_axis_inharitance
        )
        {
            return false;
        }
        return sibbling.DetachMeFromAnotherEdge(
            axis,
            max_tries
        );
    }


    AdaptivePackedCellContainer* AdaptivePackedCellContainer::FindPrevious(IAB::BidirectionalAxis axis) noexcept
    {
        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);
        IAB::GraphMutationValues gmv_values{};

        uint64_t previous = FABRIC_CELL_SENTINAL;

        if (
            !IsThisAPCValid() ||
            !FabricOwnerPtr_->ReadGraphMutationFlags(
                static_cast<uint32_t>(APCSlotIdx_),
                gmv_values
            ) ||
            !gmv_values.IsValid ||
            IAB::IsDesiredAxisLocked(gmv_values.Flags, axis) ||
            !ReadAPCMetaUnit(map.PreviousSibling, previous) ||
            !APCDataStructure::IsValid32BitAPCUnit(previous)
        )
        {
            return nullptr;
        }

        return FabricOwnerPtr_->GetAPCRuntimePtrBySlotIndex_(previous);
    }


    AdaptivePackedCellContainer* AdaptivePackedCellContainer::FindMyNext(
        IAB::BidirectionalAxis axis,
        IAB::DescOfInharitance inharitance
    ) noexcept
    {
        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);
        IAB::GraphMutationValues gmv_values{};
        uint64_t desired_next = FABRIC_CELL_SENTINAL;

        const HeaderIdentifierOfAPC next_on_inharitance = inharitance == IAB::DescOfInharitance::FIRST_CHILD ?
            map.RootOwnedChild : map.NextSibling;

        if (
            !IsThisAPCValid() ||
            !FabricOwnerPtr_->ReadGraphMutationFlags(
                static_cast<uint32_t>(APCSlotIdx_),
                gmv_values
            ) ||
            !gmv_values.IsValid ||
            IAB::IsDesiredAxisLocked(gmv_values.Flags, axis) ||
            !ReadAPCMetaUnit(next_on_inharitance, desired_next) ||
            !APCDataStructure::IsValid32BitAPCUnit(desired_next)
        )
        {
            return nullptr;
        }

        return FabricOwnerPtr_->GetAPCRuntimePtrBySlotIndex_(desired_next);

    }



}