#pragma once 
#include "../../AdaptivePackedCellContainer/CoreOFAPC/ConstructorsAndCarriersOfAPC.hpp"

namespace PredictedAdaptedEncoding 
{
    struct Double32In64ExPa
    {
        static constexpr uint64_t PackDoubleUnsigned32In64(uint32_t low_32, uint32_t high_32) noexcept
        {
            if (
                !APCDataStructure::IsThisIndexValidForAPC(low_32) ||
                !APCDataStructure::IsThisIndexValidForAPC(high_32)
            )
            {
                return FABRIC_CELL_SENTINAL;
            }

            return (
                (uint64_t{low_32} << UNSIGNED_ZERO) |
                (uint64_t{high_32} << BIT_LENGTH_OF_APC)
            );
        }

        static constexpr std::optional<uint32_t> ExtractLow32Of64(uint64_t packed_value) noexcept
        {
            if (!APCDataStructure::IsThsisIndexValidForFabric(packed_value))
            {
                return std::nullopt;
            }
            return static_cast<uint32_t>((packed_value >> UNSIGNED_ZERO) & MaskLeftOverBitsUntil64(BIT_LENGTH_OF_APC));
        }

        static constexpr std::optional<uint32_t> ExtractHigh32Of64(uint64_t packed_value) noexcept
        {
            if(!APCDataStructure::IsThsisIndexValidForFabric(packed_value))
            {
                return std::nullopt;
            }
            return static_cast<uint32_t>((packed_value >> BIT_LENGTH_OF_APC) & MaskLeftOverBitsUntil64(BIT_LENGTH_OF_APC));
        }
    };

}