#pragma once 
#include "CoreOfFabricCoordinator.hpp"

namespace PredictedAdaptedEncoding
{

struct HashFilesCarrier
{
    uint64_t HashValue = UNSIGNED_ZERO;
    uint64_t HashKey = UNSIGNED_ZERO;
    uint16_t ProbDistance = UNSIGNED_ZERO;
    FabricTableSegmentClasses HashTable = FabricTableSegmentClasses::NONE;
    LocalityPolicy AttachedLocality = LocalityPolicy::UNASSIGNED_UNUSED_NANNULL;
    bool IsValid = false;
};
static_assert(sizeof(HashFilesCarrier) == HASH_BUCKED_WIDTH_OF_FABRIC * sizeof(uint64_t));
static_assert(alignof(HashFilesCarrier) == alignof(uint64_t));

struct HashHelpers
{
    static constexpr uint8_t HASH_SHIFT_1 = 30u;
    static constexpr uint8_t HASH_SHIFT_2 = 27u;
    static constexpr uint8_t HASH_SHIFT_3 = 31u;
    static constexpr uint64_t DEFAULT_HAS_CONST_1 = 0xbf58476d1ce4e5b9ull;
    static constexpr uint64_t DEFAULT_HAS_CONST_2 = 0x94d049bb133111ebull;
    static constexpr uint64_t HASH_TOMBSTONE_KEY = PackedCell64_t::BIT_FAMILY_48_SENTINAL;
    static constexpr uint8_t MIN_LIMIT_POW_OF_2 = 16u;
    static constexpr uint8_t DEFAULT_TABLE_TAILROOM_MULT = 2u;
    static constexpr uint16_t PROB_DISTANCE_SENTINAL = UINT16_MAX;
    static constexpr uint64_t VALIDATION_MARK_OF_HASH_TABLE_BUFFER = 333;
    
    static constexpr uint64_t NextPowerOf2Unsigned48_(uint64_t given_value) noexcept
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
    static constexpr uint64_t HashUnsigned48_(uint64_t given_value) noexcept
    {
        given_value = given_value & MaskLowNBits(FAMILY_48_BIT_LEN);
        given_value ^= given_value >> HASH_SHIFT_1;
        given_value *= DEFAULT_HAS_CONST_1;
        given_value ^= given_value >> HASH_SHIFT_2;
        given_value *= DEFAULT_HAS_CONST_2;
        given_value ^=  given_value >> HASH_SHIFT_3;

        given_value = given_value & MaskLowNBits(FAMILY_48_BIT_LEN);

        return given_value == UNSIGNED_ZERO ? 1u : given_value;
    }

    static constexpr uint64_t BucketCountForExpectedEntries(uint64_t count_of_entries) noexcept
    {
        if (
            count_of_entries == UNSIGNED_ZERO ||
            count_of_entries >= APC_FABRIC_INDEX_SENTINAL
        )
        {
            return UNSIGNED_ZERO;
        }

        const uint64_t wanted_bucket_count = std::max<uint64_t>(MIN_LIMIT_POW_OF_2, count_of_entries * DEFAULT_TABLE_TAILROOM_MULT);

        return NextPowerOf2Unsigned48_(wanted_bucket_count);
        
    }
};


struct HashTableConf : public HashHelpers
{
    using SingleHashBuffer = std::array<packed64_t, HASH_BUCKED_WIDTH_OF_FABRIC + 1>;
    static constexpr size_t VALIDATION_INDEX_HASH_BUFFER = static_cast<size_t>(HASH_BUCKED_WIDTH_OF_FABRIC);

    /// @brief FILL: The buffer with UINT64_MAX EXCEPT:VALIDATION_INDEX_HASH_BUFFER -> 0
    /// @param a_hash_buffer ADDRESS: OF: SingleHashBuffer
    static constexpr void BuildEmptyHashBuffer(SingleHashBuffer& a_hash_buffer) noexcept
    {
        for (size_t i = 0; i < a_hash_buffer.size(); i++)
        {
            a_hash_buffer[i] = PackedCell64_t::PACKED_CELL_SENTINAL;
        }

        a_hash_buffer[VALIDATION_INDEX_HASH_BUFFER] = UNSIGNED_ZERO;
    }

    /// @brief 
    /// @param a_cell_view 
    /// @param caller_holds_Claim_guard IF: FALSE: Claimed Cell is Invalid & ONLY: -> SET: -> TRUE: When Caller Is the One Claimed The Cell 
    /// @return 
    static constexpr bool IsHashPackedCellRuntimeAccessable(
        const PackedCell64_t::AuthoritiveCellView& a_cell_view,
        bool caller_holds_Claim_guard = false
    ) noexcept
    {

        if (!CoreOfFabricCoordinator::CommonValidityCheckOfFabricCellsTableSegmentClasses(a_cell_view))
        {
            return false;
        }
        
        if (!caller_holds_Claim_guard && a_cell_view.LocalityOfCell == LocalityPolicy::CLAIMED)
        {
            return false;
        }

        if (
            !CoreOfFabricCoordinator::IsValidHashTable(a_cell_view.FabricTableSegmentClass) ||
            a_cell_view.Raw48BitInCellData == PackedCell64_t::BIT_FAMILY_48_SENTINAL
        )
        {
            return false;
        }

        switch (a_cell_view.CellMode)
        {
        case PackedMode::VALUE48:
            return true;

        case PackedMode::MODEL48:
            return a_cell_view.SubClassOfModel48 == Model48Subclass::SUBDIVISION16x3_INTERNAL_CELL_MODEL;
        
        case PackedMode::MODEL32:
            if (IsGroupableHashTable(a_cell_view.FabricTableSegmentClass))
            {
                return a_cell_view.SubClassOfModel32 == Model32Subclass::SELF_CLASS;
            }
            return false;
            
        default:
            return false;
        }
        
    }



    /// @brief Cell DEFAULTS: TypeFamily::VALUE48 + ContractOfConcurrency::CLAIMED_GURDED + AttributePolicy::DEPENDENT_OR_INSTRUCTION_CELL
    /// @return VALID -> Packed Cell -> OR: UINT64_MAX:: if FabricTableSegmentClasses dosent belong  BRANCH_HASH, SHARED_HASH, LOGICAL_HASH
    static constexpr packed64_t MakeAHashValueCell(
        uint64_t hash_handle,
        FabricTableSegmentClasses hash_table_class, 
        LocalityPolicy locality = LocalityPolicy::IDLE
    ) noexcept
    {
        if (!CoreOfFabricCoordinator::IsValidHashTable(hash_table_class))
        {
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }

        if (locality == LocalityPolicy::PUBLISHED && !HashIdConstructror::IsValidHashHandle(hash_handle))
        {
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }
        
        return PackedCell64_t::MakeTypedFabricValidPackedCell(
            TypeFamily::VALUE48, 
            ContractOfConcurrency::CLAIMED_GURDED, 
            hash_table_class, 
            locality,
            InternalDataTypePolicy::UnsignedPCellDataType, 
            AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
            hash_handle
        );
    }

    static constexpr packed64_t MakeHashIdKeyCell(
        uint64_t  key48,
        FabricTableSegmentClasses hash_table,
        LocalityPolicy locality = LocalityPolicy::IDLE
    ) noexcept
    {
        if (!CoreOfFabricCoordinator::IsValidHashTable(hash_table))
        {
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }
        
        if (locality == LocalityPolicy::PUBLISHED && !HashIdConstructror::IsValidAPCId48(key48))
        {
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }

        if (IsGroupableHashTable(hash_table))
        {
            const std::optional<uint32_t> prefix_32 = HashIdConstructror::GroupPreFix32FromKey48(key48);
            const std::optional<uint16_t> sequential_index = HashIdConstructror::GetSeqIndexOfAHashKey(key48);

            if (!prefix_32.has_value() || !sequential_index.has_value())
            {
                return PackedCell64_t::PACKED_CELL_SENTINAL;
            }
            
            if (locality == LocalityPolicy::PUBLISHED && prefix_32.value() == UNSIGNED_ZERO)
            {
                return PackedCell64_t::PACKED_CELL_SENTINAL;
            }
            
            return PackedCell64_t::MakeModeledFabricValidPackedCell(
                ModelFamily::MODEL32,
                static_cast<tag8_t>(Model32Subclass::SELF_CLASS),
                hash_table,
                locality,
                InternalDataTypePolicy ::UnsignedPCellDataType,
                AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
                prefix_32.value(),
                sequential_index.value()
            );
        }
        
        return PackedCell64_t::MakeTypedFabricValidPackedCell(
            TypeFamily::VALUE48, 
            ContractOfConcurrency::CLAIMED_GURDED, 
            hash_table, 
            locality,
            InternalDataTypePolicy::UnsignedPCellDataType, 
            AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
            key48
        );

    }


    /// @brief PACKEDMODE:MODE48 -> Model48Subclass::FOUR_SUBDIVISION_2x16_AND_2x8[LOWEST16->Prob Distance | MID16 -> LOWEST:16 Bit From Hash Key | HIGHIEST16 -> LOWEST:16 Bit From Hash VAlue]
    /// @param table_class FabricTableSegmentClasses -> BRANCH_HASH / LOGICAL_HASH / SHARED_HASH
    /// @return 
    static constexpr packed64_t MakeHashProbDistanceCellWithSaftyLock(
        uint64_t key48, 
        uint64_t value48,
        uint16_t prob_distance,
        FabricTableSegmentClasses hash_table,
        LocalityPolicy locality = LocalityPolicy::PUBLISHED
    ) noexcept
    {

        if (!CoreOfFabricCoordinator::IsValidHashTable(hash_table))
        {
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }

        if (
            locality == LocalityPolicy::PUBLISHED
        )
        {
            if (
                key48 == UNSIGNED_ZERO || 
                key48 >= HASH_TOMBSTONE_KEY ||
                value48 == UNSIGNED_ZERO ||
                value48 >= HASH_TOMBSTONE_KEY
            )
            {
                return PackedCell64_t::PACKED_CELL_SENTINAL;
            }
        }

        const uint16_t mid_Lock_key_low16 = static_cast<uint16_t>(key48 & MaskLowNBits(LOW16_BIT_LEN));
        const uint16_t high_lock_hash_low_16 = static_cast<uint16_t>(value48 & MaskLowNBits(LOW16_BIT_LEN));

        const uint64_t desired_prob_distance_lock = Subdevision16x3InternalMode48CellModel::PackUnsigned16x3ToMode48_(
            prob_distance,
            mid_Lock_key_low16,
            high_lock_hash_low_16
        );

        return PackedCell64_t::MakeModeledFabricValidPackedCell(
            ModelFamily::MODEL48,
            static_cast<tag8_t>(Model48Subclass::SUBDIVISION16x3_INTERNAL_CELL_MODEL),
            hash_table,
            locality,
            InternalDataTypePolicy::UnsignedPCellDataType,
            AttributePolicy::DEPENDENT_OR_INSTRUCTION_CELL,
            desired_prob_distance_lock
        );
    }

private:
    static constexpr uint64_t ExtractHashKey48FromValidViewUnchecked_(
        const PackedCell64_t::AuthoritiveCellView a_key_cell_view
    )
    {
        if (a_key_cell_view.CellMode == PackedMode::VALUE48)
        {
            return a_key_cell_view.Raw48BitInCellData;
        }

        if (a_key_cell_view.CellMode == PackedMode::MODEL32)
        {
            const uint64_t rebuild_group_key = HashIdConstructror::RebuildOriginalKey(a_key_cell_view.Raw32BitInCellData, a_key_cell_view.InCellClock16);
            return HashIdConstructror::IsValidAPCId48(rebuild_group_key) ? rebuild_group_key : PackedCell64_t::PACKED_CELL_SENTINAL;
        }

        return PackedCell64_t::PACKED_CELL_SENTINAL;
    }
public:


    /// @brief Takes HASH: KEY + VALUE + LOCK :: TRIPLET -> VALIDSTS 
    /// @param key_cell 
    /// @param value_cell 
    /// @param prob_distance_safty MUST MATCH:Raw48BitInCellData -> [LOWEST16->Prob Distance | MID16 -> LOWEST:16 Bit From Hash Key | HIGHIEST16 -> LOWEST:16 Bit From Hash Value] AND Model48Subclass::SUBDIVISION16x3_INTERNAL_CELL_MODEL
    /// @param caller_holds_Claim_guard IF: FALSE: Claimed Cell is Invalid & ONLY: -> SET: -> TRUE: When Caller Is the One Claimed The Cell 
    /// @return CELL INVALID: std::nullopt / HashFilesCarrier with Validity flag
    static constexpr HashFilesCarrier ReadKeyValueProbFromValidCells(
        packed64_t key_cell,
        packed64_t value_cell,
        packed64_t prob_distance_safty,
        bool caller_holds_Claim_guard
    ) noexcept
    {
        const PackedCell64_t::AuthoritiveCellView key_cell_auth_view = PackedCell64_t::GetAuthoritiveViewsForACell(key_cell);
        const PackedCell64_t::AuthoritiveCellView value_cell_auth_view = PackedCell64_t::GetAuthoritiveViewsForACell(value_cell);
        const PackedCell64_t::AuthoritiveCellView prob_lock_cell_auth_view = PackedCell64_t::GetAuthoritiveViewsForACell(prob_distance_safty);

        const HashFilesCarrier invalid_value{};
        if (
            !IsHashPackedCellRuntimeAccessable(key_cell_auth_view, caller_holds_Claim_guard) ||
            !IsHashPackedCellRuntimeAccessable(value_cell_auth_view, caller_holds_Claim_guard) ||
            !IsHashPackedCellRuntimeAccessable(prob_lock_cell_auth_view, caller_holds_Claim_guard)
        )
        {
            return invalid_value;
        }

        if (
            key_cell_auth_view.FabricTableSegmentClass != value_cell_auth_view.FabricTableSegmentClass ||
            value_cell_auth_view.FabricTableSegmentClass != prob_lock_cell_auth_view.FabricTableSegmentClass
        )
        {
            return invalid_value;
        }

        if (
            key_cell_auth_view.LocalityOfCell != value_cell_auth_view.LocalityOfCell ||
            value_cell_auth_view.LocalityOfCell != prob_lock_cell_auth_view.LocalityOfCell
        )
        {
            return invalid_value;
        }
        
        

        uint16_t lock_mid16_key = UNSIGNED_ZERO;
        uint16_t lock_highiest16_value = UNSIGNED_ZERO;
        uint16_t lowest_prob_distance = UNSIGNED_ZERO;

        bool ok = Subdevision16x3InternalMode48CellModel::ExtractLowMidHighFromMode48_(prob_lock_cell_auth_view.Raw48BitInCellData, lowest_prob_distance, lock_mid16_key, lock_highiest16_value);

        if (!ok)
        {
            return invalid_value;
        }

        const uint64_t maybe_rebuilded_key48 = ExtractHashKey48FromValidViewUnchecked_(key_cell_auth_view);
        if (!HashIdConstructror::IsValidAPCId48(maybe_rebuilded_key48))
        {
            return invalid_value;
        }

        const uint64_t value_handle_48 =  value_cell_auth_view.Raw48BitInCellData;
        if (!HashIdConstructror::IsValidHashHandle(value_handle_48))
        {
            return invalid_value;
        }
        
        const uint16_t key_low16 = static_cast<uint16_t>(maybe_rebuilded_key48 & MaskLowNBits(LOW16_BIT_LEN));
        const uint16_t value_low16 = static_cast<uint16_t>(value_handle_48 & MaskLowNBits(LOW16_BIT_LEN));


        if (
            lock_mid16_key == key_low16 &&
            lock_highiest16_value == value_low16
        )
        {
            return HashFilesCarrier{
                value_cell_auth_view.Raw48BitInCellData,
                maybe_rebuilded_key48,
                lowest_prob_distance,
                value_cell_auth_view.FabricTableSegmentClass,
                value_cell_auth_view.LocalityOfCell,
                true
            };
        }

        return HashFilesCarrier{
            value_cell_auth_view.Raw48BitInCellData,
            maybe_rebuilded_key48,
            lowest_prob_distance,
            value_cell_auth_view.FabricTableSegmentClass,
            value_cell_auth_view.LocalityOfCell,
            false
        };
        

    }

    /// @brief CREATES: A buffer array of HASH: [KEY | VALUE | PROB DISTANCE | VALIDATION_INDEX_HASH_BUFFER]
    /// @param provided_hash_files TAKES: A valid HashFilesCarrier
    /// @return 
    static constexpr void BuildValidatedHashBuffer(
        const HashFilesCarrier& provided_hash_files,
        SingleHashBuffer& desired_buffer
    ) noexcept
    {
        
        const size_t key_idx = static_cast<size_t>(HashTableInternalIndexing::KEY_INDEX);
        const size_t value_idx = static_cast<size_t>(HashTableInternalIndexing::VALUE_INDEX);
        const size_t prob_lock_idx = static_cast<size_t>(HashTableInternalIndexing::PROB_DISTANCE_LOCK);


        if (
            !provided_hash_files.IsValid ||
            provided_hash_files.HashKey == UNSIGNED_ZERO || provided_hash_files.HashKey >= HashTableConf::HASH_TOMBSTONE_KEY ||
            provided_hash_files.HashValue == UNSIGNED_ZERO || provided_hash_files.HashValue >= HashTableConf::HASH_TOMBSTONE_KEY
        )
        {
            BuildEmptyHashBuffer(desired_buffer);
            return;
        }

        BuildEmptyHashBuffer(desired_buffer);

        desired_buffer[key_idx] = MakeHashIdKeyCell(
            provided_hash_files.HashKey,
            provided_hash_files.HashTable,
            provided_hash_files.AttachedLocality
        );

        desired_buffer[value_idx] = MakeAHashValueCell(
            provided_hash_files.HashValue,
            provided_hash_files.HashTable,
            provided_hash_files.AttachedLocality
        );

        desired_buffer[prob_lock_idx] = MakeHashProbDistanceCellWithSaftyLock(
            provided_hash_files.HashKey,
            provided_hash_files.HashValue,
            provided_hash_files.ProbDistance,
            provided_hash_files.HashTable,
            provided_hash_files.AttachedLocality
        );

        const HashFilesCarrier from_constructed_cell = ReadKeyValueProbFromValidCells(
            desired_buffer[key_idx],
            desired_buffer[value_idx],
            desired_buffer[prob_lock_idx],
            true
        );

        if (
            from_constructed_cell.AttachedLocality == provided_hash_files.AttachedLocality &&
            from_constructed_cell.HashTable == provided_hash_files.HashTable &&
            from_constructed_cell.HashKey == provided_hash_files.HashKey &&
            from_constructed_cell.HashValue == provided_hash_files.HashValue &&
            from_constructed_cell.ProbDistance == provided_hash_files.ProbDistance
        )
        {
            desired_buffer[VALIDATION_INDEX_HASH_BUFFER] = VALIDATION_MARK_OF_HASH_TABLE_BUFFER;
            return;
        }
        desired_buffer[VALIDATION_INDEX_HASH_BUFFER] = UNSIGNED_ZERO;
    }


    static constexpr bool IfHashBufferHaveValidationMark(SingleHashBuffer& a_hash_buffer) noexcept
    {
        return a_hash_buffer[VALIDATION_INDEX_HASH_BUFFER] == VALIDATION_MARK_OF_HASH_TABLE_BUFFER;  
    }

    static constexpr void RestHashFilesCarrier(HashFilesCarrier& a_file_carrier) noexcept
    {
        a_file_carrier.HashValue = UNSIGNED_ZERO;
        a_file_carrier.HashKey = UNSIGNED_ZERO;
        a_file_carrier.ProbDistance = UNSIGNED_ZERO;
        a_file_carrier.HashTable = FabricTableSegmentClasses::NONE;
        a_file_carrier.AttachedLocality = LocalityPolicy::UNASSIGNED_UNUSED_NANNULL;
        a_file_carrier.IsValid = false;
    }

    static constexpr bool IsGroupableHashTable(FabricTableSegmentClasses hash_table) noexcept
    {
        return hash_table == FabricTableSegmentClasses::LOGICAL_HASH || hash_table == FabricTableSegmentClasses::SHARED_HASH;
    }

};




}