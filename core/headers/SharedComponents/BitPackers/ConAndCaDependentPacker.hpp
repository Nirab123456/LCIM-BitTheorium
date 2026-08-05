#pragma once 
#include "../../AdaptivePackedCellContainer/APCOrchestrators/APCDataStructure.hpp"

namespace PredictedAdaptedEncoding 
{
    struct TwinU32ToU64
    {
        static constexpr uint64_t PackDoubleUnsigned32In64(uint32_t low_32, uint32_t high_32) noexcept
        {
            return (
                (uint64_t{low_32} << UNSIGNED_ZERO) |
                (uint64_t{high_32} << BIT_LENGTH_OF_APC)
            );
        }

        static constexpr uint32_t ExtractLow32Of64(uint64_t packed_value) noexcept
        {
            return static_cast<uint32_t>((packed_value >> UNSIGNED_ZERO) & MaskLeftOverBitsUntil64(BIT_LENGTH_OF_APC));
        }

        static constexpr uint32_t ExtractHigh32Of64(uint64_t packed_value) noexcept
        {
            return static_cast<uint32_t>((packed_value >> BIT_LENGTH_OF_APC) & MaskLeftOverBitsUntil64(BIT_LENGTH_OF_APC));
        }
    };

}