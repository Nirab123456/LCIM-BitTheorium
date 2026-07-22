#pragma once 
#include <array>
#include <utility>
#include "../CoreOFAPC/IdAndIdentityOfAPC.hpp"
#include "../../SharedComponents/BitPackers/ConAndCaDependentPacker.hpp"


namespace PredictedAdaptedEncoding
{

    struct IdentityBufferConf : public AxisConstructor
    {
        static constexpr uint8_t IDENTITY_BUFFER_LEN = APCDataStructure::TotalIdentityUnitCount() + 1;
        static constexpr uint8_t IDENTITY_VALIDATION_IDX = IDENTITY_BUFFER_LEN - 1;
        static constexpr uint64_t VALIDATION_IDENTITY_MARK = 987987;

        static constexpr uint64_t IDENTY_FINGERPRINT_WRITE_LOCK = FABRIC_CELL_SENTINAL - 1u;
        static constexpr uint64_t IDENTITY_FINGERPRINT_CONSUMED = FABRIC_CELL_SENTINAL - 3u;

        enum class FingerprintHashState : uint8_t
        {
            WRITE_LOCK = 0,
            CONSUME_LOCK = 1,
            VALID = 2,
            INVALID = 3
        };

        using BufferOfAPCIdentity = std::array<uint64_t, IDENTITY_BUFFER_LEN>;

        static constexpr bool IsKnownIdentity(HeaderIdentifierOfAPC identity_unit) noexcept
        {
            return identity_unit >= HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT &&
                identity_unit <= HeaderIdentifierOfAPC::ACCESS_PASSWORD;
        }

        static constexpr bool IsHoldFingerprintState(FingerprintHashState state) noexcept
        {
            return state == FingerprintHashState::WRITE_LOCK ||
                state == FingerprintHashState::CONSUME_LOCK;
        }

        static constexpr FingerprintHashState StateOfIdentityFingerprint(uint64_t hash_value) noexcept
        {
            if (
                !IsValidAPCId(hash_value)
            )
            {
                return FingerprintHashState::INVALID;
            }

            if (hash_value == IDENTITY_FINGERPRINT_CONSUMED)
            {
                return FingerprintHashState::CONSUME_LOCK;
            }

            if (hash_value == IDENTY_FINGERPRINT_WRITE_LOCK)
            {
                return FingerprintHashState::WRITE_LOCK;
            }
            
            return FingerprintHashState::VALID;
        }

        static constexpr std::optional<uint8_t> GetBufferIdxFromIdentityUnit(HeaderIdentifierOfAPC identity_unit) noexcept
        {
            if (!IsKnownIdentity(identity_unit))
            {
                return std::nullopt;
            }
            
            return static_cast<uint8_t>(
                static_cast<uint8_t>(identity_unit) - static_cast<uint8_t>(HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT)
            );
        }

        static constexpr std::optional<HeaderIdentifierOfAPC> GetIdentityUnitFromBufferIdx(uint8_t buffer_idx) noexcept
        {
            if (
                buffer_idx >= APCDataStructure::TotalIdentityUnitCount()
            )
            {
                return std::nullopt;
            }
            return static_cast<HeaderIdentifierOfAPC>(
                buffer_idx + static_cast<uint8_t>(HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT)
            );
        }

        static constexpr void BuildNullIdentityBuffer(BufferOfAPCIdentity& identity_buffer) noexcept
        {
            identity_buffer.fill(FABRIC_CELL_SENTINAL);
            identity_buffer[IDENTITY_VALIDATION_IDX] = UNSIGNED_ZERO;
        }

        static constexpr bool InsertAnIdentityInBuffer(
            BufferOfAPCIdentity& identity_buffer,
            HeaderIdentifierOfAPC identity,
            uint64_t value
        ) noexcept
        {
            const std::optional<uint8_t> buffer_idx = GetBufferIdxFromIdentityUnit(identity);
            if (!buffer_idx.has_value())
            {
                return false;
            }

            if (
                !APCDataStructure::IsValidFabricUnit(value) &&
                !CanIdentityContainRuntimeSentinal(identity)
            )
            {
                return false;
            }
            identity_buffer[buffer_idx.value()] = value;
            return true;
        }

        static constexpr uint64_t ValueOfAnIdentityFromBuffer(
            const BufferOfAPCIdentity& identity_buffer,
            HeaderIdentifierOfAPC identity
        ) noexcept
        {
            const std::optional<uint8_t> buffer_idx = GetBufferIdxFromIdentityUnit(identity);
            if (!buffer_idx.has_value())
            {
                return FABRIC_CELL_SENTINAL;
            }

            return identity_buffer[buffer_idx.value()];
        }

        static constexpr uint64_t ComposeIdentityFingerprint(
            const BufferOfAPCIdentity& identity_buffer
        ) noexcept
        {
            uint64_t hash = HASH_64BIT_GRATIO_1;
            for (uint8_t i = 1u; i < APCDataStructure::TotalIdentityUnitCount(); ++i)
            {
                hash ^= identity_buffer[i] + HASH_64BIT_GRATIO_1 + (hash << 6u) + (hash >> 2u);
                hash = HashUnsigned64(hash);
            }

            hash &= ~uint64_t{1};

            const FingerprintHashState state_fp = StateOfIdentityFingerprint(hash);


            if (
                state_fp != FingerprintHashState::VALID
            )
            {
                return 2u;
            }
            return hash;
        }

        static constexpr bool CanIdentityContainRuntimeSentinal(HeaderIdentifierOfAPC identity) noexcept
        {

            return identity == HeaderIdentifierOfAPC::LOGICAL_GROUP_ID ||
                identity == HeaderIdentifierOfAPC::SHARED_GROUP_ID ||
                identity == HeaderIdentifierOfAPC::LOGICAL_ID_HASH_KEY ||
                identity == HeaderIdentifierOfAPC::SHARED_ID_HASH_KEY ||
                identity == HeaderIdentifierOfAPC::PREVIOUS_HORIZONTAL_HANDLE ||
                identity == HeaderIdentifierOfAPC::PREVIOUS_VERTICAL_HANDLE ||
                identity == HeaderIdentifierOfAPC::NEXT_HORIZONTAL_HANDLE ||
                identity == HeaderIdentifierOfAPC::NEXT_VERTICAL_HANDLE;
        }


        static constexpr bool DoseIdentityBufferContainsValidationMarker(const BufferOfAPCIdentity& identity_buffer) noexcept
        {
            return identity_buffer[IDENTITY_VALIDATION_IDX] == VALIDATION_IDENTITY_MARK;
        }


    };


    struct InstallAxisToBuffer : public IdentityBufferConf
    {

        static constexpr bool IsAxisDisabled(
            const BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            const AxisConstructionMap map = ConstructAxisMap(axis);

            return ValueOfAnIdentityFromBuffer(identity_buffer, map.ID) == FABRIC_CELL_SENTINAL &&
                ValueOfAnIdentityFromBuffer(identity_buffer, map.KEY) == FABRIC_CELL_SENTINAL &&
                ValueOfAnIdentityFromBuffer(identity_buffer, map.PreviousTarget) == FABRIC_CELL_SENTINAL &&
                ValueOfAnIdentityFromBuffer(identity_buffer, map.NextTarget) == FABRIC_CELL_SENTINAL &&
                ValueOfAnIdentityFromBuffer(identity_buffer, map.CountTarget) == UNSIGNED_ZERO;
        }

        static constexpr bool IsLinkedAxis(
            const BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            if (IsAxisDisabled(identity_buffer, axis))
            {
                return false;
            }

            const AxisConstructionMap map = ConstructAxisMap(axis);

            const uint64_t group_id = ValueOfAnIdentityFromBuffer(identity_buffer, map.ID);
            const uint64_t key = ValueOfAnIdentityFromBuffer(identity_buffer, map.KEY);
            const uint32_t count_of_ordinal = static_cast<uint32_t>(ValueOfAnIdentityFromBuffer(identity_buffer, map.CountTarget));
            const uint64_t previous_handle = ValueOfAnIdentityFromBuffer(identity_buffer, map.PreviousTarget);
            const uint64_t next_handle = ValueOfAnIdentityFromBuffer(identity_buffer, map.NextTarget);

            if (
                !IsValidGroupId(group_id) ||
                !APCDataStructure::IsValid32BitAPCUnit(count_of_ordinal)
            )
            {
                return false;
            }
            const uint32_t group_prefix = static_cast<uint32_t>(group_id);
            if (previous_handle == FABRIC_CELL_SENTINAL)
            {
                if (key != MakeGroupKeyFromParentGroupId(group_id, UNSIGNED_ZERO))
                {
                    return  false;
                }
            }
            else
            {
                if (
                    !IsValidGroupId(count_of_ordinal) ||
                    key != MakeGroupKeyFromParentGroupId(group_id, count_of_ordinal)
                )
                {
                    return false;
                }
            }
            return next_handle == FABRIC_CELL_SENTINAL || IsValidAPCId(next_handle);
        }

        static constexpr bool DoseBufferContainsIdentity(const BufferOfAPCIdentity& identity_buffer) noexcept
        {
            if (
                !IsValidAPCSlotIdx(ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::APC_SLOT_IDX)) ||
                !IsValidAPCId(ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::BRANCH_ID)) ||
                !IsValidAPCId(ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::ACCESS_PASSWORD)) ||
                (!IsLinkedAxis(identity_buffer, BidirectionalAxis::HORIZONTALLY_SHARED) && !IsAxisDisabled(identity_buffer, BidirectionalAxis::HORIZONTALLY_SHARED)) ||
                (!IsLinkedAxis(identity_buffer, BidirectionalAxis::VARTICAL_LOGICAL) && !IsAxisDisabled(identity_buffer, BidirectionalAxis::VARTICAL_LOGICAL))             )
            {
                return false;
            }
            return true;
        }

        static constexpr bool IsValidEven64 (uint64_t value) noexcept
        {
            return APCDataStructure::IsValidFabricUnit(value) &&
                (value & 1u) == UNSIGNED_ZERO;
        }
        
        static constexpr bool ValidateAIdentityBuffer(
            BufferOfAPCIdentity& identity_buffer
        ) noexcept
        {
            if (
                !DoseBufferContainsIdentity(identity_buffer)
            )
            {
                identity_buffer[IDENTITY_VALIDATION_IDX] = UNSIGNED_ZERO;
                return false;
            }

            const uint64_t stored_fp = ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT);

            if (
                stored_fp == ComposeIdentityFingerprint(identity_buffer) &&
                IsValidEven64(stored_fp)
            )
            {
                identity_buffer[IDENTITY_VALIDATION_IDX] = VALIDATION_IDENTITY_MARK;
                return true;
            }

            identity_buffer[IDENTITY_VALIDATION_IDX] = UNSIGNED_ZERO;
            return false;
        }

        static constexpr bool SealIdentityBuffer(
            BufferOfAPCIdentity& identity_buffer
        ) noexcept
        {
            const uint64_t fingerprint = ComposeIdentityFingerprint(identity_buffer);
            if (
                !InsertAnIdentityInBuffer(identity_buffer, HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT, fingerprint)
            )
            {
                identity_buffer[IDENTITY_VALIDATION_IDX] = UNSIGNED_ZERO;
                return false;
            }
            return ValidateAIdentityBuffer(identity_buffer);
        }


        static constexpr FingerprintHashState GetStateFingerprint(
            BufferOfAPCIdentity& identity_buffer,
            uint64_t* fingerprint = nullptr
        ) noexcept
        {
            const uint64_t identity_value = identity_buffer[static_cast<uint8_t>(HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT)];
            if (!ValidateAIdentityBuffer(identity_buffer))
            {
                return FingerprintHashState::INVALID;
            }
            if (fingerprint)
            {
                *fingerprint = identity_value;
            }
            return StateOfIdentityFingerprint(identity_value);
        }


        static constexpr bool DisableAxis (
            BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            const AxisConstructionMap map = ConstructAxisMap(axis);

            return InsertAnIdentityInBuffer(identity_buffer, map.ID, FABRIC_CELL_SENTINAL) &&
                InsertAnIdentityInBuffer(identity_buffer, map.KEY, FABRIC_CELL_SENTINAL) &&
                InsertAnIdentityInBuffer(identity_buffer, map.CountTarget, UNSIGNED_ZERO) &&
                InsertAnIdentityInBuffer(identity_buffer, map.PreviousTarget, FABRIC_CELL_SENTINAL) &&
                InsertAnIdentityInBuffer(identity_buffer, map.NextTarget, FABRIC_CELL_SENTINAL);
        }


        static constexpr bool InstallRootAxis(
            BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis,
            bool is_destructive = false
        ) noexcept
        {
            if (
                !DoseBufferContainsIdentity(identity_buffer) ||
                (!is_destructive && !IsAxisDisabled(identity_buffer, axis))
            )
            {
                return false;
            }
            const uint64_t branch_id = identity_buffer[GetBufferIdxFromIdentityUnit(HeaderIdentifierOfAPC::BRANCH_ID).value()];
            const uint64_t root_key_group_id = HashGroupId(
                branch_id,
                axis,
                UNSIGNED_ZERO
            );
            const AxisConstructionMap map = ConstructAxisMap(axis);

            return InsertAnIdentityInBuffer(identity_buffer, map.ID, static_cast<uint32_t>(root_key_group_id)) &&
                InsertAnIdentityInBuffer(identity_buffer, map.KEY, root_key_group_id) &&
                InsertAnIdentityInBuffer(identity_buffer, map.CountTarget, UNSIGNED_ZERO) &&
                InsertAnIdentityInBuffer(identity_buffer, map.PreviousTarget, FABRIC_CELL_SENTINAL) && 
                InsertAnIdentityInBuffer(identity_buffer, map.NextTarget, FABRIC_CELL_SENTINAL);
        }

        static constexpr bool InstallTopChild(
            BufferOfAPCIdentity& parent_identity_buffer,
            BufferOfAPCIdentity& own_identity_buffer,
            BidirectionalAxis axis,
            bool is_destructive = false
        ) noexcept
        {
            if (
                !IsLinkedAxis(parent_identity_buffer, axis) ||
                (!is_destructive && !IsAxisDisabled(own_identity_buffer, axis))
            )
            {
                return false;
            }

            const AxisConstructionMap map = ConstructAxisMap(axis);

            const uint32_t group_id = static_cast<uint32_t>(ValueOfAnIdentityFromBuffer(parent_identity_buffer, map.ID));
            const uint32_t cur_ordinal = static_cast<uint32_t>(ValueOfAnIdentityFromBuffer(parent_identity_buffer, map.CountTarget)) + 1u;
            const uint32_t next_of_parent = static_cast<uint32_t>(ValueOfAnIdentityFromBuffer(parent_identity_buffer, map.NextTarget));
            const uint64_t child_key = MakeGroupKeyFromParentGroupId(group_id, cur_ordinal);

            if (
                !IsValidGroupId(group_id) ||
                !IsValidGroupId(cur_ordinal) ||
                !IsValidAPCId(child_key) ||
                !APCDataStructure::IsValid32BitAPCUnit(next_of_parent)
            )
            {
                return false;
            }

            if (
                next_of_parent == UNSIGNED_ZERO ||
                next_of_parent == cur_ordinal - 1u
            )
            {
                InsertAnIdentityInBuffer(parent_identity_buffer, map.NextTarget, next_of_parent);
            }
            
            
            return InsertAnIdentityInBuffer(own_identity_buffer, map.ID, group_id) &&
                InsertAnIdentityInBuffer(own_identity_buffer, map.KEY, child_key) &&
                InsertAnIdentityInBuffer(own_identity_buffer, map.CountTarget, cur_ordinal) &&
                InsertAnIdentityInBuffer(own_identity_buffer, map.PreviousTarget, cur_ordinal - 1u) &&
                InsertAnIdentityInBuffer(own_identity_buffer, map.NextTarget, FABRIC_CELL_SENTINAL) &&
                InsertAnIdentityInBuffer(parent_identity_buffer, map.CountTarget, cur_ordinal);
        }

        static constexpr bool MutateAxisAsChild(
            const BufferOfAPCIdentity& parent_identity_buffer,
            BufferOfAPCIdentity& own_identity_buffer,
            BidirectionalAxis axis
        ) noexcept;


    };
    
    

}