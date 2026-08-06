#pragma once
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{


    uint64_t* SlabToFabricConverterAndCordinator::AllocatePackedCellRaw_(size_t count_of_cells) noexcept
    {
        auto allocation_function = AllocatorOfFabric_.AllocatePackedCellStorage ? 
            AllocatorOfFabric_.AllocatePackedCellStorage : &RawPackedCellAllocator::DefaultAllocateAtomicCells;
        
        size_t alignment = AllocatorOfFabric_.Alignment ? AllocatorOfFabric_.Alignment : BIT_LENGTH_OF_FABRIC;
        alignment = std::max<size_t>(alignment, alignof(uint64_t));
        alignment = std::max<size_t>(alignment, BIT_LENGTH_OF_FABRIC);

        return allocation_function(count_of_cells, alignment, AllocatorOfFabric_.User);

    }


    void SlabToFabricConverterAndCordinator::FreeRawPackedCells_(uint64_t* packed_cell_memory_ptr, size_t packed_cell_count) noexcept
    {
        RawPackedCellAllocator::FreeFunction free_function = AllocatorOfFabric_.FreePackedCellStorage ?
                            AllocatorOfFabric_.FreePackedCellStorage : &RawPackedCellAllocator::DefaultFreeAtomicCells;
        size_t alignment = AllocatorOfFabric_.Alignment ? AllocatorOfFabric_.Alignment : BIT_LENGTH_OF_FABRIC;
        alignment = std::max<size_t>(alignment, alignof(uint64_t));
        alignment = std::max<size_t>(alignment, BIT_LENGTH_OF_FABRIC);

        free_function(packed_cell_memory_ptr, packed_cell_count, alignment, AllocatorOfFabric_.User);
    }

    void SlabToFabricConverterAndCordinator::ResetScalarsofTheFabric_() noexcept
    {
        SlabBasePtr_ = nullptr;
        SlabCellCount_ = UNSIGNED_ZERO;
        PerAPCRuntimeCellCount_ = UNSIGNED_ZERO;
        CountOfAPC_ = UNSIGNED_ZERO;
        SlabId_ = APCDataStructure::BRANCH_VERSION;

        SegmentPoolBegin_ = CoreOfFabricCoordinator::FABRIC_UNIT_COUNT;
        SegmentPoolEnd_ = CoreOfFabricCoordinator::FABRIC_UNIT_COUNT;

        HashBucketCount_ = UNSIGNED_ZERO;
        RelationRecordCount_ = UNSIGNED_ZERO;
        DeviceViewRecordCount_ = UNSIGNED_ZERO;
        ThreadTableCapacity_  = UNSIGNED_ZERO;

        FabricInitialized_.store(false, std::memory_order_release);
        InitializationInProgress_.store(false, std::memory_order_release);
    }



    void SlabToFabricConverterAndCordinator::InitializeCompleateFabricMetaIndices_(size_t record_book_begin, size_t record_book_end) noexcept
    {
        using FMI = CoreOfFabricCoordinator::FabricMetaIndicies;

        for (size_t i = 0; i < CoreOfFabricCoordinator::FABRIC_UNIT_COUNT; i++)
        {
            DirectlyStoreFabricUnit64(i, UNSIGNED_ZERO);
        }

        SlabBasePtr_[static_cast<size_t>(FMI::MAGIC)] = CoreOfFabricCoordinator::FABRIC_MAGIC;
        SlabBasePtr_[static_cast<size_t>(FMI::SLAB_ID)] = SlabId_;
        SlabBasePtr_[static_cast<size_t>(FMI::TOTAL_CELLS)] = SlabCellCount_;
        SlabBasePtr_[static_cast<size_t>(FMI::APC_DESCRIPTION_COUNT)] = CountOfAPC_;
        SlabBasePtr_[static_cast<size_t>(FMI::SEGMENT_POOL_BEGIN_IDX)] = SegmentPoolBegin_;
        SlabBasePtr_[static_cast<size_t>(FMI::SEGMENT_POOL_END_IDX)] = SegmentPoolEnd_;
        SlabBasePtr_[static_cast<size_t>(FMI::RECORD_BOOK_OF_TSC_BEGIN)] = record_book_begin;
        SlabBasePtr_[static_cast<size_t>(FMI::RECORD_BOOK_OF_TSC_END)] = record_book_end;
        SlabBasePtr_[static_cast<size_t>(FMI::THREAD_TABLE_CAPACITY)] = ThreadTableCapacity_;
        SlabBasePtr_[static_cast<size_t>(FMI::EOF_FABRIC_HEADER)] = CoreOfFabricCoordinator::FABRIC_META_EOF;
    }


    void SlabToFabricConverterAndCordinator::InitializeAPCDescriptorTable_() noexcept
    {

        for (uint64_t desc_idx = 0; desc_idx < CountOfAPC_; desc_idx++)
        {
            const DescriptorConf::APCDescriptorRange self_range = ReadAPCDescriptionRanges_(desc_idx);
            const APCSegmentPoolRange segment_pool_range = GetSegmentPoolBegainEndForSingleAPCDescription(desc_idx);
            if (!self_range.IsValid || !segment_pool_range.IsValid)
            {
                continue;
            }

            DescriptionOfAPC::SingleAPCDescriptionCellBuffer description_buffer{};

            bool dsc_ok = DescriptionOfAPC::ConstructInitialAPCDescriptionBuffer(
                description_buffer,
                desc_idx,
                segment_pool_range.BeginIndex,
                segment_pool_range.EndIndex
            );

            if (!dsc_ok)
            {
                continue;
            }

            if (
                !ForceNxLenMemCopy(
                    self_range.BeginIndex,
                    CoreOfFabricCoordinator::DESCRIPTION_WIDTH_AND_VALIDATION_IDX,
                    description_buffer.data()
                )
            )
            {
                continue;
            }
            
            
            for (size_t seg_idx = segment_pool_range.BeginIndex; seg_idx < segment_pool_range.EndIndex; seg_idx++)
            {
                DirectlyStoreFabricUnit64(seg_idx, UNSIGNED_ZERO);
            }
            
        }
                
    }

    bool SlabToFabricConverterAndCordinator::InitializeFabric(
        uint32_t slot_count,
        uint32_t slot_cell_count,
        uint8_t slab_id,
        uint32_t fabric_thread_capacity
    ) noexcept
    {
        bool expected = false;
        if (!InitializationInProgress_.compare_exchange_strong(expected, true, std::memory_order_acq_rel, std::memory_order_acquire))
        {
            return false;
        }

        struct InitGuardSTFC
        {
            SlabToFabricConverterAndCordinator* SelfPtr{};
            bool SuccesInit{false};


            ~InitGuardSTFC()
            {
                if (!SuccesInit && SelfPtr)
                {
                    SelfPtr->FabricInitialized_.store(false, std::memory_order_release);
                }
                
                if (SelfPtr)
                {
                    SelfPtr->InitializationInProgress_.store(false, std::memory_order_release);
                }
            }
        } internal_init_guard{this, false};
        
        ShutDownFabric();

        InitializationInProgress_.store(true, std::memory_order_release);

        if (slot_count == UNSIGNED_ZERO || !APCDataStructure::IsValid32BitAPCUnit(slot_count))
        {
            return false;
        }
        
        if (!APCDataStructure::IsCapacityOfAPCValid(slot_cell_count))
        {
            return false;
        }
        CountOfAPC_ = static_cast<uint64_t>(slot_count);
        PerAPCRuntimeCellCount_ = static_cast<uint32_t>(slot_cell_count);
        SlabId_ = slab_id == UNSIGNED_ZERO ? APCDataStructure::BRANCH_VERSION : slab_id;
        ThreadTableCapacity_ = fabric_thread_capacity == UNSIGNED_ZERO ? CoreOfFabricCoordinator::DEFAULT_THREAD_TABLE_CAPACITY : fabric_thread_capacity;

        HashBucketCount_ = HashHelpers::BucketCountForExpectedEntries(CountOfAPC_);

        if (HashBucketCount_ == UNSIGNED_ZERO || HashBucketCount_ >= FABRIC_CELL_SENTINAL)
        {
            return false;
        }
        
        RelationRecordCount_ = HashIdConstructror::NextPowerOf2Unsigned64(std::max<uint64_t>(HashHelpers::MIN_LIMIT_POW_OF_2, CountOfAPC_ * HashHelpers::DEFAULT_TABLE_TAILROOM_MULT));
        DeviceViewRecordCount_ = HashIdConstructror::NextPowerOf2Unsigned64(std::max<uint64_t>(HashHelpers::MIN_LIMIT_POW_OF_2, CountOfAPC_ )); // NO EXTRA TAILROOM

        size_t cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(CoreOfFabricCoordinator::FABRIC_UNIT_COUNT);
        const size_t record_book_begin = cursor;
        const size_t record_book_end = record_book_begin + static_cast<size_t>(RecordBookConf::RECORD_BOOK_INTERNAL_SEGMENT_COUNT) * CoreOfFabricCoordinator::RECORD_BOOK_WIDTH;

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(record_book_end);
        const size_t apc_description_begin = cursor;
        const size_t apc_description_end = apc_description_begin + static_cast<size_t>(CountOfAPC_ * CoreOfFabricCoordinator::DESCRIPTION_WIDTH_AND_VALIDATION_IDX);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(apc_description_end);
        const size_t branch_hash_begin = cursor;
        const size_t branch_hash_end = branch_hash_begin + static_cast<size_t>(HashBucketCount_ * CoreOfFabricCoordinator::HASH_BUCKED_WIDTH_OF_FABRIC);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(branch_hash_end);
        const size_t logical_hash_begin = cursor;
        const size_t logical_hash_end = logical_hash_begin + static_cast<size_t>(HashBucketCount_ * CoreOfFabricCoordinator::HASH_BUCKED_WIDTH_OF_FABRIC);
        
        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(logical_hash_end);
        const size_t shared_hash_begin = cursor;
        const size_t shared_hash_end = shared_hash_begin + static_cast<size_t>(HashBucketCount_ * CoreOfFabricCoordinator::HASH_BUCKED_WIDTH_OF_FABRIC);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(shared_hash_end);
        const size_t edge_table_begin = cursor;
        const size_t edge_table_end = edge_table_begin + static_cast<size_t>(CountOfAPC_ * CoreOfFabricCoordinator::QUEUE_RECORD_WIDTH_OF_FABRIC);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(edge_table_end);
        const size_t free_list_begin = cursor;
        const size_t free_list_end = free_list_begin + static_cast<size_t>(CountOfAPC_ * CoreOfFabricCoordinator::QUEUE_RECORD_WIDTH_OF_FABRIC);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(free_list_end);
        const size_t ready_queue_begin = cursor;
        const size_t ready_queue_end = ready_queue_begin + static_cast<size_t>(CountOfAPC_ * CoreOfFabricCoordinator::QUEUE_RECORD_WIDTH_OF_FABRIC);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(ready_queue_end);
        const size_t work_queue_begin = cursor;
        const size_t work_queue_end = work_queue_begin + static_cast<size_t>(CountOfAPC_ * CoreOfFabricCoordinator::WORK_RECORD_WIDTH_OF_FABRIC);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(work_queue_end);
        const size_t device_view_table_begin = cursor;
        const size_t device_view_table_end = device_view_table_begin + static_cast<size_t>(DeviceViewRecordCount_ * CoreOfFabricCoordinator::DEVICE_VIEW_WIDTH_OF_APC_FABRIC);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(device_view_table_end);
        const size_t thread_table_begin = cursor;
        const size_t thread_table_end = thread_table_begin + static_cast<size_t>(ThreadTableCapacity_ * CoreOfFabricCoordinator::THREAD_TABLE_RECORD_WIDTH);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(thread_table_end);
        SegmentPoolBegin_ = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(std::max<size_t>(cursor, CoreOfFabricCoordinator::DEFAULT_FABRIC_CONTROLIO_LENGTH));
        SegmentPoolEnd_ = SegmentPoolBegin_ + static_cast<size_t>(CountOfAPC_ * PerAPCRuntimeCellCount_);
        SlabCellCount_ = SegmentPoolEnd_;

        if (SlabCellCount_ == UNSIGNED_ZERO || SlabCellCount_ >= FABRIC_CELL_SENTINAL)
        {
            return false;
        }
        SlabBasePtr_ = AllocatePackedCellRaw_(SlabCellCount_);
        if (!SlabBasePtr_)
        {
            return false;
        }

        for (size_t idx = 0; idx < SlabCellCount_; idx++)
        {
            DirectlyStoreFabricUnit64(idx, UNSIGNED_ZERO);
        }

        InitializeCompleateFabricMetaIndices_(record_book_begin, record_book_end);

        //RECORD_BOOK_OF_TABLE_SEGMENT_CLASS - ENTRIES
        WriteARecordBookOfTSCEntry_(FabricSegments::SLAB_RECORD_MAP, record_book_begin, record_book_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::APC_HANDLE_DESCRIPTOR, apc_description_begin, apc_description_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::BRANCH_HASH, branch_hash_begin, branch_hash_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::VERTICAL_HASH, logical_hash_begin, logical_hash_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::HORIZONTAL_HASH, shared_hash_begin, shared_hash_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::VERTICAL_EDGE_TABLE, edge_table_begin, edge_table_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::FREE_APC_LIST, free_list_begin, free_list_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::READY_QUEUE, ready_queue_begin, ready_queue_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::WORK_QUEUE, work_queue_begin, work_queue_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::DEVICE_VIEW_TABLE, device_view_table_begin, device_view_table_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::THREAD_TABLE, thread_table_begin, thread_table_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::SEGMENT_POOL, SegmentPoolBegin_, SegmentPoolEnd_);
        //ENTRIES:: END ::SLAB_RECORD_MAP

        //IDLE UNUSED FabricSegments
        IdleAFabricTableClassRangesMemory_(FabricSegments::VERTICAL_EDGE_TABLE);
        IdleAFabricTableClassRangesMemory_(FabricSegments::FREE_APC_LIST);
        IdleAFabricTableClassRangesMemory_(FabricSegments::READY_QUEUE);
        IdleAFabricTableClassRangesMemory_(FabricSegments::WORK_QUEUE);
        IdleAFabricTableClassRangesMemory_(FabricSegments::DEVICE_VIEW_TABLE);
        IdleAFabricTableClassRangesMemory_(FabricSegments::THREAD_TABLE);
        //END:: IDELING

        //INIT: HASH TABLES
        InitializeHashTable_(FabricSegments::BRANCH_HASH);
        InitializeHashTable_(FabricSegments::VERTICAL_HASH);
        InitializeHashTable_(FabricSegments::HORIZONTAL_HASH);
        //END::: 
        //INIT:DESCRIPTOR TABLE
        InitializeAPCDescriptorTable_();

        //CONFERMATION
        FabricInitialized_.store(true, std::memory_order_release);
        internal_init_guard.SuccesInit = true;
        return true;
        
    }
    
    void SlabToFabricConverterAndCordinator::ShutDownFabric() noexcept
    {

        FabricInitialized_.store(false, std::memory_order_release);
        uint64_t* old_ptr = SlabBasePtr_;
        const size_t old_count = SlabCellCount_;
        SlabBasePtr_ = nullptr;
        SlabCellCount_ = UNSIGNED_ZERO;
        if (old_ptr)
        {
            FreeRawPackedCells_(old_ptr, old_count);
        }
        ResetScalarsofTheFabric_();
    }

    std::optional<uint64_t> SlabToFabricConverterAndCordinator::NewApcFromFabric(
        APCAxisSelection desired_axis,
        IAB::BufferOfAPCIdentity& identity_buffer_new_apc
    ) noexcept
    {
        uint64_t slot_new = FABRIC_CELL_SENTINAL;
        const std::optional<uint64_t> previous_st = GetASlotForNewAPCLink(slot_new);
        const DSA::DescriptorSaftyFiles previous_files = DSA::GetDescriptionFile(previous_st.value());
        if (
            !previous_st.has_value() ||
            !previous_files.IsValid
        )
        {
            return std::nullopt;
        }

        auto ReleseReservedSlot__ = [&]()
        {
            SwitchOwnershipOfAReadyDescription(slot_new, previous_files.StateOfTheAPC, true);
        };

        DSA::SingleAPCDescriptionCellBuffer description{};

        if (
            !ReadACompleateAPCDescriptorBuffer_(slot_new, description) ||
            !DSA::BuildIdentityBufferFromDescriptionBuffer(description, identity_buffer_new_apc)
        )
        {
            ReleseReservedSlot__();
            return std::nullopt;
        }
        HTC::SingleHashBuffer branch_hash_V{};
        HTC::SingleHashBuffer axis_hash_V{};
        HTC::SingleHashBuffer branch_hash_H{};
        HTC::SingleHashBuffer axis_hash_H{};

        if (IAB::WantsVertical(desired_axis))
        {
            if (
                !HTC::InstallOwnedRoot(
                    identity_buffer_new_apc,
                    IAB::BidirectionalAxis::VARTICAL_LOGICAL,
                    branch_hash_V,
                    axis_hash_V
                )
            )
            {
                return std::nullopt;
            }

            if (!PublishPreparedHashBuffer_(FabricSegments::BRANCH_HASH, branch_hash_V))
            {
                return std::nullopt;
            }

            if (!PublishPreparedHashBuffer_(FabricSegments::VERTICAL_HASH, axis_hash_V))
            {
                RetireHashKey_(
                    FabricSegments::VERTICAL_HASH, 
                    HTC::GetAUnitFromHashBuffer(branch_hash_V, HTC::HashBufferIndexing::KEY_INDEX)
                );
                return std::nullopt;
            }
        }

        if (IAB::WantsHorizontal(desired_axis))
        {
            if (
                !HTC::InstallOwnedRoot(
                    identity_buffer_new_apc,
                    IAB::BidirectionalAxis::HORIZONTALLY_SHARED,
                    branch_hash_H,
                    axis_hash_H
                )
            )
            {
                return std::nullopt;
            }

            if (!PublishPreparedHashBuffer_(FabricSegments::BRANCH_HASH, branch_hash_H))
            {
                if (IAB::WantsHorizontal(desired_axis))
                {
                    RetireHashKey_(
                        FabricSegments::BRANCH_HASH, 
                        HTC::GetAUnitFromHashBuffer(branch_hash_V, HTC::HashBufferIndexing::KEY_INDEX)
                    );
                    RetireHashKey_(
                        FabricSegments::VERTICAL_HASH, 
                        HTC::GetAUnitFromHashBuffer(axis_hash_V, HTC::HashBufferIndexing::KEY_INDEX)
                    );
                }
                return std::nullopt;
            }

            if (!PublishPreparedHashBuffer_(FabricSegments::HORIZONTAL_HASH, axis_hash_H))
            {
                if (IAB::WantsHorizontal(desired_axis))
                {
                    RetireHashKey_(
                        FabricSegments::BRANCH_HASH, 
                        HTC::GetAUnitFromHashBuffer(branch_hash_V, HTC::HashBufferIndexing::KEY_INDEX)
                    );
                    RetireHashKey_(
                        FabricSegments::VERTICAL_HASH, 
                        HTC::GetAUnitFromHashBuffer(axis_hash_V, HTC::HashBufferIndexing::KEY_INDEX)
                    );
                    RetireHashKey_(
                        FabricSegments::BRANCH_HASH, 
                        HTC::GetAUnitFromHashBuffer(branch_hash_H, HTC::HashBufferIndexing::KEY_INDEX)
                    );
                }
                return std::nullopt;
            }
        }
        return slot_new;
    }


}
