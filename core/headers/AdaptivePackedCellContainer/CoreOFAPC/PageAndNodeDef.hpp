#pragma once
#include "IdAndIdentityOfAPC.hpp"

namespace PredictedAdaptedEncoding
{
    struct APCAndPagedNodeHelpers
    {
        static constexpr uint8_t HIGH_FOUR_NIBBLE = 0x0Fu;
        static constexpr uint8_t HIGH_ALL_EIGHT_NIBBLE = 0xFFu;
        static constexpr size_t SIZE_OF_APCPagedNodeRelMaskClasses = 16u;


        static constexpr bool IsTrackedOccupancyPageClass(MacroColumnOfAPC page_class) noexcept
        {
            return page_class != MacroColumnOfAPC::NONE &&
                page_class != MacroColumnOfAPC::NULLNAN;
        }

        static constexpr bool IsDataConsumablePageClass(MacroColumnOfAPC page_class) noexcept
        {
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
