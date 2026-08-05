#pragma once 
#include "CoreOfFabricCoordinator.hpp"

namespace PredictedAdaptedEncoding
{

struct RecordBookConf
{
    static constexpr uint8_t RECORD_BOOK_INTERNAL_SEGMENT_COUNT = static_cast<uint8_t>(FabricTableSegmentClasses::SEGMENT_POOL);

    struct RecordBookTablesBoundsCarrier
    {
        uint64_t BeginIndex = UNSIGNED_ZERO;
        uint64_t EndIndex = UNSIGNED_ZERO;
        FabricTableSegmentClasses OwnerTableOfTheBounds{};
        bool IsValid = false;  
    };

};


}