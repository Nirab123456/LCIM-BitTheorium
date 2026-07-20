#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace PredictedAdaptedEncoding
{

    void RecordBookConstructor::IdleAFabricTableClassRangesMemory_(FabricTableSegmentClasses table_class) noexcept
    {

        RecordBookConf::RecordBookTablesBoundsCarrier return_bounds{};
        if (!BegainEndIdxHeaderPairGet(table_class, return_bounds))
        {
            return;
        }

        for (size_t idx = return_bounds.BeginIndex; idx < return_bounds.EndIndex; idx++)
        {
            DirectlyStoreFabricUnit64(idx, UNSIGNED_ZERO);
        }
    }

    bool RecordBookConstructor::BegainEndIdxHeaderPairGet(
        const FabricTableSegmentClasses table_class,
        RecordBookConf::RecordBookTablesBoundsCarrier& return_bounds
    ) noexcept
    {
        const size_t begin_of_desired_table = BegainIdxOfAnyFabTableHeader(table_class);
        
        const size_t end_idx = begin_of_desired_table + static_cast<size_t>(CoreOfFabricCoordinator::RecordBookInternalIndexing::END64);

        if (end_idx >= SlabCellCount_ || begin_of_desired_table < APCDataStructure::METACELL_COUNT)
        {
            return false;
        }

        return_bounds.BeginIndex = begin_of_desired_table;
        return_bounds.IsValid = true;
        return return_bounds.IsValid;
    }


    void RecordBookConstructor::WriteARecordBookOfTSCEntry_(
        FabricTableSegmentClasses table_class, 
        size_t begin, 
        size_t end
    ) noexcept
    {
        const size_t base_idx = BegainIdxOfAnyFabTableHeader(table_class);
        if (
            base_idx == SIZE_MAX || 
            (base_idx + CoreOfFabricCoordinator::RECORD_BOOK_WIDTH > SlabCellCount_) ||
            begin >= end || end > SlabCellCount_
        )
        {
            return;
        }

        AtomicallyStoreU64Fab(
            base_idx + static_cast<size_t>(CoreOfFabricCoordinator::RecordBookInternalIndexing::BEGIN64), 
            begin
        );
        
        AtomicallyStoreU64Fab(
            base_idx + static_cast<size_t>(CoreOfFabricCoordinator::RecordBookInternalIndexing::END64), 
            end
        );                
        
    }


    size_t RecordBookConstructor::BegainIdxOfAnyFabTableHeader(
        FabricTableSegmentClasses table_class
    ) noexcept
    {
        /// ALways same derives from -> FabricMetaIndicies
        const uint64_t record_map_begin = ReadAFabricU64Directly(static_cast<size_t>(CoreOfFabricCoordinator::FabricMetaIndicies::RECORD_BOOK_OF_TSC_BEGIN));

        return static_cast<size_t>(record_map_begin + (static_cast<size_t>(table_class) * CoreOfFabricCoordinator::RECORD_BOOK_WIDTH));        
    }

        
}
