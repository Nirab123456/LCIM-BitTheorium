#pragma once
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{
    bool AdaptivePackedCellContainer::AttachSiblingOrChild(
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
        
        return FabricOwnerPtr_->AnchorADetachedChildToParent(
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
        
        return FabricOwnerPtr_->AnchorADetachedChildToParent(
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

    bool AdaptivePackedCellContainer::DetachAndReAttachMeToThisParent(
        AdaptivePackedCellContainer& root_parent,
        IAB::BidirectionalAxis axis,
        uint32_t max_tries
    ) noexcept
    {
        IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);

        uint64_t current_parent_edge = FABRIC_CELL_SENTINAL;

        if (
            !IsThisAPCValid() ||
            !root_parent.IsThisAPCValid() ||
            !ReadAPCMetaUnit(map.InheritedEgdeTableIdx, current_parent_edge, true)
        )
        {
            return false;
        }

        if (current_parent_edge == FABRIC_CELL_SENTINAL)
        {
            return AttachMeToAnother(root_parent, axis, IAB::DescOfInharitance::FIRST_CHILD);
        }

        return FabricOwnerPtr_->UnlinkAndRelinkToTail(
            APCSlotIdx_,
            static_cast<uint32_t>(current_parent_edge),
            root_parent.APCSlotIdx_,
            axis,
            max_tries
        );
    }


    bool AdaptivePackedCellContainer::DetachAndReattachMeAsEquivelentSibbling(
        AdaptivePackedCellContainer& sibbling,
        IAB::BidirectionalAxis axis,
        uint32_t max_tries
    ) noexcept
    {
        IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);

        uint64_t current_parent_edge = FABRIC_CELL_SENTINAL;
        uint64_t sibbling_parent_edge = FABRIC_CELL_SENTINAL;

        if (
            !IsThisAPCValid() ||
            !sibbling.IsThisAPCValid() ||
            !ReadAPCMetaUnit(map.InheritedEgdeTableIdx, current_parent_edge, true) ||
            !sibbling.ReadAPCMetaUnit(map.InheritedEgdeTableIdx, sibbling_parent_edge, true)
        )
        {
            return false;
        }

        if (current_parent_edge == FABRIC_CELL_SENTINAL)
        {
            return AttachMeToAnother(sibbling, axis, IAB::DescOfInharitance::LINKED_CHILD);
        }

        return FabricOwnerPtr_->UnlinkAndRelinkToTail(
            APCSlotIdx_,
            static_cast<uint32_t>(current_parent_edge),
            static_cast<uint32_t>(sibbling_parent_edge),
            axis,
            max_tries
        );

    }
    

    AdaptivePackedCellContainer* AdaptivePackedCellContainer::FindPrevious(
        IAB::BidirectionalAxis axis,
        uint32_t max_tries
    ) noexcept
    {
        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);
        IAB::GraphMutationValues gmv_values_pre{};
        IAB::GraphMutationValues gmv_values_post{};

        if (!IsThisAPCValid())
        {
            return nullptr;
        }
        
        uint64_t previous = FABRIC_CELL_SENTINAL;


        for (size_t i = 0; i < max_tries; i++)
        {
            if (
                !FabricOwnerPtr_->ReadGraphMutationFlags(
                    static_cast<uint32_t>(APCSlotIdx_),
                    gmv_values_pre
                ) ||
                IAB::IsDesiredAxisLocked(gmv_values_pre.Flags, axis)
            )
            {
                continue;
            }

            if (!ReadAPCMetaUnit(map.PreviousSibling, previous))
            {
                continue;
            }

            if (
                !FabricOwnerPtr_->ReadGraphMutationFlags(
                    static_cast<uint32_t>(APCSlotIdx_),
                    gmv_values_post
                ) ||
                IAB::IsDesiredAxisLocked(gmv_values_post.Flags, axis) ||
                !IAB::CompareGmvPreVSPostSequense(gmv_values_pre, gmv_values_post, axis)
            )
            {
                continue;
            }

            if (
                !APCDataStructure::IsValid32BitAPCUnit(previous)
            )
            {
                return nullptr;
            }

            return FabricOwnerPtr_->GetAPCRuntimePtrBySlotIndex_(previous);
        }

        return nullptr;
    }


    AdaptivePackedCellContainer* AdaptivePackedCellContainer::FindMyNext(
        IAB::BidirectionalAxis axis,
        IAB::DescOfInharitance inharitance,
        uint32_t max_tries
    ) noexcept
    {
        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);
        IAB::GraphMutationValues gmv_values_pre{};
        IAB::GraphMutationValues gmv_values_post{};

        if (!IsThisAPCValid())
        {
            return nullptr;
        }
        
        uint64_t next = FABRIC_CELL_SENTINAL;

        HeaderIdentifierOfAPC desired_next = inharitance == IAB::DescOfInharitance::FIRST_CHILD ?
            map.RootOwnedChild : map.NextSibling;

        for (size_t i = 0; i < max_tries; i++)
        {
            if (
                !FabricOwnerPtr_->ReadGraphMutationFlags(
                    static_cast<uint32_t>(APCSlotIdx_),
                    gmv_values_pre
                ) ||
                IAB::IsDesiredAxisLocked(gmv_values_pre.Flags, axis)
            )
            {
                continue;
            }

            if (!ReadAPCMetaUnit(desired_next, next))
            {
                continue;
            }

            if (
                !FabricOwnerPtr_->ReadGraphMutationFlags(
                    static_cast<uint32_t>(APCSlotIdx_),
                    gmv_values_post
                ) ||
                IAB::IsDesiredAxisLocked(gmv_values_post.Flags, axis) ||
                !IAB::CompareGmvPreVSPostSequense(gmv_values_pre, gmv_values_post, axis)
            )
            {
                continue;
            }

            if (
                !APCDataStructure::IsValid32BitAPCUnit(next)
            )
            {
                return nullptr;
            }

            return FabricOwnerPtr_->GetAPCRuntimePtrBySlotIndex_(next);
        }

        return nullptr;
    }



}