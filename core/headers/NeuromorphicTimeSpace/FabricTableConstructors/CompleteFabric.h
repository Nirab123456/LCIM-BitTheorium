#pragma once 
#include "FabricConstructor.h"

namespace PredictedAdaptedEncoding
{
    class RecordBookConstructor : public FabricConstructor
    {
        
    protected:

        /// @return LOGICALLY AND SISTAMICALLY UINT64_MAX -> INVALID
        uint64_t GetStartingOfAnyFabricTable(FabricTableSegmentClasses desired_table) noexcept;
        
        bool GetRecordMapCarrierRanges(
            const FabricTableSegmentClasses table_class,
            RecordBookConf::RecordBookTablesBoundsCarrier& return_bounds
        ) noexcept;

        void IdleAFabricTableClassRangesMemory_(FabricTableSegmentClasses table_class) noexcept;

        void WriteARecordBookOfTSCEntry_(
            FabricTableSegmentClasses table_class, 
            size_t begin, 
            size_t end 
        ) noexcept;

    };


    class APCHandleDescriptorConstructor : public RecordBookConstructor
    {
    protected:

        DescriptorConf::APCDescriptorRange ReadAPCDescriptionRanges(uint64_t apc_slot_index) noexcept;
    
        std::optional<size_t> GetIdStateIdxByDescriptionIdx(uint64_t description_idx) noexcept;

    public:

        APCSegmentPoolRange GetSegmentPoolBegainEndForSingleAPCDescription(uint64_t single_description_index) noexcept;

        bool ReadACompleateAPCDescriptorBuffer(
            uint64_t apc_description_index, 
            DescriptionOfAPC::SingleAPCDescriptionCellBuffer& return_buffer
        ) noexcept;

        /// @brief UPDATES: A Description In ONE SHOT
        bool OneShotUpdateAPCDescriptor(
            const DescriptionOfAPC::SingleAPCDescriptionCellBuffer& a_valid_description_buffer
        ) noexcept;

        DescriptionOfAPC::DescriptorSaftyFiles ReadAPCStateAtomically(uint64_t apc_description_index) noexcept;

        bool SwitchOwnershipOfAReadyDescription(
            uint64_t description_idx,
            DescriptionOfAPC::StateOfAPC updated_state
        ) noexcept;

        std::optional<uint64_t> GetASlotForNewAPCLink() noexcept;        
    };


    class HashTablesConstructor : public APCHandleDescriptorConstructor
    {
    protected:
        
        bool InsertOrUpdateRobinHoodHash48_(
            FabricTableSegmentClasses hash_table, 
            uint64_t key, 
            uint64_t value,
            std::optional<HashTableConf::StateOfAPC> hash_state = std::nullopt
        ) noexcept;

    public:

        bool ReadHashBufferFromSlab(
            uint64_t bucket_idx,
            HashTableConf::SingleHashBuffer& hash_buffer_return
        ) noexcept;

        /// @brief
        /// @return Previous State-Distence-Fingerprint / std::nullopt -> MEANS: false
        std::optional<uint64_t> ReserveHashBuffer(
            uint64_t bucked_index,
            HashTableConf::SingleHashBuffer& hash_buffer_return,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        bool ReleseHashBuffer(
            uint64_t bucket_base_idx,
            uint64_t previous_st_dist_fp 
        ) noexcept;

        void InitializeHashTable_(FabricTableSegmentClasses table_class) noexcept;

        bool RetireHashKey(FabricTableSegmentClasses table, uint64_t hash_key) noexcept;

        std::optional<uint64_t> ReadHashValueConcurrently(FabricTableSegmentClasses hash_table, uint64_t key48) noexcept;


    };

}