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
        FREE = 0,
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

    static constexpr EdgeStatus ReadEdgeFromBufferStatically(
        FabricTableSegmentClasses edge_table,
        const EdgeBuffer& edge_buffer,
        EdgeData& edge_data
    ) noexcept
    {
        edge_data.EdgeTable = edge_table;
        edge_data.Root = TwinU32ToU64::ExtractLow32Of64(edge_buffer[static_cast<uint8_t>(EdgeTableIndexing::ROOT_AND_END)]);
        edge_data.End = TwinU32ToU64::ExtractHigh32Of64(edge_buffer[static_cast<uint8_t>(EdgeTableIndexing::ROOT_AND_END)]);

        edge_data.OwnLinkCount = TwinU32ToU64::ExtractLow32Of64(edge_buffer[static_cast<uint8_t>(EdgeTableIndexing::COUNT_AND_SIBBLING)]);
        edge_data.DoubellyLinkedIndex = TwinU32ToU64::ExtractHigh32Of64(edge_buffer[static_cast<uint8_t>(EdgeTableIndexing::COUNT_AND_SIBBLING)]);

        edge_data.SeqLock = TwinU32ToU64::ExtractLow32Of64(edge_buffer[static_cast<uint8_t>(EdgeTableIndexing::SEQLOCK_STATE)]);
        edge_data.Status = static_cast<EdgeStatus>(TwinU32ToU64::ExtractHigh32Of64(edge_buffer[static_cast<uint8_t>(EdgeTableIndexing::SEQLOCK_STATE)]));
        
        edge_data.IsValid = ValidateEdgeData(edge_data);

        return edge_data.Status;
    }

    static constexpr bool BuildFreeEdgeTable(
        FabricTableSegmentClasses edge_table,
        uint32_t root_slot,
        EdgeData& edge_data
    ) noexcept
    {
        edge_data.EdgeTable = edge_table;
        edge_data.Root = root_slot;
        edge_data.End = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        edge_data.OwnLinkCount = UNSIGNED_ZERO;
        edge_data.DoubellyLinkedIndex = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        edge_data.SeqLock = UNSIGNED_ZERO;
        edge_data.IsValid = ValidateEdgeData(edge_data);
        return edge_data.IsValid;
    }

};

struct EdgeBuilder : public EdgeTableConf
{

    static constexpr bool InstallOwnedRoot(
        InstallAxisToBuffer::BufferOfAPCIdentity& identity_buffer,
        InstallAxisToBuffer::BidirectionalAxis axis,
        uint32_t owned_edge_idx,
        EdgeData& desired_edge
    ) noexcept
    {
        using IAB = InstallAxisToBuffer;

        if (
            !IAB::ValidateIdentityBuffer(identity_buffer) ||
            !IAB::IsOwnedAxisDisabled(identity_buffer, axis) ||
            !APCDataStructure::IsValid32BitAPCUnit(owned_edge_idx)
        )
        {
            return false;
        }

        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);
        const uint32_t root_slot = static_cast<uint32_t>(IAB::ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::APC_SLOT_IDX));
        const uint64_t inharited_edge_raw = IAB::ValueOfAnIdentityFromBuffer(identity_buffer, map.InheritedEgdeTableIdx);
        uint32_t roots_inharited_edge = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        if (inharited_edge_raw != FABRIC_CELL_SENTINAL)
        {
            roots_inharited_edge = static_cast<uint32_t>(inharited_edge_raw);
        }
        
        desired_edge = {};
        desired_edge.EdgeTable = map.EdgeTable;
        desired_edge.Root = root_slot;
        desired_edge.End = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        desired_edge.OwnLinkCount = UNSIGNED_ZERO;
        desired_edge.DoubellyLinkedIndex = roots_inharited_edge;
        desired_edge.SeqLock = 2u;
        desired_edge.Status = EdgeStatus::LIVE;
        desired_edge.IsValid = ValidateEdgeData(desired_edge);

        return 
            desired_edge.IsValid &&
            IAB::InsertAnIdentityInBuffer(
                identity_buffer,
                map.OwnedEgdeTableIdx,
                owned_edge_idx
            ) &&
            IAB::InsertAnIdentityInBuffer(
                identity_buffer,
                map.RootOwnedChild,
                FABRIC_CELL_SENTINAL
            ) &&
            IAB::SealIdentityBuffer(identity_buffer);

    }


    static constexpr bool PrepareInharitedAxis(
        InstallAxisToBuffer::BufferOfAPCIdentity& predessor,
        InstallAxisToBuffer::BufferOfAPCIdentity& current_identity,
        InstallAxisToBuffer::BidirectionalAxis axis,
        InstallAxisToBuffer::DescOfInharitance inharitance,
        uint32_t owner_edge_idx,
        EdgeData& owner_edge,
        EdgeData* current_owned_edge = nullptr
    ) noexcept
    {
        using IAB = InstallAxisToBuffer;
        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);
        const uint64_t predessor_slot = IAB::ValueOfAnIdentityFromBuffer(predessor, HeaderIdentifierOfAPC::APC_SLOT_IDX);
        const uint64_t current_slot = IAB::ValueOfAnIdentityFromBuffer(current_identity, HeaderIdentifierOfAPC::APC_SLOT_IDX);
        if (
            !IAB::ValidateIdentityBuffer(predessor) ||
            !IAB::ValidateIdentityBuffer(current_identity) ||
            !IAB::IsInheritedAxisDisabled(current_identity, axis) ||
            !APCDataStructure::IsValid32BitAPCUnit(owner_edge_idx) ||
            owner_edge.Status != EdgeStatus::LIVE ||
            owner_edge.EdgeTable != map.EdgeTable ||
            predessor_slot == current_slot 
        )
        {
            return false;
        }

        if (inharitance == IAB::DescOfInharitance::FIRST_CHILD)
        {
            if (
                owner_edge.Root != predessor_slot ||
                owner_edge.OwnLinkCount != UNSIGNED_ZERO ||
                owner_edge.End != APCDataStructure::APC_INDEX_BOUND_SENTINAL ||
                IAB::ValueOfAnIdentityFromBuffer(
                    predessor,
                    map.OwnedEgdeTableIdx
                ) != owner_edge_idx ||
                IAB::ValueOfAnIdentityFromBuffer(
                    predessor,
                    map.RootOwnedChild
                ) != FABRIC_CELL_SENTINAL ||
                !IAB::InsertAnIdentityInBuffer(
                    predessor,
                    map.RootOwnedChild,
                    current_slot
                )
            )
            {
                return false;
            }
        }
        else if (inharitance == IAB::DescOfInharitance::LINKED_CHILD)
        {
            if (
                owner_edge.OwnLinkCount == UNSIGNED_ZERO ||
                owner_edge.End != predessor_slot ||
                IAB::ValueOfAnIdentityFromBuffer(
                    predessor,
                    map.InheritedEgdeTableIdx
                ) != owner_edge_idx ||
                IAB::ValueOfAnIdentityFromBuffer(
                    predessor,
                    map.NextSibling
                ) != FABRIC_CELL_SENTINAL ||
                !IAB::InsertAnIdentityInBuffer(
                    predessor,
                    map.NextSibling,
                    current_slot
                )
            )
            {
                return false;
            }
            
        }
        

        if (
            !IAB::InsertAnIdentityInBuffer(current_identity, map.InheritedEgdeTableIdx, owner_edge_idx)|
            !IAB::InsertAnIdentityInBuffer(current_identity, map.PreviousSibling, predessor_slot) ||
            !IAB::InsertAnIdentityInBuffer(current_identity, map.NextSibling, FABRIC_CELL_SENTINAL)
        )
        {
            return false;
        }

        owner_edge.End = static_cast<uint32_t>(current_slot);
        ++owner_edge.OwnLinkCount;

        if (
            !IAB::IsOwnedAxisDisabled(current_identity, axis) &&
            current_owned_edge
        )
        {
            if (
                !ValidateEdgeData(*current_owned_edge) ||
                current_owned_edge->EdgeTable != map.EdgeTable ||
                current_owned_edge->Root != current_slot
            )
            {
                return false;
            }
            current_owned_edge->DoubellyLinkedIndex = owner_edge_idx;
        }
        
        return 
            owner_edge.IsValid &&
            IAB::SealIdentityBuffer(predessor) &&
            IAB::SealIdentityBuffer(current_identity);
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
        const uint64_t current_key = IAB::ValueOfAnIdentityFromBuffer(current_identity, map.InheritedEgdeTableIdx);
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