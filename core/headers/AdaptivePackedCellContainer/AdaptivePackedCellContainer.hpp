#pragma once 
#include "ConstructorsOfAPC/ReadAndWriteOfAPC.h"

namespace PredictedAdaptedEncoding
{
static_assert(__cpp_lib_atomic_wait, "C++ must suppoet atomic wait/notify");


class AdaptivePackedCellContainer : public FabricToAPCLinker
{

};


}  