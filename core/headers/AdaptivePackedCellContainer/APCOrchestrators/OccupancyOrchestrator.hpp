#pragma once
#include "../CoreOFAPC/PageAndNodeDef.hpp"

namespace PredictedAdaptedEncoding
{


struct OccupancyOrchestrator 
{

        /// @brief USES: Model48Subclass::SUBDIVISION16x3_INTERNAL_CELL_MODEL & CREATS: Occupancy cell OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER  [LocalityPolicy::PUBLISHED | LocalityPolicy::CLAIMED | LocalityPolicy::FAULTY]
        /// @param published_count LOWEST: uint16_t in PackUnsigned16x3ToMode48_
        /// @param claimed_count MID: uint16_t in PackUnsigned16x3ToMode48_
        /// @param faulty_count HIGHIEST: uint16_t in PackUnsigned16x3ToMode48_
        /// @return 
        static constexpr packed64_t ComposeAPCOwned16x3Model_48t(
            uint16_t published_count,
            uint16_t claimed_count,
            uint16_t faulty_count,
            APCPagedNodeSegmentClasses page_class,
            LocalityPolicy locality = LocalityPolicy::PUBLISHED
        ) noexcept
        {   

            const uint64_t raw_48 =  Subdevision16x3InternalMode48CellModel::PackUnsigned16x3ToMode48_(
                published_count,
                claimed_count,
                faulty_count
            );

            return PackedCell64_t::MakeModeledAPCValidPackedCell(
                ModelFamily::MODEL48,
                static_cast<tag8_t>(Model48Subclass::SUBDIVISION16x3_INTERNAL_CELL_MODEL),
                page_class,
                locality,
                InternalDataTypePolicy ::UnsignedPCellDataType,
                AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
                raw_48
            );
        }

        static constexpr std::optional<uint16_t> GetOccuupancyFromPackedCellMode48(
            packed64_t packed_cell,
            LocalityPolicy desired_occupancy_bucket,
            uint16_t physical_capacity
        ) noexcept
        {
            if (!Subdevision16x3InternalMode48CellModel::IsThisCellASubdevision_3x16_48t(packed_cell))
            {
                return std::nullopt;
            }
            const uint64_t raw48 = PackedCell64_t::ExtractRaw48FamilyBits(packed_cell);
            if (raw48 == PackedCell64_t::PACKED_CELL_SENTINAL)
            {
                return std::nullopt;
            }
            
            switch (desired_occupancy_bucket)
            {
            case LocalityPolicy::PUBLISHED :
                return Subdevision16x3InternalMode48CellModel::ExtractLow16FromUnsigned48_(raw48);
            case LocalityPolicy::CLAIMED :
                return Subdevision16x3InternalMode48CellModel::ExtractMid16FromUnsigned48_(raw48);
            case LocalityPolicy::FAULTY :
                return Subdevision16x3InternalMode48CellModel::ExtractHigh16FromUnsigned48_(raw48);
            case LocalityPolicy::IDLE :
                return DerivedIdleFromPackedCell48(packed_cell, physical_capacity);
            default:
                return std::nullopt;
            }
        }

        static constexpr uint16_t GetTootalOccupancyFromPackedCell(packed64_t packed_cell) noexcept
        {
            if (!Subdevision16x3InternalMode48CellModel::IsThisCellASubdevision_3x16_48t(packed_cell))
            {
                return UNSIGNED_ZERO;
            }
            const uint64_t raw48 = PackedCell64_t::ExtractRaw48FamilyBits(packed_cell);
            uint16_t published = UNSIGNED_ZERO;
            uint16_t claimed = UNSIGNED_ZERO;
            uint16_t faulty = UNSIGNED_ZERO;
            if(!Subdevision16x3InternalMode48CellModel::ExtractLowMidHighFromMode48_(raw48, published, claimed, faulty))
            {
                return UNSIGNED_ZERO;
            }
            return published + claimed + faulty;
        }

        static constexpr bool IsCapacityOfAPCLegal(size_t total_capacity) noexcept
        {
            return total_capacity > APCDataStructure::METACELL_COUNT && APCDataStructure::IsThisIndexValidForAPC(static_cast<uint32_t>(total_capacity));
        }

protected:
        static constexpr uint32_t SumOf3PartOccupancyOf48Bit_(uint64_t raw48) noexcept
        {
            return Subdevision16x3InternalMode48CellModel::ExtractLow16FromUnsigned48_(raw48) + 
                Subdevision16x3InternalMode48CellModel::ExtractMid16FromUnsigned48_(raw48) + 
                Subdevision16x3InternalMode48CellModel::ExtractHigh16FromUnsigned48_(raw48);
        }

        static constexpr uint16_t DeriveIdleCoundtFromRaw48General_(uint64_t raw48, uint16_t physical_capacity) noexcept
        {
            const  uint32_t in_use_potion = SumOf3PartOccupancyOf48Bit_(raw48);
            return in_use_potion > physical_capacity ? UNSIGNED_ZERO : static_cast<uint16_t>(physical_capacity - in_use_potion);
        }

        static constexpr uint16_t DerivedIdleFromPackedCell48(packed64_t packed_cell, uint16_t physical_capacity) noexcept
        {
            const uint64_t raw48 = PackedCell64_t::ExtractRaw48FamilyBits(packed_cell);
            if (raw48 == PackedCell64_t::PACKED_CELL_SENTINAL)
            {
                return UNSIGNED_ZERO;
            }
            return DeriveIdleCoundtFromRaw48General_(raw48, physical_capacity);
        }
    
};

}