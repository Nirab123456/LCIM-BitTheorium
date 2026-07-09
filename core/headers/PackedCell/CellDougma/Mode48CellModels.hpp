
#pragma once 
#include <array>
#include <utility>
#include "../CoreCellDefination.hpp"

namespace PredictedAdaptedEncoding
{

    struct Subdevision16x3InternalMode48CellModel
    {
        static constexpr uint64_t MASK_LOW_16 = MaskLowNBits(16);
        static constexpr unsigned PACK3XU16TOMODE48_SHIFT_LOW = 0u;
        static constexpr unsigned PACK3XU16TOMODE48_SHIFT_MID = 16u;
        static constexpr unsigned PACK3XU16TOMODE48_SHIFT_HIGH = 32u;

        //In future we should accomodate check
        static constexpr packed64_t Compose3Unsigned16bitIndependentInMode48(
            uint16_t low16_bits,
            uint16_t mid_16_bits,
            uint16_t high_16_bits,
            meta16_t meta16
        ) noexcept
        {
            const uint64_t raw48 = (PackUnsigned16x3ToMode48_(low16_bits, mid_16_bits, high_16_bits) & MaskLowNBits(FAMILY_48_BIT_LEN));
            return PackedCell64_t::Compose48BitFamilyPackedCell(raw48, meta16);
        }

        /// @brief Just pack 3 Unsigned value
        /// @param low16_bits LOWEST 16 bits
        /// @param mid_16_bits MIDDLE 16 BITS
        /// @param high_16_bits HIGHIEST 16 BITS
        /// @return 48bit value -> uint64_t :: HIGH16:MASKED
        static constexpr uint64_t PackUnsigned16x3ToMode48_(
            uint16_t low16_bits,
            uint16_t mid_16_bits,
            uint16_t high_16_bits
        ) noexcept
        {
            return ((uint64_t{low16_bits} << PACK3XU16TOMODE48_SHIFT_LOW)    |
                (uint64_t{mid_16_bits} << PACK3XU16TOMODE48_SHIFT_MID)      |
                (uint64_t(high_16_bits) << PACK3XU16TOMODE48_SHIFT_HIGH)) & MaskLowNBits(FAMILY_48_BIT_LEN);
        }

        static constexpr uint16_t ExtractLow16FromUnsigned48_(uint64_t raw48) noexcept
        {
            return static_cast<uint16_t>((raw48 >> PACK3XU16TOMODE48_SHIFT_LOW) & MASK_LOW_16);
        }

        static constexpr uint16_t ExtractMid16FromUnsigned48_(uint64_t raw48) noexcept
        {
            return static_cast<uint16_t>((raw48 >> PACK3XU16TOMODE48_SHIFT_MID) & MASK_LOW_16);
        }

        static constexpr uint16_t ExtractHigh16FromUnsigned48_(uint64_t raw48) noexcept
        {
            return static_cast<uint16_t>((raw48 >> PACK3XU16TOMODE48_SHIFT_HIGH) & MASK_LOW_16);
        }

        static constexpr bool IsThisCellASubdevision_3x16_48t(packed64_t packed_cell) noexcept
        {
            return PackedCell64_t::ExtractModeFromCell(packed_cell) == PackedMode::MODEL48 &&
                PackedCell64_t::ExtractSubclass48FromCell(packed_cell) == Model48Subclass::SUBDIVISION16x3_INTERNAL_CELL_MODEL &&
                PackedCell64_t::ExtractLocalityFromCell(packed_cell) != LocalityPolicy::FAULTY;
        }

        static constexpr bool ExtractLowMidHighFromMode48_(uint64_t raw48, uint16_t& low, uint16_t& mid, uint16_t& high)
        {
            if (raw48 >= PackedCell64_t::BIT_FAMILY_48_SENTINAL)
            {
                low = PackedCell64_t::CLOCK_16_SENTINAL;
                mid = PackedCell64_t::CLOCK_16_SENTINAL;
                high = PackedCell64_t::CLOCK_16_SENTINAL;
                return false;
            }
            low = ExtractLow16FromUnsigned48_(raw48);
            mid = ExtractMid16FromUnsigned48_(raw48);
            high = ExtractHigh16FromUnsigned48_(raw48);
            return true;
        }
    };


    struct Subdivision2x16Plus2x8InternalMode48CellModel
    {
        static constexpr unsigned FIRST_16BIT_SHIFT_0 = 0u;
        static constexpr unsigned SECOND_16BIT_SHIFT_1 = 16u;
        static constexpr unsigned FIRST_8BIT_SHIFT_2 = 24u;
        static constexpr unsigned SECOND_8BIT_SHIFT_3 = 32u;

        static constexpr uint64_t MASK_LOW_8 = MaskLowNBits(8);


        static constexpr packed64_t Compose4Unsigned16x2Plus8x2IndependendentInMode48(
            uint16_t lowest_16bit_0,
            uint16_t low_16bit_1,
            uint8_t high_8bit_2,
            uint8_t highest_8bit_3,
            meta16_t meta16
        ) noexcept
        {
            const uint64_t raw48 = (
                Pack2x16Plus2x8UnsignedSubdivision_(
                    lowest_16bit_0, low_16bit_1,
                    high_8bit_2, highest_8bit_3
                ) & MaskLowNBits(FAMILY_48_BIT_LEN)
            );
            const packed64_t compressed_packed_cell = PackedCell64_t::Compose48BitFamilyPackedCell(raw48, meta16);
            if (!IsThisCellAFourSubdevision_48t(compressed_packed_cell))
            {
                return PackedCell64_t::PACKED_CELL_SENTINAL;
            }
            return compressed_packed_cell;
        }

        static constexpr uint64_t Pack2x16Plus2x8UnsignedSubdivision_(
            uint16_t low16_0,
            uint16_t low16_1,
            uint8_t high8_2,
            uint8_t high8_3
        ) noexcept
        {
            return (uint64_t{low16_0} << FIRST_16BIT_SHIFT_0) |
                (uint64_t{low16_1} << SECOND_16BIT_SHIFT_1) |
                (uint64_t{high8_2} << FIRST_8BIT_SHIFT_2) |
                (uint64_t{high8_3} << SECOND_8BIT_SHIFT_3);
        }

        static constexpr uint16_t ExtractLowestFirstLow16Bit0_(uint64_t raw48) noexcept
        {
            return static_cast<uint16_t>((raw48 >> FIRST_16BIT_SHIFT_0) & Subdevision16x3InternalMode48CellModel::MASK_LOW_16);
        }

        static constexpr uint16_t ExtractSecondLow16Bit1_(uint64_t raw48) noexcept
        {
            return static_cast<uint16_t>((raw48 >> SECOND_16BIT_SHIFT_1) & Subdevision16x3InternalMode48CellModel::MASK_LOW_16);
        }

        static constexpr uint8_t ExtractHigh8Bit2_(uint64_t raw48) noexcept
        {
            return static_cast<uint8_t>((raw48 >> FIRST_8BIT_SHIFT_2) & MASK_LOW_8);
        }

        static constexpr uint8_t ExtractHighestHigh8Bit3_(uint64_t raw48) noexcept
        {
            return static_cast<uint8_t>((raw48 >> SECOND_8BIT_SHIFT_3) & MASK_LOW_8);
        }

        static constexpr bool IsThisCellAFourSubdevision_48t(packed64_t packed_cell) noexcept
        {
            const PackedCell64_t::AuthoritiveCellView this_cell_auth_view = PackedCell64_t::GetAuthoritiveViewsForACell(packed_cell);
            return this_cell_auth_view.CellMode == PackedMode::MODEL48 &&
                this_cell_auth_view.SubClassOfModel48 == Model48Subclass::FOUR_SUBDIVISION_2x16_AND_2x8 &&
                this_cell_auth_view.CellValueDataType == InternalDataTypePolicy::UNSIGNED &&
                this_cell_auth_view.LocalityOfCell != LocalityPolicy::FAULTY;
        }

        static constexpr bool ExtractAll4SubdivisionInOneGoIfValid_(
            uint64_t raw48, uint16_t& lowest_16bit_0, uint16_t& low_16bit_1,
            uint8_t& high_8bit_2, uint8_t& highest_8bit_3
        ) noexcept
        {
            if (raw48 == PackedCell64_t::PACKED_CELL_SENTINAL)
            {
                lowest_16bit_0 = PackedCell64_t::CLOCK_16_SENTINAL;
                low_16bit_1 = PackedCell64_t::CLOCK_16_SENTINAL;
                high_8bit_2 = EIGHT_BIT_SENTINAL;
                highest_8bit_3 = EIGHT_BIT_SENTINAL;
                return false;
            }
            lowest_16bit_0 = ExtractLowestFirstLow16Bit0_(raw48);
            low_16bit_1 = ExtractSecondLow16Bit1_(raw48);
            high_8bit_2 = ExtractHigh8Bit2_(raw48);
            highest_8bit_3 = ExtractHighestHigh8Bit3_(raw48);
            return true;
        }

    };

}
