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
            !ReadAPCMetaUnit(map.InheritedEgdeTableIdx, current_parent_edge)
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
            !ReadAPCMetaUnit(map.InheritedEgdeTableIdx, current_parent_edge) ||
            !sibbling.ReadAPCMetaUnit(map.InheritedEgdeTableIdx, sibbling_parent_edge)
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
    

    AdaptivePackedCellContainer::RelationOparation AdaptivePackedCellContainer::FindPrevious(
        IAB::BidirectionalAxis axis,
        uint32_t max_tries
    ) noexcept
    {
        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);
        IAB::GraphMutationValues gmv_values_pre{};
        IAB::GraphMutationValues gmv_values_post{};

        RelationOparation op_values{};
        if (!IsThisAPCValid())
        {
            return op_values;
        }

        uint64_t previous = FABRIC_CELL_SENTINAL;
        bool found = false;

        for (size_t i = 0; i < max_tries; i++)
        {
            if (
                !FabricOwnerPtr_->ReadGraphMutationFlags(
                    static_cast<uint32_t>(APCSlotIdx_),
                    gmv_values_pre
                ) 
            )
            {
                return op_values;
            }
            
            if (
                IAB::IsDesiredAxisLocked(gmv_values_pre.Flags, axis)
            )
            {
                continue;
            }

            if (
                !ReadAPCMetaUnit(map.PreviousSibling, previous) ||
                !FabricOwnerPtr_->ReadGraphMutationFlags(
                    static_cast<uint32_t>(APCSlotIdx_),
                    gmv_values_post
                ) 
            )
            {
                return op_values;
            }

            if (
                !IAB::IsDesiredAxisLocked(gmv_values_post.Flags, axis) &&
                IAB::CompareGmvPreVSPostSequense(gmv_values_pre, gmv_values_post, axis)
            )
            {
                found = true;
                break;
            }

        }

        if (!found)
        {
            op_values.MutationOP_ = SeqLockedOperation::RETRY;
            return op_values;
        }

        op_values.APCPtr_ = FabricOwnerPtr_->GetAPCRuntimePtrBySlotIndex_(previous);
        if (op_values.APCPtr_)
        {
            op_values.MutationOP_ = SeqLockedOperation::FOUND;
        }
        else
        {
            op_values.MutationOP_ = SeqLockedOperation::NONE;
        }
        
        return op_values;
    }


    AdaptivePackedCellContainer::RelationOparation AdaptivePackedCellContainer::FindMyNext(
        IAB::BidirectionalAxis axis,
        IAB::DescOfInharitance inharitance,
        uint32_t max_tries
    ) noexcept
    {
        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);
        IAB::GraphMutationValues gmv_values_pre{};
        IAB::GraphMutationValues gmv_values_post{};

        RelationOparation op_values{};
        if (!IsThisAPCValid())
        {
            return op_values;
        }
        
        uint64_t next = FABRIC_CELL_SENTINAL;

        HeaderIdentifierOfAPC desired_next = inharitance == IAB::DescOfInharitance::FIRST_CHILD ?
            map.RootOwnedChild : map.NextSibling;

        bool found = false;

        for (size_t i = 0; i < max_tries; i++)
        {
            if (
                !FabricOwnerPtr_->ReadGraphMutationFlags(
                    static_cast<uint32_t>(APCSlotIdx_),
                    gmv_values_pre
                ) 
            )
            {
                return op_values;
            }
            
            if (
                IAB::IsDesiredAxisLocked(gmv_values_pre.Flags, axis)
            )
            {
                continue;
            }

            if (
                !ReadAPCMetaUnit(desired_next, next) ||
                !FabricOwnerPtr_->ReadGraphMutationFlags(
                    static_cast<uint32_t>(APCSlotIdx_),
                    gmv_values_post
                ) 
            )
            {
                return op_values;
            }

            if (
                !IAB::IsDesiredAxisLocked(gmv_values_post.Flags, axis) &&
                IAB::CompareGmvPreVSPostSequense(gmv_values_pre, gmv_values_post, axis)
            )
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            op_values.MutationOP_ = SeqLockedOperation::RETRY;
            return op_values;
        }

        op_values.APCPtr_ = FabricOwnerPtr_->GetAPCRuntimePtrBySlotIndex_(next);
        if (op_values.APCPtr_)
        {
            op_values.MutationOP_ = SeqLockedOperation::FOUND;
        }
        else
        {
            op_values.MutationOP_ = SeqLockedOperation::NONE;
        }
        
        return op_values;
    }


    bool AdaptivePackedCellContainer::Retire(uint32_t max_tries) noexcept
    {
        if (!IsFabricBound_())
        {
            return false;
        }
        
        if (FabricOwnerPtr_->RetireAPC_(APCSlotIdx_, ExpectedGeneration_, max_tries))
        {
            return FabricOwnerPtr_->StoreAPCRuntimePtr(APCSlotIdx_, nullptr);
        }
        
        return false;
    }




}