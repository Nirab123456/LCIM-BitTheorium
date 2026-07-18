#pragma once 
#include "FabricConstructor.h"

namespace PredictedAdaptedEncoding
{

    class ReadWriteConstructor : public FabricConstructor
    {
    protected:

        static constexpr size_t DefaultFabricAlignment16Cell_(size_t value) noexcept
        {
            const uint8_t alignment_value_15 = 16 - 1;
            return (value + alignment_value_15) & ~static_cast<size_t>(alignment_value_15);
        }

    public:

        constexpr uint64_t UpdateACounterAtomically(size_t desired_idx, uint32_t delta) noexcept;

        bool ReadASnapShotFromSlab(
            size_t slab_starting_idx, 
            size_t sequential_number_of_cells, 
            uint64_t* return_buffer,
            bool atomic_required = false
        ) noexcept;

    };



}