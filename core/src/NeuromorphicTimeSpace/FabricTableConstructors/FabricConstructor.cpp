#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{

    bool FabricConstructor::ReadAFabricU64Directly(
        size_t slab_index,
        uint64_t& return_value
    ) noexcept
    {
        if (!IsDesiredIndexValidInSLab(slab_index))
        {
            return false;
        }
        return_value = SlabBasePtr_[slab_index];
        return true;
    }

    bool FabricConstructor::AtomicallyLoadReadAUnit(
        size_t slab_index,
        uint64_t& return_value
    ) noexcept
    {
        if (!IsDesiredIndexValidInSLab(slab_index))
        {
            return false;
        }
        std::atomic_ref<const uint64_t> fab_u64_ref(SlabBasePtr_[slab_index]);
        uint64_t desired_cell_raw = fab_u64_ref.load(std::memory_order_acquire);
        return_value = desired_cell_raw;
        return true;
    }

    void FabricConstructor::DirectlyStoreFabricUnit64(size_t slab_index, uint64_t fabric_unit) noexcept
    {
        if (!IsDesiredIndexValidInSLab(slab_index))
        {
            return;
        }
        SlabBasePtr_[slab_index] = fabric_unit;
    }

    void FabricConstructor::AtomicallyStoreU64Fab(
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

    bool FabricConstructor::CompareExchangeStrongFromFabric(
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

    bool FabricConstructor::CompareExchangeWeakInSlab(
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

    bool FabricConstructor::AtomicallyCopyFromBufferToFabric(
        size_t slab_starting_idx, 
        uint32_t number_of_cells, 
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

        for (size_t i = 0; i < number_of_cells; i++)
        {
            AtomicallyStoreU64Fab(
                slab_starting_idx + i,
                desired_units[i]
            );
        }
        
        return true;
    }

    bool FabricConstructor::ReadASnapShotFromSlab(
        size_t slab_starting_idx, 
        size_t sequential_number_of_cells, 
        uint64_t* return_buffer,
        bool atomic_required
    ) noexcept
    {
        if (
            !IsDesiredIndexValidInSLab(slab_starting_idx) ||
            !return_buffer ||
            sequential_number_of_cells == UNSIGNED_ZERO ||
            sequential_number_of_cells > SlabCellCount_ - slab_starting_idx
        )
        {
            return false;
        }

        try
        {
            uint64_t value_of_last_idx = return_buffer[sequential_number_of_cells - 1];
            (void) value_of_last_idx;        
        }
        catch(...)
        {
            return false;
        }
        
        if (atomic_required)
        {
            for (size_t i = 0; i < sequential_number_of_cells; i++)
            {
                const size_t current_slab_idx = slab_starting_idx + i;
                if (!AtomicallyLoadReadAUnit(current_slab_idx, return_buffer[i]))
                {
                    return false;
                }
                
            }
            return true;
        }
        
        std::memcpy(
            return_buffer,
            &SlabBasePtr_[slab_starting_idx],
            sequential_number_of_cells * sizeof(uint64_t)
        );

        return true;
    }

}