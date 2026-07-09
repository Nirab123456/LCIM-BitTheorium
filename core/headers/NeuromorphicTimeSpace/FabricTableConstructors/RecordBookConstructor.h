#pragma once 
#include "ReadWriteConstructor.h"

namespace PredictedAdaptedEncoding
{
    class RecordBookConstructor : public ReadWriteConstructor
    {
        
    protected:
        /// @brief Only Reads Valid FabricTableSegmentClasses::SLAB_RECORD_MAP -> Cells
        /// @param desired_table OriginOfRecord == FabricTableSegmentClasses -> Used Rename fore Ease of Developement
        /// @return VALID: Index / INVALID: SIZE_MAX
        constexpr size_t ReadOriginIndexBeginOfRecordBookOfFabricTableSegmentClasses_(OriginOfRecord desired_table) noexcept;
        
        /// @brief Compleatly validates by width and origin -> FabricTableSegmentClasses
        /// @param table_class desired origin table
        /// @return VALID:: 3 -> Packed Cells:: i)Begin, ii)End iii)SaftyAndOriginMeta OR: false & Maybe Inspactable data
        bool GetValidSlabRangeTripletFromRecordBookOfFTSC(
            const FabricTableSegmentClasses table_class,
            RecordBookTablesBoundsCarrier& return_bounds
        ) noexcept;

        /// @brief FILL: DESIRED: FabricTableSegmentClasses with Idle Fabric Cell -> CALLS: GetValidSlabRangeTripletFromRecordBookOfFTSC TO: Get Range In SLab
        /// @param table_class Desired FabricTableSegmentClasses You want Idle
        void IdleAFabricTableClassRangesMemory_(FabricTableSegmentClasses table_class) noexcept;

        /// @brief WRITES: A Single Entry OF: FabricTableSegmentClasses::SLAB_RECORD_MAP == (2xPackedMode::VALUE48 + 1xPackedMode::Model32)
        /// @param table_class Desired FabricTableSegmentClasses == OriginOfRecord
        /// @param begin Begin Index OF: FabricTableSegmentClasses -> Record
        /// @param end End Index OF: FabricTableSegmentClasses -> Record
        void WriteARecordBookOfTSCEntry_(
            OriginOfRecord table_class, 
            size_t begin, size_t end, 
            uint8_t slab_id = UNSIGNED_ZERO
        ) noexcept;

    public:

        /// @brief Uses -> GetValidSlabRangeTripletFromRecordBookOfFTSC to get record and packs into -> APCDescriptorRange
        /// @return VALID::APCDescriptorRange.IsValid = true || INVALID:: APCDescriptorRange.IsValid = false
        bool ReadAPCDescriptorTableBeginEndFromRecordBook(
            APCDescriptorRange& return_APC_handle_description_range
        ) noexcept;

    };

}