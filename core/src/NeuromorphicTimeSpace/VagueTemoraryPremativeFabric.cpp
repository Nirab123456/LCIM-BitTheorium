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
            desired_apc.IsActiveAPC()
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

        uint64_t* generation_cell = GetAPCGenerationPtr_(slot_new.value());
        if (!generation_cell)
        {
            RollabckDescriptors___();
            return false;
        }

        const uint64_t control_raw = std::atomic_ref<const uint64_t>(*generation_cell).load(std::memory_order_acquire);
        const HandleOfAPCStatic::ControlValues control_values = HandleOfAPCStatic::ReadControlCell(control_raw);

        if (
            !control_values.Closed ||
            control_values.ActiveAccess != UNSIGNED_ZERO ||
            !HandleOfAPCStatic::IsGenerationValid(control_values.Generation)
        )
        {
            RollabckDescriptors___();
            return false;
        }
        
        bool generation_opened = false;
        
        auto AbortCreation___ = [&]() noexcept -> void
        {

            if (generation_opened)
            {
                CloseAPCGeneration_(slot_new.value(), control_values.Generation);
            }

            StoreAPCRuntimePtr(slot_new.value(), nullptr);
            RollabckDescriptors___();
            desired_apc.ReleseFabricBindingOnly_();
        };

        RangeOfAPC range = GetSegmentPoolRange(slot_new.value());

        if (
            !range.IsValid ||
            !AttachValidIdentity(slot_new.value()) ||
            !desired_apc.BindExternalRawFabricBacking_(
                &SlabBasePtr_[range.BeginIndex],
                this,
                slot_new.value(),
                generation_cell,
                control_values.Generation
            ) ||
            !desired_apc.InitiateAPCMetaHeader(
                layout,
                dtype,
                protocol,
                version
            )
        )
        {
            AbortCreation___();
            return false;
        }

        if (!OpenAPCGeneration_(slot_new.value(), control_values.Generation))
        {
            AbortCreation___();
            return false;
        }
        
        generation_opened = true;
        
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



    std::optional<uint32_t> VagueTemoraryPremativeFabric::GetASlotForNewAPCLink() noexcept
    {
        if (
            !FabricInitialized_.load(std::memory_order_acquire) ||
            !SlabBasePtr_ || 
            !APCDataStructure::IsCapacityOfAPCValid(PerAPCRuntimeCellCount_) ||
            !IAB::IsValidAPCId(CountOfAPC_)
        )
        {
            return std::nullopt;
        }

        std::optional<uint32_t> maybe_First_free = ReadFirstFreeAPCIdx_();

        if (maybe_First_free.has_value())
        {
            for (uint32_t description_idx = maybe_First_free.value(); description_idx < CountOfAPC_; description_idx++)
            {
                const DSA::SeqLockAndStateStruct current = ReadAPCStateAtomically_(description_idx);
                if (
                    !current.IsValid ||
                    current.StateOfTheAPC != StateOfAPC::FREE
                )
                {
                    continue;
                }
                if (!SwitchDescriptionState(
                    description_idx,
                    StateOfAPC::RESERVED,
                    StateOfAPC::FREE
                ))
                {
                    continue;
                }
                uint64_t expected = maybe_First_free.value();
                UpdateFirstFreeIdx_(expected, description_idx);
                return description_idx;
            }
        }

        if (maybe_First_free.has_value())
        {
            uint64_t expected = maybe_First_free.value();
            UpdateFirstFreeIdx_(expected, FABRIC_CELL_SENTINAL);
        }
        
        for (uint32_t slot = 0; slot < CountOfAPC_; slot++)
        {
            const DSA::SeqLockAndStateStruct current = ReadAPCStateAtomically_(slot);

            if (
                current.IsValid &&
                current.StateOfTheAPC == StateOfAPC::RETIRED &&
                ReclaimRetiredSlot_(slot)
            )
            {
                return slot;
            }
        }
        return std::nullopt;
    }


}
