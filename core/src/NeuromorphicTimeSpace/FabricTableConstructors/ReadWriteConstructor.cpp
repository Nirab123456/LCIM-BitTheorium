#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace PredictedAdaptedEncoding
{


    bool ReadWriteConstructor::ReadASnapShotFromSlab(
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
                return_buffer[i] = AtomicallyLoadReadAUnit(current_slab_idx);
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