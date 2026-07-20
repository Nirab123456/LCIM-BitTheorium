#pragma once 
#include "DescriptionOfAPC.hpp"

namespace PredictedAdaptedEncoding
{

struct HashHelpers : public DescriptorConf
{
    struct HashFilesCarrier
    {
        uint64_t HashValue = FABRIC_CELL_SENTINAL;
        uint64_t HashKey = FABRIC_CELL_SENTINAL;
        uint32_t ProbDistance = UNSIGNED_ZERO;
        FabricTableSegmentClasses HashTable = FabricTableSegmentClasses::NONE;
        StateOfAPC HashState = StateOfAPC::UNASSIGNED_UNUSED_NANNULL;
        bool IsValid = false;
    };
    static_assert(sizeof(HashFilesCarrier) <= CoreOfFabricCoordinator::HASH_BUCKED_WIDTH_OF_FABRIC * sizeof(uint64_t));
    static_assert(alignof(HashFilesCarrier) == alignof(uint64_t));

    static constexpr uint8_t HASH_SHIFT_1 = 30u;
    static constexpr uint8_t HASH_SHIFT_2 = 27u;
    static constexpr uint8_t HASH_SHIFT_3 = 31u;
    static constexpr uint64_t DEFAULT_HAS_CONST_1 = 0xbf58476d1ce4e5b9ull;
    static constexpr uint64_t DEFAULT_HAS_CONST_2 = 0x94d049bb133111ebull;
    static constexpr uint64_t HASH_TOMBSTONE_KEY = FABRIC_CELL_SENTINAL;
    static constexpr uint8_t MIN_LIMIT_POW_OF_2 = 16u;
    static constexpr uint8_t DEFAULT_TABLE_TAILROOM_MULT = 2u;
    static constexpr uint32_t PROB_DISTANCE_SENTINAL = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
    static constexpr uint64_t VALIDATION_MARK_OF_HASH_TABLE_BUFFER = 333;
    
    static constexpr uint64_t NextPowerOf2Unsigned64(uint64_t given_value) noexcept
    {
        if (given_value <= 2u)
        {
            return 2u;
        }
        --given_value;
        given_value |= given_value >> 1u;
        given_value |= given_value >> 2u;
        given_value |= given_value >> 4u;
        given_value |= given_value >> 8u;
        given_value |= given_value >> 16u;
        given_value |= given_value >> 32u;

        return given_value + 1u;
    }

    /// @brief Convert A Value to Hash
    /// @param given_value Must be 
    /// @return 
    static constexpr uint64_t HashUnsigned64(uint64_t given_value) noexcept
    {
        given_value ^= given_value >> HASH_SHIFT_1;
        given_value *= DEFAULT_HAS_CONST_1;
        given_value ^= given_value >> HASH_SHIFT_2;
        given_value *= DEFAULT_HAS_CONST_2;
        given_value ^=  given_value >> HASH_SHIFT_3;

        if (!APCDataStructure::IsValidFabricUnit(given_value))
        {
            return given_value - 1u;
        }

        if (given_value == UNSIGNED_ZERO)
        {
            return 1u;
        }
        return given_value;
    }


    static constexpr uint64_t BucketCountForExpectedEntries(uint64_t count_of_entries) noexcept
    {
        if (
            count_of_entries == UNSIGNED_ZERO ||
            count_of_entries == FABRIC_CELL_SENTINAL
        )
        {
            return UNSIGNED_ZERO;
        }

        const uint64_t wanted_bucket_count = std::max<uint64_t>(MIN_LIMIT_POW_OF_2, count_of_entries * DEFAULT_TABLE_TAILROOM_MULT);

        return NextPowerOf2Unsigned64(wanted_bucket_count);
        
    }

    static constexpr bool ValidHashFilesCarrier(
        HashFilesCarrier& hash_files,
        bool check_prod_distance = false
    ) noexcept
    {
        if (
            hash_files.HashKey == UNSIGNED_ZERO ||
            !APCDataStructure::IsValidFabricUnit(hash_files.HashKey) ||
            !APCDataStructure::IsValidFabricUnit(hash_files.HashValue) ||
            !IsKnownStateOfAPC(hash_files.HashState) 
        )
        {
            hash_files.IsValid = false;
            return false;
        }

        if (
            check_prod_distance &&
            !APCDataStructure::IsValidControlAPCUnit(hash_files.ProbDistance)
        )
        {
            hash_files.IsValid = false;
            return false;
        }
        
        hash_files.IsValid = true;
        return hash_files.IsValid;
    }

    static constexpr uint32_t MakeHashFingerPrint(
        const HashFilesCarrier& carier
    ) noexcept
    {
        uint32_t hash = HASH32_GRATIO_1;

        hash ^= carier.HashValue;
        hash *= HASH32_GRATIO_2;

        hash ^= carier.HashKey;
        hash *= HASH32_GRATIO_2;

        hash ^= static_cast<uint32_t>(carier.HashState);
        hash *= HASH32_GRATIO_2;
        hash = hash & LeftOverBitMaskUntil32(Pack32_28_4BitIn64BitUnit::LEN_OF_28_BIT);

        hash ^= hash >> 16u;
        hash *= HASH32_GRATIO_1;
        hash = hash & LeftOverBitMaskUntil32(Pack32_28_4BitIn64BitUnit::LEN_OF_28_BIT);
        if (hash == UNSIGNED_ZERO)
        {
            return 1;
        }
        if (hash == Pack32_28_4BitIn64BitUnit::UINT28_MAX)
        {
            return hash - 1;
        }
        return hash;
        
    }

    static constexpr uint64_t MakeProbdistanceFingerPrintState(
        HashFilesCarrier& carrier
    ) noexcept
    {
        if (!ValidHashFilesCarrier(carrier, true))
        {
            return FABRIC_CELL_SENTINAL;
        }

        const uint32_t fingerprint = MakeHashFingerPrint(carrier);

        Pack32_28_4BitIn64BitUnit::Pack32_28_4_Carrier cell_packer_carrier{};

        cell_packer_carrier.Lowest32Bit = carrier.ProbDistance;
        cell_packer_carrier.Mid28Bit = fingerprint;
        cell_packer_carrier.High4Bit = static_cast<uint8_t>(carrier.HashState);

        return Pack32_28_4BitIn64BitUnit::PackValues(cell_packer_carrier);
    }


    static constexpr uint32_t MakeGroupIdFromBranchId(
        uint64_t branch_id,
        APCGroupReserver::BidirectionalAxis axis
    ) noexcept
    {
        const uint64_t salt = (axis == APCGroupReserver::BidirectionalAxis::HORIZONTALLY_SHARED) ? 
            HashIdConstructror::HASH_64BIT_GRATIO_1 : HashIdConstructror::HASH_64BIT_GRATIO_2;

        uint32_t group_id = static_cast<uint32_t>(HashUnsigned64(branch_id ^ salt));
        if (group_id == UNSIGNED_ZERO)
        {
            return 1;
        }

        if (!APCDataStructure::IsValidControlAPCUnit(group_id))
        {
            return group_id - 1;
        }
        
        return group_id;
    }


};


struct HashTableConf : public HashHelpers
{
    using SingleHashBuffer = std::array<uint64_t, HASH_BUCKED_WIDTH_OF_FABRIC + 1>;
    static constexpr size_t VALIDATION_INDEX_HASH_BUFFER = static_cast<size_t>(HASH_BUCKED_WIDTH_OF_FABRIC);

    /// @brief FILL: The buffer with UINT64_MAX EXCEPT:VALIDATION_INDEX_HASH_BUFFER -> 0
    /// @param a_hash_buffer ADDRESS: OF: SingleHashBuffer
    static constexpr void BuildEmptyHashBuffer(SingleHashBuffer& a_hash_buffer) noexcept
    {
        for (size_t i = 0; i < a_hash_buffer.size(); i++)
        {
            a_hash_buffer[i] = FABRIC_CELL_SENTINAL;
        }

        a_hash_buffer[VALIDATION_INDEX_HASH_BUFFER] = UNSIGNED_ZERO;
    }


public:

    static constexpr bool IfHashBufferHaveValidationMark(SingleHashBuffer& a_hash_buffer) noexcept
    {
        return a_hash_buffer[VALIDATION_INDEX_HASH_BUFFER] == VALIDATION_MARK_OF_HASH_TABLE_BUFFER;  
    }

    static constexpr void RestHashFilesCarrier(HashFilesCarrier& carrier) noexcept
    {
        carrier.HashValue = FABRIC_CELL_SENTINAL;
        carrier.HashKey = FABRIC_CELL_SENTINAL;
        carrier.ProbDistance = UNSIGNED_ZERO;
        carrier.HashTable = FabricTableSegmentClasses::NONE;
        carrier.HashState = StateOfAPC::UNASSIGNED_UNUSED_NANNULL;
        carrier.IsValid = false;
    }

    static constexpr bool IsGroupableHashTable(FabricTableSegmentClasses hash_table) noexcept
    {
        return hash_table == FabricTableSegmentClasses::LOGICAL_HASH || hash_table == FabricTableSegmentClasses::SHARED_HASH;
    }


    /// @brief CREATES: A buffer array of HASH: [KEY | VALUE | PROB DISTANCE | VALIDATION_INDEX_HASH_BUFFER]
    /// @param carrier TAKES: A valid HashFilesCarrier
    /// @return 
    static constexpr void BuildValidatedHashBuffer(
        HashFilesCarrier& carrier,
        SingleHashBuffer& hash_buffer
    ) noexcept
    {
        
        const size_t key_idx = static_cast<size_t>(HashTableInternalIndexing::KEY_INDEX);
        const size_t value_idx = static_cast<size_t>(HashTableInternalIndexing::VALUE_INDEX);
        const size_t probe_state_fp = static_cast<size_t>(HashTableInternalIndexing::PROB_DISTANCE_LOCK);

        BuildEmptyHashBuffer(hash_buffer);

        hash_buffer[key_idx] = carrier.HashKey;

        hash_buffer[value_idx] = carrier.HashValue;

        hash_buffer[probe_state_fp] = MakeProbdistanceFingerPrintState(carrier);

        if (!APCDataStructure::IsValidFabricUnit(hash_buffer[probe_state_fp]))
        {
            BuildEmptyHashBuffer(hash_buffer);
            return;
        }
        hash_buffer[VALIDATION_INDEX_HASH_BUFFER] = VALIDATION_MARK_OF_HASH_TABLE_BUFFER;
    }





};




}