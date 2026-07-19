#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace PredictedAdaptedEncoding
{

    uint64_t FabricConstructor::ReadAFabricU64Directly(size_t slab_index) noexcept
    {
        if (!IsDesiredIndexValidInSLab(slab_index))
        {
            return FABRIC_CELL_SENTINAL;
        }
        return SlabBasePtr_[slab_index];
    } 

    constexpr uint64_t FabricConstructor::AtomicallyLoadReadAUnit(size_t slab_index) noexcept
    {
        if (!IsDesiredIndexValidInSLab(slab_index))
        {
            return FABRIC_CELL_SENTINAL;
        }
        std::atomic_ref<const uint64_t> fab_u64_ref(SlabBasePtr_[slab_index]);
        const uint64_t desired_cell_raw = fab_u64_ref.load(std::memory_order_acquire);
        return desired_cell_raw;
    }

    constexpr void FabricConstructor::DirectlyStoreFabricUnit64(size_t slab_index, uint64_t fabric_unit) noexcept
    {
        if (!IsDesiredIndexValidInSLab(slab_index))
        {
            return;
        }
        SlabBasePtr_[slab_index] = fabric_unit;
    }

    constexpr void FabricConstructor::AtomicallyStoreU64Fab(
        size_t slab_index, uint64_t fabric_unit,
        std::memory_order mem_order
    ) noexcept
    {
        if (!IsDesiredIndexValidInSLab(slab_index))
        {
            return;
        }
        std::atomic_ref<uint64_t> fab_u64_ref(SlabBasePtr_[slab_index]);
        fab_u64_ref.store(fabric_unit, mem_order);
        fab_u64_ref.notify_all();
    }

    constexpr bool FabricConstructor::CompareExchangeStrongFromFabric(
        size_t slab_index, 
        uint64_t& expected_packed_cell, 
        uint64_t desired_packed_cell,
        std::memory_order mem_order_success,
        std::memory_order mem_order_failure
    ) noexcept
    {
        if (!IsDesiredIndexValidInSLab(slab_index))
        {
            return false;
        }
        std::atomic_ref<uint64_t> fab_u64_ref(SlabBasePtr_[slab_index]);
        return fab_u64_ref.compare_exchange_strong(expected_packed_cell, desired_packed_cell, mem_order_success, mem_order_failure);
    }

    constexpr bool FabricConstructor::CompareExchangeWeakInSlab(
        size_t slab_index, 
        uint64_t& expected_packed_cell, 
        uint64_t desired_packed_cell,
        std::memory_order mem_order_success,
        std::memory_order mem_order_failure
    ) noexcept
    {
        if (!IsDesiredIndexValidInSLab(slab_index))
        {
            return false;
        }
        std::atomic_ref<uint64_t> fab_u64_ref(SlabBasePtr_[slab_index]);
        return fab_u64_ref.compare_exchange_weak(expected_packed_cell, desired_packed_cell, mem_order_success, mem_order_failure);
    }

    bool FabricConstructor::ForceNxLenMemCopy(
        size_t slab_starting_idx, 
        size_t number_of_cells, 
        const uint64_t* desired_units
    ) noexcept
    {
        if (
            !IsDesiredIndexValidInSLab(slab_starting_idx + number_of_cells - 1) ||
            !desired_units ||
            number_of_cells == UNSIGNED_ZERO ||
            number_of_cells > SlabCellCount_ - slab_starting_idx
        )
        {
            return false;
        }

        try
        {
            uint64_t value_of_last_idx = desired_units[number_of_cells - 1];
            (void) value_of_last_idx;
        }
        catch(...)
        {
            return false;
        }

        std::memcpy(
            &SlabBasePtr_[slab_starting_idx],
            desired_units,
            number_of_cells * sizeof(uint64_t)
        );
        return true;
    }

    bool FabricConstructor::CompareExchangeStrongSequentiallyOrRevert(
        size_t slab_starting_idx, 
        uint8_t number_of_cells, 
        const uint64_t* desired_units
    ) noexcept
    {
        if (
            !IsDesiredIndexValidInSLab(slab_starting_idx + number_of_cells - 1) ||
            !desired_units ||
            number_of_cells == UNSIGNED_ZERO ||
            !APCDataStructure::InLimitOfUint8(number_of_cells)
        )
        {
            return false;
        }

        try
        {
            uint64_t value_of_last_idx = desired_units[number_of_cells - 1];
            (void) value_of_last_idx;        
        }
        catch(...)
        {
            return false;
        }
        

        uint16_t changed_count = UNSIGNED_ZERO;
        HeaderOrchestrator::DefaultMemCopyBuffer comp_ex_buffer{};
        HeaderOrchestrator::BuildNullMemCopyBuffer(comp_ex_buffer);

        for (uint16_t i = 0; i < number_of_cells; i++)
        {
            const size_t current_slab_idx = static_cast<size_t>(i + slab_starting_idx);
            uint64_t expected_unit = AtomicallyLoadReadAUnit(current_slab_idx);
            comp_ex_buffer[i] = expected_unit;
            if (
                !CompareExchangeStrongFromFabric(
                    current_slab_idx,
                    expected_unit,
                    desired_units[i]
                )
            )
            {
                break;
            }
            changed_count = i + 1;
        }

        if (changed_count != number_of_cells)
        {
            for (size_t i = 0; i < changed_count; i++)
            {
                const size_t current_slab_idx = static_cast<size_t>(i + slab_starting_idx);

                DirectlyStoreFabricUnit64(current_slab_idx, comp_ex_buffer[i]);
            }
            return false;
        }
        
        return true;
        
    }

}