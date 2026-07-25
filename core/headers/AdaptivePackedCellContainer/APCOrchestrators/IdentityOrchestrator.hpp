#pragma once 
#include <array>
#include <utility>
#include "../CoreOFAPC/IdAndIdentityOfAPC.hpp"


namespace PredictedAdaptedEncoding
{

    struct IdentityBufferConf : public DefineIdentityBuffer
    {

        static constexpr bool ContainsRuntimeIdentity(
            BufferOfAPCIdentity& identity_buffer
        ) noexcept
        {
            
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