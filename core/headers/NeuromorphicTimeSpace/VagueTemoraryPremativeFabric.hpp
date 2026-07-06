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

    bool ResolveBidirectionalAxis(const APCGroupReserver::APCInitialIdentityStruct& container_cfg) noexcept;

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