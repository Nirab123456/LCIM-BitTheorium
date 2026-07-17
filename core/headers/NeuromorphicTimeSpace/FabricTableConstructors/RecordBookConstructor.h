#pragma once 
#include "ReadWriteConstructor.h"

namespace PredictedAdaptedEncoding
{
    class RecordBookConstructor : public ReadWriteConstructor
    {
        
    protected:

        constexpr size_t BegainIdxOfAnyFabTableHeader(FabricTableSegmentClasses desired_table) noexcept;
        
        bool BegainEndIdxHeaderPairGet(
            const FabricTableSegmentClasses table_class,
            RecordBookConf::RecordBookTablesBoundsCarrier& return_bounds
        ) noexcept;

        /// @brief FILL: DESIRED: FabricTableSegmentClasses with Idle Fabric Cell -> CALLS: BegainEndIdxHeaderPairGet TO: Get Range In SLab
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

    public:

        /// @brief Uses -> BegainEndIdxHeaderPairGet to get record and packs into -> APCDescriptorRange
        /// @return VALID::APCDescriptorRange.IsValid = true || INVALID:: APCDescriptorRange.IsValid = false
        bool ReadAPCDescriptorTableBeginEndFromRecordBook(
            APCDescriptorRange& return_APC_handle_description_range
        ) noexcept;

    };

}