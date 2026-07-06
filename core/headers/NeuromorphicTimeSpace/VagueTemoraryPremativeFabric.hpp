#pragma once 
#include "SlabToFabricConverterAndCordinator.h"

namespace PredictedAdaptedEncoding
{

class VagueTemoraryPremativeFabric : public SlabToFabricConverterAndCordinator
{

private:

    /// @brief IN FUTURE EITHER GET RID OF THE TABLE OR: STORE INSIDE FABRICE BY USING  AttributePolicy::INSTRUCTION_RAW64_NEXT
    std::unique_ptr<std::atomic<AdaptivePackedCellContainer*>[]> APCRuntimePtrTable_{nullptr};
    std::vector<std::unique_ptr<AdaptivePackedCellContainer>> FabricOwnedAPCViews_{};

    bool BuildAPCRuntimePtrTable_() noexcept;

    void ClearAPCRuntimePtrTable_() noexcept;

    bool StoreAPCRuntimePtr(size_t apc_idx, AdaptivePackedCellContainer* apc_ptr) noexcept;

    AdaptivePackedCellContainer* GetAPCRuntimePtr(size_t apc_idx) noexcept;

    bool InitiateABidirectionalAxis_(
        APCGroupReserver::APCInitialIdentityStruct& container_cfg,
        APCGroupReserver::BidirectionalAxis desired_axis
    ) noexcept;

    bool ResolveBothAxis_(APCGroupReserver::APCInitialIdentityStruct& container_cfg) noexcept
    {
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

public:

    bool ResolveIDConfOfAPC(
        APCGroupReserver::APCInitialIdentityStruct& a_initial_acp_conf
    ) noexcept;

    std::optional<uint64_t> ConstructAnAPC_(   
        AdaptivePackedCellContainer& desired_apc,     
        APCGroupReserver::APCInitialIdentityStruct& container_conf,
        const LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& user_defined_weight = LayoutBoundsOrchestrator::DEFAULT_LAYOUT_WEIGHT,
        uint8_t version = APCDataStructure::BRANCH_VERSION,
        LocalityPolicy locality = LocalityPolicy::PUBLISHED
    ) noexcept;

    bool InitializeFabricWithPtrTable(
        uint16_t slot_count,
        size_t slot_cell_count = MINIMUM_APC_CAPACITY,
        uint8_t slab_id = APCDataStructure::BRANCH_VERSION,
        uint32_t fabric_thread_capacity = DEFAULT_THREAD_TABLE_CAPACITY
    ) noexcept;

    void ShutDownFabricWithPtrTable() noexcept
    {
        FabricOwnedAPCViews_.clear();
        ClearAPCRuntimePtrTable_();
        APCRuntimePtrTable_.reset();
        ShutDownFabric();
    }

    AdaptivePackedCellContainer* GetAPCRuntimePtrByBranchId(uint64_t apc_slot_idx) noexcept
    {
        if (apc_slot_idx == UNSIGNED_ZERO || apc_slot_idx >= PackedCell64_t::BIT_FAMILY_48_SENTINAL)
        {
            return nullptr;
        }
        
        return GetAPCRuntimePtr(apc_slot_idx);
    }

};


}