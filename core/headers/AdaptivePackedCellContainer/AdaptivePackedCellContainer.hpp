#pragma once 
#include "ConstructorsOfAPC/RegionViewConstructor.hpp"

namespace BidirectionalInMemGraph
{
static_assert(__cpp_lib_atomic_wait, "C++ must suppoet atomic wait/notify");


class AdaptivePackedCellContainer : public RegionViewConstructor
{

public:
    static constexpr uint8_t REALTION_FIND_TRIES = 1;

    bool AttachSiblingOrChild(
        AdaptivePackedCellContainer& sibbling,
        FabricSegments edge_table,
        uint32_t max_tries = DEFAULT_MAX_TRIES
    ) noexcept;

    bool AttachMeToAnother(
        AdaptivePackedCellContainer& sibbling,
        FabricSegments edge_table,
        uint32_t max_tries = DEFAULT_MAX_TRIES
    ) noexcept;

    bool DetachMyChild(
        AdaptivePackedCellContainer& sibbling,
        FabricSegments edge_table,
        uint32_t max_tries = DEFAULT_MAX_TRIES
    ) noexcept;

    bool DetachMeFromAnotherEdge(
        FabricSegments edge_table,
        uint32_t max_tries = DEFAULT_MAX_TRIES
    ) noexcept;

    bool DetachAndReAttachMeToThisParent(
        AdaptivePackedCellContainer& root_parent,
        FabricSegments edge_table,
        uint32_t max_tries = DEFAULT_MAX_TRIES
    ) noexcept;

    bool DetachAndReattachMeAsEquivelentSibbling(
        AdaptivePackedCellContainer& sibbling,
        FabricSegments edge_table,
        uint32_t max_tries = DEFAULT_MAX_TRIES
    ) noexcept;

    RelationOparation FindPrevious(
        FabricSegments edge_table,
        uint32_t max_tries = REALTION_FIND_TRIES
    ) noexcept;

    RelationOparation FindMyNext(
        FabricSegments edge_table,
        uint32_t max_tries = REALTION_FIND_TRIES
    ) noexcept;

    bool Retire(uint32_t max_tries = DEFAULT_MAX_TRIES) noexcept;
};


}  