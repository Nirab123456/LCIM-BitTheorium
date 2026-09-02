#pragma once 
#include <array>
#include <utility>
#include "../../SharedComponents/BitPackers/ConAndCaDependentPacker.hpp"

namespace BidirectionalInMemGraph
{

struct LayoutBuilderAndValidator 
{
    struct LayoutCarrier
    {
        uint32_t BeginIndex = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t EndIndex = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        MacroColumnOfAPC LayoutIdentity{};
        bool IsValid = false;
    };

    static constexpr bool ValidateALayoutCarrier(
        LayoutCarrier& a_layout
    ) noexcept
    {        
        if (
            APCDataStructure::IsValid32BitAPCUnit(a_layout.BeginIndex) &&
            APCDataStructure::IsValid32BitAPCUnit(a_layout.EndIndex) &&
            a_layout.BeginIndex <= a_layout.EndIndex
        )
        {   a_layout.IsValid = true;
            return true;
        }
        a_layout.IsValid = false;
        return false;
    }

    static constexpr std::optional<uint64_t> CreateALayoutBoundsCell(
        LayoutCarrier& a_layout
    ) noexcept
    {
        if (!ValidateALayoutCarrier(a_layout))
        {
            return FABRIC_CELL_SENTINAL;
        }

        const std::optional<uint64_t> layout_cell = TwinU32ToU64::PackDoubleUnsigned32In64(a_layout.BeginIndex, a_layout.EndIndex);
        return layout_cell;
    }

    static constexpr LayoutCarrier GetLayoutCarrierFromValidLayoutCell(
        uint64_t fabric_unit,
        MacroColumnOfAPC layout_marker
    ) noexcept
    {
        LayoutCarrier return_carrier{};

        if (!APCDataStructure::IsValidFabricUnit(fabric_unit))
        {
            return return_carrier;
        }
        
        return_carrier.BeginIndex = TwinU32ToU64::ExtractLow32Of64(fabric_unit);
        return_carrier.EndIndex = TwinU32ToU64::ExtractHigh32Of64(fabric_unit);
        return_carrier.LayoutIdentity = layout_marker;
        ValidateALayoutCarrier(return_carrier);
        return return_carrier;
    }

    static constexpr std::optional<uint32_t> SpanOflayoutFromPackedCell(
        uint64_t fabric_unit,
        MacroColumnOfAPC layout_identity
    ) noexcept
    {
        const LayoutCarrier desired_layout_files = GetLayoutCarrierFromValidLayoutCell(fabric_unit, layout_identity);
        if (!desired_layout_files.IsValid)
        {
            return std::nullopt;
        }
        return static_cast<uint32_t>(desired_layout_files.EndIndex - desired_layout_files.BeginIndex);
    }
};


struct LayoutPercentageBuilder : public LayoutBuilderAndValidator
{

    struct LayoutSpanAndPercentageCarrier
    {
        uint32_t FeedForward = 1u;
        uint32_t FeedBackward = 1u;
        uint32_t Lateral = UNSIGNED_ZERO;
        uint32_t StateSlot = UNSIGNED_ZERO;
        uint32_t ErrorSlot = 1u;
        uint32_t Weightless = UNSIGNED_ZERO;
        uint32_t WeightSlot = UNSIGNED_ZERO;
        uint32_t AUXSlot = UNSIGNED_ZERO;
        uint32_t HeterogenousPtr = UNSIGNED_ZERO;
        uint32_t FreeSlot = UNSIGNED_ZERO;
    };

    static const LayoutSpanAndPercentageCarrier  DEFAULT_REGION_SPAN;

    static constexpr std::optional<uint32_t> GetDefaultInitialPercentage(
        MacroColumnOfAPC layout_class,
        const LayoutSpanAndPercentageCarrier& user_defined_percentage
    ) noexcept
    {
        switch (layout_class)
        {
        case MacroColumnOfAPC::FEEDFORWARD_MESSAGE:
            return user_defined_percentage.FeedForward;
        case MacroColumnOfAPC::FEEDBACKWARD_MESSAGE:
            return user_defined_percentage.FeedBackward;
        case MacroColumnOfAPC::LATERAL_MESAGE:
            return user_defined_percentage.Lateral;
        case MacroColumnOfAPC::STATE_SLOT:
            return user_defined_percentage.StateSlot;
        case MacroColumnOfAPC::ERROR_SLOT:
            return user_defined_percentage.ErrorSlot;
        case MacroColumnOfAPC::WEIGHTLESS_LOOKUP:
            return user_defined_percentage.Weightless;
        case MacroColumnOfAPC::WEIGHT_SLOT:
            return user_defined_percentage.WeightSlot;
        case MacroColumnOfAPC::AUX_SLOT:
            return user_defined_percentage.AUXSlot;
        case MacroColumnOfAPC::HETEROGENOUS_PTR:
            return user_defined_percentage.HeterogenousPtr;
        case MacroColumnOfAPC::FREE_SLOT:
            return user_defined_percentage.FreeSlot;
        default:
            return std::nullopt;
        }
        
    }

    static constexpr bool NormalizeLayoutPercentageToPayloadSpan(
        LayoutSpanAndPercentageCarrier& user_defined_percentage,
        const uint32_t& payload_span
    ) noexcept
    {
        if (
            payload_span == UNSIGNED_ZERO ||
            !APCDataStructure::IsValid32BitAPCUnit(payload_span)
        )
        {
            return false;
        }

        std::array<uint32_t*, ColumnConf::CountOfMacroColumn()> layout_fields_array
        {
            &user_defined_percentage.FeedForward,
            &user_defined_percentage.FeedBackward,
            &user_defined_percentage.Lateral,
            &user_defined_percentage.StateSlot,
            &user_defined_percentage.ErrorSlot,
            &user_defined_percentage.Weightless,
            &user_defined_percentage.WeightSlot,
            &user_defined_percentage.AUXSlot,
            &user_defined_percentage.HeterogenousPtr,
            &user_defined_percentage.FreeSlot
        };

        uint64_t total_weight = UNSIGNED_ZERO;
        for (size_t i = 0; i < ColumnConf::CountOfMacroColumn(); i++)
        {
            total_weight += static_cast<uint32_t>(*layout_fields_array[i]);
        }

        if (total_weight == UNSIGNED_ZERO)
        {
            return false;
        }

        std::array<uint32_t, ColumnConf::CountOfMacroColumn()> normalize_counts_array{};
        std::array<uint32_t, ColumnConf::CountOfMacroColumn()> reminders_array{};

        uint32_t assigned_count = UNSIGNED_ZERO;
        for (size_t i = 0; i < ColumnConf::CountOfMacroColumn(); i++)
        {
            const uint32_t weight = static_cast<uint32_t>(*layout_fields_array[i]);
            const uint64_t scaled = weight * payload_span;

            const uint32_t base_count = static_cast<uint32_t>(scaled / total_weight);
            const uint32_t reminder = static_cast<uint32_t>(scaled % total_weight);

            normalize_counts_array[i] = static_cast<uint32_t>(base_count);
            reminders_array[i] = reminder;
            assigned_count += base_count;
        }

        if (assigned_count > payload_span)
        {
            return false;
        }

        uint32_t remaining_cells  = static_cast<uint32_t>(payload_span) - assigned_count;

        while (remaining_cells > UNSIGNED_ZERO)
        {
            size_t best_idx = ColumnConf::CountOfMacroColumn();
            uint32_t best_reminder = UNSIGNED_ZERO;
            for (size_t i = 0; i < ColumnConf::CountOfMacroColumn(); i++)
            {
                if (
                    reminders_array[i] > best_reminder ||
                    (
                        reminders_array[i] == best_reminder && 
                        best_idx == ColumnConf::CountOfMacroColumn() &&
                        *layout_fields_array[i]
                    )
                )
                {
                    best_reminder = reminders_array[i];
                    best_idx = i;
                }
            }

            if (best_idx == ColumnConf::CountOfMacroColumn())
            {
                return false;
            }
            ++normalize_counts_array[best_idx];
            reminders_array[best_idx] = UNSIGNED_ZERO;
            --remaining_cells;
        }
        
        uint32_t final_sum = UNSIGNED_ZERO;

        for (size_t i = 0; i < ColumnConf::CountOfMacroColumn(); i++)
        {
            final_sum += normalize_counts_array[i];
        }

        if (final_sum != payload_span)
        {
            return false;
        }
        
        
        for (size_t i = 0; i < ColumnConf::CountOfMacroColumn(); i++)
        {
            *layout_fields_array[i] = normalize_counts_array[i];
        }
        return true;
    }


};

inline constexpr LayoutPercentageBuilder::LayoutSpanAndPercentageCarrier LayoutPercentageBuilder::DEFAULT_REGION_SPAN{};

struct BufferConfForTracking : public LayoutPercentageBuilder
{
    static constexpr uint8_t LEN_OF_APC_TRACKING_BUFFER = ColumnConf::CountOfMacroColumn();
    using TrackingBufferOfAPC = std::array<uint64_t, LEN_OF_APC_TRACKING_BUFFER>;


    static constexpr void BuildNullTrackingBuffer(TrackingBufferOfAPC& a_layout_buffer) noexcept
    {
        for (size_t i = 0; i < a_layout_buffer.size(); i++)
        {
            a_layout_buffer[i] = FABRIC_CELL_SENTINAL;
        }
    }
protected:
    static constexpr MacroColumnOfAPC GetMacroColumnFromBufferIdx(uint8_t buffer_idx) noexcept
    {
        return static_cast<MacroColumnOfAPC>(
            static_cast<uint8_t>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE) + buffer_idx
        );
    }


    static constexpr std::optional<uint8_t> GetBufferIdxFromMacroColumn(MacroColumnOfAPC column) noexcept
    {
        return static_cast<uint8_t>(
            static_cast<uint8_t>(column) - static_cast<uint8_t>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE)
        );
    }
};

struct LayoutBoundsOrchestrator : public BufferConfForTracking
{
    static constexpr std::optional<uint8_t> GetBufferIndexForALayout(LayoutCarrier& a_valid_layout) noexcept
    {
        if (!ValidateALayoutCarrier(a_valid_layout))
        {
            return std::nullopt;
        }

        const uint8_t start_idx = static_cast<uint8_t>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE);
        return static_cast<uint8_t>(static_cast<uint8_t>(a_valid_layout.LayoutIdentity) - start_idx);
    }

    static constexpr MacroColumnOfAPC GetOriginForLayoutClassByBufferIdx(uint8_t buffer_idx) noexcept
    {
        return static_cast<MacroColumnOfAPC>(
            static_cast<uint8_t>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE) +
            buffer_idx
        );
    }


    static constexpr bool InsertALayoutCellInBuffer(
        TrackingBufferOfAPC& layout_buffer,
        LayoutCarrier& a_valid_layout_carrier
    ) noexcept
    {
        std::optional<uint8_t> maybe_buffer_idx = GetBufferIndexForALayout(a_valid_layout_carrier);
        if (!maybe_buffer_idx.has_value())
        {
            return false;
        }

        const std::optional<uint64_t> layout_cell = CreateALayoutBoundsCell(a_valid_layout_carrier);

        if (
            !layout_cell.has_value() ||
            !APCDataStructure::IsValidFabricUnit(layout_cell.value())
        )
        {
            return false;
        }

        layout_buffer[maybe_buffer_idx.value()] = layout_cell.value();
        
        return true;
    }

    static constexpr bool ValidateALayoutBuffer(
        TrackingBufferOfAPC& a_layout_buffer,
        uint32_t capacity_of_the_apc
    ) noexcept
    {
        if (!APCDataStructure::IsCapacityOfAPCValid(capacity_of_the_apc))
        {
            return false;
        }

        const uint32_t payload_begin = static_cast<uint32_t>(APCDataStructure::METACELL_COUNT);
        const uint32_t payload_end = capacity_of_the_apc;
        const uint32_t payload_span = payload_end - payload_begin;

        if (payload_span == UNSIGNED_ZERO)
        {
            return false;
        }

        uint32_t expected_begin = payload_begin;
        for (uint8_t i = 0; i < ColumnConf::CountOfMacroColumn(); i++)
        {
            const uint64_t current_packed_cell = a_layout_buffer[i];
            LayoutCarrier current_layout = GetLayoutCarrierFromValidLayoutCell(
                current_packed_cell, 
                GetMacroColumnFromBufferIdx(i)
            );
            if (!current_layout.IsValid)
            {
                return false;
            }

            const std::optional<uint8_t> maybe_current_idx = GetBufferIndexForALayout(current_layout);
            if (!maybe_current_idx.has_value() || maybe_current_idx.value() != i)
            {
                return false;
            }

            if (
                current_layout.BeginIndex != expected_begin ||
                current_layout.EndIndex < current_layout.BeginIndex ||
                current_layout.EndIndex > payload_end
            )
            {
                return false;
            }
            
            expected_begin = current_layout.EndIndex;
        }

        if (
            expected_begin != payload_end ||
            !APCDataStructure::IsValid32BitAPCUnit(expected_begin)
        )
        {
            return false;
        }        
        return true;
    }

    static constexpr bool BuildInitialLayoutBuffer(
        TrackingBufferOfAPC& return_buffer,
        uint32_t capacity_of_the_apc,
        const LayoutSpanAndPercentageCarrier& provided_layout_weight = DEFAULT_REGION_SPAN
    ) noexcept
    {
        BuildNullTrackingBuffer(return_buffer);
        if (
            capacity_of_the_apc < MINIMUM_APC_CELL_COUNT ||
            !APCDataStructure::IsValid32BitAPCUnit(capacity_of_the_apc)
        )
        {
            return false;
        }

        const uint32_t payload_begin = static_cast<uint32_t>(APCDataStructure::METACELL_COUNT);
        const uint32_t payload_span = capacity_of_the_apc - payload_begin;

        if (capacity_of_the_apc == UNSIGNED_ZERO)
        {
            return false;
        }

        LayoutSpanAndPercentageCarrier normalized_layout = provided_layout_weight;
        if (!NormalizeLayoutPercentageToPayloadSpan(normalized_layout, payload_span))
        {
            BuildNullTrackingBuffer(return_buffer);
            return false;
        }

        uint32_t cursor = payload_begin;
        for (uint8_t i = 0; i < ColumnConf::CountOfMacroColumn(); i++)
        {
            const MacroColumnOfAPC layout_class = GetOriginForLayoutClassByBufferIdx(i);
            const std::optional<uint32_t> maybe_span = GetDefaultInitialPercentage(layout_class, normalized_layout);

            if (
                !maybe_span.has_value() ||
                maybe_span.value() > (capacity_of_the_apc - cursor)
            )
            {
                BuildNullTrackingBuffer(return_buffer);
                return false;
            }

            LayoutCarrier current_layout{};
            current_layout.BeginIndex = cursor;
            current_layout.EndIndex = static_cast<uint32_t>(cursor + maybe_span.value());
            current_layout.LayoutIdentity = layout_class;

            if (!InsertALayoutCellInBuffer(return_buffer, current_layout))
            {
                BuildNullTrackingBuffer(return_buffer);
                return false;
            }
            cursor = current_layout.EndIndex;
        }

        return ValidateALayoutBuffer(return_buffer, capacity_of_the_apc);
    }
};

}