#pragma once
#include "LayoutBoundsOrchestrator.hpp"

namespace PredictedAdaptedEncoding
{


struct OccupancyBuilderAndValidator : public TrackingBufferConf
{
    struct OccupancyCarrier
    {
        uint16_t IdleOccupancy = APCDataStructure::APC_INDEX_SENTINAL;
        uint16_t ClaimedOccupancy = APCDataStructure::APC_INDEX_SENTINAL;
        uint16_t PublishedOccupancy = APCDataStructure::APC_INDEX_SENTINAL;
        APCPagedNodeSegmentClasses OccupancyOrigin = APCPagedNodeSegmentClasses::NULLNAN; ////
        MetaIndexOfAPCNode MetaIndexForThis = MetaIndexOfAPCNode::UNASSIGNED_UNUSED_NANNULL;
        LocalityPolicy localityOfThisOccupancy = LocalityPolicy::UNASSIGNED_UNUSED_NANNULL;
        bool IsValid = false;
    };
    static_assert(sizeof(OccupancyCarrier) <= 2 * sizeof(packed64_t));

    static constexpr void RestOccupancyCarrier(OccupancyCarrier& a_occupancy_carrier) noexcept
    {
        a_occupancy_carrier.IdleOccupancy = APCDataStructure::APC_INDEX_SENTINAL;;
        a_occupancy_carrier.ClaimedOccupancy = APCDataStructure::APC_INDEX_SENTINAL;;
        a_occupancy_carrier.PublishedOccupancy = APCDataStructure::APC_INDEX_SENTINAL;;
        a_occupancy_carrier.OccupancyOrigin = APCPagedNodeSegmentClasses::NULLNAN;
        a_occupancy_carrier.MetaIndexForThis = MetaIndexOfAPCNode::UNASSIGNED_UNUSED_NANNULL;
        a_occupancy_carrier.localityOfThisOccupancy = LocalityPolicy::UNASSIGNED_UNUSED_NANNULL;
        a_occupancy_carrier.IsValid = false;
    }

    static constexpr bool IsValidateAnOccupancyCarrier(
        OccupancyCarrier& a_occupancy_carrier,
        bool is_blank_description_valid = false
    ) noexcept
    {
        if (
            APCDataStructure::IsThisIndexValidForAPC(a_occupancy_carrier.IdleOccupancy) &&
            APCDataStructure::IsThisIndexValidForAPC(a_occupancy_carrier.ClaimedOccupancy) &&
            APCDataStructure::IsThisIndexValidForAPC(a_occupancy_carrier.PublishedOccupancy) &&
            a_occupancy_carrier.localityOfThisOccupancy != LocalityPolicy::UNASSIGNED_UNUSED_NANNULL
        )
        {
            const std::optional<APCPagedNodeSegmentClasses> maybe_derived_node = PageNodeOrchestrator::GetNodeClassForOccupancyMetaIdx(a_occupancy_carrier.MetaIndexForThis);
            if (maybe_derived_node.has_value() && PageNodeOrchestrator::IsValidTrackedAPCNode(a_occupancy_carrier.OccupancyOrigin))
            {
                if (maybe_derived_node.value() == a_occupancy_carrier.OccupancyOrigin)
                {
                    a_occupancy_carrier.IsValid = true;
                    return true;
                }
                a_occupancy_carrier.IsValid = false;
                return false;
            }

            if (
                maybe_derived_node.has_value() &&
                a_occupancy_carrier.OccupancyOrigin == APCPagedNodeSegmentClasses::NULLNAN
            )
            {
                a_occupancy_carrier.OccupancyOrigin = maybe_derived_node.value();
                a_occupancy_carrier.IsValid = true;
                return true;
            }

            const std::optional<MetaIndexOfAPCNode> maybe_derived_meta_idx = PageNodeOrchestrator::OccupancyMetaIdxFromNodeClass(a_occupancy_carrier.OccupancyOrigin);
            if (
                maybe_derived_meta_idx.has_value() &&
                !maybe_derived_node
            )
            {
                a_occupancy_carrier.MetaIndexForThis = maybe_derived_meta_idx.value();
                a_occupancy_carrier.IsValid = true;
                return true;
            }

            if (is_blank_description_valid)
            {
                a_occupancy_carrier.IsValid = true;
                return true;
            }
            
        }

        a_occupancy_carrier.IsValid = false;
        return false;
    }

    /// @brief USES: Model48Subclass::SUBDIVISION16x3_INTERNAL_CELL_MODEL & CREATS: Occupancy cell OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER  [LocalityPolicy::PUBLISHED | LocalityPolicy::CLAIMED | LocalityPolicy::FAULTY]
    /// @param PublishedOccupancy LOWEST: uint16_t in PackUnsigned16x3ToMode48_
    /// @param ClaimedOccupancy MID: uint16_t in PackUnsigned16x3ToMode48_
    /// @param IdleOccupancy HIGHIEST: uint16_t in PackUnsigned16x3ToMode48_
    /// @return 
    static constexpr packed64_t CreateAPCOccupancyCell(
        OccupancyCarrier& valid_occupancy_carrier
    ) noexcept
    {   
        if (!IsValidateAnOccupancyCarrier(valid_occupancy_carrier, true))
        {
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }
        
        const uint64_t raw_48 =  Subdevision16x3InternalMode48CellModel::PackUnsigned16x3ToMode48_(
            valid_occupancy_carrier.PublishedOccupancy,
            valid_occupancy_carrier.ClaimedOccupancy,
            valid_occupancy_carrier.IdleOccupancy
        );

        return PackedCell64_t::MakeModeledAPCValidPackedCell(
            ModelFamily::MODEL48,
            static_cast<tag8_t>(Model48Subclass::SUBDIVISION16x3_INTERNAL_CELL_MODEL),
            APCPagedNodeSegmentClasses::META_HEADER,
            valid_occupancy_carrier.localityOfThisOccupancy,
            InternalDataTypePolicy ::UnsignedPCellDataType,
            AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
            raw_48
        );
    }


    static constexpr OccupancyCarrier GetAnOccupancyCarrierFromValidOccupancyCell(packed64_t packed_cell) noexcept
    {
        OccupancyCarrier return_occupancy{};
        const PackedCell64_t::AuthoritiveCellView desired_auth_view = PackedCell64_t::GetAuthoritiveViewsForACell(packed_cell);
        
        if (
            !PageNodeOrchestrator::IsValidAPCHeaderCell(desired_auth_view) ||
            desired_auth_view.SubClassOfModel48 != Model48Subclass::SUBDIVISION16x3_INTERNAL_CELL_MODEL
        )
        {
            return return_occupancy;
        }

        return_occupancy.IsValid = Subdevision16x3InternalMode48CellModel::ExtractLowMidHighFromMode48_(
            desired_auth_view.Raw48BitInCellData,
            return_occupancy.PublishedOccupancy,
            return_occupancy.ClaimedOccupancy,
            return_occupancy.IdleOccupancy
        );

        IsValidateAnOccupancyCarrier(return_occupancy, true);

        return return_occupancy;
    }


    static constexpr bool IsCapacityOfAPCLegal(size_t total_capacity) noexcept
    {
        return total_capacity > APCDataStructure::METACELL_COUNT && APCDataStructure::IsThisIndexValidForAPC(static_cast<uint32_t>(total_capacity));
    }

    static constexpr std::optional<uint16_t> GetOccupancyWithoutErrorFromPackedCell(packed64_t packed_cell) noexcept
    {
        const OccupancyCarrier desired_occupancy_files = GetAnOccupancyCarrierFromValidOccupancyCell(packed_cell);

        const uint32_t sum = desired_occupancy_files .IdleOccupancy + desired_occupancy_files.ClaimedOccupancy + desired_occupancy_files.PublishedOccupancy;

        if (APCDataStructure::IsThisIndexValidForAPC(sum))
        {
            return static_cast<uint16_t>(sum);
        }

        return std::nullopt;
    }


////////////////////////////////////////////////////////////////////////// MOST WILL BE OBSULE BELLOW IN THIS STRUCT

        static constexpr std::optional<uint16_t> GetOccuupancyFromPackedCellMode48(
            packed64_t packed_cell,
            LocalityPolicy desired_occupancy_bucket,
            uint16_t physical_capacity
        ) noexcept
        {
            if (!Subdevision16x3InternalMode48CellModel::IsThisCellASubdevision_3x16_48t(packed_cell))
            {
                return std::nullopt;
            }
            const uint64_t raw48 = PackedCell64_t::ExtractRaw48FamilyBits(packed_cell);
            if (raw48 == PackedCell64_t::PACKED_CELL_SENTINAL)
            {
                return std::nullopt;
            }
            
            switch (desired_occupancy_bucket)
            {
            case LocalityPolicy::PUBLISHED :
                return Subdevision16x3InternalMode48CellModel::ExtractLow16FromUnsigned48_(raw48);
            case LocalityPolicy::CLAIMED :
                return Subdevision16x3InternalMode48CellModel::ExtractMid16FromUnsigned48_(raw48);
            case LocalityPolicy::FAULTY :
                return Subdevision16x3InternalMode48CellModel::ExtractHigh16FromUnsigned48_(raw48);
            case LocalityPolicy::IDLE :
                return DerivedIdleFromPackedCell48(packed_cell, physical_capacity);
            default:
                return std::nullopt;
            }
        }


protected:
        static constexpr uint32_t SumOf3PartOccupancyOf48Bit_(uint64_t raw48) noexcept
        {
            return Subdevision16x3InternalMode48CellModel::ExtractLow16FromUnsigned48_(raw48) + 
                Subdevision16x3InternalMode48CellModel::ExtractMid16FromUnsigned48_(raw48) + 
                Subdevision16x3InternalMode48CellModel::ExtractHigh16FromUnsigned48_(raw48);
        }

        static constexpr uint16_t DeriveIdleCoundtFromRaw48General_(uint64_t raw48, uint16_t physical_capacity) noexcept
        {
            const  uint32_t in_use_potion = SumOf3PartOccupancyOf48Bit_(raw48);
            return in_use_potion > physical_capacity ? UNSIGNED_ZERO : static_cast<uint16_t>(physical_capacity - in_use_potion);
        }

        static constexpr uint16_t DerivedIdleFromPackedCell48(packed64_t packed_cell, uint16_t physical_capacity) noexcept
        {
            const uint64_t raw48 = PackedCell64_t::ExtractRaw48FamilyBits(packed_cell);
            if (raw48 == PackedCell64_t::PACKED_CELL_SENTINAL)
            {
                return UNSIGNED_ZERO;
            }
            return DeriveIdleCoundtFromRaw48General_(raw48, physical_capacity);
        }
    
};



struct OccupancyOrchestrator : public OccupancyBuilderAndValidator
{
    static constexpr uint64_t VALIDATION_OCCUPANCY_BUFFER_MARK = 22222;

    static constexpr std::optional<uint8_t> GetOccupancyBufferIdxFromPageClass(
        APCPagedNodeSegmentClasses page_class
    ) noexcept
    {
        if (PageNodeOrchestrator::IsValidTrackedAPCNode(page_class))
        {
            return static_cast<uint8_t>(static_cast<uint8_t>(page_class) - static_cast<uint8_t>(APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE));
        }
        return std::nullopt;
    }

    static constexpr bool InsertAOccupacncyInBuffer(
        TrackingBufferOfAPC& occupancy_buffer,
        OccupancyCarrier& valid_occupancy_carrier
    ) noexcept
    {
        if (!IsValidateAnOccupancyCarrier(valid_occupancy_carrier, false))
        {
            return false;
        }

        const std::optional<uint8_t> buffer_idx = GetOccupancyBufferIdxFromPageClass(valid_occupancy_carrier.OccupancyOrigin);

        const packed64_t packed_cell = CreateAPCOccupancyCell(valid_occupancy_carrier);

        if (packed_cell == PackedCell64_t::PACKED_CELL_SENTINAL || !buffer_idx)
        {
            return false;
        }

        occupancy_buffer[buffer_idx.value()] = packed_cell;
        return true;
    }



    static constexpr bool BuildInitialOccupancyBuffer(
        TrackingBufferOfAPC& return_occupancy_buffer,
        const TrackingBufferOfAPC& a_valid_layout_buffer
    ) noexcept
    {
        BuildNullTrackingBuffer(return_occupancy_buffer);

        if (!LayoutBoundsOrchestrator::IsLayouBufferValidationMarked(a_valid_layout_buffer))
        {
            return false;
        }

        OccupancyCarrier occupancy_files_buffer{};

        for (uint8_t i = 0; i < PageNodeOrchestrator::TrackedAPCNodeLen(); i++)
        {
            RestOccupancyCarrier(occupancy_files_buffer);

            const std::optional<uint16_t> maybe_occupancy_span = LayoutBoundsOrchestrator::SpanOflayoutFromPackedCell(a_valid_layout_buffer[i]);
            if (!maybe_occupancy_span.has_value())
            {
                BuildNullTrackingBuffer(return_occupancy_buffer);
                return false;
            }
            const APCPagedNodeSegmentClasses current_layout_class = LayoutBoundsOrchestrator::GetOriginForLayoutClassByBufferIdx(i);
            if (current_layout_class == APCPagedNodeSegmentClasses::NULLNAN)
            {
                BuildNullTrackingBuffer(return_occupancy_buffer);
                return false;
            }
            occupancy_files_buffer.IdleOccupancy = maybe_occupancy_span.value();
            occupancy_files_buffer.ClaimedOccupancy = UNSIGNED_ZERO;
            occupancy_files_buffer.PublishedOccupancy = UNSIGNED_ZERO;
            occupancy_files_buffer.OccupancyOrigin = current_layout_class;
            occupancy_files_buffer.localityOfThisOccupancy = LocalityPolicy::PUBLISHED;
            
            bool ok = InsertAOccupacncyInBuffer(return_occupancy_buffer, occupancy_files_buffer);
            if (!ok)
            {
                BuildNullTrackingBuffer(return_occupancy_buffer);
                return false;
            }
            
        }
        return_occupancy_buffer[VALIDATION_IDX_OF_TRACKING_BUFFER] = VALIDATION_OCCUPANCY_BUFFER_MARK;
        return true;
    }
};


}