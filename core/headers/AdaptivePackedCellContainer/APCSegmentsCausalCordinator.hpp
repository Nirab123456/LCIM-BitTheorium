#pragma once
#include <array>
#include <utility>
#include "SegmentIODefinition.hpp"

namespace PredictedAdaptedEncoding
{
class APCSegmentsCausalCordinator : public SegmentIODefinition
{
private:

     static MetaIndexOfAPCNode AcceptedClockIndex_(APCPagedNodeSegmentClasses region) noexcept
     {
         switch (region)
         {
         case APCPagedNodeSegmentClasses::FEEDBACKWARD_MESSAGE:
             return MetaIndexOfAPCNode::LAST_ACCEPTED_FEED_BACKWARD_CLOCK16;
    

         case APCPagedNodeSegmentClasses::STATE_SLOT:
         case APCPagedNodeSegmentClasses::ERROR_SLOT:
         case APCPagedNodeSegmentClasses::AUX_SLOT:
         case APCPagedNodeSegmentClasses::LATERAL_MESAGE:
         case APCPagedNodeSegmentClasses::EDGE_DESCRIPTOR:
         case APCPagedNodeSegmentClasses::WEIGHT_SLOT:
         case APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE:
         default:
             return MetaIndexOfAPCNode::LAST_ACCEPTED_FEED_FORWARD_CLOCK16;
         }
     }

    static MetaIndexOfAPCNode EmittedClockIndex_(APCPagedNodeSegmentClasses region) noexcept
    {
        return region == APCPagedNodeSegmentClasses::FEEDBACKWARD_MESSAGE
            ? MetaIndexOfAPCNode::LAST_EMITTED_FEED_BACKWARD_CLOCK16
            : MetaIndexOfAPCNode::LAST_EMITTED_FEED_FORWARD_CLOCK16;
    }

    bool TryAdvanceClock_(MetaIndexOfAPCNode idx, clk16_t candidate) noexcept
    {

        while (true)
        {
            const uint64_t current32 = ReadValuFromAPCMetaIndecies(idx);
            const clk16_t current = static_cast<clk16_t>(current32);

            if (current32 != UNSIGNED_ZERO &&
                !APCAndPagedNodeHelpers::INewerClock16(candidate, current))
            {
                return false;
            }

            if (ReplaceOnlyMetaCellValue(idx, current32, static_cast<uint32_t>(candidate), false))
            {
                return true;
            }
        }
    }

public:
    APCSegmentsCausalCordinator() noexcept = default;
    ~APCSegmentsCausalCordinator() = default;

    bool AcceptCausalCell(APCPagedNodeSegmentClasses region, packed64_t cell) noexcept
    {
        const clk16_t incoming = PackedCell64_t::ExtractClk16(cell);
        return TryAdvanceClock_(AcceptedClockIndex_(region), incoming);
    }

    bool MarkEmittedCausalCell(APCPagedNodeSegmentClasses region, packed64_t cell) noexcept
    {
        const clk16_t emitted = PackedCell64_t::ExtractClk16(cell);
        return TryAdvanceClock_(EmittedClockIndex_(region), emitted);
    }

    bool PublishCausal(
        AdaptivePackedCellContainer& apc,
        APCPagedNodeSegmentClasses region,
        packed64_t cell,
        std::atomic<uint64_t>* growth_counter = nullptr
    ) noexcept;

    std::optional<packed64_t> ConsumeCausal(
        AdaptivePackedCellContainer& apc,
        APCPagedNodeSegmentClasses region,
        size_t& scan_cursor,
        std::atomic<uint64_t>* older_counter = nullptr,
        bool drop_older = false
    ) noexcept;

};
}