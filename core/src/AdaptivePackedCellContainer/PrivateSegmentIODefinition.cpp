#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include <iostream>

namespace PredictedAdaptedEncoding
{

    void SegmentIODefinition::InsertTypedValue48MetaCellOfAPC_(
        MetaIndexOfAPCNode idx,
        uint64_t value48,
        LocalityPolicy locality
    ) noexcept
    {
        size_t index = static_cast<size_t>(idx);
        if (!ValidMetaIdx(idx))
        {
            return;
        }

        const packed64_t packed_cell = PackedCell64_t::MakeTypedAPCValidPackedCell(
            TypeFamily::VALUE48,
            AccessContractOfValue::CAS_RMW,
            APCPagedNodeSegmentClasses::META_HEADER,
            locality,
            InternalDataTypePolicy::UnsignedPCellDataType,
            AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
            value48,
            UNSIGNED_ZERO
        );

        BackingPtr[index].store(packed_cell, MoStoreSeq_);
        BackingPtr[index].notify_all();
    }

    void SegmentIODefinition::WrireAPCMetaModel_48t(
        MetaIndexOfAPCNode idx,
        uint64_t raw48_value,
        Model48Subclass sub_class,
        LocalityPolicy locality,
        InternalDataTypePolicy dtype,
        AttributePolicy attribute 
    ) noexcept
    {
        size_t index = static_cast<size_t>(idx);
        if (!ValidMetaIdx(idx))
        {
            return;
        }
        const meta16_t meta16 = PackedCell64_t::MakeMeta16ForAnyOwnerAndItsClassModel_48t(
            OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER,
            static_cast<tag8_t>(APCPagedNodeSegmentClasses::META_HEADER),
            sub_class, attribute, locality, dtype
        );
        const packed64_t packed_cell = PackedCell64_t::Compose48BitFamilyPackedCell(raw48_value & MaskLowNBits(FAMILY_48_BIT_LEN), meta16);
        BackingPtr[index].store(packed_cell, MoStoreSeq_);
        BackingPtr[index].notify_all();
    }

    void SegmentIODefinition::InitDefaultAPCSegmentedNodeLayout_() noexcept
    {
        const uint32_t total_capacity = GetTotalCapacityForThisAPC();

        CompleteAPCNodeRegionsLayout full_paged_node_layout{};        
        
        CompleteAPCNodeRegionsLayout::BuildInitialLayoutPlan(full_paged_node_layout, total_capacity, APCDataStructure::BRANCH_VERSION);

        if (!WriteAllRegionsLayoutToHeader_(full_paged_node_layout, static_cast<uint16_t>(BRANCH_VERSION), false))
        {
            #ifndef NDEBUG
                std::cerr << "[APC INIT BUG] failed to write initial versioned layout\n";
                std::terminate();
            #endif
                return;
        }

        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::EDGE_TABLE_COUNT, UNSIGNED_ZERO);
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::WEIGHT_TABLE_COUNT, UNSIGNED_ZERO);
        #ifndef NDEBUG
            auto layout = ReadAndGetFullRegionLayout_(false);
            if (!layout)
            {
                std::cerr << "[APC INIT BUG] layout version validation failed after init\n";
                std::terminate();
            }
        #endif        
    }


    bool SegmentIODefinition::WriteBoundsPairToHeader_(
        const LayoutBoundsOfSingleRelNodeClass layout_bound,
        std::optional<uint16_t> version_number,
        bool caller_holds_the_flag
    ) noexcept
    {
        const bool valid_layout_class =
            APCAndPagedNodeHelpers::IsDataConsumablePageClass(
                layout_bound.PAGE_LAYOUT_CLASS
            ) ||
            layout_bound.PAGE_LAYOUT_CLASS == APCPagedNodeSegmentClasses::FREE_SLOT;

        if (!valid_layout_class)
        {
            return true;
        }

        if (caller_holds_the_flag && !IsLayoutMutationFlagActive())
        {
            return false;
        }

        const uint32_t payload_begin = METACELL_COUNT;
        const uint32_t payload_end = GetTotalCapacityForThisAPC();

        if (!layout_bound.IsValid(payload_begin, payload_end))
        {
            return false;
        }

        if (layout_bound.BeginIndex > APC_ALL_INDEX_LIMIT ||
            layout_bound.EndIndex > APC_ALL_INDEX_LIMIT ||
            layout_bound.EndIndex < layout_bound.BeginIndex)
        {
            return false;
        }

        const MetaIndexOfAPCNode layout_idx =
            LayoutBoundsOfSingleRelNodeClass::GetLayoutCellMetaIndexForPageClass(
                layout_bound.PAGE_LAYOUT_CLASS
            );

        if (!ValidMetaIdx(layout_idx) ||
            layout_idx == MetaIndexOfAPCNode::EOF_APC_HEADER)
        {
            return false;
        }

        uint16_t resolved_version =
            version_number.has_value()
                ? *version_number
                : ReadGlobalLayoutVersion_().value_or(
                    static_cast<uint16_t>(BRANCH_VERSION)
                );

        if (resolved_version == UNSIGNED_ZERO ||
            resolved_version == APC_INDEX_SENTINAL)
        {
            resolved_version = static_cast<uint16_t>(BRANCH_VERSION);
        }
        
        const packed64_t desired_layout = OccupancyOrchestrator::ComposeAPCOwned16x3Model_48t(
            static_cast<uint16_t>(layout_bound.BeginIndex),
            static_cast<uint16_t>(layout_bound.EndIndex),
            resolved_version,
            layout_bound.PAGE_LAYOUT_CLASS
        );

        BackingPtr[static_cast<size_t>(layout_idx)].store(
            desired_layout,
            MoStoreSeq_
        );

        BackingPtr[static_cast<size_t>(layout_idx)].notify_all();

        return true;
    }



    bool SegmentIODefinition::UpdateAPCModeFlagsInHeader_(uint64_t flags_to_turn_on, uint64_t flags_to_turn_off, MetaIndexOfAPCNode desired_flag_idx) noexcept
    {
        if (desired_flag_idx != MetaIndexOfAPCNode::SEGMENT_CONF_FLAGS && desired_flag_idx != MetaIndexOfAPCNode::FABRIC_FLAG)
        {
            return false;
        }
        
        while (true)
        {
            const uint64_t current_flags = ReadValuFromAPCMetaIndecies(desired_flag_idx);
            if (current_flags == APC_META_CELL_SENTINAL)
            {
                return false;
            }
            
            uint64_t next_flags = current_flags;
            next_flags |= flags_to_turn_on;
            next_flags &= ~flags_to_turn_off;
            if (next_flags == current_flags)
            {
                return true;
            }
            if (ReplaceOnlyMetaCellValue(desired_flag_idx, current_flags, next_flags))
            {
                return true;
            }
        }
    }

    std::optional<CompleteAPCNodeRegionsLayout> SegmentIODefinition::ReadAndGetFullRegionLayout_(bool caller_holds_layout_flag) noexcept
    {  
        if (IsLayoutMutationFlagActive() && !caller_holds_layout_flag)
        {
            return std::nullopt;
        }
         
        auto LoadOne = [&](APCPagedNodeSegmentClasses desired_rel_mask, LayoutBoundsOfSingleRelNodeClass& out_one) noexcept->bool
        {
            auto maybe_one = ReadLayoutBoundsAndVersion(desired_rel_mask, caller_holds_layout_flag);
            if (!maybe_one)
            {
                return false;
            }
            out_one = *maybe_one;
            return true;
        };

        CompleteAPCNodeRegionsLayout out_layout{};
        bool ok = true;
        ok = LoadOne(APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE, out_layout.FeedForwardLayout) && ok;
        ok = LoadOne(APCPagedNodeSegmentClasses::FEEDBACKWARD_MESSAGE, out_layout.FeedBackwardLayout) && ok;
        ok = LoadOne(APCPagedNodeSegmentClasses::LATERAL_MESAGE, out_layout.LateralLayout) && ok;
        ok = LoadOne(APCPagedNodeSegmentClasses::STATE_SLOT, out_layout.StateLayout) && ok;
        ok = LoadOne(APCPagedNodeSegmentClasses::ERROR_SLOT, out_layout.ErrorLayout) && ok;
        ok = LoadOne(APCPagedNodeSegmentClasses::EDGE_DESCRIPTOR, out_layout.EdgeDescriptorLayout) && ok;
        ok = LoadOne(APCPagedNodeSegmentClasses::WEIGHT_SLOT, out_layout.WeightLayout) && ok;
        ok = LoadOne(APCPagedNodeSegmentClasses::AUX_SLOT, out_layout.AUXLayout) && ok;
        ok = LoadOne(APCPagedNodeSegmentClasses::HETEROGENOUS_RAW_MEMORY, out_layout.HeterogenousMemoryLayout) && ok;
        ok = LoadOne(APCPagedNodeSegmentClasses::SLOT_TABLE_DESCRIPTOR, out_layout.LocalPairedPointerLayout) && ok;
        ok = LoadOne(APCPagedNodeSegmentClasses::PAIRED_POINTER_IN_MEMORY, out_layout.DistancePairedLayout) && ok;
        ok = LoadOne(APCPagedNodeSegmentClasses::UNDEFINED, out_layout.UndefinedLayout) && ok;
        ok = LoadOne(APCPagedNodeSegmentClasses::FREE_SLOT, out_layout.FreeLayout) && ok;
        if (!ok)
        {
            return std::nullopt;
        }

        const std::optional<uint16_t> maybe_lobal_version = ReadGlobalLayoutVersion_();
        if (!maybe_lobal_version)
        {
            return std::nullopt;
        }
        if (!caller_holds_layout_flag && !out_layout.DoseAllPhysicalLayoutCarrySameVersionNumberAsGlobal(*maybe_lobal_version))
        {
            return std::nullopt;
        }
        
        return out_layout;
    }

    bool SegmentIODefinition::WriteAllRegionsLayoutToHeader_(
        const CompleteAPCNodeRegionsLayout& full_layout,
        std::optional<uint16_t> forced_version_number,
        bool caller_holds_the_flag
    ) noexcept
    {
        bool owns_layout_flag = false;

        if (caller_holds_the_flag)
        {
            if (!IsLayoutMutationFlagActive())
            {
                return false;
            }
        }
        else
        {
            if (!TrySetLayoutMutationInFlight())
            {
                return false;
            }
            owns_layout_flag = true;
        }

        auto ClearIfOwned = [this, owns_layout_flag]() noexcept
        {
            if (owns_layout_flag)
            {
                ClearOneControlEnumFlagOfAPC(ControlEnumOfAPCSegment::LAYOUT_MUTATION_INFLIGHT);
            }
        };

        auto FailedWrite = [&]()noexcept -> bool
        {
            ClearIfOwned();
            return false;
        };
        const uint32_t payload_begin = METACELL_COUNT;
        const uint32_t payload_end = GetTotalCapacityForThisAPC();
        auto orderd_layout_view = const_cast<CompleteAPCNodeRegionsLayout&>(full_layout).OrderedViewsFIFO();

        uint32_t cursor = payload_begin;
        for (const auto* one_layout : orderd_layout_view)
        {
            if (!one_layout)
            {
                return FailedWrite();
            }
            if (!one_layout->IsValid(payload_begin, payload_end))
            {
                return FailedWrite();
            }
            if (one_layout->BeginIndex != cursor)
            {
                return FailedWrite();
            }
            if (one_layout->EndIndex < one_layout->BeginIndex)
            {
                return FailedWrite();
            }
            cursor = one_layout->EndIndex;
        }
        
        if (cursor != payload_end)
        {
            return FailedWrite();
        }
        

        uint16_t commit_version = 0;
        if (forced_version_number.has_value() && *forced_version_number != UNSIGNED_ZERO && *forced_version_number != APC_INDEX_SENTINAL)
        {
            commit_version = *forced_version_number;
        }
        else
        {
            commit_version = NextGlobalLayoutVersion_().value_or(static_cast<uint16_t>(BRANCH_VERSION));
        }

        if (commit_version == UNSIGNED_ZERO || commit_version == APC_INDEX_SENTINAL)
        {
            return FailedWrite();
        }
        

        
        const bool ok =
            WriteBoundsPairToHeader_(full_layout.FeedForwardLayout, commit_version, true) &&
            WriteBoundsPairToHeader_(full_layout.FeedBackwardLayout, commit_version, true) &&
            WriteBoundsPairToHeader_(full_layout.LateralLayout, commit_version, true) &&
            WriteBoundsPairToHeader_(full_layout.StateLayout, commit_version, true) &&
            WriteBoundsPairToHeader_(full_layout.ErrorLayout, commit_version, true) &&
            WriteBoundsPairToHeader_(full_layout.EdgeDescriptorLayout, commit_version, true) &&
            WriteBoundsPairToHeader_(full_layout.WeightLayout, commit_version, true) &&
            WriteBoundsPairToHeader_(full_layout.AUXLayout, commit_version, true) &&
            WriteBoundsPairToHeader_(full_layout.HeterogenousMemoryLayout, commit_version, true) &&
            WriteBoundsPairToHeader_(full_layout.LocalPairedPointerLayout, commit_version, true) &&
            WriteBoundsPairToHeader_(full_layout.DistancePairedLayout, commit_version, true) &&
            WriteBoundsPairToHeader_(full_layout.UndefinedLayout, commit_version, true) &&
            WriteBoundsPairToHeader_(full_layout.FreeLayout, commit_version, true);

        if (!ok)
        {
            return FailedWrite();
        }

        if (!WriteGlobalLayoutVersion_(commit_version))
        {
            return FailedWrite();
        }
        TurnOnASegmentFlag(ControlEnumOfAPCSegment::HAS_LAYOUT_DIR);
        ClearIfOwned();
        return true;
    }

    bool SegmentIODefinition::TurnOnReadyBitForDesiredPagedNode_(APCPagedNodeSegmentClasses desired_region_class) noexcept
    {
        const uint32_t anew_readybit = APCAndPagedNodeHelpers::MakeOneAPCNodeClassReadyBit(desired_region_class);
        if (anew_readybit == 0)
        {
            return false;
        }
        while (true)
        {
            const uint32_t compleate_current_paged_node_ready_bit = static_cast<uint32_t>(ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode::PAGED_NODE_READY_BIT));
            const uint32_t updated_current_ready_bit = compleate_current_paged_node_ready_bit | anew_readybit;
            if (updated_current_ready_bit == compleate_current_paged_node_ready_bit)
            {
                return true;
            }
            if (ReplaceOnlyMetaCellValue(MetaIndexOfAPCNode::PAGED_NODE_READY_BIT, compleate_current_paged_node_ready_bit, updated_current_ready_bit))
            {
                return true;
            }
        }
        return false;
    }

    bool SegmentIODefinition::ClearTheDesiredPagedNodeReadyBit_(APCPagedNodeSegmentClasses desired_region_class) noexcept
    {
        const uint32_t anew_readybit = APCAndPagedNodeHelpers::MakeOneAPCNodeClassReadyBit(desired_region_class);
        if (anew_readybit == 0)
        {
            return false;
        }
        while (true)
        {
            const uint32_t compleate_current_paged_node_ready_bit = static_cast<uint32_t>(ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode::PAGED_NODE_READY_BIT));
            const uint32_t updated_current_ready_bit = compleate_current_paged_node_ready_bit & ~anew_readybit;
            if (updated_current_ready_bit == compleate_current_paged_node_ready_bit)
            {
                return true;
            }
            if (ReplaceOnlyMetaCellValue(MetaIndexOfAPCNode::PAGED_NODE_READY_BIT, compleate_current_paged_node_ready_bit, updated_current_ready_bit))
            {
                return true;
            }
        }
        return false;
    }

    bool SegmentIODefinition::ResetALLOccupancy16x3ModelToZero_() noexcept
    {
        if (!IsBound())
        {
            return false;
        }

        auto StoreCount = [&](
            MetaIndexOfAPCNode meta_idx,
            APCPagedNodeSegmentClasses page_class,
            uint16_t published,
            uint16_t claimed,
            uint16_t faulty
        )
        {
            (void) page_class;
            
            if (!ValidMetaIdx(meta_idx))
            {
                return false;
            }
            const packed64_t wanted_cell = OccupancyOrchestrator::ComposeAPCOwned16x3Model_48t(
                published, claimed, faulty,
                APCPagedNodeSegmentClasses::META_HEADER,
                LocalityPolicy::PUBLISHED
            );
            BackingPtr[static_cast<size_t>(meta_idx)].store(wanted_cell, MoStoreSeq_);
            BackingPtr[static_cast<size_t>(meta_idx)].notify_all();
            return true;
        };

        const uint16_t meta_published = static_cast<uint16_t>(
            std::min<size_t>(METACELL_COUNT, APC_ALL_INDEX_LIMIT)
        );

        StoreCount(
            MetaIndexOfAPCNode::COMBINED_OCCUPANCY_PUBLISHED_CLAIMED_FAULTY_3x16_48,
            APCPagedNodeSegmentClasses::META_HEADER,
            meta_published,
            UNSIGNED_ZERO,
            UNSIGNED_ZERO
        );

        for (uint8_t i = 0; i < APCAndPagedNodeHelpers::SIZE_OF_APCPagedNodeRelMaskClasses; i++)
        {
            const APCPagedNodeSegmentClasses current_page_class = static_cast<APCPagedNodeSegmentClasses>(i);
            if (!APCAndPagedNodeHelpers::IsTrackedOccupancyPageClass(current_page_class))
            {
                continue;
            }

            const MetaIndexOfAPCNode idx_of_current_page_class = APCAndPagedNodeHelpers::GetOccupancyMetIndexByRegionClass(current_page_class);
            StoreCount(
                idx_of_current_page_class,
                current_page_class,
                UNSIGNED_ZERO,
                UNSIGNED_ZERO,
                UNSIGNED_ZERO
            );
        }
        
        StoreCount(
            MetaIndexOfAPCNode::REGION_OCCUPANCY_CONTROL,
            APCPagedNodeSegmentClasses::META_HEADER,
            meta_published,
            UNSIGNED_ZERO,
            UNSIGNED_ZERO
        );

        ForceAutoReplaceAPCMetaCellValue(
            MetaIndexOfAPCNode::PAGED_NODE_READY_BIT,
            UNSIGNED_ZERO
        );
        return true;
    }


    std::optional<uint16_t> SegmentIODefinition::ReadGlobalLayoutVersion_() noexcept
    {
        const uint16_t raw = static_cast<uint16_t>(ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode::GLOBAL_CURRENT_VERSION));
        if (raw > APC_ALL_INDEX_LIMIT || raw == UNSIGNED_ZERO)
        {
            return std::nullopt;
        }
        return raw;
    }

    bool SegmentIODefinition::WriteGlobalLayoutVersion_(uint16_t layout_version) noexcept
    {
        if (layout_version == 0)
        {
            return false;
        }

        while (true)
        {
            const uint64_t current_version = ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode::GLOBAL_CURRENT_VERSION);
            if ((current_version) == layout_version)
            {
                return true;
            }
            if (ReplaceOnlyMetaCellValue(
                MetaIndexOfAPCNode::GLOBAL_CURRENT_VERSION,
                current_version,
                static_cast<uint32_t>(layout_version)
            ))
            {
                return true;
            }
        }
    }
    
    std::optional<uint16_t> SegmentIODefinition::NextGlobalLayoutVersion_() noexcept
    {
        std::optional<uint16_t> maybe_current_layout_version = ReadGlobalLayoutVersion_();
        uint16_t current_global_version = maybe_current_layout_version.has_value() ? *maybe_current_layout_version : static_cast<uint16_t>(BRANCH_VERSION);
        uint16_t next_global_layout_version = current_global_version + 1;
        if (next_global_layout_version == APC_INDEX_SENTINAL || next_global_layout_version == UNSIGNED_ZERO)
        {
            return static_cast<uint16_t>(BRANCH_VERSION);
        }
        return next_global_layout_version;
    }

    std::optional<LayoutBoundsOfSingleRelNodeClass> SegmentIODefinition::GetVirtualControlSlotLayout_() noexcept
    {
        LayoutBoundsOfSingleRelNodeClass out_virtual_control_slot{};
        out_virtual_control_slot.BeginIndex = 0u;
        out_virtual_control_slot.EndIndex = static_cast<uint32_t>(METACELL_COUNT);
        out_virtual_control_slot.VersionNumber = ReadGlobalLayoutVersion_().value_or(static_cast<uint16_t>(BRANCH_VERSION));
        out_virtual_control_slot.PAGE_LAYOUT_CLASS = APCPagedNodeSegmentClasses::META_HEADER;
        out_virtual_control_slot.SetOrResetPercentage(
            static_cast<uint32_t>(GetTotalCapacityForThisAPC() == UNSIGNED_ZERO ? METACELL_COUNT : GetTotalCapacityForThisAPC())
        );
        return out_virtual_control_slot;
    }

    bool SegmentIODefinition::ApplyRegionalMigrationOccupancyTransitionCell(
        LocalityPolicy from_locality_of_source_cell,
        LocalityPolicy destination_to_locality_of_source_cell,
        APCPagedNodeSegmentClasses source_page_class,
        APCPagedNodeSegmentClasses destination_page_class
    ) noexcept
    {
        if (!APCAndPagedNodeHelpers::IsTrackedOccupancyPageClass(source_page_class) || !APCAndPagedNodeHelpers::IsTrackedOccupancyPageClass(destination_page_class))
        {
            return false;
        }

        /*
            1. source region: source_from -> idle
        */
        const bool source_region_ok = CasUpdateOccupancy3x16ThreeSubdivisionCell__(
            from_locality_of_source_cell,
            LocalityPolicy::IDLE, source_page_class,
            LocalityPolicy::PUBLISHED, false
        );

        if (!source_region_ok)
        {
            return false;
        }

        /*
            2. destination region: idle -> destination_to
        */
        const bool destination_region_ok = CasUpdateOccupancy3x16ThreeSubdivisionCell__(
            LocalityPolicy::IDLE,
            destination_to_locality_of_source_cell, destination_page_class,
            LocalityPolicy::PUBLISHED, false
        );

        if (!destination_region_ok)
        {
            CasUpdateOccupancy3x16ThreeSubdivisionCell__(
                LocalityPolicy::IDLE,
                from_locality_of_source_cell, source_page_class,
                LocalityPolicy::PUBLISHED, false
            );
            return false;
        }
        
        /*
            3. central source decrement
        */
       const bool central_source_ok = CasUpdateOccupancy3x16ThreeSubdivisionCell__(
        from_locality_of_source_cell,
        LocalityPolicy::IDLE, std::nullopt,
        LocalityPolicy::PUBLISHED, true
       );

       if (!central_source_ok)
       {
            CasUpdateOccupancy3x16ThreeSubdivisionCell__(
                destination_to_locality_of_source_cell,
                LocalityPolicy::IDLE,
                destination_page_class,
                LocalityPolicy::PUBLISHED,
                false
            );
            
            CasUpdateOccupancy3x16ThreeSubdivisionCell__(
                LocalityPolicy::IDLE,
                from_locality_of_source_cell,
                source_page_class,
                LocalityPolicy::PUBLISHED,
                false
            );
            return false;
       }
       
        /*
            4. central destination increment
        */
        
        const bool central_destination_ok = CasUpdateOccupancy3x16ThreeSubdivisionCell__(
            LocalityPolicy::IDLE, destination_to_locality_of_source_cell,
            std::nullopt, LocalityPolicy::PUBLISHED,
            true
        );

        if (!central_destination_ok)
        {
            CasUpdateOccupancy3x16ThreeSubdivisionCell__(
                LocalityPolicy::IDLE, from_locality_of_source_cell,
                std::nullopt, LocalityPolicy::PUBLISHED,
                true
            );
            CasUpdateOccupancy3x16ThreeSubdivisionCell__(
                destination_to_locality_of_source_cell,
                LocalityPolicy::IDLE, destination_page_class,
                LocalityPolicy::PUBLISHED,
                false
            );
            CasUpdateOccupancy3x16ThreeSubdivisionCell__(
                LocalityPolicy::IDLE,
                from_locality_of_source_cell,
                source_page_class,
                LocalityPolicy::PUBLISHED,
                false
            );
            return false;
        }
        RefreshReadyBitForRegionFromOccupancy(source_page_class);
        RefreshReadyBitForRegionFromOccupancy(destination_page_class);
        return true;
    }


    bool SegmentIODefinition::ValidateAPCOccupancyInvarient() noexcept
    {
        uint32_t published_sum = 0;
        uint32_t claimed_sum = 0;
        uint32_t faulty_sum = 0;

        for (size_t i = 0; i < APCAndPagedNodeHelpers::SIZE_OF_APCPagedNodeRelMaskClasses; i++)
        {
            const APCPagedNodeSegmentClasses page_class = static_cast<APCPagedNodeSegmentClasses>(i);
            if (!APCAndPagedNodeHelpers::IsTrackedOccupancyPageClass(page_class))
            {
                continue;
            }
            
            published_sum += ReadRegionOccupancyOfALocality(LocalityPolicy::PUBLISHED, page_class);
            claimed_sum += ReadRegionOccupancyOfALocality(LocalityPolicy::CLAIMED, page_class);
            faulty_sum += ReadRegionOccupancyOfALocality(LocalityPolicy::FAULTY, page_class);
        }

        const uint32_t central_published = ReadCentralAPCOccupancyOfALocality(LocalityPolicy::PUBLISHED);
        const uint32_t central_claimed = ReadCentralAPCOccupancyOfALocality(LocalityPolicy::CLAIMED);
        const uint32_t central_faulty = ReadCentralAPCOccupancyOfALocality(LocalityPolicy::FAULTY);

        return central_published == published_sum &&
            central_claimed == claimed_sum &&
            central_faulty == faulty_sum;
    }


}