#pragma once 
#include <array>
#include <utility>
#include "../CoreOFAPC/PageAndNodeDef.hpp"

namespace PredictedAdaptedEncoding
{

struct LayoutBuilderAndValidator : public TrackingBufferConf
{
    struct LayoutCarrier
    {
        uint16_t BeginIndex = APCDataStructure::APC_INDEX_SENTINAL;
        uint16_t EndIndex = APCDataStructure::APC_INDEX_SENTINAL;
        uint8_t Version = UINT8_MAX;
        APCPagedNodeSegmentClasses LayoutIdentity = APCPagedNodeSegmentClasses::NULLNAN;
        LocalityPolicy LocalityOfLayout = LocalityPolicy::UNASSIGNED_UNUSED_NANNULL;
        bool IsValid = false;
    };
    static_assert(sizeof(LayoutCarrier) <= sizeof(packed64_t));

    static constexpr bool ValidateALayoutCarrier(
        LayoutCarrier& a_layout,
        bool is_claimed_valid = true
    ) noexcept
    {
        if (a_layout.LocalityOfLayout == LocalityPolicy::UNASSIGNED_UNUSED_NANNULL)
        {
            return false;
        }
        if (!is_claimed_valid && a_layout.LocalityOfLayout == LocalityPolicy::CLAIMED)
        {
            return false;
        }
        
        if (
            APCDataStructure::IsThisIndexValidForAPC(a_layout.BeginIndex) &&
            APCDataStructure::IsThisIndexValidForAPC(a_layout.EndIndex) &&
            APCDataStructure::ThisVersionValid(a_layout.Version) &&
            a_layout.BeginIndex <= a_layout.EndIndex &&
            PageNodeOrchestrator::IsValidTrackedAPCNode(a_layout.LayoutIdentity)
        )
        {   a_layout.IsValid = true;
            return true;
        }
        a_layout.IsValid = false;
        return false;
    }

    static constexpr packed64_t CreateALayoutBoundsCell(
        LayoutCarrier& a_layout
    ) noexcept
    {
        if (!ValidateALayoutCarrier(a_layout))
        {
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }

        const uint64_t raw48_layout = Subdivision2x16Plus2x8InternalMode48CellModel::Pack2x16Plus2x8UnsignedSubdivision_(
            a_layout.BeginIndex,
            a_layout.EndIndex,
            a_layout.Version,
            static_cast<uint8_t>(a_layout.LayoutIdentity)
        );

        return PackedCell64_t::MakeModeledAPCValidPackedCell(
            ModelFamily::MODEL48,
            static_cast<tag8_t>(Model48Subclass::FOUR_SUBDIVISION_2x16_AND_2x8),
            APCPagedNodeSegmentClasses::META_HEADER,
            a_layout.LocalityOfLayout,
            InternalDataTypePolicy ::UnsignedPCellDataType,
            AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
            raw48_layout
        );

    }

    static constexpr LayoutCarrier GetLayoutCarrierFromValidLayoutCell(
        packed64_t packed_cell,
        bool is_claimed_valid = true
    ) noexcept
    {
        LayoutCarrier return_carrier{};
        const PackedCell64_t::AuthoritiveCellView desired_auth_view = PackedCell64_t::GetAuthoritiveViewsForACell(packed_cell);
        
        if (
            !PageNodeOrchestrator::IsValidAPCHeaderCell(desired_auth_view) ||
            desired_auth_view.SubClassOfModel48 != Model48Subclass::FOUR_SUBDIVISION_2x16_AND_2x8
        )
        {
            return return_carrier;
        }
        
        return_carrier.BeginIndex = Subdivision2x16Plus2x8InternalMode48CellModel::ExtractLowestFirstLow16Bit0_(desired_auth_view.Raw48BitInCellData);
        return_carrier.EndIndex = Subdivision2x16Plus2x8InternalMode48CellModel::ExtractSecondLow16Bit1_(desired_auth_view.Raw48BitInCellData);
        return_carrier.Version = Subdivision2x16Plus2x8InternalMode48CellModel::ExtractHigh8Bit2_(desired_auth_view.Raw48BitInCellData);
        return_carrier.LayoutIdentity = static_cast<APCPagedNodeSegmentClasses>(Subdivision2x16Plus2x8InternalMode48CellModel::ExtractHighestHigh8Bit3_(desired_auth_view.Raw48BitInCellData));
        return_carrier.LocalityOfLayout = desired_auth_view.LocalityOfCell;

        ValidateALayoutCarrier(return_carrier, is_claimed_valid);
        return return_carrier;
    }

    static constexpr std::optional<uint16_t> SpanOflayoutFromPackedCell(
        packed64_t packed_cell,
        bool caller_holds_claim_guard = false
    ) noexcept
    {
        const LayoutCarrier desired_layout_files = GetLayoutCarrierFromValidLayoutCell(packed_cell, caller_holds_claim_guard);
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
        uint16_t EdgeDescriptor = EDGEDESCRIPTOR_PERCENTAGE;
        uint16_t WeightSlot = WEIGHTSLOT_PERCENTAGE;
        uint16_t AUXSlot = AUXSLOT_PERCENTAGE;
        uint16_t HeterogenousSlot = HETEROGENOUS_RAW_PERCENTAGE;
        uint16_t Raw64BitSlot = RAW64_BIT_PERCENTAGE;
        uint16_t PairedPtr = PAIRED_POINTER_PERCENTAGE;
        uint16_t FreeSlot = FREE_PERCENTAGE;
        uint16_t UndefinedSlot = UNDEFINED_PERCENTAGE;
    };


    static constexpr LayoutSpanAndPercentageCarrier DEFAULT_LAYOUT_WEIGHT{};

    static constexpr std::optional<uint16_t> GetDefaultInitialPercentage(
        APCPagedNodeSegmentClasses layout_class,
        const LayoutSpanAndPercentageCarrier& user_defined_percentage = DEFAULT_LAYOUT_WEIGHT
    ) noexcept
    {
        if (!PageNodeOrchestrator::IsValidTrackedAPCNode(layout_class))
        {
            return std::nullopt;
        }

        switch (layout_class)
        {
        case APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE:
            return user_defined_percentage.FeedForward;
        case APCPagedNodeSegmentClasses::FEEDBACKWARD_MESSAGE:
            return user_defined_percentage.FeedBackward;
        case APCPagedNodeSegmentClasses::LATERAL_MESAGE:
            return user_defined_percentage.Lateral;
        case APCPagedNodeSegmentClasses::STATE_SLOT:
            return user_defined_percentage.StateSlot;
        case APCPagedNodeSegmentClasses::ERROR_SLOT:
            return user_defined_percentage.ErrorSlot;
        case APCPagedNodeSegmentClasses::EDGE_DESCRIPTOR:
            return user_defined_percentage.EdgeDescriptor;
        case APCPagedNodeSegmentClasses::WEIGHT_SLOT:
            return user_defined_percentage.WeightSlot;
        case APCPagedNodeSegmentClasses::AUX_SLOT:
            return user_defined_percentage.AUXSlot;
        case APCPagedNodeSegmentClasses::HETEROGENOUS_RAW_MEMORY:
            return user_defined_percentage.HeterogenousSlot;
        case APCPagedNodeSegmentClasses::RAW_64BIT_MEMORY:
            return user_defined_percentage.Raw64BitSlot;
        case APCPagedNodeSegmentClasses::PAIRED_POINTER_IN_MEMORY:
            return user_defined_percentage.PairedPtr;
        case APCPagedNodeSegmentClasses::FREE_SLOT:
            return user_defined_percentage.FreeSlot;
        case APCPagedNodeSegmentClasses::UNDEFINED:
            return user_defined_percentage.UndefinedSlot;
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

        std::array<uint16_t*, PageNodeOrchestrator::TrackedAPCNodeLen()> layout_fields_array
        {
            &user_defined_percentage.FeedForward,
            &user_defined_percentage.FeedBackward,
            &user_defined_percentage.Lateral,
            &user_defined_percentage.StateSlot,
            &user_defined_percentage.ErrorSlot,
            &user_defined_percentage.EdgeDescriptor,
            &user_defined_percentage.WeightSlot,
            &user_defined_percentage.AUXSlot,
            &user_defined_percentage.HeterogenousSlot,
            &user_defined_percentage.Raw64BitSlot,
            &user_defined_percentage.PairedPtr,
            &user_defined_percentage.FreeSlot,
            &user_defined_percentage.UndefinedSlot
        };

        uint32_t total_weight = UNSIGNED_ZERO;
        for (size_t i = 0; i < PageNodeOrchestrator::TrackedAPCNodeLen(); i++)
        {
            total_weight += static_cast<uint32_t>(*layout_fields_array[i]);
        }

        if (total_weight == UNSIGNED_ZERO)
        {
            return false;
        }

        std::array<uint16_t, PageNodeOrchestrator::TrackedAPCNodeLen()> normalize_counts_array{};
        std::array<uint32_t, PageNodeOrchestrator::TrackedAPCNodeLen()> reminders_array{};

        uint32_t assigned_count = UNSIGNED_ZERO;
        for (size_t i = 0; i < PageNodeOrchestrator::TrackedAPCNodeLen(); i++)
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
            size_t best_idx = PageNodeOrchestrator::TrackedAPCNodeLen();
            uint32_t best_reminder = UNSIGNED_ZERO;
            for (size_t i = 0; i < PageNodeOrchestrator::TrackedAPCNodeLen(); i++)
            {
                if (
                    reminders_array[i] > best_reminder ||
                    (
                        reminders_array[i] == best_reminder && 
                        best_idx == PageNodeOrchestrator::TrackedAPCNodeLen() &&
                        *layout_fields_array[i]
                    )
                )
                {
                    best_reminder = reminders_array[i];
                    best_idx = i;
                }
            }

            if (best_idx == PageNodeOrchestrator::TrackedAPCNodeLen())
            {
                return false;
            }
            ++normalize_counts_array[best_idx];
            reminders_array[best_idx] = UNSIGNED_ZERO;
            --remaining_cells;
        }
        
        uint32_t final_sum = UNSIGNED_ZERO;

        for (size_t i = 0; i < PageNodeOrchestrator::TrackedAPCNodeLen(); i++)
        {
            final_sum += normalize_counts_array[i];
        }

        if (final_sum != payload_span)
        {
            return false;
        }
        
        
        for (size_t i = 0; i < PageNodeOrchestrator::TrackedAPCNodeLen(); i++)
        {
            *layout_fields_array[i] = normalize_counts_array[i];
        }
        return true;
    }


};

struct LayoutBoundsOrchestrator : public LayoutPercentageBuilder
{
    static constexpr uint64_t VALIDATION_LAYOUT_BUFFER_MARK = 11111;

    static constexpr std::optional<uint8_t> GetBufferIndexForALayout(LayoutCarrier& a_valid_layout) noexcept
    {
        if (!ValidateALayoutCarrier(a_valid_layout, true))
        {
            return std::nullopt;
        }

        const uint8_t start_idx = static_cast<uint8_t>(APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE);
        return static_cast<uint8_t>(static_cast<uint8_t>(a_valid_layout.LayoutIdentity) - start_idx);
    }

    static constexpr APCPagedNodeSegmentClasses GetOriginForLayoutClassByBufferIdx(uint8_t buffer_idx) noexcept
    {
        if (buffer_idx >= PageNodeOrchestrator::TrackedAPCNodeLen())
        {
            return APCPagedNodeSegmentClasses::NULLNAN;
        }
        return static_cast<APCPagedNodeSegmentClasses>(
            static_cast<uint8_t>(APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE) +
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

        const packed64_t layout_cell = CreateALayoutBoundsCell(a_valid_layout_carrier);

        if (layout_cell == PackedCell64_t::PACKED_CELL_SENTINAL)
        {
            return false;
        }

        layout_buffer[maybe_buffer_idx.value()] = layout_cell;
        
        return true;
    }

    static constexpr bool ValidateALayoutBuffer(
        TrackingBufferOfAPC& a_layout_buffer,
        uint16_t capacity_of_the_apc,
        bool is_claimed_valid = true
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

        uint16_t expected_begin = payload_begin;
        uint8_t expected_version = UINT8_MAX;

        for (uint8_t i = 0; i < PageNodeOrchestrator::TrackedAPCNodeLen(); i++)
        {
            const packed64_t current_packed_cell = a_layout_buffer[i];
            LayoutCarrier current_layout = GetLayoutCarrierFromValidLayoutCell(current_packed_cell, is_claimed_valid);
            if (!current_layout.IsValid)
            {
                return false;
            }

            const std::optional<uint8_t> maybe_current_idx = GetBufferIndexForALayout(current_layout);
            if (!maybe_current_idx.has_value() || maybe_current_idx.value() != i)
            {
                return false;
            }

            if (expected_version == UINT8_MAX)
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
            capacity_of_the_apc < MINIMUM_APC_CAPACITY ||
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

        uint16_t cursor = payload_begin;
        for (uint8_t i = 0; i < PageNodeOrchestrator::TrackedAPCNodeLen(); i++)
        {
            const APCPagedNodeSegmentClasses layout_class = GetOriginForLayoutClassByBufferIdx(i);
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
            current_layout.LocalityOfLayout = LocalityPolicy::PUBLISHED;

            if (!InsertALayoutCellInBuffer(return_buffer, current_layout))
            {
                BuildNullTrackingBuffer(return_buffer);
                return false;
            }
            cursor = current_layout.EndIndex;
        }

        return ValidateALayoutBuffer(return_buffer, capacity_of_the_apc, false);
    }

    static constexpr bool MutateAPCLayout() noexcept;

};
}