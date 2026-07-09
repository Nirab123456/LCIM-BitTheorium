#pragma once 
#include "SlabToFabricConverterAndCordinator.h"

namespace PredictedAdaptedEncoding
{

class VagueTemoraryPremativeFabric : public SlabToFabricConverterAndCordinator
{

private:

    /// @brief IN FUTURE EITHER GET RID OF THE TABLE OR: STORE INSIDE FABRICE BY USING  WildCardOfPackedCell::RAW_30x2BIT
    std::unique_ptr<std::atomic<AdaptivePackedCellContainer*>[]> APCRuntimePtrTable_{nullptr};

    bool BuildAPCRuntimePtrTable_() noexcept;

    void ClearAPCRuntimePtrTable_() noexcept;

    bool StoreAPCRuntimePtr(size_t apc_idx, AdaptivePackedCellContainer* apc_ptr) noexcept;

    AdaptivePackedCellContainer* GetAPCRuntimePtrBySlotIndex_(size_t apc_idx) noexcept;
    AdaptivePackedCellContainer* HandleBasedAPCPtrRetrival_(size_t apc_handle) noexcept;

    bool InitiateABidirectionalAxis_(
        APCGroupReserver::APCInitialIdentityStruct& container_cfg,
        APCGroupReserver::BidirectionalAxis desired_axis
    ) noexcept;
    
    bool ResolveBothAxis_(APCGroupReserver::APCInitialIdentityStruct& container_cfg) noexcept;

    bool InstallAxisMirrorLinksAfterPublish_(
        const APCGroupReserver::APCInitialIdentityStruct& complete_cfg,
        APCGroupReserver::BidirectionalAxis desired_axis
    ) noexcept;

public:

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
        ClearAPCRuntimePtrTable_();
        APCRuntimePtrTable_.reset();
        ShutDownFabric();
    }

};


}