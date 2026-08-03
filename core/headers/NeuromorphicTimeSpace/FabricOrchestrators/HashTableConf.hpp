#pragma once 
#include "DescriptionOfAPC.hpp"

namespace PredictedAdaptedEncoding
{

struct DefaultHashings : public DescriptorConf
{
    static constexpr uint8_t MIN_LIMIT_POW_OF_2 = 16u;
    static constexpr uint8_t DEFAULT_TABLE_TAILROOM_MULT = 2u;
    static constexpr uint32_t PROB_DISTANCE_SENTINAL = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
    static constexpr uint64_t VALIDATION_MARK_OF_HASH_TABLE_BUFFER = 333;

    using HashState = StateOfAPC;

    struct AxisTopAndCountForBranchHashValue
    {
        uint32_t AxisTopWaterMark = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t MemberCount = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
    };

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

        return HashIdConstructror::NextPowerOf2Unsigned64(wanted_bucket_count);
    }

    static constexpr uint64_t PackAxisTopAndCount(
        const AxisTopAndCountForBranchHashValue&  hash_values
    ) noexcept
    {
        return Double32In64ForAPCandFabric::PackDoubleUnsigned32In64(
            hash_values.AxisTopWaterMark,
            hash_values.MemberCount
        );
    }

    static constexpr AxisTopAndCountForBranchHashValue GetBranchHashValues(uint64_t value) noexcept
    {
        AxisTopAndCountForBranchHashValue both_value{};

        if (!APCDataStructure::IsValidFabricUnit(value))
        {
            return both_value;
        }

        both_value.AxisTopWaterMark = Double32In64ForAPCandFabric::ExtractLow32Of64(value);
        both_value.MemberCount = Double32In64ForAPCandFabric::ExtractHigh32Of64(value);
        return both_value;
    }

};


struct HashHelpers : public DefaultHashings
{

    using SingleHashBuffer = std::array<uint64_t, HASH_BUCKED_WIDTH_OF_FABRIC + 1>;
    static constexpr size_t VALIDATION_INDEX_HASH_BUFFER = static_cast<size_t>(HASH_BUCKED_WIDTH_OF_FABRIC);


    static constexpr uint64_t GetAUnitFromHashBuffer(
        const SingleHashBuffer& hash_buffer,
        HashBufferIndexing index
    ) noexcept
    {
        return hash_buffer[static_cast<uint8_t>(index)]; 
    }

    static constexpr void SetHashBufferUnit(
        SingleHashBuffer& hash_buffer,
        HashBufferIndexing index,
        uint64_t unit_value
    ) noexcept
    {
        hash_buffer[static_cast<uint8_t>(index)] = unit_value;
    }

    static constexpr bool IsValidHashBuffer (const SingleHashBuffer& hash_buffer) noexcept
    {
        return 
            HashIdConstructror::IsValidAPCId(GetAUnitFromHashBuffer(hash_buffer, HashBufferIndexing::KEY_INDEX)) &&
            APCDataStructure::IsValidFabricUnit(GetAUnitFromHashBuffer(hash_buffer, HashBufferIndexing::VALUE_INDEX));
    }


    static constexpr uint32_t MakeHashFingerPrint(
        const SingleHashBuffer& buffer,
        HashState state
    ) noexcept
    {
        uint32_t hash = HASH32_GRATIO_1;

        hash ^= GetAUnitFromHashBuffer(buffer, HashBufferIndexing::VALUE_INDEX);
        hash *= HASH32_GRATIO_2;

        hash ^= GetAUnitFromHashBuffer(buffer, HashBufferIndexing::KEY_INDEX);
        hash *= HASH32_GRATIO_2;

        hash ^= static_cast<uint32_t>(state);
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

    static constexpr bool RecompileStateInBuffer(
        SingleHashBuffer& hash_buffer,
        HashState updated_state,
        std::optional<uint32_t> updated_prob = std::nullopt
    ) noexcept
    {
        Pack32_28_4BitIn64BitUnit::Pack32_28_4_Carrier state_dist_fp = GetStDistFp(hash_buffer);
        state_dist_fp.High4Bit = static_cast<uint8_t>(updated_state);
        if (updated_prob.has_value())
        {
            state_dist_fp.Lowest32Bit = updated_prob.value();
        }
        state_dist_fp.Mid28Bit = MakeHashFingerPrint(hash_buffer, updated_state);
        
        SetHashBufferUnit(
            hash_buffer,
            HashBufferIndexing::PROB_DISTANCE_LOCK,
            Pack32_28_4BitIn64BitUnit::PackValues(state_dist_fp)
        );

        return ValidateHashBuffer(hash_buffer);
    }

    static constexpr bool SealHashBuffer(
        SingleHashBuffer& hash_buffer,
        uint32_t prob_distance,
        HashState hash_state
    ) noexcept
    {
        return RecompileStateInBuffer(hash_buffer, hash_state, prob_distance);
    }

    static constexpr bool MakeValidHashBuffer(
        SingleHashBuffer& hash_buffer,
        const uint64_t& key,
        const uint64_t& value,
        const uint32_t& prob_distance,
        HashState hash_state
    ) noexcept
    {
        SetHashBufferUnit(
            hash_buffer,
            HashBufferIndexing::KEY_INDEX,
            key
        );

        SetHashBufferUnit(
            hash_buffer,
            HashBufferIndexing::VALUE_INDEX,
            value
        );

        return RecompileStateInBuffer(hash_buffer, hash_state, prob_distance);
    }

    static constexpr Pack32_28_4BitIn64BitUnit::Pack32_28_4_Carrier GetStDistFp(const SingleHashBuffer& hash_buffer) noexcept
    {
        const Pack32_28_4BitIn64BitUnit::Pack32_28_4_Carrier state_dist_fp = Pack32_28_4BitIn64BitUnit::UnpackUnitToCarrier(
            GetAUnitFromHashBuffer(hash_buffer, HashBufferIndexing::PROB_DISTANCE_LOCK)
        );
        return state_dist_fp;
    }


    static constexpr bool ValidateHashBuffer(
        SingleHashBuffer& hash_buffer,
        Pack32_28_4BitIn64BitUnit::Pack32_28_4_Carrier* dist_and_validation_files = nullptr
    ) noexcept
    {

        const Pack32_28_4BitIn64BitUnit::Pack32_28_4_Carrier state_dist_fp = GetStDistFp(hash_buffer);

        if (
            !IsValidHashBuffer(hash_buffer) ||
            !state_dist_fp.IsValid ||
            state_dist_fp.High4Bit > static_cast<uint8_t>(HashState::RETIRED_OR_TOMBSTONE) ||
            MakeHashFingerPrint(hash_buffer, static_cast<HashState>(state_dist_fp.High4Bit)) != state_dist_fp.Mid28Bit
        )
        {
            hash_buffer[VALIDATION_INDEX_HASH_BUFFER] = UNSIGNED_ZERO;
            return false;
        }
        if (dist_and_validation_files)
        {
            *dist_and_validation_files = state_dist_fp;
        }
        hash_buffer[VALIDATION_INDEX_HASH_BUFFER] = VALIDATION_MARK_OF_HASH_TABLE_BUFFER;
        return true;
    }

};


struct HashTableConf : public HashHelpers
{


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


    static constexpr bool IsExpectedHashStateBuffer(
        SingleHashBuffer& hash_buffer, 
        uint64_t desired_key,
        HashState desired_state =  HashState::LIVE_OR_PUBLISHED,
        Pack32_28_4BitIn64BitUnit::Pack32_28_4_Carrier* state_distance_fp_carrier = nullptr
    ) noexcept
    {
        const Pack32_28_4BitIn64BitUnit::Pack32_28_4_Carrier state_dist_fp = GetStDistFp(hash_buffer);
        const HashState current_state = static_cast<HashState>(state_dist_fp.High4Bit);
        const uint64_t current_key = GetAUnitFromHashBuffer(
            hash_buffer,
            HashBufferIndexing::KEY_INDEX
        );

        if (state_distance_fp_carrier)
        {
            *state_distance_fp_carrier = state_dist_fp;
        }
        
        return 
            ValidateHashBuffer(hash_buffer) &&
            current_key == desired_key &&
            current_state == desired_state;
    }

public:

    static constexpr bool IfHashBufferHaveValidationMark(SingleHashBuffer& a_hash_buffer) noexcept
    {
        return a_hash_buffer[VALIDATION_INDEX_HASH_BUFFER] == VALIDATION_MARK_OF_HASH_TABLE_BUFFER;  
    }

    static constexpr bool IsGroupableHashTable(FabricTableSegmentClasses hash_table) noexcept
    {
        return hash_table == FabricTableSegmentClasses::VERTICAL_HASH || hash_table == FabricTableSegmentClasses::HORIZONTAL_HASH;
    }


    static constexpr bool PrepareInharitedAxis(
        InstallAxisToBuffer::BufferOfAPCIdentity& predessor,
        InstallAxisToBuffer::BufferOfAPCIdentity& current_identity,
        InstallAxisToBuffer::BidirectionalAxis axis,
        InstallAxisToBuffer::DescOfInharitance inharitance,
        SingleHashBuffer& axis_hash_buffer,
        SingleHashBuffer& branch_hash_buffer
    ) noexcept
    {
        using IAB = InstallAxisToBuffer;

        if (
            !IAB::ValidateIdentityBuffer(predessor) ||
            !IAB::ValidateIdentityBuffer(current_identity) ||
            !IAB::IsInheritedAxisDisabled(current_identity, axis) ||
            !ValidateHashBuffer(branch_hash_buffer)
        )
        {
            return false;
        }

        BuildEmptyHashBuffer(axis_hash_buffer);
        
        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);

        const uint64_t predessor_slot = IAB::ValueOfAnIdentityFromBuffer(predessor, HeaderIdentifierOfAPC::APC_SLOT_IDX);
        const uint64_t current_slot = IAB::ValueOfAnIdentityFromBuffer(current_identity, HeaderIdentifierOfAPC::APC_SLOT_IDX);

        uint32_t axis_id = UNSIGNED_ZERO;

        auto GetPreDesInfo__ = [&](HeaderIdentifierOfAPC position) noexcept -> bool
        {
            std::optional<uint32_t> maybe_axis_id = IAB::GroupPreFix32FromKey(
                IAB::ValueOfAnIdentityFromBuffer(
                    predessor, position
                )
            );
            if (!maybe_axis_id.has_value())
            {
                return false;
            }
            axis_id = maybe_axis_id.value();
            return true;
        };

        if (inharitance == IAB::DescOfInharitance::FIRST_CHILD)
        {
            if (
                IAB::IsOwnedAxisDisabled(predessor, axis) ||
                !IAB::IsValidOwnedRoot(predessor, axis) ||
                IAB::ValueOfAnIdentityFromBuffer(predessor, map.RootOwnedChild) != FABRIC_CELL_SENTINAL ||
                !GetPreDesInfo__(map.OwnRootKey)
            )
            {
                return false;
            }
        }
        else if (inharitance == IAB::DescOfInharitance::LINKED_CHILD)
        {
            if (
                IAB::IsInheritedAxisDisabled(predessor, axis) ||
                !IAB::IsValidInheritedAxis(predessor, axis) ||
                IAB::ValueOfAnIdentityFromBuffer(predessor, map.NextSibling) != FABRIC_CELL_SENTINAL ||
                !GetPreDesInfo__(map.OrdinalKey)
            )
            {
                return false;
            }
        }
        else
        {
            return false;
        }

        const uint8_t key_idx = static_cast<uint8_t>(HashBufferIndexing::KEY_INDEX);
        const uint8_t value_idx = static_cast<uint8_t>(HashBufferIndexing::VALUE_INDEX);
        const uint8_t control_idx = static_cast<uint8_t>(HashBufferIndexing::PROB_DISTANCE_LOCK);

        if (
            branch_hash_buffer[key_idx] != axis_id ||
            !IsExpectedHashStateBuffer(
                branch_hash_buffer,
                axis_id,
                HashState::LIVE_OR_PUBLISHED
            )
        )
        {
            return false;
        }

        const AxisTopAndCountForBranchHashValue old_branch_hash_values = GetBranchHashValues(branch_hash_buffer[value_idx]);
        AxisTopAndCountForBranchHashValue updated_branch_hash_values{};
        updated_branch_hash_values.AxisTopWaterMark = old_branch_hash_values.AxisTopWaterMark + 1u;
        updated_branch_hash_values.MemberCount = old_branch_hash_values.MemberCount + 1u;        
        if (
            !APCDataStructure::IsValid32BitAPCUnit(updated_branch_hash_values.AxisTopWaterMark) ||
            !APCDataStructure::IsValid32BitAPCUnit(updated_branch_hash_values.MemberCount)
        )
        {
            return false;
        }
        const uint64_t current_key = IAB::MakeGroupKeyFromParentGroupId(
            axis_id,
            updated_branch_hash_values.AxisTopWaterMark
        );

        if (inharitance == IAB::DescOfInharitance::FIRST_CHILD)
        {
            if (
                !IAB::InsertAnIdentityInBuffer(predessor, map.RootOwnedChild, current_slot)
            )
            {
                return false;
            }
        }
        else
        {
            if (
                !IAB::InsertAnIdentityInBuffer(predessor, map.NextSibling, current_slot)
            )
            {
                return false;
            }
        }

        if (
            !IAB::InsertAnIdentityInBuffer(current_identity, map.OrdinalKey, current_key) ||
            !IAB::InsertAnIdentityInBuffer(current_identity, map.PreviousSibling, predessor_slot) ||
            !IAB::InsertAnIdentityInBuffer(current_identity, map.NextSibling, FABRIC_CELL_SENTINAL)
        )
        {
            return false;
        }

        bool branch_hash_ok = MakeValidHashBuffer(
            branch_hash_buffer,
            axis_id,
            PackAxisTopAndCount(updated_branch_hash_values),
            Pack32_28_4BitIn64BitUnit::UnpackUnitToCarrier(
                branch_hash_buffer[control_idx]
            ).Lowest32Bit,
            HashTableConf::HashState::LIVE_OR_PUBLISHED
        );

        bool axis_hash_ok = MakeValidHashBuffer(
            axis_hash_buffer,
            current_key,
            HashIdConstructror::APCSlotIdxToHashTableHandler(current_slot),
            UNSIGNED_ZERO,
            HashTableConf::HashState::LIVE_OR_PUBLISHED
        );
        
        return branch_hash_ok &&
            axis_hash_ok &&
            InstallAxisToBuffer::SealIdentityBuffer(predessor) &&
            InstallAxisToBuffer::SealIdentityBuffer(current_identity);
    }

    static constexpr bool PrepareForDetachmentOfInharitedAxis(
        InstallAxisToBuffer::BufferOfAPCIdentity& predecessor_identity,
        InstallAxisToBuffer::BufferOfAPCIdentity& current_identity,
        InstallAxisToBuffer::BufferOfAPCIdentity* next_identity,
        InstallAxisToBuffer::BidirectionalAxis axis,
        SingleHashBuffer& branch_hash_buffer,
        SingleHashBuffer& axis_hash_buffer
    ) noexcept
    {
        using IAB = InstallAxisToBuffer;
        if (
            !IAB::ValidateIdentityBuffer(predecessor_identity) ||
            !IAB::ValidateIdentityBuffer(current_identity) ||
            IAB::IsInheritedAxisDisabled(current_identity, axis) ||
            !ValidateHashBuffer(branch_hash_buffer) ||
            !ValidateHashBuffer(axis_hash_buffer)
        )
        {
            return false;
        }

        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);

        const uint64_t predecessor_slot = IAB::ValueOfAnIdentityFromBuffer(predecessor_identity, HeaderIdentifierOfAPC::APC_SLOT_IDX);
        const uint64_t current_slot = IAB::ValueOfAnIdentityFromBuffer(current_identity, HeaderIdentifierOfAPC::APC_SLOT_IDX);
        const uint64_t next_of_current = IAB::ValueOfAnIdentityFromBuffer(current_identity, map.NextSibling);
        const uint64_t current_key = IAB::ValueOfAnIdentityFromBuffer(current_identity, map.OrdinalKey);
        const std::optional<uint32_t> axis_id = IAB::GroupPreFix32FromKey(current_key);

        if (
            IAB::ValueOfAnIdentityFromBuffer(current_identity, map.PreviousSibling) != predecessor_slot ||
            !axis_id.has_value()
        )
        {
            return false;
        }
        const uint64_t next_of_own_for_predecessor = IAB::ValueOfAnIdentityFromBuffer(predecessor_identity, map.RootOwnedChild);
        const uint64_t next_sibbling_of_predecessor = IAB::ValueOfAnIdentityFromBuffer(predecessor_identity, map.NextSibling);

        const bool predecessor_is_owner = IAB::IsValidOwnedRoot(predecessor_identity, axis) && next_of_own_for_predecessor == current_slot;

        if (predecessor_is_owner)
        {
            if (
                !IAB::InsertAnIdentityInBuffer(
                    predecessor_identity,
                    map.RootOwnedChild,
                    next_of_current
                )
            )
            {
                return false;
            }
        }
        else
        {
            if (
                next_sibbling_of_predecessor != current_slot ||
                !IAB::InsertAnIdentityInBuffer(
                    predecessor_identity,
                    map.NextSibling,
                    next_of_current
                )
            )
            {
                return false;
            }
        }
        

        if (
            next_of_current != FABRIC_CELL_SENTINAL
        )
        {
            if (
                !next_identity ||
                !IAB::ValidateIdentityBuffer(*next_identity) ||
                IAB::ValueOfAnIdentityFromBuffer(*next_identity, HeaderIdentifierOfAPC::APC_SLOT_IDX) != next_of_current ||
                !IAB::InsertAnIdentityInBuffer(*next_identity, map.PreviousSibling, predecessor_slot) ||
                !IAB::SealIdentityBuffer(*next_identity)
            )
            {
                return false;
            }
        }
        else if (next_identity)
        {
            return false;
        }
        const uint8_t value_idx = static_cast<uint8_t>(
            HashBufferIndexing::VALUE_INDEX
        );
        const AxisTopAndCountForBranchHashValue old_branch_hash_values = GetBranchHashValues(branch_hash_buffer[value_idx]);
        if (
            !APCDataStructure::IsValid32BitAPCUnit(old_branch_hash_values.AxisTopWaterMark) ||
            !HashIdConstructror::IsValidGroupId(old_branch_hash_values.MemberCount)
        )
        {
            return false;
        }
        AxisTopAndCountForBranchHashValue new_branch_hash_values{};
        new_branch_hash_values.AxisTopWaterMark = old_branch_hash_values.AxisTopWaterMark;
        new_branch_hash_values.MemberCount = old_branch_hash_values.MemberCount - 1u;
        const uint64_t new_branch_hash_value = PackAxisTopAndCount(new_branch_hash_values);
        SetHashBufferUnit(branch_hash_buffer, HashBufferIndexing::VALUE_INDEX, new_branch_hash_value);

        const bool branch_hash_buffer_ok = MakeValidHashBuffer(
            branch_hash_buffer,
            axis_id.value(),
            new_branch_hash_value,
            GetStDistFp(branch_hash_buffer).Lowest32Bit,
            HashState::LIVE_OR_PUBLISHED
        );

        if (
            !branch_hash_buffer_ok ||
            !RecompileStateInBuffer(axis_hash_buffer, HashState::RETIRED_OR_TOMBSTONE) ||
            !ValidateHashBuffer(branch_hash_buffer) ||
            !ValidateHashBuffer(axis_hash_buffer) ||
            !IAB::DisableInharitadAxis(current_identity, axis)
        )
        {
            return false;
        }
        
        return 
            IAB::SealIdentityBuffer(predecessor_identity) &&
            IAB::SealIdentityBuffer(current_identity);  
    }

};

}