#pragma once 
#include "FabricTableOrchestrator.hpp"

namespace BidirectionalInMemGraph
{
struct EdgeTableConf : public DescriptionOfAPC
{

    using EdgeStatus = StateOfAPC;
    using DirtyRelationMask = uint64_t;
    using EdgeLockValues = SeqLockAndStateStruct;

    static constexpr uint32_t RELATION_NULL = APCDataStructure::APC_INDEX_BOUND_SENTINAL;

    static constexpr uint8_t COMPILED_MAX_DIRECTED_PARENTS_PER_AXIS = 64u;
    static constexpr uint8_t RELATION_ORDINAL = 8u;
    static constexpr uint8_t RELATION_SLOT_BITS = 24u;
    static constexpr uint8_t EDGE_SEQUENSE_BITS = 28u;

    static constexpr uint8_t LEN_OF_TAIL = LEN_OF_BYTE_IN_BITS * sizeof(uint32_t);
    static constexpr uint8_t TAIL_PLUS_SEQUENSE_LEN = LEN_OF_TAIL + EDGE_SEQUENSE_BITS;

    static constexpr uint32_t RELATION_SLOT_MASK = MaskLowBitsForU32(RELATION_SLOT_BITS);
    static constexpr uint32_t EDGE_SEQUENSE_MASK = MaskLowBitsForU32(EDGE_SEQUENSE_BITS);
    static constexpr uint32_t EDGE_STATUS_MASK = 0x0fu;


    struct alignas(uint64_t) ParentRelation final
    {
        uint64_t ParentHandle = FABRIC_CELL_SENTINAL;
        uint64_t SiblingLocators = FABRIC_CELL_SENTINAL;
    };

    static_assert(sizeof(ParentRelation) == 2u * sizeof(uint64_t));
    static_assert(alignof(ParentRelation) == alignof(uint64_t));
    static_assert(std::is_trivially_copyable_v<ParentRelation>);

    struct EdgeData final
    {
        uint32_t Tail = RELATION_NULL;
        uint32_t SeqLock = UNSIGNED_ZERO;
        EdgeStatus Status = EdgeStatus::FREE;

        std::array<ParentRelation, COMPILED_MAX_DIRECTED_PARENTS_PER_AXIS> Parents{};
    };

    static constexpr size_t EdgeTableRecordWidth(uint8_t max_direct_parent) noexcept
    {
        return 
            1u + static_cast<size_t>(max_direct_parent) * 
            (sizeof(ParentRelation) / sizeof(uint64_t));
    }

    static constexpr bool IsValidConfigurableParentCapacity(uint8_t value) noexcept
    {
        return  value > UNSIGNED_ZERO && value <= COMPILED_MAX_DIRECTED_PARENTS_PER_AXIS;
    }

    static constexpr uint32_t PackRelationAllocator(uint32_t apc_slot, uint8_t relation_ordinal) noexcept
    {
        return 
            (static_cast<uint32_t>(relation_ordinal) << RELATION_SLOT_BITS) | apc_slot;
    }

    static constexpr uint32_t RelationSlot(uint32_t locator) noexcept
    {
        return locator & RELATION_SLOT_MASK;
    }

    static constexpr uint8_t RelationOrdinal(uint32_t locator) noexcept
    {
        return static_cast<uint8_t>(locator >> RELATION_SLOT_BITS);
    }

    static constexpr bool IsValidRelationLocator(
        uint32_t locator,
        uint64_t apc_count,
        uint8_t maxc_direct_parents
    ) noexcept
    {
        return locator == RELATION_NULL ||
            (
                RelationSlot(locator) < apc_count &&
                RelationOrdinal(locator) < maxc_direct_parents
            );
    }

    static constexpr uint64_t FirstChildCellFromLocator(uint32_t locator) noexcept
    {
        return locator == RELATION_NULL ?
            FABRIC_CELL_SENTINAL : static_cast<uint64_t>(locator);
    }

    static constexpr uint32_t LocatorFromFirstChild(uint64_t raw) noexcept
    {
        return raw == FABRIC_CELL_SENTINAL ?
            RELATION_NULL : static_cast<uint32_t>(raw);
    }

    static constexpr uint64_t MakeParentHandle(uint32_t parent_slot, uint32_t parent_generation) noexcept
    {
        return TwinU32ToU64::PackDoubleUnsigned32In64(parent_slot, parent_generation);
    }

    static constexpr uint32_t ParentSlot(const ParentRelation& relation) noexcept
    {
        return TwinU32ToU64::ExtractLow32Of64(relation.ParentHandle);
    }

    static constexpr uint32_t ParentGeneration(const ParentRelation& relation) noexcept
    {
        return TwinU32ToU64::ExtractHigh32Of64(relation.ParentHandle);
    }

    static constexpr uint32_t PreviousLocator(const ParentRelation& relation) noexcept
    {
        return TwinU32ToU64::ExtractHigh32Of64(relation.SiblingLocators);
    }

    static constexpr uint32_t NextLocator(const ParentRelation& relation) noexcept
    {
        return TwinU32ToU64::ExtractHigh32Of64(relation.SiblingLocators);
    }

    static constexpr void setSibblingLocators(
        ParentRelation& relation,
        uint32_t previous,
        uint32_t next
    ) noexcept
    {
        relation.SiblingLocators = TwinU32ToU64::PackDoubleUnsigned32In64(previous, next);
    }

    static constexpr ParentRelation MakeParentRelation(
        uint32_t parent_slot,
        uint32_t parent_generation,
        uint32_t previous,
        uint32_t next
    ) noexcept
    {
        return ParentRelation{
            MakeParentHandle(parent_slot, parent_generation),
            TwinU32ToU64::PackDoubleUnsigned32In64(previous, next)
        };
    }


    static constexpr bool IsEmpty(const ParentRelation& relation) noexcept
    {
        return relation.ParentHandle == FABRIC_CELL_SENTINAL &&
            relation.SiblingLocators == FABRIC_CELL_SENTINAL;
    }

    static constexpr void Clear(ParentRelation& relation) noexcept
    {
        relation = ParentRelation{};
    }

    static constexpr uint32_t NextGeneration(uint32_t current) noexcept
    {
        return (current + 1u) & EDGE_SEQUENSE_MASK;
    }

    static constexpr uint64_t PackEdgeHandler(
        uint32_t tail,
        uint32_t sequense,
        EdgeStatus status
    ) noexcept
    {
        return 
            static_cast<uint64_t>(tail) |
            (static_cast<uint64_t>(sequense & EDGE_SEQUENSE_MASK) << LEN_OF_TAIL) |
            (static_cast<uint64_t>(status) << TAIL_PLUS_SEQUENSE_LEN);
    }

    static constexpr void UnpackEdgeHader(uint64_t raw, EdgeData& edge) noexcept
    {
        edge.Tail = static_cast<uint32_t>(raw);
        edge.SeqLock = static_cast<uint32_t>((raw >> LEN_OF_TAIL) & EDGE_SEQUENSE_MASK);
        edge.Status = static_cast<EdgeStatus>((raw >> TAIL_PLUS_SEQUENSE_LEN) & EDGE_STATUS_MASK);
    }

    static constexpr DirtyRelationMask DirtyBit(uint8_t ordinal) noexcept
    {
        return uint64_t{1u} << ordinal;
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

        IAB::GraphMutationValues values_gm{};

        if (
            !IAB::ValidateIdentityBuffer(identity_buffer) ||
            !IAB::IsOwnedAxisDisabled(identity_buffer, axis) ||
            !APCDataStructure::IsValid32BitAPCUnit(owned_edge_idx) ||
            !IAB::GetGraphMutationValues(identity_buffer, values_gm) ||
            !IAB::IsDesiredAxisLocked(values_gm.Flags, axis)
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
        desired_edge.OwnerAPCSlot = root_slot;
        desired_edge.Tail = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        desired_edge.OwnLinkCount = UNSIGNED_ZERO;
        desired_edge.ParentEdgeIndex = roots_inharited_edge;
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
        EdgeData* current_owned_edge = nullptr,
        ParentEdgePolicy parent_policy = ParentEdgePolicy::FOLLOW_RELATION
    ) noexcept
    {
        using IAB = InstallAxisToBuffer;
        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);
        const uint64_t predessor_slot = IAB::ValueOfAnIdentityFromBuffer(predessor, HeaderIdentifierOfAPC::APC_SLOT_IDX);
        const uint64_t current_slot = IAB::ValueOfAnIdentityFromBuffer(current_identity, HeaderIdentifierOfAPC::APC_SLOT_IDX);
        IAB::GraphMutationValues gmv_predecessor{};
        IAB::GraphMutationValues gmv_current{};
        if (
            !IAB::ValidateIdentityBuffer(predessor) ||
            !IAB::ValidateIdentityBuffer(current_identity) ||
            !IAB::IsInheritedAxisDisabled(current_identity, axis) ||
            !APCDataStructure::IsValid32BitAPCUnit(owner_edge_idx) ||
            owner_edge.Status != EdgeStatus::LIVE ||
            owner_edge.EdgeTable != map.EdgeTable ||
            predessor_slot == current_slot  ||
            !IAB::GetGraphMutationValues(predessor, gmv_predecessor) ||
            !IAB::IsDesiredAxisLocked(gmv_predecessor.Flags, axis) ||
            !IAB::GetGraphMutationValues(current_identity, gmv_current) ||
            !IAB::IsDesiredAxisLocked(gmv_current.Flags, axis)
        )
        {
            return false;
        }

        if (inharitance == IAB::DescOfInharitance::FIRST_CHILD)
        {
            if (
                owner_edge.OwnerAPCSlot != predessor_slot ||
                owner_edge.OwnLinkCount != UNSIGNED_ZERO ||
                owner_edge.Tail != APCDataStructure::APC_INDEX_BOUND_SENTINAL ||
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
                owner_edge.Tail != predessor_slot ||
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
            !IAB::InsertAnIdentityInBuffer(current_identity, map.InheritedEgdeTableIdx, owner_edge_idx) ||
            !IAB::InsertAnIdentityInBuffer(current_identity, map.PreviousSibling, predessor_slot) ||
            !IAB::InsertAnIdentityInBuffer(current_identity, map.NextSibling, FABRIC_CELL_SENTINAL)
        )
        {
            return false;
        }

        owner_edge.Tail = static_cast<uint32_t>(current_slot);
        ++owner_edge.OwnLinkCount;


        if (
            !IAB::IsOwnedAxisDisabled(current_identity, axis)
        )
        {
            if (parent_policy == ParentEdgePolicy::PRESERVE)
            {
                if (current_owned_edge)
                {
                    return false;
                }
                
            }
            
            if (
                !current_owned_edge ||
                !ValidateEdgeData(*current_owned_edge) ||
                current_owned_edge->EdgeTable != map.EdgeTable ||
                current_owned_edge->OwnerAPCSlot != current_slot
            )
            {
                return false;
            }
            current_owned_edge->ParentEdgeIndex = owner_edge_idx;
        }
        else if (current_owned_edge)
        {
            return false;
        }
        
        
        return 
            ValidateEdgeData(owner_edge) &&
            IAB::SealIdentityBuffer(predessor) &&
            IAB::SealIdentityBuffer(current_identity);
    }

    static constexpr bool PrepareForDetachmentOfInharitedAxis(
        InstallAxisToBuffer::BufferOfAPCIdentity& predecessor_identity,
        InstallAxisToBuffer::BufferOfAPCIdentity& current_identity,
        InstallAxisToBuffer::BufferOfAPCIdentity* next_identity,
        InstallAxisToBuffer::BidirectionalAxis axis,
        uint32_t owner_edge_index,
        EdgeData& owner_edge,
        EdgeData* current_owned_edge = nullptr,
        ParentEdgePolicy parent_policy = ParentEdgePolicy::FOLLOW_RELATION
    ) noexcept
    {
        using IAB = InstallAxisToBuffer;
        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);
        const uint64_t current_slot = IAB::ValueOfAnIdentityFromBuffer(current_identity, HeaderIdentifierOfAPC::APC_SLOT_IDX);
        const uint64_t current_inharited_edge = IAB::ValueOfAnIdentityFromBuffer(current_identity, map.InheritedEgdeTableIdx);
        const uint64_t next_of_current = IAB::ValueOfAnIdentityFromBuffer(current_identity, map.NextSibling);
        const uint64_t prev_of_current = IAB::ValueOfAnIdentityFromBuffer(current_identity, map.PreviousSibling);

        const uint64_t predecessor_slot = IAB::ValueOfAnIdentityFromBuffer(predecessor_identity, HeaderIdentifierOfAPC::APC_SLOT_IDX);
        const uint64_t first_child_of_predecessor = IAB::ValueOfAnIdentityFromBuffer(predecessor_identity, map.RootOwnedChild);

        IAB::GraphMutationValues gmv_predecessor{};
        IAB::GraphMutationValues gmv_current{};
        IAB::GraphMutationValues gmv_next{};

        if (
            !IAB::ValidateIdentityBuffer(predecessor_identity) ||
            !IAB::ValidateIdentityBuffer(current_identity) ||
            IAB::IsInheritedAxisDisabled(current_identity, axis) ||
            !owner_edge.IsValid ||
            owner_edge.Status != EdgeStatus::LIVE ||
            owner_edge.OwnLinkCount == UNSIGNED_ZERO ||
            current_inharited_edge != owner_edge_index ||
            prev_of_current != predecessor_slot ||
            owner_edge.EdgeTable != map.EdgeTable ||
            !IAB::GetGraphMutationValues(predecessor_identity, gmv_predecessor) ||
            !IAB::GetGraphMutationValues(current_identity, gmv_current) ||
            !IAB::IsDesiredAxisLocked(gmv_predecessor.Flags, axis) ||
            !IAB::IsDesiredAxisLocked(gmv_current.Flags, axis) ||
            (
                next_identity &&
                (
                    !IAB::GetGraphMutationValues(*next_identity, gmv_next) ||
                    !IAB::IsDesiredAxisLocked(gmv_next.Flags, axis)
                )
            )
        )
        {
            return false;
        }

        const bool is_predessor_is_owner = owner_edge.OwnerAPCSlot == predecessor_slot &&
            first_child_of_predecessor == current_slot;

        if (is_predessor_is_owner)
        {
            if (!IAB::InsertAnIdentityInBuffer(
                predecessor_identity,
                map.RootOwnedChild,
                next_of_current
            ))
            {
                return false;
            }
        }
        else
        {
            if (
                IAB::ValueOfAnIdentityFromBuffer(
                    predecessor_identity,
                    map.NextSibling
                ) != current_slot ||
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
                !IAB::InsertAnIdentityInBuffer(*next_identity, map.PreviousSibling, predecessor_slot)
            )
            {
                return false;
            }
        }
        else if (next_identity)
        {
            return false;
        }


        if (owner_edge.Tail == current_slot)
        {
            owner_edge.Tail = owner_edge.OwnLinkCount == 1u ? 
                APCDataStructure::APC_INDEX_BOUND_SENTINAL : static_cast<uint32_t>(predecessor_slot);
        }
        --owner_edge.OwnLinkCount;
        if (
            owner_edge.OwnLinkCount == UNSIGNED_ZERO &&
            owner_edge.Tail != APCDataStructure::APC_INDEX_BOUND_SENTINAL
        )
        {
            return false;
        }

        if (!IAB::DisableInharitadAxis(current_identity, axis))
        {
            return false;
        }

        if (!IAB::IsOwnedAxisDisabled(current_identity, axis))
        {
            if (parent_policy == ParentEdgePolicy::PRESERVE)
            {
                if (current_owned_edge)
                {
                    return false;
                }
                
            }
            if (
                !current_owned_edge ||
                !current_owned_edge->IsValid ||
                current_owned_edge->OwnerAPCSlot != current_slot ||
                current_owned_edge->EdgeTable != map.EdgeTable ||
                current_owned_edge->ParentEdgeIndex != owner_edge_index
            )
            {
                return false;
            }
            current_owned_edge->ParentEdgeIndex = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        }
        else if (current_owned_edge)
        {
            return false;
        }

        owner_edge.IsValid = ValidateEdgeData(owner_edge);

        if (
            !owner_edge.IsValid ||
            (
                current_owned_edge &&
                !ValidateEdgeData(*current_owned_edge)
            )
        )
        {
            return false;
        }
        return 
            IAB::SealIdentityBuffer(predecessor_identity) &&
            IAB::SealIdentityBuffer(current_identity) &&
            (
                !next_identity ||
                IAB::SealIdentityBuffer(*next_identity)
            );
    }


    static constexpr bool PreparedOwnedRootForRetirement(
        InstallAxisToBuffer::BufferOfAPCIdentity& identity_buffer,
        InstallAxisToBuffer::BidirectionalAxis axis,
        EdgeData& owned_edge
    ) noexcept
    {
        using IAB = InstallAxisToBuffer;
        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);

        const uint64_t root_slot = IAB::ValueOfAnIdentityFromBuffer(
            identity_buffer,
            HeaderIdentifierOfAPC::APC_SLOT_IDX
        );

        if (
            !IAB::ValidateDefaultIdentity(identity_buffer) ||
            IAB::IsOwnedAxisDisabled(identity_buffer, axis) ||
            !ValidateEdgeData(owned_edge) ||
            owned_edge.OwnerAPCSlot != root_slot ||
            owned_edge.OwnLinkCount != UNSIGNED_ZERO ||
            owned_edge.Tail != APCDataStructure::APC_INDEX_BOUND_SENTINAL
        )
        {
            return false;
        }

        owned_edge.ParentEdgeIndex = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        owned_edge.Status = EdgeStatus::RETIRED;
        return 
            IAB::DisableOwnedRoot(identity_buffer, axis) &&
            IAB::SealIdentityBuffer(identity_buffer);
    }



};

}