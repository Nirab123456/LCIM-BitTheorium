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

    AdaptivePackedCellContainer* VagueTemoraryPremativeFabric::HandleBasedAPCPtrRetrival_(size_t apc_handle) noexcept
    {
        if (!HashIdConstructror::IsValidAPCId(apc_handle))
        {
            return nullptr;
        }
        const size_t apc_idx = HashIdConstructror::HashTableHandlerToAPCSlotIdx(apc_handle);
        return GetAPCRuntimePtrBySlotIndex_(apc_idx);
    }

    bool VagueTemoraryPremativeFabric::InitializeFabricWithPtrTable(
        uint32_t slot_count,
        uint32_t slot_cell_count,
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


}
