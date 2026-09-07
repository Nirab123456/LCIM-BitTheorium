#pragma once
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{


    bool AdaptivePackedCellContainer::AddParent(
        AdaptivePackedCellContainer& parent,
        FabricSegments edge_table,
        uint32_t max_tries
    ) noexcept
    {
        APCUseScope child_use = AcquireAPCUse_();
        APCUseScope parent_use = parent.AcquireAPCUse_();

        return 
            child_use &&
            parent_use &&
            FabricOwnerPtr_ == parent.FabricOwnerPtr_ &&
            FabricOwnerPtr_->AddParentRelation_(
                parent.APCSlotIdx_,
                parent.ExpectedGeneration_,
                APCSlotIdx_,
                ExpectedGeneration_,
                edge_table,
                max_tries
            );
    }

    bool AdaptivePackedCellContainer::RemoveParent(
        AdaptivePackedCellContainer& parent,
        FabricSegments edge_table,
        uint32_t max_tries
    ) noexcept
    {
        APCUseScope child_use = AcquireAPCUse_();
        APCUseScope parent_use = parent.AcquireAPCUse_();

        return
            child_use &&
            parent_use &&
            FabricOwnerPtr_ == parent.FabricOwnerPtr_ &&
            FabricOwnerPtr_->RemoveParentRelation_(
                parent.APCSlotIdx_,
                parent.ExpectedGeneration_,
                APCSlotIdx_,
                ExpectedGeneration_,
                edge_table,
                max_tries
            );
    }

    bool AdaptivePackedCellContainer::ReplaceParent(
        AdaptivePackedCellContainer& old_parent,
        AdaptivePackedCellContainer& new_parent,
        FabricSegments edge_table,
        uint32_t max_tries
    ) noexcept
    {
        if (&old_parent == &new_parent)
        {
            return false;
        }

        APCUseScope child_use = AcquireAPCUse_();
        APCUseScope old_parent_use = old_parent.AcquireAPCUse_();
        APCUseScope new_parent_use = new_parent.AcquireAPCUse_();

        return
            child_use &&
            old_parent_use &&
            new_parent_use &&
            FabricOwnerPtr_ == old_parent.FabricOwnerPtr_ &&
            FabricOwnerPtr_ == new_parent.FabricOwnerPtr_ &&
            FabricOwnerPtr_->ReplaceParentRelation_(
                old_parent.APCSlotIdx_,
                old_parent.ExpectedGeneration_,
                new_parent.APCSlotIdx_,
                new_parent.ExpectedGeneration_,
                APCSlotIdx_,
                ExpectedGeneration_,
                edge_table,
                max_tries
            );
    }

    bool AdaptivePackedCellContainer::AttachMyChild(
        AdaptivePackedCellContainer& child,
        FabricSegments edge_table,
        uint32_t max_tries
    ) noexcept
    {
        return child.AddParent(*this, edge_table, max_tries);
    }

    bool AdaptivePackedCellContainer::DetachMyChild(
        AdaptivePackedCellContainer& child,
        FabricSegments edge_table,
        uint32_t max_tries
    ) noexcept
    {
        return child.RemoveParent(*this, edge_table, max_tries);
    }


    FabricToAPCLinker::RelationOparation
    AdaptivePackedCellContainer::FindParent(
        FabricSegments edge_table,
        uint8_t relation_ordinal,
        uint32_t max_tries
    ) noexcept
    {
        APCUseScope use = AcquireAPCUse_();
        return use
            ? FabricOwnerPtr_->FindParent_(
                APCSlotIdx_,
                ExpectedGeneration_,
                edge_table,
                relation_ordinal,
                max_tries
            )
            : RelationOparation{};
    }

    FabricToAPCLinker::RelationOparation
    AdaptivePackedCellContainer::FindFirstChild(
        FabricSegments edge_table,
        uint32_t max_tries
    ) noexcept
    {
        APCUseScope use = AcquireAPCUse_();
        return use
            ? FabricOwnerPtr_->FindFirstChild_(
                APCSlotIdx_,
                ExpectedGeneration_,
                edge_table,
                max_tries
            )
            : RelationOparation{};
    }

    FabricToAPCLinker::RelationOparation
    AdaptivePackedCellContainer::FindLastChild(
        FabricSegments edge_table,
        uint32_t max_tries
    ) noexcept
    {
        APCUseScope use = AcquireAPCUse_();
        return use
            ? FabricOwnerPtr_->FindLastChild_(
                APCSlotIdx_,
                ExpectedGeneration_,
                edge_table,
                max_tries
            )
            : RelationOparation{};
    }

    FabricToAPCLinker::RelationOparation
    AdaptivePackedCellContainer::FindNextChild(
        FabricSegments edge_table,
        uint32_t current_relation_locator,
        uint32_t max_tries
    ) noexcept
    {
        APCUseScope use = AcquireAPCUse_();
        return use
            ? FabricOwnerPtr_->FindNextChild_(
                APCSlotIdx_,
                ExpectedGeneration_,
                edge_table,
                current_relation_locator,
                max_tries
            )
            : RelationOparation{};
    }

    FabricToAPCLinker::RelationOparation
    AdaptivePackedCellContainer::FindPreviousChild(
        FabricSegments edge_table,
        uint32_t current_relation_locator,
        uint32_t max_tries
    ) noexcept
    {
        APCUseScope use = AcquireAPCUse_();
        return use
            ? FabricOwnerPtr_->FindPreviousChild_(
                APCSlotIdx_,
                ExpectedGeneration_,
                edge_table,
                current_relation_locator,
                max_tries
            )
            : RelationOparation{};
    }

    bool AdaptivePackedCellContainer::Retire(
        uint32_t max_tries
    ) noexcept
    {
        if (!IsFabricBound_())
        {
            return false;
        }

        VagueTemoraryPremativeFabric* owner = FabricOwnerPtr_;
        const uint32_t slot = APCSlotIdx_;
        const uint32_t generation = ExpectedGeneration_;

        if (!owner->RetireAPC_(slot, generation, max_tries))
        {
            return false;
        }
        owner->StoreAPCRuntimePtr(slot, nullptr);
        ReleseFabricBindingOnly_();
        return true;
    }

}