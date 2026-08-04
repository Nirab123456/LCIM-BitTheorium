#pragma once 
#include "FabricConstructor.h"

namespace PredictedAdaptedEncoding
{
    class RecordBookConstructor : public FabricConstructor
    {
        
    protected:
        using LBO = LayoutBoundsOrchestrator;
        using SD = SchemaDefinition;
        using IAB = InstallAxisToBuffer;
        using DSA = DescriptionOfAPC;

        /// @return LOGICALLY AND SISTAMICALLY UINT64_MAX -> INVALID
        uint64_t GetStartingOfAnyFabricTable_(FabricTableSegmentClasses desired_table) noexcept;
        
        bool GetRecordMapCarrierRanges_(
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

        DescriptorConf::APCDescriptorRange ReadAPCDescriptionRanges_(uint64_t apc_slot_index) noexcept;
    
        std::optional<size_t> GetIdStateIdxByDescriptionIdx_(uint64_t description_idx) noexcept;

        bool ReadACompleateAPCDescriptorBuffer_(
            uint64_t apc_description_index, 
            DescriptionOfAPC::SingleAPCDescriptionCellBuffer& return_buffer
        ) noexcept;

        /// @brief UPDATES: A Description In ONE SHOT
        bool OneShotUpdateAPCDescriptor_(
            const DescriptionOfAPC::SingleAPCDescriptionCellBuffer& a_valid_description_buffer
        ) noexcept;

        DescriptionOfAPC::DescriptorSaftyFiles ReadAPCStateAtomically_(uint64_t apc_description_index) noexcept;

    public:
        APCSegmentPoolRange GetSegmentPoolBegainEndForSingleAPCDescription(uint64_t single_description_index) noexcept;

        /// @return previous ID_STATE -> raw value for reverting safely 
        std::optional<uint64_t> SwitchOwnershipOfAReadyDescription(
            uint64_t description_idx,
            DescriptionOfAPC::StateOfAPC updated_state,
            bool caller_holds_reservation = false,
            uint32_t max_tries = DEFAULT_MAX_TRIES
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

        bool ReadHashBuffeByIndex_(
            uint64_t bucket_idx,
            HashTableConf::SingleHashBuffer& hash_buffer_return
        ) noexcept;

        /// @brief
        /// @return Previous State-Distence-Fingerprint / std::nullopt -> MEANS: false
        std::optional<uint64_t> ReserveHashBuffer_(
            uint64_t bucked_index,
            HashTableConf::SingleHashBuffer& hash_buffer_return,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        bool ReleseHashBuffer_(
            uint64_t bucket_base_idx,
            uint64_t previous_st_dist_fp 
        ) noexcept;

        void InitializeHashTable_(FabricTableSegmentClasses table_class) noexcept;

        bool RetireHashKey_(FabricTableSegmentClasses table, uint64_t hash_key) noexcept;

        bool PublishPreparedHashBuffer_(
            FabricTableSegmentClasses hash_table, 
            HashTableConf::SingleHashBuffer& hash_buffer
        ) noexcept;

    public:

        //Compleately lockfree and waitless
        std::optional<uint64_t> ReadHashValueByKey(
            FabricTableSegmentClasses hash_table, 
            uint64_t hash_key,
            HashTableConf::SingleHashBuffer* hash_buffer_return = nullptr
        ) noexcept;
    };

}