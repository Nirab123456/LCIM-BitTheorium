#pragma once 
#include "FabricConstructor.h"

namespace PredictedAdaptedEncoding
{
    class RecordBookConstructor : public FabricConstructor
    {
        
    protected:

        uint64_t GetStartingOfAnyFabricTable(FabricTableSegmentClasses desired_table) noexcept;
        
        bool GetRecordMapCarrierRanges(
            const FabricTableSegmentClasses table_class,
            RecordBookConf::RecordBookTablesBoundsCarrier& return_bounds
        ) noexcept;

        /// @brief FILL: DESIRED: FabricTableSegmentClasses with Idle Fabric Cell -> CALLS: GetRecordMapCarrierRanges TO: Get Range In SLab
        /// @param table_class Desired FabricTableSegmentClasses You want Idle
        void IdleAFabricTableClassRangesMemory_(FabricTableSegmentClasses table_class) noexcept;

        /// @brief WRITES: A Single Entry OF: FabricTableSegmentClasses::SLAB_RECORD_MAP == (2xPackedMode::VALUE48 + 1xPackedMode::Model32)
        /// @param table_class Desired FabricTableSegmentClasses == FabricTableSegmentClasses
        /// @param begin Begin Index OF: FabricTableSegmentClasses -> Record
        /// @param end End Index OF: FabricTableSegmentClasses -> Record
        void WriteARecordBookOfTSCEntry_(
            FabricTableSegmentClasses table_class, 
            size_t begin, 
            size_t end 
        ) noexcept;

    };

}