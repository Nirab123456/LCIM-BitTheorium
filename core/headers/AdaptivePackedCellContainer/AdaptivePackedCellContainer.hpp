#pragma once 
#include "ConstructorsOfAPC/ReadAndWriteOfAPC.h"

namespace BidirectionalInMemGraph
{
static_assert(__cpp_lib_atomic_wait, "C++ must suppoet atomic wait/notify");


class AdaptivePackedCellContainer : public ReadAndWriteOfAPC
{
public:
    
};


}  