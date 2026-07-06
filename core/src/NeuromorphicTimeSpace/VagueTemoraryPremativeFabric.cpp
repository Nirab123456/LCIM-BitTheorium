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

    AdaptivePackedCellContainer* VagueTemoraryPremativeFabric::GetAPCRuntimePtr(size_t apc_idx) noexcept
    {
        if (!APCRuntimePtrTable_ || apc_idx >= CountOfAPC_)
        {
            return nullptr;
        }

        return APCRuntimePtrTable_[apc_idx].load(MoLoad_);
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
        if (!desired_apc_segment_pool_range.IsVAlid)
        {
            return std::nullopt;
        }

        const size_t capacity = desired_apc_segment_pool_range.EndIndex - desired_apc_segment_pool_range.BeginIndex;
        if (capacity != PerAPCRuntimeCellCount_)
        {
            return std::nullopt;
        }
        
        container_conf.APCSlotIndex = desired_apc_slot.value();
        if (!ResolveIDConfOfAPC(container_conf))
        {
            return std::nullopt;
        }
        
        packed64_t* raw_apc_segment_ptr = &SlabBasePtr_[desired_apc_segment_pool_range.BeginIndex];

        if (!desired_apc.BindExternalRawFabricBacking_(
            raw_apc_segment_ptr,
            static_cast<uint16_t>(PerAPCRuntimeCellCount_),
            this,
            desired_apc_slot.value(),
            false
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


    /// @brief //////////// have to fix
    bool VagueTemoraryPremativeFabric::ResolveIDConfOfAPC(
        APCGroupReserver::APCInitialIdentityStruct& container_initial_conf
    ) noexcept
    {
        if (
            !APCGroupReserver::IsMinimalValidCreateRequestOfAPC(container_initial_conf) || 
            container_initial_conf.APCSlotIndex >= CountOfAPC_ 
        )
        {
            container_initial_conf.IsAssignable = false;
            return false;
        }

        const uint64_t handle = HashIdConstructror::APCSlotIdxToHashTableHandler(container_initial_conf.APCSlotIndex);
        if (!HashIdConstructror::IsValidHashHandle(handle))
        {
            container_initial_conf.IsAssignable = false;
            return false;
        }

        container_initial_conf.BranchID = HashIdConstructror::MakeARandom48bitValue();
        container_initial_conf.AccessPassword = HashIdConstructror::MakeARandom48bitValue();
        if (
            !HashIdConstructror::IsValidAPCId48(container_initial_conf.BranchID) ||
            !HashIdConstructror::IsValidAPCId48(container_initial_conf.AccessPassword)
        )
        {
            container_initial_conf.IsAssignable = false;
            return false;
        }

        
        
        

        APCGroupReserver::IsMinimalValidCreateRequestOfAPC(container_initial_conf);

        container_initial_conf.BranchID = HashIdConstructror::MakeARandom48bitValue();


        if (container_initial_conf.HorizontalSharedState != APCGroupReserver::APCIdentityDef::UNASSIGNED_UNUSED_NANNULL)
        {
            container_initial_conf.SharedHashKey = HashIdConstructror::MakeGroupAccessKey48(container_initial_conf.SharedID, UNSIGNED_ZERO);

            const std::optional<uint64_t> maybe_root_branch_handle = FindHashValue48_(FabricTableSegmentClasses::SHARED_HASH, container_initial_conf.SharedHashKey);
            if (!maybe_root_branch_handle.has_value())
            {
                if (!InsertOrUpdateRobinHoodHash48_(
                    FabricTableSegmentClasses::SHARED_HASH,
                    container_initial_conf.SharedHashKey, 
                    HashIdConstructror::APCSlotIdxToHashTableHandler(container_initial_conf.APCSlotIndex)
                ))
                {
                    return false;
                }

                container_initial_conf.SharedPreviousId = UNSIGNED_ZERO;
                container_initial_conf.SharedNextId = UNSIGNED_ZERO;
                container_initial_conf.HorizontalSharedState = APCGroupReserver::APCIdentityDef::ROOT;
            }

            const uint64_t root_apc_slot_idx = HashIdConstructror::HashTableHandlerToAPCSlotIdx(*maybe_root_branch_handle);
            AdaptivePackedCellContainer* root_apc = GetAPCRuntimePtr(root_apc_slot_idx);
            if (!root_apc || !root_apc->IsThisAPCValid())
            {
                return false;  ///////????
            }
            
            

        }



        
        return true;
    }


}
