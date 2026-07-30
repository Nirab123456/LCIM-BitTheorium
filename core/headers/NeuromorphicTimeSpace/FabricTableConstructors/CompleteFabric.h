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

        /// @brief FILL: DESIRED: FabricTableSegmentClasses with Idle Fabric Cell -> CALLS: GetRecordMapCarrierRanges TO: Get Range In SLab
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

        /// @brief Just Reads the DescriptionUnitIdentity::ID_STATE_CONCURRENT Cell  Without Validating with Other Descriptor Cells
        /// @param apc_description_index 
        /// @return 
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
            uint64_t key48, 
            uint64_t value48,
            std::optional<HashTableConf::StateOfAPC> hash_state = std::nullopt
        ) noexcept;

        std::optional<uint64_t> FindUsedHashValue(FabricTableSegmentClasses hash_table, uint64_t key48) noexcept;

    public:

        HashTableConf::HashFilesCarrier ReadHashFilesFromSlab(
            uint64_t bucked_base_index
        ) noexcept;

        bool ReadHashFilesFromSlab_(
            uint64_t bucked_base_index,
            HashTableConf::SingleHashBuffer hash_buffer_return
        ) noexcept;
        
        void InitializeHashTable_(FabricTableSegmentClasses table_class) noexcept;

        bool RetireHashKey(FabricTableSegmentClasses table, uint64_t hash_key) noexcept;

    };

}