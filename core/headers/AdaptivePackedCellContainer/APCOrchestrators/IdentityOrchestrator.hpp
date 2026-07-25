#pragma once 
#include <array>
#include <utility>
#include "../CoreOFAPC/IdAndIdentityOfAPC.hpp"


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
                identity_unit <= HeaderIdentifierOfAPC::PREVIOUS_VERTICAL_SLOT;
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
                !(value == FABRIC_CELL_SENTINAL && CanIdentityContainRuntimeSentinal(identity))
            )
            {
                return false;
            }
            identity_buffer[buffer_idx.value()] = value;
            identity_buffer[IDENTITY_VALIDATION_IDX] = UNSIGNED_ZERO;
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

            return 
                identity == HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT ||
                identity == HeaderIdentifierOfAPC::VERTICAL_ORDINAL_KEY ||
                identity == HeaderIdentifierOfAPC::HORIZONTAL_ORDINAL_KEY ||
                identity == HeaderIdentifierOfAPC::HORIZONTAL_ROOT_KEY ||
                identity == HeaderIdentifierOfAPC::VERTICAL_ROOT_KEY ||
                identity == HeaderIdentifierOfAPC::PREVIOUS_HORIZONTAL_SLOT ||
                identity == HeaderIdentifierOfAPC::PREVIOUS_VERTICAL_SLOT ||
                identity == HeaderIdentifierOfAPC::NEXT_HORIZONTAL_SLOT ||
                identity == HeaderIdentifierOfAPC::NEXT_VERTICAL_SLOT ||
                identity == HeaderIdentifierOfAPC::HORIZONTAL_NEXT_OF_ROOT ||
                identity == HeaderIdentifierOfAPC::VERTICAL_NEXT_OF_ROOT;
        }


        static constexpr bool DoseIdentityBufferContainsValidationMarker(const BufferOfAPCIdentity& identity_buffer) noexcept
        {
            return identity_buffer[IDENTITY_VALIDATION_IDX] == VALIDATION_IDENTITY_MARK;
        }


    };


    struct InstallAxisToBuffer : public IdentityBufferConf
    {

        static constexpr bool IsChildAxisDisabled(
            const BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            const AxisConstructionMap map = ConstructAxisMap(axis);

            return
                ValueOfAnIdentityFromBuffer(identity_buffer, map.KeyAndID) == FABRIC_CELL_SENTINAL &&
                ValueOfAnIdentityFromBuffer(identity_buffer, map.PreviousTarget) == FABRIC_CELL_SENTINAL &&
                ValueOfAnIdentityFromBuffer(identity_buffer, map.NextTarget) == FABRIC_CELL_SENTINAL;
        }

        static constexpr bool IsLinkedChild(
            const BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            if (IsChildAxisDisabled(identity_buffer, axis))
            {
                return false;
            }

            const AxisConstructionMap map = ConstructAxisMap(axis);

            const uint64_t slot_idx = ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::APC_SLOT_IDX);
            const uint64_t key = ValueOfAnIdentityFromBuffer(identity_buffer, map.KeyAndID);
            const uint64_t previous_handle = ValueOfAnIdentityFromBuffer(identity_buffer, map.PreviousTarget);
            const uint64_t next_handle = ValueOfAnIdentityFromBuffer(identity_buffer, map.NextTarget);

            if (
                !APCDataStructure::IsValid32BitAPCUnit(slot_idx) ||
                !APCDataStructure::IsValid32BitAPCUnit(previous_handle) ||
                (!APCDataStructure::IsValid32BitAPCUnit(next_handle) && next_handle != FABRIC_CELL_SENTINAL) ||
                !IsValidAPCId(key)

            )
            {
                return false;
            }
            return true;
        }

        static constexpr bool IsRootOwner(
            const BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            const AxisConstructionMap map = ConstructAxisMap(axis);
            const uint64_t slot_idx = ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::APC_SLOT_IDX);
            const uint64_t root_key = ValueOfAnIdentityFromBuffer(identity_buffer, map.OwnRootKey);
            const uint64_t next_slot = ValueOfAnIdentityFromBuffer(identity_buffer, map.RootNext);

            const uint64_t check_key = HashGroupId(
                APCSlotIdxToHashTableHandler(slot_idx),
                axis,
                UNSIGNED_ZERO
            );

            if (
                APCDataStructure::IsValid32BitAPCUnit(slot_idx) &&
                IsValidAPCId(root_key) &&
                root_key == check_key &&
                APCDataStructure::IsValid32BitAPCUnit(next_slot)
            )
            {
                return true;
            }
            
            return false;
        }

        static constexpr bool HashIdentityAsChild(const BufferOfAPCIdentity& identity_buffer) noexcept
        {
            if (
                !IsValidAPCSlotIdx(ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::APC_SLOT_IDX)) ||
                (!IsLinkedChild(identity_buffer, BidirectionalAxis::HORIZONTALLY_SHARED) && !IsChildAxisDisabled(identity_buffer, BidirectionalAxis::HORIZONTALLY_SHARED)) ||
                (!IsLinkedChild(identity_buffer, BidirectionalAxis::VARTICAL_LOGICAL) && !IsChildAxisDisabled(identity_buffer, BidirectionalAxis::VARTICAL_LOGICAL))             )
            {
                return false;
            }
            return true;
        }

        static constexpr bool IsValidEven64(uint64_t value) noexcept
        {
            return APCDataStructure::IsValidFabricUnit(value) &&
                (value & 1u) == UNSIGNED_ZERO;
        }
        
        static constexpr bool ValidateAIdentityBuffer(
            BufferOfAPCIdentity& identity_buffer
        ) noexcept
        {
            if (
                !HashIdentityAsChild(identity_buffer)
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
            const uint64_t identity_value = ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT);
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

            return
                InsertAnIdentityInBuffer(identity_buffer, map.KeyAndID, FABRIC_CELL_SENTINAL) &&
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
                !HashIdentityAsChild(identity_buffer) ||
                (!is_destructive && !IsChildAxisDisabled(identity_buffer, axis))
            )
            {
                return false;
            }

            const uint32_t apc_slot_idx = static_cast<uint32_t>(ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::APC_SLOT_IDX));

            const uint64_t root_key_group_id = HashGroupId(
                APCSlotIdxToHashTableHandler(apc_slot_idx),
                axis,
                UNSIGNED_ZERO
            );
            const AxisConstructionMap map = ConstructAxisMap(axis);

            return
                InsertAnIdentityInBuffer(identity_buffer, map.KeyAndID, root_key_group_id) &&
                InsertAnIdentityInBuffer(identity_buffer, map.PreviousTarget, FABRIC_CELL_SENTINAL) && 
                InsertAnIdentityInBuffer(identity_buffer, map.NextTarget, FABRIC_CELL_SENTINAL);
        }

        static constexpr bool InstallChildAxis(
            BufferOfAPCIdentity& parent_identity_buffer,
            BufferOfAPCIdentity& own_identity_buffer,
            BidirectionalAxis axis,
            bool is_destructive = false
        ) noexcept
        {
            if (
                (!IsLinkedChild(parent_identity_buffer, axis)) ||
                (!is_destructive && !IsChildAxisDisabled(own_identity_buffer, axis))
            )
            {
                return false;
            }

            const AxisConstructionMap map = ConstructAxisMap(axis);
            const uint32_t apc_slot_idx = static_cast<uint32_t>(ValueOfAnIdentityFromBuffer(own_identity_buffer, HeaderIdentifierOfAPC::APC_SLOT_IDX));
            const uint64_t parent_key = ValueOfAnIdentityFromBuffer(parent_identity_buffer, map.KeyAndID);
            const std::optional<uint32_t> parent_id = GroupPreFix32FromKey(parent_key);
            const std::optional<uint32_t> parent_ordinal = GetOrdinalFromKey(parent_key);
            const uint32_t next_of_parent = static_cast<uint32_t>(ValueOfAnIdentityFromBuffer(parent_identity_buffer, map.NextTarget));

            if (
                !APCDataStructure::IsValid32BitAPCUnit(apc_slot_idx) ||
                !parent_id.has_value() ||
                !!parent_ordinal.has_value() ||
                !APCDataStructure::IsValid32BitAPCUnit(next_of_parent) ||
                next_of_parent != apc_slot_idx
            )
            {
                return false;
            }
            const uint64_t child_key = MakeGroupKeyFromParentGroupId(parent_id.value(), parent_ordinal.value() + 1);
            
            return
                InsertAnIdentityInBuffer(own_identity_buffer, map.KeyAndID, child_key) &&
                InsertAnIdentityInBuffer(own_identity_buffer, map.PreviousTarget, parent_ordinal.value()) &&
                InsertAnIdentityInBuffer(own_identity_buffer, map.NextTarget, FABRIC_CELL_SENTINAL);
        }

        static constexpr bool MutateAxisAsChild(
            const BufferOfAPCIdentity& parent_identity_buffer,
            BufferOfAPCIdentity& own_identity_buffer,
            BidirectionalAxis axis
        ) noexcept;


    };
    
    

}