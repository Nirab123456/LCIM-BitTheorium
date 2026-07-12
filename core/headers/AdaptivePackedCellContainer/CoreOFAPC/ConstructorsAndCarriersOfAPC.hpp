
#pragma once 
#include <array>
#include <utility>
#include "APCEnums.h"

namespace PredictedAdaptedEncoding
{

struct APCDataStructure
{

    static constexpr size_t METACELL_COUNT = 96;
    static constexpr uint32_t BRANCH_MAGIC = 0x41504342u;//big-endian
    static constexpr uint32_t EOF_HEADER = 0x72616600;//big-endian
    static constexpr uint8_t BRANCH_VERSION = 1u;
    static constexpr uint32_t APC_INDEX_BOUND_SENTINAL = UINT32_MAX;
    static constexpr uint32_t APC_ALL_INDEX_LIMIT = APC_INDEX_BOUND_SENTINAL - 1;
    static constexpr size_t APC_CACHELINE_SIZE = 64u;
    static constexpr size_t APC_SIZE_SENTINAL = SIZE_MAX;


    static constexpr bool IsThisIndexValidForAPC(uint32_t index) noexcept
    {
        if (index < APC_INDEX_BOUND_SENTINAL)
        {
            return true;
        }
        return false;
    }

    static constexpr bool IsThsisIndexValidForFabric(uint64_t index) noexcept
    {
        if (index < FABRIC_CELL_SENTINAL)
        {
            return true;
        }
        return false;
    }

    static constexpr bool ThisVersionValid(uint32_t version) noexcept
    {
        if (version < UINT8_MAX)
        {
            return true;
        }
        return false;
    }

    static constexpr bool IsCapacityOfAPCValid(uint32_t capacity) noexcept
    {
        return capacity >= MINIMUM_APC_CAPACITY &&
            IsThisIndexValidForAPC(capacity);
    }

        


protected:
        static constexpr void FreeAlignedRawPackedCells_(uint64_t* backing_ptr) noexcept
        {
            if (!backing_ptr)
            {
                return;
            }
            ::operator delete[](static_cast<void*>(backing_ptr), std::align_val_t{APC_CACHELINE_SIZE});
        }
};


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