#pragma once 
#include "SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{

class VagueTemoraryPremativeFabric : public ConstructAPCIdentity
{

private:

    /// @brief IN FUTURE EITHER GET RID OF THE TABLE OR: STORE INSIDE FABRICE BY USING  WildCardOfPackedCell::RAW_30x2BIT
    std::unique_ptr<std::atomic<AdaptivePackedCellContainer*>[]> APCRuntimePtrTable_{nullptr};

    bool BuildAPCRuntimePtrTable_() noexcept;

    void ClearAPCRuntimePtrTable_() noexcept;

    bool StoreAPCRuntimePtr(size_t apc_idx, AdaptivePackedCellContainer* apc_ptr) noexcept;

    AdaptivePackedCellContainer* GetAPCRuntimePtrBySlotIndex_(size_t apc_idx) noexcept;

public:

    bool InitializeFabricWithPtrTable(
        uint32_t slot_count,
        uint32_t slot_cell_count = MINIMUM_APC_CELL_COUNT,
        uint8_t slab_id = APCDataStructure::BRANCH_VERSION,
        uint32_t fabric_thread_capacity = UNSIGNED_ZERO
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
    ) noexcept
    {
        if (
            !IsFabricActive() ||
            desired_apc.IsThisAPCValid()
        )
        {
            return false;
        }

        std::optional<uint32_t> slot_new = GetASlotForNewAPCLink();
        if (
            slot_new.has_value()
        )
        {
            return false;
        }
        
        bool descriptor_live  = false;
        auto RollabckDescriptors___ = [&]() noexcept -> void
        {
            if (descriptor_live)
            {
                SwitchDescriptionState(
                    slot_new.value(),
                    StateOfAPC::RESERVED,
                    StateOfAPC::LIVE,
                    internal_max_tries
                );
            }
            SwitchDescriptionState(
                slot_new.value(),
                StateOfAPC::FREE,
                StateOfAPC::RESERVED,
                internal_max_tries
            );
        };

        auto AbortCreation___ = [&]()
        {
            StoreAPCRuntimePtr(slot_new.value(), nullptr);
            RollabckDescriptors___();
            desired_apc.ReleseFabricBindingOnly_();
            return false;
        };

        RangeOfAPC range = GetSegmentPoolRange(slot_new.value());

        if (
            !range.IsValid ||
            !AttachValidIdentity(slot_new.value()) ||
            !desired_apc.BindExternalRawFabricBacking_(
                &SlabBasePtr_[range.BeginIndex],
                PerAPCRuntimeCellCount_,
                this,
                slot_new.value()
            ) ||
            !desired_apc.InitiateAPCMetaHeader(
                layout,
                dtype,
                protocol,
                version
            )
        )
        {
            RollabckDescriptors___();
            return false;
        }
        
        if (
            !StoreAPCRuntimePtr(slot_new.value(), &desired_apc) ||
            !SwitchDescriptionState(
                slot_new.value(),
                StateOfAPC::LIVE,
                StateOfAPC::RESERVED,
                internal_max_tries
            )
        )
        {
            AbortCreation___();
            return false;
        }
        
        EdgeBuilder::EdgeData berore_edge_horizontal{};
        EdgeBuilder::EdgeData berore_edge_vertical{};

        if (
            wants_horizontal_root &&
            (
                !ReserveAnEdge_(
                    FabricSegments::HORIZONTAL_EDGE_TABLE,
                    slot_new.value(),
                    &berore_edge_horizontal,
                    StateOfAPC::FREE,
                    internal_max_tries
                ) ||
                !InitiateRootAxis(slot_new.value(), IAB::BidirectionalAxis::HORIZONTAL)
            )

        )
        {
            AbortCreation___();
            return false;
        }
        
        if (
            wants_vertical_root &&
            (
                !ReserveAnEdge_(
                    FabricSegments::VERTICAL_EDGE_TABLE,
                    slot_new.value(),
                    &berore_edge_vertical,
                    StateOfAPC::FREE,
                    internal_max_tries
                ) ||
                !InitiateRootAxis(slot_new.value(), IAB::BidirectionalAxis::VERTICAL)
            )
        )
        {
            PublishReservedEdge_(berore_edge_horizontal, slot_new.value());
            AbortCreation___();
            return false;
        }
        
        return true;
    }
};


}