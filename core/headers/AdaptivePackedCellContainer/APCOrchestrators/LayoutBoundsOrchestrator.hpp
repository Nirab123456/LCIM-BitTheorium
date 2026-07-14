#pragma once 
#include <array>
#include <utility>
#include "../CoreOFAPC/IdAndIdentityOfAPC.hpp"
#include "../../SharedComponents/BitPackers/ConAndCaDependentPacker.hpp"

namespace PredictedAdaptedEncoding
{

struct LayoutBuilderAndValidator 
{
    struct LayoutCarrier
    {
        uint32_t BeginIndex = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t EndIndex = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint8_t Version = UINT8_MAX;
        MacroColumnOfAPC LayoutIdentity = MacroColumnOfAPC::NULLNAN;
        bool IsValid = false;
    };

    static constexpr bool ValidateALayoutCarrier(
        LayoutCarrier& a_layout
    ) noexcept
    {        
        if (
            APCDataStructure::IsThisIndexValidForAPC(a_layout.BeginIndex) &&
            APCDataStructure::IsThisIndexValidForAPC(a_layout.EndIndex) &&
            a_layout.BeginIndex <= a_layout.EndIndex &&
            RegionCursorIndexOrchestrator::IsValidTrackedAPCNode(a_layout.LayoutIdentity)
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

        const std::optional<uint64_t> layout_cell = Double32In64ExPa::PackDoubleUnsigned32In64(a_layout.BeginIndex, a_layout.EndIndex);
        return layout_cell;
    }

    static constexpr LayoutCarrier GetLayoutCarrierFromValidLayoutCell(
        uint64_t packed_cell,
        MacroColumnOfAPC layout_marker
    ) noexcept
    {
        LayoutCarrier return_carrier{};

        if (!APCDataStructure::IsThsisIndexValidForFabric(packed_cell))
        {
            return return_carrier;
        }
        
        const std::optional<uint32_t> begin_idx = Double32In64ExPa::ExtractLow32Of64(packed_cell);
        const std::optional<uint32_t> end_idx = Double32In64ExPa::ExtractHigh32Of64(packed_cell);
        return_carrier.BeginIndex =  begin_idx.has_value() ? begin_idx.value() : APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        return_carrier.EndIndex =  end_idx.has_value() ? end_idx.value() : APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        return_carrier.LayoutIdentity = layout_marker;

        ValidateALayoutCarrier(return_carrier);
        return return_carrier;
    }

    static constexpr std::optional<uint16_t> SpanOflayoutFromPackedCell(
        uint64_t packed_cell,
        MacroColumnOfAPC layout_identity
    ) noexcept
    {
        const LayoutCarrier desired_layout_files = GetLayoutCarrierFromValidLayoutCell(packed_cell, layout_identity);
        if (!desired_layout_files.IsValid)
        {
            return std::nullopt;
        }
        return static_cast<uint16_t>(desired_layout_files.EndIndex - desired_layout_files.BeginIndex);
    }
};


struct LayoutPercentageBuilder : public LayoutBuilderAndValidator
{

    struct LayoutSpanAndPercentageCarrier
    {
        uint16_t FeedForward = FEEDFOEWARD_PERCENTAGE;
        uint16_t FeedBackward = FEEDBACKWARD_PERCENTAGE;
        uint16_t Lateral = LATERAL_PERCENTAGE;
        uint16_t StateSlot = STATESLOT_PERCENTAGE;
        uint16_t ErrorSlot = ERRORSLOT_PERCENTAGE;
        uint16_t Weightless = EDGEDESCRIPTOR_PERCENTAGE;
        uint16_t WeightSlot = WEIGHTSLOT_PERCENTAGE;
        uint16_t AUXSlot = AUXSLOT_PERCENTAGE;
        uint16_t HeterogenousPtr = HETEROGENOUS_RAW_PERCENTAGE;
        uint16_t FreeSlot = FREE_PERCENTAGE;
    };


    static constexpr LayoutSpanAndPercentageCarrier DEFAULT_LAYOUT_WEIGHT{};

    static constexpr std::optional<uint16_t> GetDefaultInitialPercentage(
        MacroColumnOfAPC layout_class,
        const LayoutSpanAndPercentageCarrier& user_defined_percentage = DEFAULT_LAYOUT_WEIGHT
    ) noexcept
    {
        if (!RegionCursorIndexOrchestrator::IsValidTrackedAPCNode(layout_class))
        {
            return std::nullopt;
        }

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
        const uint16_t& payload_span
    ) noexcept
    {
        if (
            payload_span == UNSIGNED_ZERO ||
            !APCDataStructure::IsThisIndexValidForAPC(payload_span)
        )
        {
            return false;
        }

        std::array<uint16_t*, RegionCursorIndexOrchestrator::TrackedAPCNodeLen()> layout_fields_array
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

        uint32_t total_weight = UNSIGNED_ZERO;
        for (size_t i = 0; i < RegionCursorIndexOrchestrator::TrackedAPCNodeLen(); i++)
        {
            total_weight += static_cast<uint32_t>(*layout_fields_array[i]);
        }

        if (total_weight == UNSIGNED_ZERO)
        {
            return false;
        }

        std::array<uint16_t, RegionCursorIndexOrchestrator::TrackedAPCNodeLen()> normalize_counts_array{};
        std::array<uint32_t, RegionCursorIndexOrchestrator::TrackedAPCNodeLen()> reminders_array{};

        uint32_t assigned_count = UNSIGNED_ZERO;
        for (size_t i = 0; i < RegionCursorIndexOrchestrator::TrackedAPCNodeLen(); i++)
        {
            const uint32_t weight = static_cast<uint32_t>(*layout_fields_array[i]);
            const uint32_t scaled = weight * static_cast<uint32_t>(payload_span);

            const uint32_t base_count = scaled / total_weight;
            const uint32_t reminder = scaled % total_weight;

            normalize_counts_array[i] = static_cast<uint16_t>(base_count);
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
            size_t best_idx = RegionCursorIndexOrchestrator::TrackedAPCNodeLen();
            uint32_t best_reminder = UNSIGNED_ZERO;
            for (size_t i = 0; i < RegionCursorIndexOrchestrator::TrackedAPCNodeLen(); i++)
            {
                if (
                    reminders_array[i] > best_reminder ||
                    (
                        reminders_array[i] == best_reminder && 
                        best_idx == RegionCursorIndexOrchestrator::TrackedAPCNodeLen() &&
                        *layout_fields_array[i]
                    )
                )
                {
                    best_reminder = reminders_array[i];
                    best_idx = i;
                }
            }

            if (best_idx == RegionCursorIndexOrchestrator::TrackedAPCNodeLen())
            {
                return false;
            }
            ++normalize_counts_array[best_idx];
            reminders_array[best_idx] = UNSIGNED_ZERO;
            --remaining_cells;
        }
        
        uint32_t final_sum = UNSIGNED_ZERO;

        for (size_t i = 0; i < RegionCursorIndexOrchestrator::TrackedAPCNodeLen(); i++)
        {
            final_sum += normalize_counts_array[i];
        }

        if (final_sum != payload_span)
        {
            return false;
        }
        
        
        for (size_t i = 0; i < RegionCursorIndexOrchestrator::TrackedAPCNodeLen(); i++)
        {
            *layout_fields_array[i] = normalize_counts_array[i];
        }
        return true;
    }


};

struct TrackingBufferConf : public LayoutPercentageBuilder
{
    static constexpr uint8_t LEN_OF_APC_TRACKING_BUFFER = RegionCursorIndexOrchestrator::TrackedAPCNodeLen() + 1;
    static constexpr uint8_t VALIDATION_IDX_OF_TRACKING_BUFFER = LEN_OF_APC_TRACKING_BUFFER - 1;
    using TrackingBufferOfAPC = std::array<uint64_t, LEN_OF_APC_TRACKING_BUFFER>;

    static constexpr void BuildNullTrackingBuffer(TrackingBufferOfAPC& a_layout_buffer) noexcept
    {
        for (size_t i = 0; i < a_layout_buffer.size(); i++)
        {
            a_layout_buffer[i] = FABRIC_CELL_SENTINAL;
        }
    }
protected:
    static constexpr MacroColumnOfAPC GetAPCPagdNodeFromBufferIdx(uint8_t buffer_idx) noexcept
    {
        if (buffer_idx >= VALIDATION_IDX_OF_TRACKING_BUFFER)
        {
            return MacroColumnOfAPC::NULLNAN;
        }

        return static_cast<MacroColumnOfAPC>(
            static_cast<uint8_t>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE) + buffer_idx
        );
    }


    static constexpr std::optional<uint8_t> GetBufferIdxFromMacroColumn(MacroColumnOfAPC column) noexcept
    {
        if (!LayoutHeaderIdentityOrchestrator::IsValidTrackedAPCNode(column))
        {
            return std::nullopt;
        }

        return static_cast<uint8_t>(
            static_cast<uint8_t>(column) - static_cast<uint8_t>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE)
        );
    }
};

struct LayoutBoundsOrchestrator : public TrackingBufferConf
{
    static constexpr uint64_t VALIDATION_LAYOUT_BUFFER_MARK = 11111;

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
        if (buffer_idx >= RegionCursorIndexOrchestrator::TrackedAPCNodeLen())
        {
            return MacroColumnOfAPC::NULLNAN;
        }
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
            !APCDataStructure::IsThsisIndexValidForFabric(layout_cell.value())
        )
        {
            return false;
        }

        layout_buffer[maybe_buffer_idx.value()] = layout_cell.value();
        
        return true;
    }

    static constexpr bool ValidateALayoutBuffer(
        TrackingBufferOfAPC& a_layout_buffer,
        uint16_t capacity_of_the_apc
    ) noexcept
    {
        if (!APCDataStructure::IsCapacityOfAPCValid(capacity_of_the_apc))
        {
            return false;
        }

        const uint16_t payload_begin = static_cast<uint16_t>(APCDataStructure::METACELL_COUNT);
        const uint16_t payload_end = capacity_of_the_apc;
        const uint16_t payload_span = payload_end - payload_begin;

        if (payload_span == UNSIGNED_ZERO)
        {
            return false;
        }

        uint32_t expected_begin = payload_begin;
        uint8_t expected_version = UINT8_MAX;

        for (uint8_t i = 0; i < RegionCursorIndexOrchestrator::TrackedAPCNodeLen(); i++)
        {
            const uint64_t current_packed_cell = a_layout_buffer[i];
            LayoutCarrier current_layout = GetLayoutCarrierFromValidLayoutCell(
                current_packed_cell, 
                GetAPCPagdNodeFromBufferIdx(i)
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

            if (!APCDataStructure::ThisVersionValid(current_layout.Version))
            {
                expected_version = current_layout.Version;
            }
            else if (current_layout.Version != expected_version)
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
            !APCDataStructure::IsThisIndexValidForAPC(expected_version)
        )
        {
            return false;
        }
        
        a_layout_buffer[VALIDATION_IDX_OF_TRACKING_BUFFER] = VALIDATION_LAYOUT_BUFFER_MARK;
        return true;
    }


    static constexpr bool IsLayouBufferValidationMarked(const TrackingBufferOfAPC& a_layout_buffer) noexcept
    {
        if (a_layout_buffer[VALIDATION_IDX_OF_TRACKING_BUFFER] == VALIDATION_LAYOUT_BUFFER_MARK)
        {
            return true;
        }
        return false;
    }

    static constexpr bool BuildInitialLayoutBuffer(
        TrackingBufferOfAPC& return_buffer,
        uint16_t capacity_of_the_apc,
        const LayoutSpanAndPercentageCarrier& provided_layout_weight = DEFAULT_LAYOUT_WEIGHT,
        uint8_t desired_version = APCDataStructure::BRANCH_VERSION
    ) noexcept
    {
        BuildNullTrackingBuffer(return_buffer);
        if (
            capacity_of_the_apc < MINIMUM_APC_CELL_COUNT ||
            !APCDataStructure::IsThisIndexValidForAPC(capacity_of_the_apc) ||
            !APCDataStructure::ThisVersionValid(desired_version)
        )
        {
            return false;
        }

        const uint16_t payload_begin = static_cast<uint16_t>(APCDataStructure::METACELL_COUNT);
        const uint16_t payload_span = capacity_of_the_apc - payload_begin;

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
        for (uint8_t i = 0; i < RegionCursorIndexOrchestrator::TrackedAPCNodeLen(); i++)
        {
            const MacroColumnOfAPC layout_class = GetOriginForLayoutClassByBufferIdx(i);
            const std::optional<uint16_t> maybe_span = GetDefaultInitialPercentage(layout_class, normalized_layout);

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
            current_layout.EndIndex = static_cast<uint16_t>(cursor + maybe_span.value());
            current_layout.Version = desired_version;
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

    static constexpr bool MutateAPCLayout() noexcept;

};
}