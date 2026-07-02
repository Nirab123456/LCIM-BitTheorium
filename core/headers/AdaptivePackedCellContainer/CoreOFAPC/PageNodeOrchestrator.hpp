#pragma once
#include "IdAndIdentityOfAPC.hpp"

namespace PredictedAdaptedEncoding
{
    struct APCAndPagedNodeHelpers
    {
        static constexpr uint8_t HIGH_FOUR_NIBBLE = 0x0Fu;
        static constexpr uint8_t HIGH_ALL_EIGHT_NIBBLE = 0xFFu;
        static constexpr size_t SIZE_OF_APCPagedNodeRelMaskClasses = 16u;

        static constexpr bool INewerClock16(clk16_t candidate, clk16_t baseline) noexcept
        {
            if (candidate == baseline)
            {
                return false;
            }
            return static_cast<uint16_t>(candidate - baseline) < HALF16Bit_THRESHOLD_WRAP;
            
        }

        static constexpr uint32_t MakeOneAPCNodeClassReadyBit(APCPagedNodeSegmentClasses desired_rel_class) noexcept
        {
            const uint32_t rel_class = static_cast<uint8_t>(desired_rel_class) & HIGH_FOUR_NIBBLE;
            if (rel_class == static_cast<uint8_t>(APCPagedNodeSegmentClasses::NONE) || rel_class == static_cast<uint8_t>(APCPagedNodeSegmentClasses::NULLNAN))
            {
                return UNSIGNED_ZERO;
            }
            return (1u << rel_class);
        }


        static constexpr bool CanCellBeConsumedForThisRegion(packed64_t packed_cell, APCPagedNodeSegmentClasses region_kind) noexcept
        {
            return PackedCell64_t::ExtractLocalityPolicy(packed_cell) == LocalityPolicy::PUBLISHED &&
                PackedCell64_t::ExtractAPCPagedNodeSegmentClasse(packed_cell) == region_kind;        
        }

        static constexpr MetaIndexOfAPCNode GetOccupancyMetIndexByRegionClass(
            APCPagedNodeSegmentClasses desired_region_class
        )noexcept
        {
            return static_cast<MetaIndexOfAPCNode>(
                static_cast<size_t>(MetaIndexOfAPCNode::REGION_OCCUPANCY_NONE) +
                (static_cast<uint8_t>(desired_region_class) & HIGH_FOUR_NIBBLE)
                );
        }

        static constexpr bool IsEmbededControlCell(const PackedCell64_t::AuthoritiveCellView& a_cell_view) noexcept
        {
            if (a_cell_view.PageClass == APCPagedNodeSegmentClasses::META_HEADER)
            {
                return true;
            }
            
            return false;
        }

        static constexpr bool IsEmbededTimerCell(const PackedCell64_t::AuthoritiveCellView& a_cell_view) noexcept
        {
            return a_cell_view.CellMode == PackedMode::MODEL48 && 
                a_cell_view.SubClassOfModel48 == Model48Subclass::PURE_TIMER_48;
        }

        static constexpr bool IsThisCellAppropriateAndGenericToConsume(const PackedCell64_t::AuthoritiveCellView& a_cell_view, APCPagedNodeSegmentClasses page_class) noexcept
        {

            if (
                !a_cell_view.IsCellValid ||
                a_cell_view.LocalityOfCell != LocalityPolicy::PUBLISHED ||
                page_class != a_cell_view.PageClass ||
                !IsDataConsumablePageClass(a_cell_view.PageClass)
            )
            {
                return false;
            }
            
            return true;
        }


        static constexpr bool IsTrackedOccupancyPageClass(APCPagedNodeSegmentClasses page_class) noexcept
        {
            /*
                Occupancy-tracked means:
                - it has a REGION_OCCUPANCY_* counter cell
                - it can contribute PUBLISHED / CLAIMED / FAULTY to central occupancy

                This includes CONTROL_SLOT because metacells are real packed cells.
                This includes UNDEFINED because it is the quarantine/emergence lane.
                This includes FREE_SLOT only for non-idle abnormal transitions.
                Normal idle free capacity is still derived, not counted.
            */
            return page_class != APCPagedNodeSegmentClasses::NONE &&
                page_class != APCPagedNodeSegmentClasses::NULLNAN;
        }

        static constexpr bool IsDataConsumablePageClass(APCPagedNodeSegmentClasses page_class) noexcept
        {
            /*
                These regions may contain normal user/runtime data.
                CONTROL_SLOT is not data-consumable.
                FREE_SLOT is not data-consumable.
                NONE/NULLNAN are invalid.
            */
            return page_class != APCPagedNodeSegmentClasses::NONE &&
                page_class != APCPagedNodeSegmentClasses::NULLNAN &&
                page_class != APCPagedNodeSegmentClasses::META_HEADER &&
                page_class != APCPagedNodeSegmentClasses::FREE_SLOT;
        }

        static constexpr bool DoseThisCellUpdateableAsOccupancy16x3(
            const PackedCell64_t::AuthoritiveCellView& occupancy_cell_view,
            LocalityPolicy desired_cell_locality = LocalityPolicy::PUBLISHED
        ) noexcept
        {
            if (
                !occupancy_cell_view.IsCellValid || occupancy_cell_view.PageClass != APCPagedNodeSegmentClasses::META_HEADER ||
                occupancy_cell_view.CellMode != PackedMode::MODEL48 ||
                occupancy_cell_view.LocalityOfCell != desired_cell_locality ||
                occupancy_cell_view.SubClassOfModel48 != Model48Subclass::SUBDIVISION16x3_INTERNAL_CELL_MODEL
            )
            {
                return false;
            }
            return true;
        }

};


struct PageNodeOrchestrator
{
    static constexpr uint8_t GetBeginIndexOfLayoutBufferOfAPC() noexcept
    {
        return static_cast<uint8_t>(MetaIndexOfAPCNode::FEEDFORWARD_BOUNDS);
    }

    static constexpr uint8_t GetEndIndexOfLayouyBufferOfAPC() noexcept
    {
        return static_cast<uint8_t>(MetaIndexOfAPCNode::GLOBAL_CURRENT_VERSION);
    }

    static constexpr uint8_t GetLenOfLayoutConstructorInAPCHeader() noexcept
    {
        return GetEndIndexOfLayouyBufferOfAPC() - GetBeginIndexOfLayoutBufferOfAPC() + 1;
    }

    static constexpr bool IsValidLayoutNode(APCPagedNodeSegmentClasses layout_node) noexcept
    {
        if (
            layout_node > APCPagedNodeSegmentClasses::NONE &&
            layout_node < APCPagedNodeSegmentClasses::META_HEADER
        )
        {
            return true;
        }
        return false;
    }

    static constexpr bool IsValidAPCHeaderCell(const PackedCell64_t::AuthoritiveCellView a_auth_view) noexcept
    {
        if (
            !a_auth_view.IsCellValid || 
            a_auth_view.PageClass != APCPagedNodeSegmentClasses::META_HEADER
        )
        {
            return false;
        }

        if (
            a_auth_view.CellMode == PackedMode::VALUE48 || 
            a_auth_view.CellMode == PackedMode::MODEL48
        )
        {
            return true;
        }
        
        return false;
        
    }

};


}
