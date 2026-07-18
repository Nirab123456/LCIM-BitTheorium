#pragma once 
#include "ConstructorsOfAPC/ReadAndWriteOfAPC.h"

namespace PredictedAdaptedEncoding
{
static_assert(__cpp_lib_atomic_wait, "C++ must suppoet atomic wait/notify");


class AdaptivePackedCellContainer : public ReadAndWriteOfAPC
{
public:
    constexpr bool IsThisAPCValid() noexcept
    {
        if (
            FabricOwnerPtr_ != nullptr &&
            HashIdConstructror::IsValidAPCId(IdxOfThisAPCInFabric_) &&
            RangeOfThisAPCInSlab_.IsValid &&
            APCDataStructure::IsCapacityOfAPCValid(CapacityOfThisAPC_)
        )
        {
            return true;
        }
        return false;
    }

};


}  