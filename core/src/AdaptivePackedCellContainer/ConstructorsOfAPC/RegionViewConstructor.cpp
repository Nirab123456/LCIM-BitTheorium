#pragma once
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{
    bool RegionViewConstructor::ResolveRegionView_(
        MacroColumnOfAPC column_name,
        uint32_t record_ordinal,
        ResolveRegionBiteView& out
    ) noexcept
    {
        using SD = SchemaDefinition;
        using ASG = APCStorageGeometry;

        out = ResolveRegionBiteView{};
        if (
            !IsActiveAPC() ||
            !RawAPCBasePtr_ 
        )
        {
            return false;
        }
        

    }

}