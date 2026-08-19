#pragma once
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
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
            APCRuntimePtrTable_[i].store(nullptr, std::memory_order_release);
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
            APCRuntimePtrTable_[i].store(nullptr, std::memory_order_release);
        }
    }

    bool VagueTemoraryPremativeFabric::StoreAPCRuntimePtr(size_t apc_idx, AdaptivePackedCellContainer* apc_ptr) noexcept
    {
        if (!APCRuntimePtrTable_ || apc_idx >= CountOfAPC_)
        {
            return false;
        }

        APCRuntimePtrTable_[apc_idx].store(apc_ptr, std::memory_order_release);
        return true;
    }

    AdaptivePackedCellContainer* VagueTemoraryPremativeFabric::GetAPCRuntimePtrBySlotIndex_(size_t apc_idx) noexcept
    {
        if (!APCRuntimePtrTable_ || apc_idx >= CountOfAPC_)
        {
            return nullptr;
        }

        return APCRuntimePtrTable_[apc_idx].load(std::memory_order_acquire);
    }


    bool VagueTemoraryPremativeFabric::InitializeFabricWithPtrTable(
        uint32_t slot_count,
        uint32_t slot_cell_count
    ) noexcept
    {
        APCRuntimePtrTable_.reset();
        const bool base_ok = InitializeFabric(
            slot_count,
            slot_cell_count
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

    bool VagueTemoraryPremativeFabric::CreateAPC(
        AdaptivePackedCellContainer& desired_apc,
        bool wants_horizontal_root,
        bool wants_vertical_root,
        const LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& layout,
        const SchemaDefinition::InitialRegionalDtypeConf& dtype,
        const SchemaDefinition::InitialRegionalProtocol& protocol,
        uint8_t version,
        uint32_t internal_max_tries
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
            !slot_new.has_value()
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
            !StoreAPCRuntimePtr(slot_new.value(), &desired_apc)
        )
        {
            AbortCreation___();
            return false;
        }

        descriptor_live = true;
        
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
                !OpenForestGateOnAxis(slot_new.value(), IAB::BidirectionalAxis::HORIZONTAL, internal_max_tries)
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
                !OpenForestGateOnAxis(slot_new.value(), IAB::BidirectionalAxis::VERTICAL, internal_max_tries)
            )
        )
        {
            PublishReservedEdge_(berore_edge_horizontal, slot_new.value());
            AbortCreation___();
            return false;
        }
        
        return true;
    }


}
