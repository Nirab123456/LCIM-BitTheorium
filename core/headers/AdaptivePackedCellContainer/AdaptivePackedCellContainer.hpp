#pragma once 
#include "ConstructorsOfAPC/ReadAndWriteOfAPC.h"

namespace BidirectionalInMemGraph
{
static_assert(__cpp_lib_atomic_wait, "C++ must suppoet atomic wait/notify");


class AdaptivePackedCellContainer : public ReadAndWriteOfAPC
{
public:
    using IAB = InstallAxisToBuffer;

    bool AttachAnotherToMe(
        AdaptivePackedCellContainer& sibbling,
        IAB::BidirectionalAxis axis,
        IAB::DescOfInharitance inharitance,
        uint32_t max_tries = DEFAULT_MAX_TRIES
    ) noexcept;

    bool AttachMeToAnother(
        AdaptivePackedCellContainer& sibbling,
        IAB::BidirectionalAxis axis,
        IAB::DescOfInharitance inharitance,
        uint32_t max_tries = DEFAULT_MAX_TRIES
    ) noexcept;

    /// @brief MY IDEA IS VAUGE.
    bool DetachMySibbling(
        IAB::BidirectionalAxis axis,
        IAB::DescOfInharitance inharitance
    ) noexcept;

    bool DetachMe(
        IAB::BidirectionalAxis axis,
        IAB::DescOfInharitance inharitance
    ) noexcept;



};


}  