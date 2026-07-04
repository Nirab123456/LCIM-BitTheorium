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


    bool SegmentIODefinition::UpdateAPCModeFlagsInHeader_(uint64_t flags_to_turn_on, uint64_t flags_to_turn_off, MetaIndexOfAPCNode desired_flag_idx) noexcept
    {
        
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