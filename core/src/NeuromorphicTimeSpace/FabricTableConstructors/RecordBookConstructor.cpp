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
        return_bounds = {};
        const uint64_t begin_of_desired_table = BegainIdxOfAnyFabTableHeader(table_class);
        
        const uint64_t end_idx = begin_of_desired_table + static_cast<uint64_t>(CoreOfFabricCoordinator::RecordBookInternalIndexing::END64);

        if (
            end_idx > SlabCellCount_ || 
            begin_of_desired_table < APCDataStructure::METACELL_COUNT ||
            !APCDataStructure::IsValidFabricUnit(begin_of_desired_table)
        )
        {
            return false;
        }

        return_bounds.BeginIndex = begin_of_desired_table;
        return_bounds.EndIndex = end_idx;
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
            !APCDataStructure::IsValidFabricUnit(base_idx) || 
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


    uint64_t RecordBookConstructor::BegainIdxOfAnyFabTableHeader(
        FabricTableSegmentClasses table_class
    ) noexcept
    {
        const uint64_t record_map_begin = ReadAFabricU64Directly(static_cast<size_t>(CoreOfFabricCoordinator::FabricMetaIndicies::RECORD_BOOK_OF_TSC_BEGIN));
        const std::optional<uint8_t> table_ordinal = CoreOfFabricCoordinator::GetOrdinalOfFabricTable(table_class);
        if (
            !table_ordinal.has_value() ||
            !APCDataStructure::IsValidFabricUnit(record_map_begin)
        )
        {
            return FABRIC_CELL_SENTINAL;
        }
        return static_cast<uint64_t>(record_map_begin + (table_ordinal.value() * CoreOfFabricCoordinator::RECORD_BOOK_WIDTH));        
    }

        
}
