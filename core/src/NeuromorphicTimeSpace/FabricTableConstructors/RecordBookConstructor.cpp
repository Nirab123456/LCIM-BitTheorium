#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace PredictedAdaptedEncoding
{

    void RecordBookConstructor::IdleAFabricTableClassRangesMemory_(FabricTableSegmentClasses table_class) noexcept
    {

        RecordBookTablesBoundsCarrier return_bounds{};
        bool bounds_ok = GetValidSlabRangeTripletFromRecordBookOfFTSC(table_class, return_bounds);

        if (!bounds_ok)
        {
            return;
        }

        const packed64_t idle_table_cell = PackedCell64_t::MakeTypedFabricValidPackedCell(
            TypeFamily::VALUE48, ContractOfConcurrency::BOUNDED_RETRY_CAS_NO_CLAIMED,
            table_class, LocalityPolicy::IDLE,
            InternalDataTypePolicy::UnsignedPCellDataType
        );

        for (size_t idx = return_bounds.BeginIndex; idx < return_bounds.EndIndex; idx++)
        {
            StorePackedCellUncheckedDirectly(idx, idle_table_cell);
        }
        
    }

    bool RecordBookConstructor::GetValidSlabRangeTripletFromRecordBookOfFTSC(
        const FabricTableSegmentClasses table_class,
        RecordBookTablesBoundsCarrier& return_bounds
    ) noexcept
    {
        if (!PackedCell64_t::IsKnownFabricRegion(table_class))
        {
            return false;
        }

        const size_t begin_of_desired_table = ReadOriginIndexBeginOfRecordBookOfFabricTableSegmentClasses_(table_class);
        
        const size_t end_idx = begin_of_desired_table + static_cast<size_t>(RecordBookInternalIndexing::END48);
        const size_t safty_lock_meta_cell = begin_of_desired_table + static_cast<size_t>(RecordBookInternalIndexing::META32);
        if (end_idx >= SlabCellCount_ || begin_of_desired_table < APCDataStructure::METACELL_COUNT)
        {
            return false;
        }

        const RecordBookCellTripletGroup triplet{ 
            ReadCompletePackedCellDirectly(begin_of_desired_table),
            ReadCompletePackedCellDirectly(end_idx),
            ReadCompletePackedCellDirectly(safty_lock_meta_cell)
        };


        return_bounds  = RecordBookConf::ValidateAFabricTableRangeStruct(triplet, table_class);
        if (
            return_bounds.IsValid &&
            return_bounds.BeginIndex >= APCDataStructure::METACELL_COUNT &&
            return_bounds.EndIndex > return_bounds.BeginIndex
        )
        {
            return true;
        }

        return_bounds.IsValid = false;
        return false;
    }


    void RecordBookConstructor::WriteARecordBookOfTSCEntry_(
        OriginOfRecord table_class, 
        size_t begin, size_t end, 
        uint8_t slab_id
    ) noexcept
    {
        if (!SlabBasePtr_ || !PackedCell64_t::IsKnownFabricRegion(table_class))
        {
            return;
        }

        const size_t base_idx = ReadOriginIndexBeginOfRecordBookOfFabricTableSegmentClasses_(table_class);
        if (
            base_idx == APCDataStructure::APC_SIZE_SENTINAL || 
            (base_idx + RECORD_BOOK_WIDTH > SlabCellCount_) ||
            begin >= end || end > SlabCellCount_
        )
        {
            return;
        }

        RecordBookCellTripletGroup desired_record_triplet {};

        desired_record_triplet.BeginIdxRawType48Cell = RecordBookConf::MakeRecordBookCellOfTSC(
            static_cast<uint64_t>(begin)
        );

        desired_record_triplet.EndIdxRawType48Cell =  RecordBookConf::MakeRecordBookCellOfTSC(
            static_cast<uint64_t>(end)
        );

        desired_record_triplet.WidthVersionOriginSafty = RecordBookConf::MakeRecordBookSaftyLock(
            begin, end, table_class, 
            LocalityPolicy::PUBLISHED, slab_id
        );


        const RecordBookTablesBoundsCarrier validated_bounds = RecordBookConf::ValidateAFabricTableRangeStruct(desired_record_triplet, table_class);
        if (
            !validated_bounds.IsValid 
        )
        {
            return;
        }

        AtomicallyStorePackedCellUnchecked(
            base_idx + static_cast<size_t>(RecordBookInternalIndexing::BEGIN48), 
            desired_record_triplet.BeginIdxRawType48Cell
        );
        
        AtomicallyStorePackedCellUnchecked(
            base_idx + static_cast<size_t>(RecordBookInternalIndexing::END48), 
            desired_record_triplet.EndIdxRawType48Cell
        );                
        
        AtomicallyStorePackedCellUnchecked(
            base_idx + static_cast<size_t>(RecordBookInternalIndexing::META32), 
            desired_record_triplet.WidthVersionOriginSafty
        );
        
    }


    constexpr size_t RecordBookConstructor::ReadOriginIndexBeginOfRecordBookOfFabricTableSegmentClasses_(
        OriginOfRecord table_class
    ) noexcept
    {
        if (!PackedCell64_t::IsKnownFabricRegion(table_class))
        {
            return APCDataStructure::APC_SIZE_SENTINAL;
        }

        /// ALways same derives from -> FabricMetaIndicies
        const packed64_t directory_begin_cell = ReadCompletePackedCellDirectly(static_cast<size_t>(FabricMetaIndicies::RECORD_BOOK_OF_TSC_BEGIN));

        const PackedCell64_t::AuthoritiveCellView base_idx_record_book_view = PackedCell64_t::GetAuthoritiveViewsForACell(directory_begin_cell);

        if (!CoreOfFabricCoordinator::IsCellValidFabricMetaIndecies(base_idx_record_book_view))
        {
            return APCDataStructure::APC_SIZE_SENTINAL;
        }

        return static_cast<size_t>(base_idx_record_book_view.Raw48BitInCellData + (static_cast<size_t>(table_class) * RECORD_BOOK_WIDTH));        

    }

    bool RecordBookConstructor::ReadAPCDescriptorTableBeginEndFromRecordBook(
        APCDescriptorRange& return_APC_handle_description_range
    ) noexcept
    {
        RecordBookTablesBoundsCarrier return_bounds{};

        bool bounds_ok = GetValidSlabRangeTripletFromRecordBookOfFTSC(FabricTableSegmentClasses::APC_HANDLE_DESCRIPTOR, return_bounds);

        if (!bounds_ok)
        {
            return_APC_handle_description_range.IsValid = false;
            return false;
        }

        return_APC_handle_description_range.BeginIndex = return_bounds.BeginIndex;
        return_APC_handle_description_range.EndIndex = return_bounds.EndIndex;
        return_APC_handle_description_range.IsValid = return_bounds.IsValid;

        return return_bounds.IsValid;
    }
        
}
