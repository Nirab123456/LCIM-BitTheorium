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
        return 
            FabricOwnerPtr_ != nullptr &&
            APCDataStructure::IsValid32BitAPCUnit(IdxOfThisAPCInFabric_) &&
            RangeOfThisAPCInSlab_.IsValid &&
            APCDataStructure::IsCapacityOfAPCValid(CapacityOfThisAPC_);
    }
};


}  