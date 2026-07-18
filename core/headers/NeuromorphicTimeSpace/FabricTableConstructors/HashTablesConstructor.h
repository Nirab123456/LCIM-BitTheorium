#pragma once 
#include "DescriptionConstructor.h"

namespace PredictedAdaptedEncoding
{
    class HashTablesConstructor : public APCHandleDescriptorConstructor
    {
    protected:
        
        bool InsertOrUpdateRobinHoodHash48_(FabricTableSegmentClasses hash_table, uint64_t key48, uint64_t value48) noexcept;

        std::optional<uint64_t> FindHashValue48_(FabricTableSegmentClasses hash_table, uint64_t key48) noexcept;

    public:
        /// @brief Takes Base Bucket Index -> GATHER: 3 Cells -> CALLS: HashTableConf::ReadKeyValueProbFromValidCells
        /// @param bucked_base_index First Index In Slab For That Hash SIMPLY: HashTableInternalIndexing::KEY_INDEX OF: ANY: Hash Table
        /// @param caller_holds_Claim_guard IF: FALSE: Claimed Cell is Invalid & ONLY: -> SET: -> TRUE: When Caller Is the One Claimed The Cell 
        /// @return VALID: HashFilesCarrier.IsValid -> true || INVALID: HashFilesCarrier.IsValid = false
        HashTableConf::HashFilesCarrier ReadHashFilesFromSlab(size_t bucked_base_index, bool caller_holds_Claim_guard) noexcept;

    };

}