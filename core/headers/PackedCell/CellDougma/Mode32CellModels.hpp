
#pragma once 
#include <array>
#include <utility>
#include "Mode48CellModels.hpp"

namespace PredictedAdaptedEncoding
{
    struct Clock16Subdivision1x8Plus2x4InMode32CellModel
    {
        static constexpr unsigned LOWEST_8BIT_SHIFT = 0u;
        static constexpr unsigned MID_4BIT_SHIFT = 8u;
        static constexpr unsigned HIGHIEST_4bit_SHIFT = 12u;
        static constexpr uint16_t MASK_LOW_4 = 0x0Fu;

        static constexpr packed64_t ComposeMode32WithThreeClock16Subdivision(
            uint32_t value32,
            uint8_t lowest_8_bit_of_clock16,
            uint8_t mid_4_bit_of_clock16,
            uint8_t highiest_4_bit_of_clock16,
            meta16_t meta16
        )
        {
            const clk16_t combined_subdevision_value = Pack1x8Plus2x4InUnsigned16_(lowest_8_bit_of_clock16, mid_4_bit_of_clock16, highiest_4_bit_of_clock16);
            const packed64_t compressed_packed_cell = PackedCell64_t::Compose32BitFamilyPackedCell(value32, combined_subdevision_value, meta16);
            if (!IsThisCellA32BitMetaNoClock16Mode32(compressed_packed_cell))
            {
                return PackedCell64_t::PACKED_CELL_SENTINAL;
            }
            return compressed_packed_cell;
        }

        static constexpr uint16_t Pack1x8Plus2x4InUnsigned16_(
            uint8_t lowest_8_bit,
            uint8_t mid_4_bit,
            uint8_t highiest_4_bit
        ) noexcept
        {
            return (uint16_t{lowest_8_bit} << LOWEST_8BIT_SHIFT) |
                (uint16_t{mid_4_bit} << MID_4BIT_SHIFT) |
                (uint16_t{highiest_4_bit} << HIGHIEST_4bit_SHIFT);
        }

        static constexpr uint8_t ExtractLowest8Bit_(uint16_t raw16) noexcept
        {
            return static_cast<uint8_t>((raw16 >> LOWEST_8BIT_SHIFT) & Subdivision2x16Plus2x8InternalMode48CellModel::MASK_LOW_8);
        }

        static constexpr uint8_t ExtractMid4Bit_(uint16_t raw16) noexcept
        {
            return static_cast<uint8_t>((raw16 >>MID_4BIT_SHIFT) & MASK_LOW_4);
        }

        static constexpr uint8_t ExtractHighiest4Bit_(uint16_t raw16) noexcept
        {
            return static_cast<uint8_t>((raw16 >>HIGHIEST_4bit_SHIFT) & MASK_LOW_4);
        }

        static constexpr bool IsThisCellA32BitMetaNoClock16Mode32(packed64_t packed_cell) noexcept
        {
            const PackedCell64_t::AuthoritiveCellView this_cell_auth_view = PackedCell64_t::GetAuthoritiveViewsForACell(packed_cell);
            return this_cell_auth_view.CellMode == PackedMode::MODEL32 &&
                this_cell_auth_view.SubClassOfModel32 == Model32Subclass::UNCLOCKED_1x8_PLUS_2x4 &&
                this_cell_auth_view.CellValueDataType == InternalDataTypePolicy::UNSIGNED &&
                this_cell_auth_view.LocalityOfCell != LocalityPolicy::FAULTY;
        }

    };

    struct PairedVersionedCellModelOfMode32
    {
        //In paired cell Ideology clk16 is a version count-> CLOCK is unnecessery because it will be mostly used for contron / paired pointers
        static constexpr std::pair<packed64_t, packed64_t> GetPairOfLow32FAndHigh32SFromUnsigned64ForAPC(
            uint64_t value, 
            clk16_t version,
            LocalityPolicy locality = LocalityPolicy::IDLE,
            APCPagedNodeSegmentClasses page_class = APCPagedNodeSegmentClasses::META_HEADER
        ) noexcept
        {
            const std::pair<packed64_t, packed64_t> lowf_highs = GetPairOfLow32FAndHigh32SFromUnsigned64_(
                value, version, 
                locality, 
                OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER, 
                static_cast<tag8_t>(page_class)
            );

            if (page_class == APCPagedNodeSegmentClasses::META_HEADER && value <= BIT_FAMILY_32_SENTINAL)
            {
                return std::pair<packed64_t, packed64_t>(lowf_highs.first, PackedCell64_t::PACKED_CELL_SENTINAL);
            }
            return lowf_highs;
        }

        static constexpr std::pair<packed64_t, packed64_t> GetPairOfLow32FAndHigh32SFromUnsigned64ForFabric(
            uint64_t value, clk16_t version,
            LocalityPolicy locality = LocalityPolicy::IDLE,
            FabricTableSegmentClasses fabric_segment_class = FabricTableSegmentClasses::GLOBAL_AND_CONFIG
        ) noexcept
        {
            const std::pair<packed64_t, packed64_t> lowf_highs = GetPairOfLow32FAndHigh32SFromUnsigned64_(
                value, version, 
                locality, 
                OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC, 
                static_cast<tag8_t>(fabric_segment_class)
            );

            if (fabric_segment_class != FabricTableSegmentClasses::GLOBAL_AND_CONFIG && value <= BIT_FAMILY_32_SENTINAL)
            {
                return std::pair<packed64_t, packed64_t>(lowf_highs.first, PackedCell64_t::PACKED_CELL_SENTINAL);
            }
            return lowf_highs;
        }

    
        static constexpr std::optional<uint64_t> GetFullUnsigned64FromPairedVersionedCell(
            packed64_t low_half, packed64_t high_half,
            PackedCell64_t::AuthoritiveCellView* low_half_view_ptr = nullptr,
            PackedCell64_t::AuthoritiveCellView* high_half_view_ptr = nullptr
        ) noexcept
        {
            const PackedCell64_t::AuthoritiveCellView low_half_view = PackedCell64_t::GetAuthoritiveViewsForACell(low_half);
            const PackedCell64_t::AuthoritiveCellView high_half_view = PackedCell64_t::GetAuthoritiveViewsForACell(high_half);

            if (low_half_view_ptr)
            {
                *low_half_view_ptr = low_half_view;
            }

            if (high_half_view_ptr)
            {
                *high_half_view_ptr = high_half_view;
            }

            if (low_half_view.RawCell == PackedCell64_t::PACKED_CELL_SENTINAL && high_half_view.RawCell == PackedCell64_t::PACKED_CELL_SENTINAL)
            {
                return PackedCell64_t::PACKED_CELL_SENTINAL;
            }

            if (
                high_half_view.RawCell == PackedCell64_t::PACKED_CELL_SENTINAL && 
                low_half_view.IsCellValid && 
                low_half_view.SubClassOfModel32 == Model32Subclass::LOW_OF_PAIRED_VERSIONED_CELL 
            )
            {
                return static_cast<uint64_t>(low_half_view.Raw32BitInCellData);
            }

            if (
                low_half_view.IsCellValid && high_half_view.IsCellValid &&
                low_half_view.SubClassOfModel32 == Model32Subclass::LOW_OF_PAIRED_VERSIONED_CELL && 
                high_half_view.SubClassOfModel32 == Model32Subclass::HIGH_OF_PAIRED_VERSIONED_CELL
            )
            {
                return static_cast<packed64_t>(low_half_view.Raw32BitInCellData) | 
                        (static_cast<packed64_t>(high_half_view.Raw32BitInCellData ) << VALBITS);
            }

            return std::nullopt;            

        }
    
private:
        static constexpr std::pair<packed64_t, packed64_t> GetPairOfLow32FAndHigh32SFromUnsigned64_(
            uint64_t value, clk16_t version,
            LocalityPolicy locality,
            OwnershipPolicy ownership,
            tag8_t page_class
        ) noexcept
        {
            const uint32_t low_half32 = static_cast<uint32_t>(value & MaskLeftOverBitsUntil64(VALBITS));
            const uint32_t high_half32 = static_cast<uint32_t>((value >> VALBITS) & MaskLeftOverBitsUntil64(VALBITS));

            const packed64_t low_half_packed_cell = PackedCell64_t::MakeInitialValidGeneralPackedCell(
                PackedMode::MODEL32, locality, ownership, page_class,
                InternalDataTypePolicy::UNSIGNED, low_half32, version,
                static_cast<tag8_t>(Model32Subclass::LOW_OF_PAIRED_VERSIONED_CELL)
            );
    
            const packed64_t high_half_packed_cell = PackedCell64_t::MakeInitialValidGeneralPackedCell(
                PackedMode::MODEL32, locality, ownership, page_class,
                InternalDataTypePolicy::UNSIGNED, high_half32, version,
                static_cast<tag8_t>(Model32Subclass::HIGH_OF_PAIRED_VERSIONED_CELL)
            );

            return std::pair(low_half_packed_cell, high_half_packed_cell);
        }

    };

}
