#pragma once
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace PredictedAdaptedEncoding
{

    bool VagueTemoraryPremativeFabric::BuildAPCRuntimePtrTable_() noexcept
    {
        if (CountOfAPC_ == UNSIGNED_ZERO)
        {
            return false;
        }
        APCRuntimePtrTable_.reset(new (std::nothrow) std::atomic<AdaptivePackedCellContainer*>[static_cast<size_t>(CountOfAPC_)]);

        if (!APCRuntimePtrTable_)
        {
            return false;
        }

        for (size_t i = 0; i < static_cast<size_t>(CountOfAPC_); i++)
        {
            APCRuntimePtrTable_[i].store(nullptr, MoStoreSeq_);
        }
        
        return true;
    }

    void VagueTemoraryPremativeFabric::ClearAPCRuntimePtrTable_() noexcept
    {
        if (!APCRuntimePtrTable_)
        {
            return;
        }
        for (size_t i = 0; i < static_cast<size_t>(CountOfAPC_); i++)
        {
            APCRuntimePtrTable_[i].store(nullptr, MoStoreSeq_);
        }
    }

    bool VagueTemoraryPremativeFabric::StoreAPCRuntimePtr(size_t apc_idx, AdaptivePackedCellContainer* apc_ptr) noexcept
    {
        if (!APCRuntimePtrTable_ || apc_idx >= CountOfAPC_)
        {
            return false;
        }

        APCRuntimePtrTable_[apc_idx].store(apc_ptr, MoStoreSeq_);
        return true;
    }

    AdaptivePackedCellContainer* VagueTemoraryPremativeFabric::GetAPCRuntimePtrBySlotIndex_(size_t apc_idx) noexcept
    {
        if (!APCRuntimePtrTable_ || apc_idx >= CountOfAPC_)
        {
            return nullptr;
        }

        return APCRuntimePtrTable_[apc_idx].load(MoLoad_);
    }

    AdaptivePackedCellContainer* VagueTemoraryPremativeFabric::HandleBasedAPCPtrRetrival_(size_t apc_handle) noexcept
    {
        if (!HashIdConstructror::IsValidHashHandle(apc_handle))
        {
            return nullptr;
        }
        const size_t apc_idx = HashIdConstructror::HashTableHandlerToAPCSlotIdx(apc_handle);
        return GetAPCRuntimePtrBySlotIndex_(apc_idx);
    }


    std::optional<uint64_t> VagueTemoraryPremativeFabric::ConstructAnAPC_(   
        AdaptivePackedCellContainer& desired_apc,     
        APCGroupReserver::APCInitialIdentityStruct& container_conf,
        const LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& user_defined_weight,
        uint8_t version,
        LocalityPolicy locality
    ) noexcept
    {

        std::optional<uint64_t> desired_apc_slot = GetASlotForNewAPCLink();
        if (!desired_apc_slot.has_value())
        {
            return std::nullopt;
        }
        
        APCSegmentPoolRange desired_apc_segment_pool_range = GetSegmentPoolBegainEndForSingleAPCDescription(desired_apc_slot.value());
        if (!desired_apc_segment_pool_range.IsValid)
        {
            return std::nullopt;
        }

        const size_t capacity = desired_apc_segment_pool_range.EndIndex - desired_apc_segment_pool_range.BeginIndex;
        if (capacity != PerAPCRuntimeCellCount_)
        {
            return std::nullopt;
        }
        
        container_conf.APCSlotIndex = desired_apc_slot.value();
        if (!ResolveBothAxis_(container_conf))
        {
            return std::nullopt;
        }
        
        packed64_t* raw_apc_segment_ptr = &SlabBasePtr_[desired_apc_segment_pool_range.BeginIndex];

        if (!desired_apc.BindExternalRawFabricBacking_(
            raw_apc_segment_ptr,
            static_cast<uint16_t>(PerAPCRuntimeCellCount_),
            this,
            desired_apc_slot.value()
        ))
        {
            return std::nullopt;
        }

        if (!desired_apc.InitiateAPCMetaHeader(
            static_cast<uint16_t>(capacity),
            container_conf,
            user_defined_weight,
            version,
            locality
        ))
        {
            desired_apc.FreeAll();
            return std::nullopt;
        }
        
        if (!StoreAPCRuntimePtr(desired_apc_slot.value(), &desired_apc))
        {
            desired_apc.FreeAll();
            return std::nullopt;
        }
    
        return desired_apc_slot.value();
    }

    bool VagueTemoraryPremativeFabric::InitializeFabricWithPtrTable(
        uint16_t slot_count,
        size_t slot_cell_count,
        uint8_t slab_id,
        uint32_t fabric_thread_capacity
    ) noexcept
    {
        APCRuntimePtrTable_.reset();
        const bool base_ok = InitializeFabric(
            slot_count,
            slot_cell_count,
            slab_id,
            fabric_thread_capacity
        );

        if (!base_ok)
        {
            return false;
        }

        if (!BuildAPCRuntimePtrTable_())
        {
            ShutDownFabricWithPtrTable();
            return false;
        }
        
        return true;
    }


    bool VagueTemoraryPremativeFabric::ResolveBothAxis_(APCGroupReserver::APCInitialIdentityStruct& container_cfg) noexcept
    {
        if (!ResolveIDConfOfAPC(container_cfg))
        {
            return false;
        }
        
        const APCGroupReserver::APCIdentityDef horizontal_identity_s = APCGroupReserver::RuntimeAxisIdentityResolved(
            APCGroupReserver::BidirectionalAxis::HORIZONTAL_SHARED,
            container_cfg
        );

        const APCGroupReserver::APCIdentityDef vartical_identity_l = APCGroupReserver::RuntimeAxisIdentityResolved(
            APCGroupReserver::BidirectionalAxis::VARTICAL_LOGICAL,
            container_cfg
        );

        if (
            horizontal_identity_s == APCGroupReserver::APCIdentityDef::UNASSIGNED_UNUSED_NANNULL || 
            vartical_identity_l == APCGroupReserver::APCIdentityDef::UNASSIGNED_UNUSED_NANNULL
        )
        {
            return false;
        }

        bool horizontal_initiated = false;
        if (horizontal_identity_s != APCGroupReserver::APCIdentityDef::NULL_USER_INSTRUCTION)
        {
            horizontal_initiated = InitiateABidirectionalAxis_(container_cfg, APCGroupReserver::BidirectionalAxis::HORIZONTAL_SHARED);
        }
        else
        {
            horizontal_initiated = true;
        }

        bool vartical_initiated = false;
        if (vartical_identity_l != APCGroupReserver::APCIdentityDef::NULL_USER_INSTRUCTION)
        {
            vartical_initiated = InitiateABidirectionalAxis_(container_cfg, APCGroupReserver::BidirectionalAxis::VARTICAL_LOGICAL);
        }
        else
        {
            vartical_initiated = true;
        }
        
        if (!horizontal_initiated || !vartical_initiated)
        {
            return false;
        }
        
        return true;
        
    }



    bool VagueTemoraryPremativeFabric::InitiateABidirectionalAxis_(
        APCGroupReserver::APCInitialIdentityStruct& container_cfg,
        APCGroupReserver::BidirectionalAxis desired_axis
    ) noexcept
    {
        const APCGroupReserver::AxisConstructionMap desired_axis_map = APCGroupReserver::ConstructAxisMap(desired_axis);

        APCGroupReserver::APCIdentityDef& desired_state = (
            APCGroupReserver::IsHorizontalSharedAxis(desired_axis) ? 
            container_cfg.HorizontalSharedState : container_cfg.VarticalLogicState
        );

        uint64_t& current_group_id = (
            APCGroupReserver::IsHorizontalSharedAxis(desired_axis) ?
            container_cfg.SharedID : container_cfg.LogicalId
        );

        uint64_t& current_group_key = (
            APCGroupReserver::IsHorizontalSharedAxis(desired_axis) ?
            container_cfg.SharedHashKey : container_cfg.LogicalHashKey
        );

        uint64_t& previous_handle = (
            APCGroupReserver::IsHorizontalSharedAxis(desired_axis) ?
            container_cfg.SharedPreviousHandle : container_cfg.LogicalPreviousHandle
        );

        uint64_t& next_handle = (
            APCGroupReserver::IsHorizontalSharedAxis(desired_axis) ?
            container_cfg.SharedNextHandle : container_cfg.LogicalNextHandle
        );

        uint16_t& current_sequential_count = (
            APCGroupReserver::IsHorizontalSharedAxis(desired_axis) ?
            container_cfg.SharedSequentialCount : container_cfg.LogicalSequentalCount
        );

        if (
            desired_state == APCGroupReserver::APCIdentityDef::UNASSIGNED_UNUSED_NANNULL ||
            desired_state == APCGroupReserver::APCIdentityDef::NULL_USER_INSTRUCTION
        )
        {
            return false;
        }

        const uint64_t probable_root_key = HashIdConstructror::MakeGroupAccessKey48(current_group_id, UNSIGNED_ZERO);
        const uint64_t this_handle = HashIdConstructror::APCSlotIdxToHashTableHandler(container_cfg.APCSlotIndex);
        if (
            !HashIdConstructror::IsValidAPCId48(probable_root_key) ||
            !HashIdConstructror::IsValidAPCId48(this_handle)
        )
        {
            return false;
        }

        const std::optional<uint64_t> maybe_root_handle = FindHashValue48_(desired_axis_map.HashTable, probable_root_key);
        if (!maybe_root_handle.has_value())
        {
            if (!InsertOrUpdateRobinHoodHash48_(desired_axis_map.HashTable, probable_root_key, this_handle))
            {
                return false;
            }

            desired_state = APCGroupReserver::APCIdentityDef::ROOT;
            current_group_key = probable_root_key;
            previous_handle = PackedCell64_t::BIT_FAMILY_48_SENTINAL;
            next_handle = PackedCell64_t::BIT_FAMILY_48_SENTINAL;
            current_sequential_count = UNSIGNED_ZERO;
            return true;
        }

        AdaptivePackedCellContainer* root_apc = HandleBasedAPCPtrRetrival_(maybe_root_handle.value());

        if (!root_apc || !root_apc->IsThisAPCValid())
        {
            return false;
        }
        
        uint64_t new_sequense_count = root_apc->AtomicallyUpdateMetaCellCounter(desired_axis_map.CountTarget, 1);

        const uint64_t next_key = HashIdConstructror::MakeGroupAccessKey48(current_group_id, static_cast<uint16_t>(new_sequense_count));
        const uint64_t previous_key = HashIdConstructror::MakeGroupAccessKey48(current_group_id, static_cast<uint16_t>(new_sequense_count - 1));

        if (
            !HashIdConstructror::IsValidAPCId48(next_key) ||
            !HashIdConstructror::IsValidAPCId48(previous_key)
        )
        {
            return false;
        }
        
        const std::optional<uint64_t> maybe_previous_handle = FindHashValue48_(desired_axis_map.HashTable, previous_key);
        if (
            !maybe_previous_handle.has_value() ||
            !HashIdConstructror::IsValidAPCId48(maybe_previous_handle.value())
        )
        {
            return false;
        }
        
        if (!InsertOrUpdateRobinHoodHash48_(desired_axis_map.HashTable, next_key, this_handle))
        {
            return false;
        }
        
        desired_state = APCGroupReserver::APCIdentityDef::CHILD;
        current_group_key = next_key;
        previous_handle = maybe_previous_handle.value();
        next_handle = PackedCell64_t::BIT_FAMILY_48_SENTINAL;
        current_sequential_count = static_cast<uint16_t>(new_sequense_count);

        return true; 
    }

    // bool VagueTemoraryPremativeFabric::InstallAxisMirrorLinksAfterPublish_(
    //     const APCGroupReserver::APCInitialIdentityStruct& complete_cfg,
    //     APCGroupReserver::BidirectionalAxis desired_axis
    // ) noexcept
    // {
    //     const APCGroupReserver::AxisConstructionMap desired_axis_map = APCGroupReserver::ConstructAxisMap(desired_axis);
    //     const bool is_horizontal = APCGroupReserver::IsHorizontalSharedAxis(desired_axis);

    //     const APCGroupReserver::APCIdentityDef desired_state = is_horizontal ? complete_cfg.HorizontalSharedState : complete_cfg.VarticalLogicState;

    //     if (
    //         desired_state == APCGroupReserver::APCIdentityDef::NULL_USER_INSTRUCTION ||
    //         desired_state == APCGroupReserver::APCIdentityDef::ROOT
    //     )
    //     {
    //         return true;
    //     }

    //     const uint64_t this_handle = HashIdConstructror::APCSlotIdxToHashTableHandler(complete_cfg.APCSlotIndex);
    //     const uint64_t previous_handle = is_horizontal ? complete_cfg.SharedPreviousHandle : complete_cfg.LogicalPreviousHandle;

    //     AdaptivePackedCellContainer* previous_grouped_apc = HandleBasedAPCPtrRetrival_(previous_handle);
    //     if (
    //         !previous_grouped_apc ||
    //         !previous_grouped_apc->IsThisAPCValid() ||
    //         !HashIdConstructror::IsValidHashHandle(this_handle)
    //     )
    //     {
    //         return false;
    //     }


        
        
    // }

}
