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

public:

    bool InitializeFabricWithPtrTable(
        uint16_t slot_count,
        size_t slot_cell_count = MINIMUM_APC_CELL_COUNT,
        uint8_t slab_id = APCDataStructure::BRANCH_VERSION,
        uint32_t fabric_thread_capacity = UNSIGNED_ZERO
    ) noexcept;

    void ShutDownFabricWithPtrTable() noexcept
    {
        ClearAPCRuntimePtrTable_();
        APCRuntimePtrTable_.reset();
        ShutDownFabric();
    }

};


}