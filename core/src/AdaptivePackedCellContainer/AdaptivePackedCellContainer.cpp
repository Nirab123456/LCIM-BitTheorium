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

    bool AdaptivePackedCellContainer::DetachMeFromAnotherEdge(
        IAB::BidirectionalAxis axis,
        uint32_t max_tries
    ) noexcept
    {
        uint64_t my_slot = FABRIC_CELL_SENTINAL;

        if (
            !IsThisAPCValid() ||
            !GetThisSlotIdx(my_slot)
        )
        {
            return false;
        }

        return FabricOwnerPtr_->UnlinkTwoAPC(
            static_cast<uint32_t>(my_slot),
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
        uint64_t my_slot_idx = FABRIC_CELL_SENTINAL;

        if (
            !IsThisAPCValid() ||
            !GetThisSlotIdx(my_slot_idx) ||
            !sibbling.ReadAPCMetaUnit(map.InheritedEgdeTableIdx, childs_current_axis_inharitance) ||
            my_slot_idx != childs_current_axis_inharitance
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
        
        uint64_t slot_idx = FABRIC_CELL_SENTINAL;

        uint64_t previous = FABRIC_CELL_SENTINAL;

        if (
            !IsThisAPCValid() ||
            !GetThisSlotIdx(slot_idx) ||
            !FabricOwnerPtr_->ReadGraphMutationFlags(
                static_cast<uint32_t>(slot_idx),
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

        DescriptionOfAPC::DescriptionLockValues previous_apc_dsc_lock = FabricOwnerPtr_->ReadAPCStateAtomically_(previous);

        if (
            previous_apc_dsc_lock.IsValid && 
            IsLiveSateOfAPC(previous_apc_dsc_lock.StateOfTheAPC)
        )
        {
            return FabricOwnerPtr_->GetAPCRuntimePtrBySlotIndex_(previous);
        }
        return nullptr;
    }


    AdaptivePackedCellContainer* AdaptivePackedCellContainer::FindMyNext(
        IAB::BidirectionalAxis axis,
        IAB::DescOfInharitance inharitance
    ) noexcept
    {
        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);
        IAB::GraphMutationValues gmv_values{};
        uint64_t slot_idx = FABRIC_CELL_SENTINAL;
        uint64_t desired_next = FABRIC_CELL_SENTINAL;

        const HeaderIdentifierOfAPC next_on_inharitance = inharitance == IAB::DescOfInharitance::FIRST_CHILD ?
            map.NextSibling : map.RootOwnedChild;

        if (
            !IsThisAPCValid() ||
            !GetThisSlotIdx(slot_idx) ||
            !FabricOwnerPtr_->ReadGraphMutationFlags(
                static_cast<uint32_t>(slot_idx),
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
        DescriptionOfAPC::DescriptionLockValues next_apc_dsc_lock = FabricOwnerPtr_->ReadAPCStateAtomically_(desired_next);
        if (
            next_apc_dsc_lock.IsValid && 
            IsLiveSateOfAPC(next_apc_dsc_lock.StateOfTheAPC)
        )
        {
            return FabricOwnerPtr_->GetAPCRuntimePtrBySlotIndex_(desired_next);
        }
        return nullptr;
    }



}