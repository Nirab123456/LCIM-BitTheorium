
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
    static constexpr uint64_t APC_META_CELL_SENTINAL = UINT32_MAX;
    static constexpr uint32_t APC_ALL_INDEX_LIMIT = UINT16_MAX - 1;
    static constexpr uint16_t APC_INDEX_SENTINAL = UINT16_MAX;
    static constexpr size_t APC_CACHELINE_SIZE = 64u;
    static constexpr size_t APC_SIZE_SENTINAL = SIZE_MAX;


    static constexpr bool IsThisIndexValidForAPC(uint32_t index) noexcept
    {
        if (index < APC_INDEX_SENTINAL)
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

    static constexpr bool IsCapacityOfAPCValid(uint16_t capacity) noexcept
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


}