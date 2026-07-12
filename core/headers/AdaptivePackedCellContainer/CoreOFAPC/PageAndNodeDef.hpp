#pragma once
#include "IdAndIdentityOfAPC.hpp"

namespace PredictedAdaptedEncoding
{
    struct APCAndPagedNodeHelpers
    {
        static constexpr uint8_t HIGH_FOUR_NIBBLE = 0x0Fu;
        static constexpr uint8_t HIGH_ALL_EIGHT_NIBBLE = 0xFFu;
        static constexpr size_t SIZE_OF_APCPagedNodeRelMaskClasses = 16u;



        static constexpr HeaderIdentifierOfAPC GetOccupancyMetIndexByRegionClass(
            MacroColumnOfAPC desired_region_class
        )noexcept
        {
            return static_cast<HeaderIdentifierOfAPC>(
                static_cast<size_t>(HeaderIdentifierOfAPC::FEEDBACKWARD_OCC) +
                (static_cast<uint8_t>(desired_region_class) & HIGH_FOUR_NIBBLE)
                );
        }

        static constexpr bool IsTrackedOccupancyPageClass(MacroColumnOfAPC page_class) noexcept
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
            return page_class != MacroColumnOfAPC::NONE &&
                page_class != MacroColumnOfAPC::NULLNAN;
        }

        static constexpr bool IsDataConsumablePageClass(MacroColumnOfAPC page_class) noexcept
        {
            /*
                These regions may contain normal user/runtime data.
                CONTROL_SLOT is not data-consumable.
                FREE_SLOT is not data-consumable.
                NONE/NULLNAN are invalid.
            */
            return page_class != MacroColumnOfAPC::NONE &&
                page_class != MacroColumnOfAPC::NULLNAN &&
                page_class != MacroColumnOfAPC::META_HEADER &&
                page_class != MacroColumnOfAPC::FREE_SLOT;
        }

};

struct MetaIdxOrchestrator
{
    static constexpr bool IsValidTrackedAPCNode(MacroColumnOfAPC layout_node) noexcept
    {
        if (
            layout_node > MacroColumnOfAPC::NONE &&
            layout_node < MacroColumnOfAPC::META_HEADER
        )
        {
            return true;
        }
        return false;
    }

    static constexpr uint8_t LayoutBufferBegainInMetaIndecies() noexcept
    {
        return static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_BOUNDS);
    }

    static constexpr uint8_t LayoutBufferEndInMetaIndecies() noexcept
    {
        return static_cast<uint8_t>(HeaderIdentifierOfAPC::FREE_BOUNDS);
    }

    static constexpr uint8_t OccupencyBufferBegainInMetaIndecies() noexcept
    {
        return static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_OCC);
    }

    static constexpr uint8_t OccupancyBufferEndInMetaIndecies() noexcept
    {
        return static_cast<uint8_t>(HeaderIdentifierOfAPC::FREE_OCC);
    }

    static constexpr std::optional<HeaderIdentifierOfAPC>OccupancyMetaIdxFromNodeClass(MacroColumnOfAPC node_class) noexcept
    {
        if (!IsValidTrackedAPCNode(node_class))
        {
            return std::nullopt;
        }
        
        return static_cast<HeaderIdentifierOfAPC>(
            static_cast<size_t>(HeaderIdentifierOfAPC::FEEDFORWARD_OCC) +
            (static_cast<uint8_t>(node_class) - static_cast<uint8_t>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE))
            );
    }

    static constexpr std::optional<MacroColumnOfAPC>GetNodeClassForOccupancyMetaIdx(HeaderIdentifierOfAPC meta_index) noexcept
    {
        const uint8_t meta_index_u = static_cast<uint8_t>(meta_index);
        if (
            meta_index_u >= OccupencyBufferBegainInMetaIndecies() &&
            meta_index_u <= OccupancyBufferEndInMetaIndecies()
        )
        {
            return static_cast<MacroColumnOfAPC>(
                meta_index_u - OccupencyBufferBegainInMetaIndecies() + static_cast<uint8_t>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE)
            );
        }

        return std::nullopt;
    }

};



struct PageNodeOrchestrator : public MetaIdxOrchestrator
{

    static constexpr uint8_t TrackedAPCNodeLen() noexcept
    {
        return static_cast<uint8_t>(MacroColumnOfAPC::META_HEADER) - static_cast<uint8_t>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE);
    }


    static constexpr bool IsThisCellAPCMetaHeader(MacroColumnOfAPC page_class) noexcept
    {
        if (page_class == MacroColumnOfAPC::META_HEADER)
        {
            return true;
        }
        
        return false;
    }

};


}
