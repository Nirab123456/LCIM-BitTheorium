#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace PredictedAdaptedEncoding
{


    bool ReadWriteConstructor::ReadASnapShotFromSlab(
        size_t slab_starting_idx, 
        size_t sequential_number_of_cells, 
        const uint64_t* return_buffer
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

        std::memcpy(
            &return_buffer,
            &SlabBasePtr_[slab_starting_idx],
            sequential_number_of_cells * sizeof(uint64_t)
        );

        return true;
    }


}