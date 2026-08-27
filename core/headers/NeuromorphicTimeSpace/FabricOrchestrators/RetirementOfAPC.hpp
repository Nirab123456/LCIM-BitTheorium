#pragma once 
#include "EdgeTableConf.hpp"

namespace BidirectionalInMemGraph
{

    struct RetirementOfAPC
    {

        enum class RetirementIndexing : uint8_t
        {
            GENERATION = 0,
            ACCESSPATTERN = 1
        };

        static constexpr uint8_t RETIREMENT_TABLE_WIDTH = static_cast<uint8_t>(RetirementIndexing::ACCESSPATTERN) + 1u;

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

        static constexpr void GetActiveAccessAndAcceptence(uint64_t raw_cell, RetirementValues& values) noexcept
        {
            if(!APCDataStructure::IsValidFabricUnit(raw_cell))
            {
                return;
            }

            values.ActiveAccess = TwinU32ToU64::ExtractLow32Of64(raw_cell);
            values.AcceptingAccess = static_cast<bool>(TwinU32ToU64::ExtractHigh32Of64(raw_cell));
        }

    };
    



}