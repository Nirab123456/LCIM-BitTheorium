#pragma once 
#include "SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{

class VagueTemoraryPremativeFabric : public ConstructDAGOnEachAxis
{
    friend class AdaptivePackedCellContainer;
private:
    /// @brief IN FUTURE EITHER GET RID OF THE TABLE OR: STORE INSIDE FABRICE BY USING  WildCardOfPackedCell::RAW_30x2BIT
    std::unique_ptr<std::atomic<AdaptivePackedCellContainer*>[]> APCRuntimePtrTable_{nullptr};

    bool BuildAPCRuntimePtrTable_() noexcept;

    void ClearAPCRuntimePtrTable_() noexcept;

    bool StoreAPCRuntimePtr(size_t apc_idx, AdaptivePackedCellContainer* apc_ptr) noexcept;

    AdaptivePackedCellContainer* GetAPCRuntimePtrBySlotIndex_(size_t apc_idx) noexcept;

    std::optional<uint32_t> GetASlotForNewAPCLink() noexcept;        

    SeqLockedOperation ResolveChildLocator_(
        uint32_t parent_slot,
        uint32_t parent_generation,
        FabricSegments edge_table,
        uint32_t locator,
        AdaptivePackedCellContainer*& child
    ) noexcept;

    FabricToAPCLinker::RelationOparation FindParent_(
        uint32_t child_slot,
        uint32_t child_generation,
        FabricSegments edge_table,
        uint8_t relation_ordinal,
        uint32_t max_tries
    ) noexcept;

    FabricToAPCLinker::RelationOparation FindFirstChild_(
        uint32_t parent_slot,
        uint32_t parent_generation,
        FabricSegments edge_table,
        uint32_t max_tries
    ) noexcept;

    FabricToAPCLinker::RelationOparation FindLastChild_(
        uint32_t parent_slot,
        uint32_t parent_generation,
        FabricSegments edge_table,
        uint32_t max_tries
    ) noexcept;

    FabricToAPCLinker::RelationOparation FindNextChild_(
        uint32_t parent_slot,
        uint32_t parent_generation,
        FabricSegments edge_table,
        uint32_t current_relation_locator,
        uint32_t max_tries
    ) noexcept;

    FabricToAPCLinker::RelationOparation FindPreviousChild_(
        uint32_t parent_slot,
        uint32_t parent_generation,
        FabricSegments edge_table,
        uint32_t current_relation_locator,
        uint32_t max_tries
    ) noexcept;

public:

    bool InitializeFabricWithPtrTable(
        uint32_t slot_count,
        uint32_t slot_cell_count = MINIMUM_APC_CELL_COUNT,
        uint8_t max_direct_parents_per_axis = DEFAULT_DIRECTED_PARENT_PER_AXIS
    ) noexcept;

    void ShutDownFabricWithPtrTable() noexcept
    {
        ClearAPCRuntimePtrTable_();
        APCRuntimePtrTable_.reset();
        ShutDownFabric();
    }

    bool CreateAPC(
        AdaptivePackedCellContainer& desired_apc,
        bool wants_horizontal_root,
        bool wants_vertical_root,
        const LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& layout,
        const SchemaDefinition::InitialRegionalDtypeConf& dtype,
        const SchemaDefinition::InitialRegionalProtocol& protocol,
        uint8_t version,
        uint32_t internal_max_tries = DEFAULT_MAX_TRIES
    ) noexcept;
    
};


}