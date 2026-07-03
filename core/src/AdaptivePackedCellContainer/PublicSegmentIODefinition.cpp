#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include <iostream>

namespace PredictedAdaptedEncoding
{

    uint16_t SegmentIODefinition::ReadCentralAPCOccupancyOfALocality(LocalityPolicy locality_type) noexcept
    {
        const uint16_t total_capacity = static_cast<uint16_t>(
            std::min<size_t>(GetTotalCapacityForThisAPC(), APC_ALL_INDEX_LIMIT)
        );

        const std::optional<uint16_t> desired_occupancy = OccupancyOrchestrator::GetOccuupancyFromPackedCellMode48(
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

    void SegmentIODefinition::InitNodeSemantics(
        uint64_t aux_param_uint48
    ) noexcept
    {
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::NODE_COMPUTE_KIND, UNSIGNED_ZERO);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::NODE_AUX_PARAM_U32, aux_param_uint48);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::LAST_ACCEPTED_FEED_FORWARD_CLOCK16, UNSIGNED_ZERO);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::LAST_ACCEPTED_FEED_BACKWARD_CLOCK16, UNSIGNED_ZERO);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::LAST_EMITTED_FEED_FORWARD_CLOCK16, UNSIGNED_ZERO);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::LAST_EMITTED_FEED_BACKWARD_CLOCK16, UNSIGNED_ZERO);

        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::FEEDFORWARD_IN_TARGET_ID, APC_META_CELL_SENTINAL);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::FEEDFORWARD_OUT_TARGET_ID, APC_META_CELL_SENTINAL);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::FEEDBACKWARD_IN_TARGET_ID, APC_META_CELL_SENTINAL);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::FEEDBACKWARD_OUT_TARGET_ID, APC_META_CELL_SENTINAL);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::LATERAL_0_TARGET_ID, APC_META_CELL_SENTINAL);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::LATERAL_1_TARGET_ID, APC_META_CELL_SENTINAL);
    }



    void SegmentIODefinition::InitRootOrChildBranch(
        size_t total_capacity,
        const APCGroupReserver::APCInitialIdentityStruct& container_configuration
    ) noexcept
    {
        if (!IsBound())
        {
            return;
        }

        const uint64_t safe_capacity = static_cast<uint16_t>(std::min<size_t>(total_capacity, APC_ALL_INDEX_LIMIT));
        
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::MAGIC_ID, BRANCH_MAGIC);

        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::BRANCH_PRIORITY, UNSIGNED_ZERO);
        
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::CURRENT_ACTIVE_THREADS, UNSIGNED_ZERO);
        WrireAPCMetaModel_48t(MetaIndexOfAPCNode::COMBINED_OCCUPANCY_PUBLISHED_CLAIMED_FAULTY_3x16_48, UNSIGNED_ZERO);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::TOTAL_CAPACITY_OF_THIS_SEGEMENT, safe_capacity);                                                                                        
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::LAST_SPLIT_EPOCH, UNSIGNED_ZERO);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::PAGED_NODE_READY_BIT, UNSIGNED_ZERO);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::DEFINED_MODE_OF_CURRENT_APC, static_cast<uint32_t>(container_configuration.InitialMode));
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::PRODUCER_CURSOR_PLACEMENT, static_cast<uint32_t>(METACELL_COUNT));
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::CONSUMER_CURSORE_PLACEMENT, static_cast<uint32_t>(METACELL_COUNT));
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::CURRENTLY_OWNED, UNSIGNED_ZERO);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::TOTAL_CAS_FAILURE_FOR_THIS_APC_BRANCH, UNSIGNED_ZERO);

        ConfigureThisAPCIdentity(container_configuration);
        for (uint8_t i = 0; i < APCAndPagedNodeHelpers::SIZE_OF_APCPagedNodeRelMaskClasses; i++)
        {
            InsertTypedValue48MetaCellOfAPC_(
                static_cast<MetaIndexOfAPCNode>(static_cast<size_t>(MetaIndexOfAPCNode::REGION_OCCUPANCY_NONE) + i), 
                UNSIGNED_ZERO
            );
        }

        ResetALLOccupancy16x3ModelToZero_();
        
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::EOF_APC_HEADER, EOF_HEADER);

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

    bool SegmentIODefinition::CasUpdateOccupancy3x16ThreeSubdivisionCell__(
        LocalityPolicy from_locality,
        LocalityPolicy to_locality,
        std::optional<APCPagedNodeSegmentClasses> page_class,
        LocalityPolicy control_or_meta_cells_own_locality,
        bool is_this_cell_central_occupancy_counter
    ) noexcept
    {
        if (from_locality == to_locality)
        {
            return true;
        }

        if (is_this_cell_central_occupancy_counter)
        {
            if (page_class.has_value())
            {
                return false;
            }
        }
        else
        {
            if (!page_class.has_value() || !APCAndPagedNodeHelpers::IsTrackedOccupancyPageClass(*page_class))
            {
                return false;
            }
        }
        const MetaIndexOfAPCNode meta_idx = page_class.has_value() ? APCAndPagedNodeHelpers::GetOccupancyMetIndexByRegionClass(*page_class) : MetaIndexOfAPCNode::COMBINED_OCCUPANCY_PUBLISHED_CLAIMED_FAULTY_3x16_48;

        if (!ValidMetaIdx(meta_idx))
        {
            return false;
        }
        
        packed64_t observed_cell = ReadFullMetaCell(meta_idx);
        while (true)
        {

            const PackedCell64_t::AuthoritiveCellView occupancy_cell_view = PackedCell64_t::GetAuthoritiveViewsForACell(observed_cell);

            if (!APCAndPagedNodeHelpers::DoseThisCellUpdateableAsOccupancy16x3(occupancy_cell_view))
            {
                return false;
            }

            //return addresses
            uint16_t published_count = UNSIGNED_ZERO;
            uint16_t claimed_count = UNSIGNED_ZERO;
            uint16_t faulty_count = UNSIGNED_ZERO;
            //

            const uint64_t raw48 = APCDataStructure::AutoExtractDataOfAValidAPCCell(observed_cell);

            if (!Subdevision16x3InternalMode48CellModel::ExtractLowMidHighFromMode48_(raw48, published_count, claimed_count, faulty_count))
            {
                return false;
            }

            auto DecrementLocalityCount = [&](LocalityPolicy locality) noexcept->bool
            {
                switch (locality)
                {
                case LocalityPolicy::IDLE :
                    return true;

                case LocalityPolicy::PUBLISHED :
                    if (published_count > UNSIGNED_ZERO)
                    {
                        --published_count;
                        return true;
                    }
                    return false;

                case LocalityPolicy::CLAIMED :
                    if (claimed_count > UNSIGNED_ZERO)
                    {
                        --claimed_count;
                        return true;
                    }
                    return false;

                case LocalityPolicy::FAULTY :
                    if (faulty_count > UNSIGNED_ZERO)
                    {
                        --faulty_count;
                        return true;
                    }
                    return false;
                    
                default:
                    return false;
                }
            };

            auto IncrementLocalityCount = [&](LocalityPolicy locality) noexcept
            {
                switch (locality)
                {
                case LocalityPolicy::IDLE :
                    return true;
                case LocalityPolicy::PUBLISHED :
                    if (published_count < APC_ALL_INDEX_LIMIT)
                    {
                        published_count++;
                        return true;
                    }
                    return false;
                case LocalityPolicy::CLAIMED :
                    if (claimed_count < APC_ALL_INDEX_LIMIT)
                    {
                        claimed_count++;
                        return true;
                    }
                    return false;
                case LocalityPolicy::FAULTY :
                    if (faulty_count < APC_ALL_INDEX_LIMIT)
                    {
                        faulty_count++;
                        return true;
                    }
                    return false;
                default:
                    return false;
                }
            };

            const bool decrement_ok = DecrementLocalityCount(from_locality);
            const bool increment_ok = IncrementLocalityCount(to_locality);
            if (!increment_ok || !decrement_ok)
            {
                return false;
            }

            const packed64_t desired_cell = OccupancyOrchestrator::ComposeAPCOwned16x3Model_48t(
                published_count, claimed_count, faulty_count, 
                APCPagedNodeSegmentClasses::META_HEADER,
                control_or_meta_cells_own_locality
            );

            packed64_t expected_cell = observed_cell;

            if (BackingPtr[static_cast<size_t>(meta_idx)].compare_exchange_strong(expected_cell, desired_cell, OnExchangeSuccess, OnExchangeFailure))
            {
                BackingPtr[static_cast<size_t>(meta_idx)].notify_all();
                return true;
            }
            observed_cell = expected_cell;

            std::this_thread::yield();
            
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

    uint16_t SegmentIODefinition::ReadTotalOccuPancyOfAnyPageClass(APCPagedNodeSegmentClasses page_class) noexcept
    {

        const packed64_t packed_cell = page_class != APCPagedNodeSegmentClasses::NULLNAN ?
            ReadRegionOccupancyCombinedCell(page_class) : ReadCentralAPCOccupancyCellForThisPagedNode();

        const uint16_t full_combined_occupancy = OccupancyOrchestrator::GetTootalOccupancyFromPackedCell(packed_cell);
        return full_combined_occupancy;
    }
}