#pragma once 
#include "ConstructorsOfAPC/FabricToAPCLinker.hpp"

namespace BidirectionalInMemGraph
{
static_assert(__cpp_lib_atomic_wait, "C++ must suppoet atomic wait/notify");


class AdaptivePackedCellContainer : public FabricToAPCLinker
{
public:
    static constexpr uint8_t REALTION_FIND_TRIES = 1;
    using IAB = InstallAxisToBuffer;

    bool AttachSiblingOrChild(
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

    bool DetachMyChild(
        AdaptivePackedCellContainer& sibbling,
        IAB::BidirectionalAxis axis,
        uint32_t max_tries = DEFAULT_MAX_TRIES
    ) noexcept;

    bool DetachMeFromAnotherEdge(
        IAB::BidirectionalAxis axis,
        uint32_t max_tries = DEFAULT_MAX_TRIES
    ) noexcept;

    bool DetachAndReAttachMeToThisParent(
        AdaptivePackedCellContainer& root_parent,
        IAB::BidirectionalAxis axis,
        uint32_t max_tries = DEFAULT_MAX_TRIES
    ) noexcept;

    bool DetachAndReattachMeAsEquivelentSibbling(
        AdaptivePackedCellContainer& sibbling,
        IAB::BidirectionalAxis axis,
        uint32_t max_tries = DEFAULT_MAX_TRIES
    ) noexcept;

    RelationOparation FindPrevious(
        IAB::BidirectionalAxis axis,
        uint32_t max_tries = REALTION_FIND_TRIES
    ) noexcept;

    RelationOparation FindMyNext(
        IAB::BidirectionalAxis axis,
        IAB::DescOfInharitance inharitance,
        uint32_t max_tries = REALTION_FIND_TRIES
    ) noexcept;



};


}  