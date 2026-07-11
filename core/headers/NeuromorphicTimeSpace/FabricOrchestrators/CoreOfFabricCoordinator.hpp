#pragma once 
#include "../../AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"

namespace PredictedAdaptedEncoding
{

    static constexpr uint64_t APC_FABRIC_INDEX_SENTINAL = PackedCell64_t::BIT_FAMILY_48_SENTINAL;


    /// UNCHECKED
    static constexpr size_t RELATION_WIDTH_OF_FABRIC = 8u;
    static constexpr size_t QUEUE_RECORD_WIDTH_OF_FABRIC = 2u;
    static constexpr size_t WORK_RECORD_WIDTH_OF_FABRIC = 4u;
    static constexpr size_t DEVICE_VIEW_WIDTH_OF_APC_FABRIC = 8u;
    static constexpr size_t THREAD_TABLE_RECORD_WIDTH = 4u;
    static constexpr size_t DEFAULT_THREAD_TABLE_CAPACITY = 256u;

    static constexpr size_t DEFAULT_FABRIC_CONTROLIO_LENGTH = 1024u;
    ///--------------------------


    enum class RecordBookInternalIndexing : tag8_t
    {
        BEGIN48 = 0,
        END48 = 1,
        META32 = 2,
        UNASSIGNED_UNUSED_NANNULL = 3
    };
    static constexpr size_t RECORD_BOOK_WIDTH = static_cast<size_t>(RecordBookInternalIndexing::UNASSIGNED_UNUSED_NANNULL);

    enum class HashTableInternalIndexing : tag8_t
    {
        KEY_INDEX = 0,
        VALUE_INDEX = 1,
        PROB_DISTANCE_LOCK = 2,
        UNASSIGNED_UNUSED_NANNULL = 3
    };
    static constexpr size_t HASH_BUCKED_WIDTH_OF_FABRIC = static_cast<size_t>(HashTableInternalIndexing::UNASSIGNED_UNUSED_NANNULL);

    /// @brief DESCRIBS: Initial Fundamental Meta for An APC When Created 
    enum class APCDescriptorCellType : uint8_t
    {
        CURRENT_DESCRIPTOR_INDEX = 0,
        OWNER_BRANCH = 1,
        APC_SEGMENTPOOL_BEGAIN_SLAB = 2,
        APC_SEGMENTPOOL_END_SLAB = 3,
        NEXT_APC_SAGMANTPOOL_BEGAIN = 4,
        LOGICAL_ID = 5,
        SHARED_GROUP_ID = 6,
        RELATION_HEADS = 7,
        RETIRE_EPOCH48 = 8,
        APC_FLAGS_FOR_THIS = 9,
        OCCUPANCY_CELL16x3 = 10,
        STATE_OWNERSHIP_VESION_SAFTY = 11,
        UNASSIGNED_UNUSED_NANNULL = 12
    };
    static constexpr size_t APC_DESCRIPTOR_WIDTH_OR_VALIDATION_INDEX = static_cast<size_t>(APCDescriptorCellType::UNASSIGNED_UNUSED_NANNULL);



    enum class FabricMetaIndicies : uint8_t
    {
        MAGIC = 0,
        VERSION = 1,
        FLAGS = 2,
        SLAB_ID = 3,
        TOTAL_CELLS = 4,
        CONTROL_CELLS_OF_FABRIC = 5,
        APC_DESCRIPTION_COUNT = 6,
        PER_APC_RUNTIME_CELL_COUNT = 7,
        SEGMENT_POOL_BEGIN_IDX = 8,
        SEGMENT_POOL_END_IDX = 9,
        
        TOTAL_APC_IN_USE  = 10,
        RETIRE_SLOT_HEAD = 11,
        RELATION_FREE_HEAD = 12,
        RESERVED_13 = 13,
        RESERVED_14 = 14,
        NEXT_BRANCH_ID = 15,
        NEXT_RELATION_ID = 16,
        NEXT_DEVICE_VIEW_ID = 17,


        WORK_WRITE_CURSOR = 18,
        WORK_READ_CURSOR = 19,
        READY_WRITE_CURSOR = 20,
        READY_READ_CURSOR = 21,

        //COUNTS
        GLOBAL_EPOCH48 = 22,
        MIN_SAFE_EPOCH48 = 23,
        RELATION_RECLAIM_COUNT = 24,
        WORK_QUEUE_DROPPED_COUNT = 25,
        THREAD_ACTIVE_COUNT = 26,
        THREAD_REGISTRATION_FAILURE = 27,
        RELATION_TOMBSTONE_COUNT = 28,
        RELATION_UNLINK_FAILURES = 29,
        WORK_QUEUE_CLAIM_FAILURES = 30,
        CAS_FAILURE_COUNT = 31,
        ERROR_COUNT = 32,
        RETIRED_COUNT = 33,
        LIVE_SLOT_COUNT = 34,
        FABRIC_CLOCK16 = 35,
        ///end count
        
        RECORD_BOOK_OF_TSC_BEGIN = 36,
        RECORD_BOOK_OF_TSC_END = 37,
        TABLE_DIRECTORY_COUNT = 38,
        TABLE_DIRECTORY_VERSION = 39,

        // It dosent have a use case 
        FABRIC_OCCUPANCY_APPROXIMATION_IDLE_LOW32 = 40,
        FABRIC_OCCUPANCY_APPROXIMATION_IDLE_HIGH32 = 41,
        FABRIC_OCCUPANCY_APPROXIMATION_PUBLISHED_LOW32 = 42,
        FABRIC_OCCUPANCY_APPROXIMATION_PUBLISHED_HIGH32 = 43,
        FABRIC_OCCUPANCY_APPROXIMATION_CLAIMED_LOW32 = 44,
        FABRIC_OCCUPANCY_APPROXIMATION_CLAIMED_HIGH32 = 45,
        FABRIC_OCCUPANCY_APPROXIMATION_FAULTY_LOW32 = 46,
        FABRIC_OCCUPANCY_APPROXIMATION_FAULTY_HIGH32 = 47,
        //end pair

        HASH_TOMBSTONE_COUNT = 48,
        HASH_COMPACTION_COUNT = 49,
        WORK_QUEUE_OCCUPANCY = 50,
        READY_QUEUE_OCCUPANCY = 51,

        BACKOFF_SPIN_LIMIT = 52,
        BACKOFF_YIELD_LIMIT = 53,
        INITIALIZATION_STATE = 54,
        HAS_COMPACTION_INFLIGHT = 55,

        THREAD_TABLE_CAPACITY = 56,


        RESERVED_57_UPTO_94 = 57,

        EOF_FABRIC_HEADER = 95

    };


    enum class JustifyClaimCas
    {
        SUCCESS = 0,
        OUT_OF_BOUND = 1,
        INVALID_CELL = 2,
        CELL_SENTINAL_STATE = 3,
        CELL_INVALID = 4,
        CAS_LOOP_RANOUT = 5,
        UNDEFINED_CAS_FAILURE = 6,
        INVALID_USE_OF_METHOD = 7

    };

    /// @brief To Choose a Buffer to use 
    enum class SizeOfABuffer : uint8_t
    {
        SMALL = 0,
        MIDIUM = 1,
        LARGE = 3
    };

    struct CoreOfFabricCoordinator
    {
        static constexpr uint32_t FABRIC_MAGIC = 0x41504643u;
        static constexpr uint32_t FABRIC_META_EOF = 0x41474946u;
        static constexpr uint8_t EACH_TABLE_RECORD_SENTINAL = UINT8_MAX;

        static constexpr bool IsValidHashTable(FabricTableSegmentClasses table_class) noexcept
        {
            return table_class == FabricTableSegmentClasses::BRANCH_HASH ||
                table_class == FabricTableSegmentClasses::LOGICAL_HASH ||
                table_class == FabricTableSegmentClasses::SHARED_HASH;
        }

        static constexpr bool IsQueueTable(FabricTableSegmentClasses table_class) noexcept
        {
            return table_class == FabricTableSegmentClasses::FREE_APC_LIST ||
                table_class == FabricTableSegmentClasses::READY_QUEUE ||
                table_class == FabricTableSegmentClasses::WORK_QUEUE;
        }

        static constexpr FabricMetaIndicies GetDesiredLowIdxOfOccupancyPairFromLocality(LocalityPolicy locality) noexcept
        {
            switch (locality)
            {
            case LocalityPolicy::IDLE :
                return FabricMetaIndicies::FABRIC_OCCUPANCY_APPROXIMATION_IDLE_LOW32;

            case LocalityPolicy::PUBLISHED :
                return FabricMetaIndicies::FABRIC_OCCUPANCY_APPROXIMATION_PUBLISHED_LOW32;

            case LocalityPolicy::CLAIMED :
                return FabricMetaIndicies::FABRIC_OCCUPANCY_APPROXIMATION_CLAIMED_LOW32;

            case LocalityPolicy::FAULTY :
                return FabricMetaIndicies::FABRIC_OCCUPANCY_APPROXIMATION_FAULTY_LOW32;

            default:
                return FabricMetaIndicies::EOF_FABRIC_HEADER;
            }
        }


        static constexpr uint8_t GetWidthOfValidFabricTable(FabricTableSegmentClasses table_idintity) noexcept
        {
            switch (table_idintity)
            {
            case FabricTableSegmentClasses::SLAB_RECORD_MAP:
                return static_cast<uint8_t>(RECORD_BOOK_WIDTH);
            
            case FabricTableSegmentClasses::APC_HANDLE_DESCRIPTOR:
                return static_cast<uint8_t>(APC_DESCRIPTOR_WIDTH_OR_VALIDATION_INDEX);
            
            case FabricTableSegmentClasses::BRANCH_HASH:
            case FabricTableSegmentClasses::SHARED_HASH:
            case FabricTableSegmentClasses::LOGICAL_HASH:
                return static_cast<uint8_t>(HASH_BUCKED_WIDTH_OF_FABRIC);
            
            case FabricTableSegmentClasses::EDGE_TABLE:
                return static_cast<uint8_t>(RELATION_WIDTH_OF_FABRIC);

            case FabricTableSegmentClasses::FREE_APC_LIST:
            case FabricTableSegmentClasses::READY_QUEUE:
                return static_cast<uint8_t>(QUEUE_RECORD_WIDTH_OF_FABRIC);

            case FabricTableSegmentClasses::WORK_QUEUE:
                return static_cast<uint8_t>(WORK_RECORD_WIDTH_OF_FABRIC);

            case FabricTableSegmentClasses::DEVICE_VIEW_TABLE:
                return static_cast<uint8_t>(DEVICE_VIEW_WIDTH_OF_APC_FABRIC);

            case FabricTableSegmentClasses::THREAD_TABLE:
                return static_cast<uint8_t>(THREAD_TABLE_RECORD_WIDTH);

            case FabricTableSegmentClasses::SEGMENT_POOL:
                return UNSIGNED_ZERO;
            
            default:
                return EACH_TABLE_RECORD_SENTINAL;
            }
        }


        static constexpr bool CommonValidityCheckOfFabricCellsTableSegmentClasses(const PackedCell64_t::AuthoritiveCellView& a_cell_view) noexcept
        {
            if (
                !a_cell_view.IsCellValid || 
                a_cell_view.CellOwnership != OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC ||
                a_cell_view.CellValueDataType != InternalDataTypePolicy::UNSIGNED
            )
            {
                return false;
            }
            return true;
        }



        //kept for safty
        static constexpr bool IsThisFebricMetaIdxAValidIncrementalCountType(FabricMetaIndicies meta_idx) noexcept
        {
            switch (meta_idx)
            {
            case FabricMetaIndicies::TOTAL_APC_IN_USE:
            case FabricMetaIndicies::RETIRE_SLOT_HEAD:
            case FabricMetaIndicies::RELATION_FREE_HEAD:
            case FabricMetaIndicies::GLOBAL_EPOCH48:
            case FabricMetaIndicies::MIN_SAFE_EPOCH48:
            case FabricMetaIndicies::NEXT_DEVICE_VIEW_ID:
            case FabricMetaIndicies::RELATION_RECLAIM_COUNT:
            case FabricMetaIndicies::WORK_QUEUE_DROPPED_COUNT:
            case FabricMetaIndicies::THREAD_ACTIVE_COUNT:
            case FabricMetaIndicies::THREAD_REGISTRATION_FAILURE:
            case FabricMetaIndicies::RELATION_TOMBSTONE_COUNT:
            case FabricMetaIndicies::RELATION_UNLINK_FAILURES:
            case FabricMetaIndicies::WORK_QUEUE_CLAIM_FAILURES:
            case FabricMetaIndicies::CAS_FAILURE_COUNT:
            case FabricMetaIndicies::ERROR_COUNT:
            case FabricMetaIndicies::RETIRED_COUNT:
            case FabricMetaIndicies::LIVE_SLOT_COUNT:
            case FabricMetaIndicies::FABRIC_CLOCK16:
                return true;
            default:
                return false;
            }
        }

        static constexpr uint64_t UpdateACountMetaCell(uint64_t packed_cell, FabricMetaIndicies user_validation, int delta) noexcept
        {
            const PackedCell64_t::AuthoritiveCellView auth_view_of_the_cell = PackedCell64_t::GetAuthoritiveViewsForACell(packed_cell);
            if (
                !CommonValidityCheckOfFabricCellsTableSegmentClasses(auth_view_of_the_cell) ||
                auth_view_of_the_cell.CellMode != PackedMode::VALUE48 ||
                auth_view_of_the_cell.CellValueDataType != InternalDataTypePolicy::UNSIGNED ||
                !IsThisFebricMetaIdxAValidIncrementalCountType(user_validation)
            )
            {
                return FABRIC_CELL_SENTINAL;
            }
            
            const int64_t updated_count = auth_view_of_the_cell.Raw48BitInCellData <= PackedCell64_t::BIT_FAMILY_48_SENTINAL ?
                auth_view_of_the_cell.Raw48BitInCellData + delta : delta;
            if (updated_count >= PackedCell64_t::BIT_FAMILY_48_SENTINAL || updated_count < UNSIGNED_ZERO)
            {
                return FABRIC_CELL_SENTINAL;
            }

            const uint64_t desired_count = static_cast<uint64_t>(updated_count) & MaskLeftOverBitsUntil64(FAMILY_48_BIT_LEN);
            
            return PackedCell64_t::SetMETA16InPacked(desired_count, auth_view_of_the_cell.InCellMeta16);

        }

        // using DefaultMemCopyBuffer = std::array<uint64_t, MAXIMUM_CLAIMABLE_COUNT_SEQUENTIALLY>;

        // static constexpr void BuildNullMemCopyBuffer(DefaultMemCopyBuffer& a_default_buffer) noexcept
        // {
        //     for (size_t i = 0; i < MAXIMUM_CLAIMABLE_COUNT_SEQUENTIALLY; i++)
        //     {
        //         a_default_buffer[i] = FABRIC_CELL_SENTINAL;
        //     }
        // }


        static constexpr bool IsCellValidFabricMetaIndecies(const PackedCell64_t::AuthoritiveCellView& a_cell_view) noexcept
        {
            if (
                !a_cell_view.IsCellValid ||
                a_cell_view.CellOwnership != OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC ||
                a_cell_view.FabricTableSegmentClass != FabricTableSegmentClasses::CONTROL_HEADER
            )
            {
                return false;
            }

            switch (a_cell_view.CellMode)
            {

            case PackedMode::MODEL48:
                return a_cell_view.SubClassOfModel48 == Model48Subclass::SUBDIVISION16x3_INTERNAL_CELL_MODEL;
            
            case PackedMode::VALUE48:
                return true;

            default:
                return false;
            }
            
        }


    
    };


    struct RawPackedCellAllocator
    {
        using AllocateFunction = uint64_t* (*)(
            size_t count_of_packed_cell, size_t alignment, void* user
        ) noexcept;

        using FreeFunction = void (*)(
            uint64_t* packed_cell_storage_ptr, 
            size_t count_of_cell, size_t alignment, void*user
        ) noexcept;

        AllocateFunction AllocatePackedCellStorage{nullptr};
        FreeFunction FreePackedCellStorage{nullptr};
        void* User{nullptr};
        size_t Alignment{BIT_LENGTH_OF_A_PACKED_CELL};

        static size_t AlignBiteCount_(size_t bytes, size_t alignment) noexcept
        {
            if (alignment == UNSIGNED_ZERO)
            {
                return bytes;
            }

            const size_t remaining_bytes = bytes % alignment;
            return remaining_bytes == UNSIGNED_ZERO ? bytes : bytes + (alignment - remaining_bytes);
        }

        static uint64_t* DefaultAllocateAtomicCells(
            size_t count_of_packed_cell, size_t alignment, void*
        ) noexcept
        {
            if (count_of_packed_cell == UNSIGNED_ZERO)
            {
                return nullptr;
            }

            alignment = std::max<size_t>(alignment, alignof(uint64_t));
            const size_t byte_count = sizeof(uint64_t) * count_of_packed_cell;
            const size_t aligned_bytes = AlignBiteCount_(byte_count, alignment);

#if defined(_MSC_VER)

            void* raw_packed_cell_memory = _aligned_malloc(aligned_bytes, alignment);
#else
            void* raw_packed_cell_memory = std::aligned_alloc(alignment, aligned_bytes);
#endif
            if (!raw_packed_cell_memory)
            {
                return nullptr;
            }
            std::memset(raw_packed_cell_memory, UNSIGNED_ZERO, aligned_bytes);
            return static_cast<uint64_t*>(raw_packed_cell_memory);
            
        }

        static void DefaultFreeAtomicCells(
            uint64_t* packed_cell_storage_ptr, 
            size_t, size_t, void*
        ) noexcept
        {
            if (!packed_cell_storage_ptr)
            {
                return;
            }
#if defined(_MSC_VER)
            _aligned_free(packed_cell_storage_ptr);
#else
            std::free(packed_cell_storage_ptr);
#endif
        }
    };


}
