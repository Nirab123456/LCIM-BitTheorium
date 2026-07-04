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

        static constexpr MetaIndexOfAPCNode GetOccupancyMetIndexByRegionClass(
            APCPagedNodeSegmentClasses desired_region_class
        )noexcept
        {
            return static_cast<MetaIndexOfAPCNode>(
                static_cast<size_t>(MetaIndexOfAPCNode::FEEDBACKWARD_OCC) +
                (static_cast<uint8_t>(desired_region_class) & HIGH_FOUR_NIBBLE)
                );
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

struct MetaIdxOrchestrator
{
    static constexpr uint8_t LayoutBufferBegainInMetaIndecies() noexcept
    {
        return static_cast<uint8_t>(MetaIndexOfAPCNode::FEEDFORWARD_BOUNDS_VERSION);
    }

    static constexpr uint8_t LayoutBufferEndInMetaIndecies() noexcept
    {
        return static_cast<uint8_t>(MetaIndexOfAPCNode::UNDEFINED_BOUNDS_VERSION);
    }

    static constexpr uint8_t OccupencyBufferBegainInMetaIndecies() noexcept
    {
        return static_cast<uint8_t>(MetaIndexOfAPCNode::FEEDFORWARD_OCC);
    }

    static constexpr uint8_t OccupancyBufferEndInMetaIndecies() noexcept
    {
        return static_cast<uint8_t>(MetaIndexOfAPCNode::UNDEFINED_OCC);
    }

    static constexpr std::optional<MetaIndexOfAPCNode>OccupancyMetaIdxFromNodeClass(APCPagedNodeSegmentClasses node_class) noexcept
    {
        if (!PageNodeOrchestrator::IsValidTrackedAPCNode(node_class))
        {
            return std::nullopt;
        }
        
        return static_cast<MetaIndexOfAPCNode>(
            static_cast<size_t>(MetaIndexOfAPCNode::FEEDBACKWARD_OCC) +
            (static_cast<uint8_t>(node_class) - static_cast<uint8_t>(APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE))
            );
    }

    static constexpr std::optional<APCPagedNodeSegmentClasses>GetNodeClassForOccupancyMetaIdx(MetaIndexOfAPCNode meta_index) noexcept
    {
        const uint8_t meta_index_u = static_cast<uint8_t>(meta_index);
        if (
            meta_index_u >= OccupencyBufferBegainInMetaIndecies() &&
            meta_index_u <= OccupancyBufferEndInMetaIndecies()
        )
        {
            return static_cast<APCPagedNodeSegmentClasses>(
                meta_index_u - OccupencyBufferBegainInMetaIndecies() + static_cast<uint8_t>(APCPagedNodeSegmentClasses::FEEDBACKWARD_MESSAGE)
            );
        }
    }

};



struct PageNodeOrchestrator : public MetaIdxOrchestrator
{

    static constexpr uint8_t TrackedAPCNodeLen() noexcept
    {
        return static_cast<uint8_t>(APCPagedNodeSegmentClasses::META_HEADER) - static_cast<uint8_t>(APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE);
    }

    static constexpr bool IsValidTrackedAPCNode(APCPagedNodeSegmentClasses layout_node) noexcept
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


    static constexpr bool IsThisCellAPCMetaHeader(APCPagedNodeSegmentClasses page_class) noexcept
    {
        if (page_class == APCPagedNodeSegmentClasses::META_HEADER)
        {
            return true;
        }
        
        return false;
    }

    static constexpr bool IsValidAPCHeaderCell(const PackedCell64_t::AuthoritiveCellView a_auth_view) noexcept
    {
        if (
            !a_auth_view.IsCellValid || 
            !IsThisCellAPCMetaHeader(a_auth_view.PageClass)
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

struct TrackingBufferConf
{
    static constexpr uint8_t LEN_OF_APC_TRACKING_BUFFER = PageNodeOrchestrator::TrackedAPCNodeLen() + 1;
    static constexpr uint8_t VALIDATION_IDX_OF_TRACKING_BUFFER = LEN_OF_APC_TRACKING_BUFFER - 1;
    using TrackingBufferOfAPC = std::array<packed64_t, LEN_OF_APC_TRACKING_BUFFER>;

    static constexpr void BuildNullTrackingBuffer(TrackingBufferOfAPC& a_layout_buffer) noexcept
    {
        for (size_t i = 0; i < a_layout_buffer.size(); i++)
        {
            a_layout_buffer[i] = PackedCell64_t::PACKED_CELL_SENTINAL;
        }
    }
};


}
