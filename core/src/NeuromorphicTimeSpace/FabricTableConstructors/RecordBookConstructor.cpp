#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{

    void RecordBookConstructor::IdleAFabricTableClassRangesMemory_(FabricTableSegmentClasses table_class) noexcept
    {

        RecordBookConf::RecordBookTablesBoundsCarrier return_bounds{};
        if (!GetRecordMapCarrierRanges_(table_class, return_bounds))
        {
            return;
        }

        for (size_t idx = return_bounds.BeginIndex; idx < return_bounds.EndIndex; idx++)
        {
            DirectlyStoreFabricUnit64(idx, UNSIGNED_ZERO);
        }
    }

    bool RecordBookConstructor::GetRecordMapCarrierRanges_(
        const FabricTableSegmentClasses table_class,
        RecordBookConf::RecordBookTablesBoundsCarrier& return_bounds
    ) noexcept
    {
        return_bounds = {};
        const uint64_t entry_idx = GetStartingOfAnyFabricTable_(table_class);
        if (
            entry_idx + CoreOfFabricCoordinator::RECORD_BOOK_WIDTH > SlabCellCount_ ||
            !AtomicallyLoadReadAUnit(
                entry_idx + static_cast<uint8_t>(CoreOfFabricCoordinator::RecordBookInternalIndexing::BEGIN64),
                return_bounds.BeginIndex
            ) ||
            !AtomicallyLoadReadAUnit(
                entry_idx + static_cast<uint8_t>(CoreOfFabricCoordinator::RecordBookInternalIndexing::END64),
                return_bounds.EndIndex
            ) ||
            return_bounds.BeginIndex >= return_bounds.EndIndex ||
            return_bounds.EndIndex > SlabCellCount_
        )
        {
            return_bounds.IsValid = false;
            return false;
        }
        return_bounds.IsValid = true;
        return return_bounds.IsValid;
    }


    void RecordBookConstructor::WriteARecordBookOfTSCEntry_(
        FabricTableSegmentClasses table_class, 
        size_t begin, 
        size_t end
    ) noexcept
    {
        const size_t base_idx = GetStartingOfAnyFabricTable_(table_class);
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


    uint64_t RecordBookConstructor::GetStartingOfAnyFabricTable_(
        FabricTableSegmentClasses table_class
    ) noexcept
    {   uint64_t record_map_begin = UNSIGNED_ZERO;
        const bool read_ok = ReadAFabricU64Directly(
            static_cast<size_t>(CoreOfFabricCoordinator::FabricMetaIndicies::RECORD_BOOK_OF_TSC_BEGIN),
            record_map_begin
        );
        const std::optional<uint8_t> table_ordinal = CoreOfFabricCoordinator::GetOrdinalOfFabricTable(table_class);
        if (
            !read_ok ||
            !table_ordinal.has_value() ||
            !APCDataStructure::IsValidFabricUnit(record_map_begin)
        )
        {
            return FABRIC_CELL_SENTINAL;
        }
        return static_cast<uint64_t>(record_map_begin + (table_ordinal.value() * CoreOfFabricCoordinator::RECORD_BOOK_WIDTH));        
    }

        
}
