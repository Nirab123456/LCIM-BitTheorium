#pragma once
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace PredictedAdaptedEncoding
{

    std::optional<uint64_t> VagueTemoraryPremativeFabric::ConstructAnAPC_(   
        AdaptivePackedCellContainer& desired_apc,     
        APCGroupReserver::APCInitialIdentityStruct& container_conf,
        const LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& user_defined_weight,
        uint8_t version,
        LocalityPolicy locality
    ) noexcept
    {
        if (desired_apc.IsThisAPCValid())
        {
            return std::nullopt;
        }

        if (
            !FabricInitialized_.load(MoLoad_) ||
            !SlabBasePtr_ || 
            PerAPCRuntimeCellCount_ < MINIMUM_APC_CAPACITY ||
            CountOfAPC_ == UNSIGNED_ZERO ||
            CountOfAPC_ >= PackedCell64_t::BIT_FAMILY_48_SENTINAL
        )
        {
            return std::nullopt;
        }

        uint64_t desired_apc_slot = PackedCell64_t::PACKED_CELL_SENTINAL;

        for (uint64_t description_idx = 0; description_idx < CountOfAPC_; description_idx++)
        {
            const DescriptionOfAPC::DescriptorSaftyFiles desired_files = OneShotTryReadingDescriptionState_(description_idx);
            if (
                desired_files.IsValid && 
                desired_files.WidthOfAPC == PerAPCRuntimeCellCount_ &&
                desired_files.LocalityOfTheDescription ==LocalityPolicy::PUBLISHED && 
                desired_files.WhoHoldsTheAcess != OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER &&  
                desired_files.StateOfTheAPC == DescriptionOfAPC::StateOfSingleAPCDescription::RECORD_WITH_SEGMENT_POOL
            )
            {
                if (ClaimACompleateAPCDescriptorCells(description_idx))
                {
                    desired_apc_slot = description_idx;
                    break;
                }
            }
        }

        if (desired_apc_slot >= CountOfAPC_)
        {
            return std::nullopt;
        }

        DescriptionOfAPC::SingleAPCDescriptionCellBuffer  desired_apc_description_buffer{};
        
        const bool buffer_ok = ReadACompleateAPCDescriptorBuffer(
            desired_apc_slot, 
            desired_apc_description_buffer, 
            false, 
            OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC, 
            DescriptionOfAPC::StateOfSingleAPCDescription::RECORD_WITH_SEGMENT_POOL,
            std::nullopt
        );
        if (!buffer_ok)
        {
            return std::nullopt;
        }

        const packed64_t updated_safty = DescriptionOfAPC::SwitchStateOrAPCOwnerOfSaftyCell(
            desired_apc_description_buffer[static_cast<size_t>(APCDescriptorCellType::STATE_OWNERSHIP_VESION_SAFTY)],
            DescriptionOfAPC::StateOfSingleAPCDescription::OWNED_BY_APC,
            OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER
        );

        const bool update_ok_safty = DescriptionOfAPC::SetStateSaftyCellInBuffer(desired_apc_description_buffer, updated_safty);
        if (!update_ok_safty)
        {
            return std::nullopt;
        }


        const bool claimed_descripor_for_caller = OneShotUpdateAPCDescriptor(desired_apc_description_buffer, true);
        if (!claimed_descripor_for_caller)
        {
            return std::nullopt;
        }
        
        APCSegmentPoolRange desired_apc_segment_pool_range = GetSegmentPoolBegainEndForSingleAPCDescription(desired_apc_slot);
        if (!desired_apc_segment_pool_range.IsVAlid)
        {
            return std::nullopt;
        }

        const size_t capacity = desired_apc_segment_pool_range.EndIndex - desired_apc_segment_pool_range.BeginIndex;
        if (capacity != PerAPCRuntimeCellCount_)
        {
            return std::nullopt;
        }
        

        packed64_t* raw_apc_segment_ptr = &SlabBasePtr_[desired_apc_segment_pool_range.BeginIndex];

        if (!desired_apc.BindExternalRawFabricBacking_(
            raw_apc_segment_ptr,
            static_cast<uint16_t>(PerAPCRuntimeCellCount_),
            this,
            desired_apc_slot,
            false
        ))
        {
            return std::nullopt;
        }

        const uint64_t branch_id = HashIdConstructror::MakeARandom48bitValue();
        const uint64_t final_logical_id = 000000;
        const uint64_t final_shared_id = 0000000;

        if (!desired_apc.InitOnFabricBackingAfterBind(
            static_cast<uint16_t>(capacity),
            container_conf
        ))
        {
            desired_apc.FreeAll();
            return std::nullopt;
        }
        

        if (!StoreAPCRuntimePtr(desired_apc_slot, &desired_apc))
        {
            desired_apc.FreeAll();
            return std::nullopt;
        }


        const uint64_t branch_retire_lock = 00000000000000;
        if (branch_retire_lock == UNSIGNED_ZERO || branch_retire_lock >= PackedCell64_t::BIT_FAMILY_48_SENTINAL)
        {
            desired_apc.FreeAll();
            return std::nullopt;
        }
        
        const bool branch_ok = InsertOrUpdateRobinHoodHash48_(FabricTableSegmentClasses::BRANCH_HASH, branch_id, branch_retire_lock);
        const bool logical_ok = InsertOrUpdateRobinHoodHash48_(FabricTableSegmentClasses::LOGICAL_HASH, final_logical_id, branch_id);
        const bool shared_ok = InsertOrUpdateRobinHoodHash48_(FabricTableSegmentClasses::SHARED_HASH, final_shared_id, branch_id);

        packed64_t current_apc_count_cell = AtomicallyLoadReadCompletePackedCell(static_cast<size_t>(FabricMetaIndicies::TOTAL_APC_IN_USE));

        const packed64_t updated_apc_count_cell = CoreOfFabricCoordinator::UpdateACountMetaCell(
            current_apc_count_cell,
            FabricMetaIndicies::TOTAL_APC_IN_USE,
            1u
        );

        if (!branch_ok || !logical_ok || !shared_ok || updated_apc_count_cell == PackedCell64_t::PACKED_CELL_SENTINAL)
        {
            StoreAPCRuntimePtr(desired_apc_slot, nullptr);
            desired_apc.FreeAll();
            return std::nullopt;
        }
        
        CompareExchangeStrongFromFabric(
            static_cast<size_t>(FabricMetaIndicies::TOTAL_APC_IN_USE),
            current_apc_count_cell,
            updated_apc_count_cell
        );

        return desired_apc_slot;
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


    bool VagueTemoraryPremativeFabric::ResolveIDConfOfAPC(
        APCGroupReserver::APCInitialIdentityStruct& a_initial_acp_conf
    ) noexcept
    {
        if (!APCGroupReserver::IsMinimalValidCreateRequestOfAPC(a_initial_acp_conf) || a_initial_acp_conf.APCSlotIndex >= CountOfAPC_)
        {
            a_initial_acp_conf.IsAssignable = false;
            return false;
        }

        APCGroupReserver::IsMinimalValidCreateRequestOfAPC(a_initial_acp_conf);

        a_initial_acp_conf.BranchID = HashIdConstructror::MakeARandom48bitValue();


        if (a_initial_acp_conf.HorizontalSharedState != APCGroupReserver::APCIdentityDef::UNASSIGNED_UNUSED_NANNULL)
        {
            a_initial_acp_conf.SharedHashKey = HashIdConstructror::MakeGroupAccessKey48(a_initial_acp_conf.SharedID, UNSIGNED_ZERO);

            const std::optional<uint64_t> maybe_root_branch_handle = FindHashValue48_(FabricTableSegmentClasses::SHARED_HASH, a_initial_acp_conf.SharedHashKey);
            if (!maybe_root_branch_handle.has_value())
            {
                if (!InsertOrUpdateRobinHoodHash48_(
                    FabricTableSegmentClasses::SHARED_HASH,
                    a_initial_acp_conf.SharedHashKey, 
                    HashIdConstructror::APCSlotIdxToHashTableHandler(a_initial_acp_conf.APCSlotIndex)
                ))
                {
                    return false;
                }

                a_initial_acp_conf.SharedPreviousId = UNSIGNED_ZERO;
                a_initial_acp_conf.SharedNextId = UNSIGNED_ZERO;
                a_initial_acp_conf.HorizontalSharedState = APCGroupReserver::APCIdentityDef::ROOT;
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
