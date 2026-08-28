#pragma once 
#include "EdgeTableConf.hpp"

namespace BidirectionalInMemGraph
{

    struct HandleOfAPCStatic
    {

        enum class RetirementIndexing : uint8_t
        {
            GENERATION = 0,
            ACCESS_PATTERN = 1
        };

        static constexpr uint8_t HANDLE_TABLE_WIDTH = static_cast<uint8_t>(RetirementIndexing::ACCESS_PATTERN) + 1u;

        struct RetirementValues
        {
            uint64_t Generation = UNSIGNED_ZERO;
            uint32_t ActiveAccess = UNSIGNED_ZERO;
            bool AcceptingAccess = false;
        };

        static constexpr uint64_t MakeAccessPatternCell(const RetirementValues& values) noexcept
        {
            return TwinU32ToU64::PackDoubleUnsigned32In64(values.ActiveAccess, static_cast<uint32_t>(values.AcceptingAccess));
        }

        static constexpr bool GetActiveAccessAndAcceptence(uint64_t raw_cell, RetirementValues& values) noexcept
        {
            if(!APCDataStructure::IsValidFabricUnit(raw_cell))
            {
                return false;
            }

            uint32_t accepting_raw = static_cast<bool>(TwinU32ToU64::ExtractHigh32Of64(raw_cell));

            if (accepting_raw > 1u)
            {
                return false;
            }
            values.ActiveAccess = TwinU32ToU64::ExtractLow32Of64(raw_cell);
            values.AcceptingAccess = accepting_raw == UNSIGNED_ZERO ? false : true;

            return true;
        }

        static constexpr size_t CellOffset(uint32_t slot, RetirementIndexing cell) noexcept
        {
            return static_cast<size_t>(slot) * HANDLE_TABLE_WIDTH + static_cast<size_t>(cell);
        }

        static constexpr bool CanAdvanceGeneration(uint64_t generation) noexcept
        {
            return APCDataStructure::IsValidFabricUnit(generation);
        }


    };


}