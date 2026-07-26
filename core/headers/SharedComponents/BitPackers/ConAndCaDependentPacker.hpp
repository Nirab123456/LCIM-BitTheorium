#pragma once 
#include "../../AdaptivePackedCellContainer/CoreOFAPC/ConstructorsAndCarriersOfAPC.hpp"

namespace PredictedAdaptedEncoding 
{
    struct Double32In64ExPa
    {
        static constexpr uint64_t PackDoubleUnsigned32In64(uint32_t low_32, uint32_t high_32) noexcept
        {
            if (
                !APCDataStructure::IsValid32BitAPCUnit(low_32) ||
                !APCDataStructure::IsValid32BitAPCUnit(high_32)
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
            if (!APCDataStructure::IsValidFabricUnit(packed_value))
            {
                return std::nullopt;
            }
            return static_cast<uint32_t>((packed_value >> UNSIGNED_ZERO) & MaskLeftOverBitsUntil64(BIT_LENGTH_OF_APC));
        }

        static constexpr std::optional<uint32_t> ExtractHigh32Of64(uint64_t packed_value) noexcept
        {
            if(!APCDataStructure::IsValidFabricUnit(packed_value))
            {
                return std::nullopt;
            }
            return static_cast<uint32_t>((packed_value >> BIT_LENGTH_OF_APC) & MaskLeftOverBitsUntil64(BIT_LENGTH_OF_APC));
        }
    };

    struct Pack32_28_4BitIn64BitUnit
    {
        static constexpr uint8_t UINT4_MAX = 0x0fu;
        static constexpr uint8_t LEN_OF_28_BIT = 28u;
        static constexpr uint32_t UINT28_MAX = UINT32_MAX & LeftOverBitMaskUntil32(LEN_OF_28_BIT);

        struct Pack32_28_4_Carrier
        {
            uint32_t Lowest32Bit = UINT32_MAX;
            uint32_t Mid28Bit = UINT32_MAX;
            uint8_t High4Bit = UINT8_MAX;
            bool IsValid = false;
        };
        
        static constexpr bool IsCarrierValid(Pack32_28_4_Carrier& carrier) noexcept
        {
            if (
                !APCDataStructure::IsValid32BitAPCUnit(carrier.Lowest32Bit)||
                carrier.Mid28Bit >= UINT28_MAX ||
                carrier.High4Bit >= UINT4_MAX
            )
            {
                carrier.IsValid = false;
                return false;
            }

            carrier.IsValid = true;
            return true;
        }

        static constexpr uint64_t PackValues(
            Pack32_28_4_Carrier& carrier
        ) noexcept
        {
            if (!IsCarrierValid(carrier))
            {
                return FABRIC_CELL_SENTINAL;
            }
            
            return(
                (uint64_t(carrier.Lowest32Bit) << UNSIGNED_ZERO) |
                (uint64_t(carrier.Mid28Bit) << (BIT_LENGTH_OF_FABRIC - LEN_OF_28_BIT)) |
                (uint64_t(carrier.High4Bit) << (BIT_LENGTH_OF_FABRIC - 4u))
            );
        }

        static constexpr Pack32_28_4_Carrier UnpackUnitToCarrier(uint64_t value) noexcept
        {
            Pack32_28_4_Carrier carrier{};

            if (!APCDataStructure::IsValidFabricUnit(value))
            {
                return carrier;
            }

            carrier.Lowest32Bit = static_cast<uint32_t>((value >> UNSIGNED_ZERO) & MaskLeftOverBitsUntil64(32u));
            carrier.Mid28Bit = static_cast<uint32_t>((value >> (BIT_LENGTH_OF_FABRIC - LEN_OF_28_BIT)) & MaskLeftOverBitsUntil64(LEN_OF_28_BIT));
            carrier.High4Bit = static_cast<uint8_t>((value >> (BIT_LENGTH_OF_FABRIC - 4u)) & MaskLeftOverBitsUntil64(4u));
            IsCarrierValid(carrier);
            return carrier;
        }

    };
    

}