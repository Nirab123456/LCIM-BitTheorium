#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include <iostream>

namespace PredictedAdaptedEncoding
{

    uint16_t SegmentIODefinition::ReadCentralAPCOccupancyOfALocality(LocalityPolicy locality_type) noexcept
    {
        const uint16_t total_capacity = static_cast<uint16_t>(
            std::min<size_t>(GetTotalCapacityForThisAPC(), APC_ALL_INDEX_LIMIT)
        );

        const std::optional<uint16_t> desired_occupancy = OccupancyBuilderAndValidator::GetOccuupancyFromPackedCellMode48(
                ReadCentralAPCOccupancyCellForThisPagedNode(),
                locality_type,
                total_capacity
            );
        return desired_occupancy ? *desired_occupancy : UNSIGNED_ZERO;
    }

    uint64_t SegmentIODefinition::ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode idx) noexcept
    {
        if (!ValidMetaIdx(idx))
        {
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }
        size_t index = static_cast<size_t>(idx);
        return APCDataStructure::AutoExtractDataOfAValidAPCCell(BackingPtr[index].load(MoLoad_));
    }

    void SegmentIODefinition::TouchLocalMetaClock48() noexcept
    {
        return;
    }

    packed64_t SegmentIODefinition::PackPureClock48AsPackedCell(
        std::optional<uint64_t> clock48,
        AttributePolicy attribute,
        LocalityPolicy locality,
        APCPagedNodeSegmentClasses page_class
    ) noexcept
    {
        
        const meta16_t meta16 = PackedCell64_t::MakeMeta16ForAnyOwnerAndItsClassModel_48t(
            OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER,
            static_cast<tag8_t>(page_class),
            Model48Subclass::PURE_TIMER_48, 
            attribute, locality, 
            InternalDataTypePolicy::UnsignedPCellDataType
        );

        if (clock48)
        {
            return PackedCell64_t::Compose48BitFamilyPackedCell(clock48.value(), meta16);
        }
        
        Timer48 now_timer;
        return PackedCell64_t::Compose48BitFamilyPackedCell((now_timer.NowTicks() & MaskLowNBits(FAMILY_48_BIT_LEN)), meta16);
    }

    void SegmentIODefinition::WriteOrUpdateMetaClock48(AttributePolicy attribute, std::optional<uint64_t>meta_clock_48 ) noexcept
    {
        size_t idx = static_cast<size_t>(MetaIndexOfAPCNode::LOCAL_CLOCK48);
        packed64_t wanted_cell = PackPureClock48AsPackedCell(meta_clock_48, attribute, LocalityPolicy::PUBLISHED);
        BackingPtr[idx].store(wanted_cell, MoStoreSeq_);
        BackingPtr[idx].notify_all();
    }

    bool SegmentIODefinition::ReplaceOnlyMetaCellValue(
        MetaIndexOfAPCNode idx,
        uint64_t expected_value,
        uint64_t desired_value,
        bool refresh_clock16
    ) noexcept
    {
        (void) refresh_clock16;
        
        if (!ValidMetaIdx(idx))
        {
            return false;
        }
        const size_t index = static_cast<size_t>(idx);
        packed64_t expected_packed = BackingPtr[index].load(MoLoad_);
        if (APCDataStructure::AutoExtractDataOfAValidAPCCell(expected_packed) != expected_value)
        {
            return false;
        }

        const packed64_t desired_packed = APCDataStructure::ReplaceValueInAPCTypeFamilyCell(expected_packed, desired_value, false);
        return BackingPtr[index].compare_exchange_strong(
            expected_packed,
            desired_packed,
            OnExchangeSuccess,
            OnExchangeFailure
        );
        
    }

    void SegmentIODefinition::ConfigureThisAPCIdentity(
        const APCGroupReserver::APCInitialIdentityStruct& container_configuration
    ) noexcept
    {
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::APC_SLOT_IDX,UNSIGNED_ZERO);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::BRANCH_ID, UNSIGNED_ZERO);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::LOGICAL_GROUP_ID, container_configuration.LogicalId);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::SHARED_GROUP_ID, container_configuration.SharedID);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::SHARED_ID_HASH_KEY, container_configuration.SharedHashKey);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::LOGICAL_ID_HASH_KEY, container_configuration.LogicalHashKey);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::TOTAL_HORIZONTAL_COUNT_S, UNSIGNED_ZERO);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::TOTAL_VERTICAL_COUNT_L, UNSIGNED_ZERO);

        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::PREVIOUS_HORIZONTAL_S, APC_META_CELL_SENTINAL);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::NEXT_HORIZONTAL_S, APC_META_CELL_SENTINAL);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::NEXT_VERTICAL_L, APC_META_CELL_SENTINAL);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::PREVIOUS_VERTICAL_L, APC_META_CELL_SENTINAL);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::ACCESS_PASSWORD, APC_META_CELL_SENTINAL);        
        if (container_configuration.SharedSequentialCount > UNSIGNED_ZERO || container_configuration.SharedSequentialCount > UNSIGNED_ZERO)
        {
            TurnOnMultipleSegmentFlagsAtOnce_(static_cast<uint32_t>(ControlEnumOfAPCSegment::IS_GRAPH_NODE) | static_cast<uint32_t>(ControlEnumOfAPCSegment::IS_SHARED_ROOT));
            ClearOneControlEnumFlagOfAPC(ControlEnumOfAPCSegment::IS_SHARED_MAMBER);
        }
        else
        {
            TurnOnMultipleSegmentFlagsAtOnce_(static_cast<uint32_t>(ControlEnumOfAPCSegment::IS_GRAPH_NODE) | static_cast<uint32_t>(ControlEnumOfAPCSegment::IS_SHARED_MAMBER));
            ClearOneControlEnumFlagOfAPC(ControlEnumOfAPCSegment::IS_SHARED_ROOT);    
        }
        
    }

    bool SegmentIODefinition::TryIncrementOrDecrementActiveThreadCount(int8_t change_count) noexcept
    {
        ///for now
        if (change_count < 0)
        {
            change_count = -1;
        }
        else if (change_count > 0)
        {
            change_count = 1;
        }
        else
        {
            return true;
        }
        ///
        
        
        while (true)
        {
            uint32_t current_thread_count = static_cast<uint32_t>(ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode::CURRENT_ACTIVE_THREADS));
            if (current_thread_count == BIT_FAMILY_32_SENTINAL)
            {
                return false;
            }

            if (change_count < 0 && current_thread_count == 0)
            {
                return false;
            }

            const uint32_t desired = static_cast<uint32_t>(static_cast<int64_t>(current_thread_count) + static_cast<int64_t>(change_count));
            
            if (ReplaceOnlyMetaCellValue(MetaIndexOfAPCNode::CURRENT_ACTIVE_THREADS, current_thread_count, desired))
            {
                return true;
            }
        }
    }

    bool SegmentIODefinition::TryBindPortTarget(MetaIndexOfAPCNode port_meta_idx, uint64_t target_branch_id) noexcept
    {
        if (target_branch_id == APC_META_CELL_SENTINAL)
        {
            return false;
        }
        while (true)
        {
            const uint64_t current_meta_value = ReadValuFromAPCMetaIndecies(port_meta_idx);
            if (current_meta_value == target_branch_id)
            {
                return true;
            }
            if (current_meta_value != APC_META_CELL_SENTINAL)
            {
                return false;
            }
            if (ReplaceOnlyMetaCellValue(port_meta_idx, current_meta_value, target_branch_id))
            {
                return true;
            }
        }
    }


    uint64_t SegmentIODefinition::TotalCASFailForThisBranchIncreaseAndGet(uint32_t increment) noexcept
    {
        while (true)
        {
            uint64_t current_total_cas_failure = ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode::TOTAL_CAS_FAILURE_FOR_THIS_APC_BRANCH);
            if (current_total_cas_failure == APC_META_CELL_SENTINAL)
            {
                return APC_META_CELL_SENTINAL;
            }
            
            if (ReplaceOnlyMetaCellValue(MetaIndexOfAPCNode::TOTAL_CAS_FAILURE_FOR_THIS_APC_BRANCH, current_total_cas_failure, current_total_cas_failure + increment))
            {
                return current_total_cas_failure + increment;
            }   
        }
    }

    clk16_t SegmentIODefinition::ReadLastAcceptedClok16ForThisSegment(APCPagedNodeSegmentClasses region_kind) noexcept
    {
        switch (region_kind)
        {
            case APCPagedNodeSegmentClasses::FEEDBACKWARD_MESSAGE :
                return static_cast<clk16_t>(ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode::LAST_ACCEPTED_FEED_BACKWARD_CLOCK16));
        
        default:
            return static_cast<clk16_t>(ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode::LAST_ACCEPTED_FEED_FORWARD_CLOCK16));
        }
    }

    clk16_t SegmentIODefinition::ReadLastEmittedClok16ForThisSegment(APCPagedNodeSegmentClasses region_kind) noexcept
    {
        switch (region_kind)
        {
            case APCPagedNodeSegmentClasses::FEEDBACKWARD_MESSAGE :
                return static_cast<clk16_t>(ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode::LAST_EMITTED_FEED_BACKWARD_CLOCK16));
        
        default:
            return static_cast<clk16_t>(ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode::LAST_EMITTED_FEED_FORWARD_CLOCK16));
        }
    }

    bool SegmentIODefinition::ForceAutoReplaceAPCMetaCellValue(MetaIndexOfAPCNode idx, uint64_t value) noexcept
    {
        while (true)
        {
            const uint64_t current_value = ReadValuFromAPCMetaIndecies(idx);
            if (current_value == value)
            {
                return true;
            }
            if (ReplaceOnlyMetaCellValue(idx, current_value, value))
            {
                return true;
            }
        }
    }

    bool SegmentIODefinition::ApplyCentralAndRegionOccupancyTransitionCell(
        packed64_t old_cell,
        packed64_t new_cell,
        APCPagedNodeSegmentClasses physical_page_class
    ) noexcept
    {
        const LocalityPolicy from_locality = PackedCell64_t::ExtractLocalityPolicy(old_cell);
        const LocalityPolicy to_locality = PackedCell64_t::ExtractLocalityPolicy(new_cell);
        if (from_locality == to_locality)
        {
            return true;
        }

        if (!APCAndPagedNodeHelpers::IsTrackedOccupancyPageClass(physical_page_class))
        {
            return false;
        }
        


        
        const bool region_ok = CasUpdateOccupancy3x16ThreeSubdivisionCell__(from_locality, to_locality, physical_page_class, LocalityPolicy::PUBLISHED, false);

        if (!region_ok)
        {
            return false;
        }

        const bool central_ok = CasUpdateOccupancy3x16ThreeSubdivisionCell__(from_locality, to_locality, std::nullopt, LocalityPolicy::PUBLISHED, true);

        if (!central_ok)
        {
            CasUpdateOccupancy3x16ThreeSubdivisionCell__(
                to_locality,
                from_locality,
                physical_page_class,
                LocalityPolicy::PUBLISHED,
                false
            );
            return false;
        }

        RefreshReadyBitForRegionFromOccupancy(physical_page_class);
        return region_ok;
    }

    bool SegmentIODefinition::RefreshReadyBitForRegionFromOccupancy(APCPagedNodeSegmentClasses page_class) noexcept
    {
        if (!APCAndPagedNodeHelpers::IsDataConsumablePageClass(page_class))
        {
            return true;
        }
        const uint32_t published = ReadPublishedOccupancyOfAPageClass(page_class);
        if (published > UNSIGNED_ZERO)
        {
            return TurnOnReadyBitForDesiredPagedNode_(page_class);
        }
        return ClearTheDesiredPagedNodeReadyBit_(page_class);
    }

    size_t SegmentIODefinition::PayloadCapacityFromHeader() noexcept
    {
        const uint32_t payload_begain = METACELL_COUNT;
        const uint32_t payload_end  = GetTotalCapacityForThisAPC();
        if (payload_end > payload_begain)
        {
            return static_cast<size_t>(payload_end - payload_begain);
        }
        return UNSIGNED_ZERO;
    }

}