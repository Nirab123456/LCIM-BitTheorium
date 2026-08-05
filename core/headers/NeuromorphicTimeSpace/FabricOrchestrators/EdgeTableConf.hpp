#pragma once 
#include "DescriptionOfAPC.hpp"

namespace PredictedAdaptedEncoding
{


struct EdgeTableConf : public DescriptionOfAPC
{
    enum class EdgeTableIndexing : uint8_t
    {
        ROOT_AND_END = 0,
        COUNT_AND_SIBBLING = 1,
        SEQLOCK_STATE = 2
    };

    enum class EdgeStatus : uint8_t
    {
        FREE_RETIRED = 0,
        RESERVED = 1,
        LIVE = 2,
        RETIRED = 3,
        HAULTED = 4,

        RETRY_REQUIRED = 5,
        CORRUPTED = 6
    };

    static constexpr bool IsStorableStatus(EdgeStatus status) noexcept
    {
        return 
            status < EdgeStatus::RETRY_REQUIRED;
    }

    static constexpr uint8_t EDGE_TABLE_RECORD_WIDTH = static_cast<uint8_t>(EdgeTableIndexing::SEQLOCK_STATE) + 1u;
    using EdgeBuffer = std::array<uint64_t, EDGE_TABLE_RECORD_WIDTH>;

    struct EdgeData
    {
        FabricTableSegmentClasses EdgeTable{};
        //PAIR -1 
        uint32_t Root = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t End = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        //PAIR-2
        uint32_t OwnLinkCount = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t DoubellyLinkedIndex = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        //PAIR-3
        uint32_t SeqLock = UNSIGNED_ZERO;
        EdgeStatus Status = EdgeStatus::CORRUPTED;

        bool IsValid = false;
    };

    static constexpr bool ValidateEdgeData(
        const EdgeData& edge
    ) noexcept
    {
        if (
            !IsValidEdgeTable(edge.EdgeTable) ||
            !IsStorableStatus(edge.Status) ||
            !APCDataStructure::IsValid32BitAPCUnit(edge.OwnLinkCount) ||
            !APCDataStructure::IsValid32BitAPCUnit(edge.SeqLock) ||
            !(
                edge.DoubellyLinkedIndex == APCDataStructure::APC_INDEX_BOUND_SENTINAL ||
                APCDataStructure::IsValid32BitAPCUnit(edge.DoubellyLinkedIndex)
            )
        )
        {
            return false;
        }

        const bool end_is_valid = (
            edge.End == 
            APCDataStructure::APC_INDEX_BOUND_SENTINAL ||
            APCDataStructure::IsValid32BitAPCUnit(edge.End)
        );

        if (!end_is_valid)
        {
            return false;
        }

        if (
            edge.Status == EdgeStatus::RESERVED &&
            InstallAxisToBuffer::IsValidEven64(edge.SeqLock)
        )
        {
            return false;
        }

        if (
            edge.Status != EdgeStatus::RESERVED &&
            !InstallAxisToBuffer::IsValidEven64(edge.SeqLock)
        )
        {
            return false;
        }
        
        if (
            edge.OwnLinkCount == UNSIGNED_ZERO &&
            edge.End != APCDataStructure::APC_INDEX_BOUND_SENTINAL
        )
        {
            return false;
        }
        
        if (
            edge.OwnLinkCount != UNSIGNED_ZERO &&
            edge.End == APCDataStructure::APC_INDEX_BOUND_SENTINAL
        )
        {
            return false;
        }
        
        return true;
    }

    static constexpr bool BuildEdgeBuffer(
        EdgeBuffer& buffer,
        EdgeData edge
    ) noexcept
    {
        if (!ValidateEdgeData(edge))
        {
            edge.IsValid = false;
            buffer.fill(FABRIC_CELL_SENTINAL);
            return false;
        }
        edge.IsValid = true;
        buffer[static_cast<uint8_t>(EdgeTableIndexing::ROOT_AND_END)] = 
            TwinU32ToU64::PackDoubleUnsigned32In64(edge.Root, edge.End);
        
        buffer[static_cast<uint8_t>(EdgeTableIndexing::COUNT_AND_SIBBLING)] =
            TwinU32ToU64::PackDoubleUnsigned32In64(edge.OwnLinkCount, edge.DoubellyLinkedIndex);
        
        buffer[static_cast<uint8_t>(EdgeTableIndexing::SEQLOCK_STATE)] = 
            TwinU32ToU64::PackDoubleUnsigned32In64(edge.SeqLock, static_cast<uint8_t>(edge.Status));
        return true;
    }

};

struct EdgeBuilder : public EdgeTableConf
{

    static constexpr bool PrepareInharitedAxis(
        InstallAxisToBuffer::BufferOfAPCIdentity& predessor,
        InstallAxisToBuffer::BufferOfAPCIdentity& current_identity,
        InstallAxisToBuffer::BidirectionalAxis axis,
        InstallAxisToBuffer::DescOfInharitance inharitance
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


    static constexpr bool InstallOwnedRoot(
        InstallAxisToBuffer::BufferOfAPCIdentity& identity_buffer,
        InstallAxisToBuffer::BidirectionalAxis axis,
        SingleHashBuffer& branch_hash_buffer,
        SingleHashBuffer& axis_owned_hash_buffer
    ) noexcept
    {
        using IAB = InstallAxisToBuffer;

        if (
            !IAB::ValidateIdentityBuffer(identity_buffer) ||
            !IAB::IsOwnedAxisDisabled(identity_buffer, axis)
        )
        {
            return false;
        }

        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);

        const uint64_t slot_handle = IAB::APCSlotIdxToHashTableHandler(
            IAB::ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::APC_SLOT_IDX)
        );

        const uint64_t root_key = IAB::ComposeNewGroupKey(
            slot_handle,
            axis,
            UNSIGNED_ZERO
        );

        AxisTopAndCountForBranchHashValue branch_hash_values{};
        branch_hash_values.AxisTopWaterMark = UNSIGNED_ZERO;
        branch_hash_values.MemberCount = UNSIGNED_ZERO;

        BuildEmptyHashBuffer(branch_hash_buffer);
        BuildEmptyHashBuffer(axis_owned_hash_buffer);


        return 
            IAB::InsertAnIdentityInBuffer(identity_buffer, map.OwnRootKey, root_key) &&
            IAB::InsertAnIdentityInBuffer(identity_buffer, map.RootOwnedChild, FABRIC_CELL_SENTINAL) &&
            MakeValidHashBuffer(
                branch_hash_buffer,
                root_key,
                PackAxisTopAndCount(branch_hash_values),
                UNSIGNED_ZERO,
                HashState::LIVE_OR_PUBLISHED
            ) &&
            MakeValidHashBuffer(
                axis_owned_hash_buffer,
                root_key,
                slot_handle,
                UNSIGNED_ZERO,
                HashState::LIVE_OR_PUBLISHED
            ) &&
            IAB::SealIdentityBuffer(identity_buffer);

    }

};

}