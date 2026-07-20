#pragma once 
#include "DescriptionConstructor.h"

namespace PredictedAdaptedEncoding
{
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
            size_t bucked_base_index
        ) noexcept;

        void InitializeHashTable_(FabricTableSegmentClasses table_class) noexcept;

    };

}