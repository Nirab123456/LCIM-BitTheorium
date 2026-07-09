#pragma once

#include "MetaAndLogicDefinationOfPackedCell.h"
namespace PredictedAdaptedEncoding
{

    // when user extracting a cell it can return UINT64_MAX as a symbole of invalid extraction method for that cell.

    struct PackedCellExtractors
    {
        
        static constexpr uint16_t CLOCK_16_SENTINAL = UINT16_MAX;
        static constexpr uint64_t PACKED_CELL_SENTINAL = UINT64_MAX;

        static constexpr meta16_t ExtractMeta16fromPackedCell(packed64_t packed_cell) noexcept
        {
            return static_cast<meta16_t>((packed_cell >> TOTAL_LOW) & MaskLowNBits(META16_B16));
        }

        static constexpr PackedMode ExtractModeFromCell(packed64_t packed_cell) noexcept
        {
            return static_cast<PackedMode>(ExtractCellModeFromMETA16_U_(ExtractMeta16fromPackedCell(packed_cell)));
        }

        static constexpr  bool IsPackedCellFrom32BitFamily(packed64_t packed_cell) noexcept
        {
            const PackedMode packed_mode = ExtractModeFromCell(packed_cell);

            if (packed_mode == PackedMode::MODEL32 || packed_mode == PackedMode::VALUE32)
            {
                return true;
            }
            
            return false;
        }
        
        /// @return PROHABITAD USE AND SHOLD BE REMOVED AFTER DETACHING EVERYTHING
        static constexpr val32_t ExtractRaw32FamilyBits(packed64_t packed_cell) noexcept
        {
            if (!IsPackedCellFrom32BitFamily(packed_cell))
            {
                return BIT_FAMILY_32_SENTINAL;
            }
            
            return static_cast<val32_t>(packed_cell & MaskLowNBits(VALBITS));
        }


        static constexpr clk16_t ExtractClk16(packed64_t packed_cell) noexcept
        {
            if (!IsPackedCellFrom32BitFamily(packed_cell))
            {
                return CLOCK_16_SENTINAL;
            }
            return static_cast<clk16_t>((packed_cell >> (VALBITS)) & MaskLowNBits(LOW16_BIT_LEN));
        }

        static constexpr uint64_t ExtractRaw48FamilyBits(packed64_t packed_cell) noexcept
        {
            if (IsPackedCellFrom32BitFamily(packed_cell))
            {
                return PACKED_CELL_SENTINAL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          
            }
            return static_cast<uint64_t>(packed_cell & MaskLowNBits(FAMILY_48_BIT_LEN));
        }

        static constexpr WildCardOfPackedCell ExtractWildCardFromCell(packed64_t packed_cell) noexcept
        {
            return static_cast<WildCardOfPackedCell>(ExtractWildCardFromMeta16_(ExtractMeta16fromPackedCell(packed_cell)));
        }

        static constexpr LocalityPolicy ExtractLocalityFromCell(packed64_t packed_cell) noexcept
        {
            return static_cast<LocalityPolicy>(ExtractLocalityFromMeta16_(ExtractMeta16fromPackedCell(packed_cell)));
        }

        static constexpr OwnershipPolicy ExtractOwnershipFromCell(packed64_t packed_cell) noexcept
        {
            return static_cast<OwnershipPolicy>(ExtractOwnershipFromMeta16_(ExtractMeta16fromPackedCell(packed_cell)));
        }

        static constexpr APCPagedNodeSegmentClasses ExtractAPCRegionFromCell(packed64_t packed_cell) noexcept
        {
            return static_cast<APCPagedNodeSegmentClasses>(ExtractRegionFromMeta16_(ExtractMeta16fromPackedCell(packed_cell)));
        }

        static constexpr Model32Subclass ExtractSubclass32FromCell(packed64_t packed_cell) noexcept
        {
            return static_cast<Model32Subclass>(ExtractSubClassOrContractFromMeta16_(ExtractMeta16fromPackedCell(packed_cell)));
        }

        static constexpr Model48Subclass ExtractSubclass48FromCell(packed64_t packed_cell) noexcept
        {
            return static_cast<Model48Subclass>(ExtractSubClassOrContractFromMeta16_(ExtractMeta16fromPackedCell(packed_cell)));
        }

        static constexpr InternalDataTypePolicy ExtractDataTypeFromCell(packed64_t packed_cell) noexcept
        {
            return static_cast<InternalDataTypePolicy>(ExtractDataTypeFromMeta16_(ExtractMeta16fromPackedCell(packed_cell)));
        }

        template <typename PCDT>
        static constexpr std::optional<PCDT> ExtractAnyPackedValueX(packed64_t packed_cell)
        {
            constexpr InternalDataTypePolicy expected_dtype = BridgeOfPackedCellDataType_v<PCDT>;
            if(ExtractDataTypeFromCell(packed_cell) != expected_dtype)
            {
                return std::nullopt;
            }
            if (IsPackedCellFrom32BitFamily(packed_cell))
            {
                if (sizeof(PCDT) > sizeof(val32_t))
                {
                    return std::nullopt;
                }
                const val32_t value_bits32 = ExtractRaw32FamilyBits(packed_cell);
                return BitCastMaybe<PCDT>(value_bits32);
            }

            if (sizeof(PCDT) > SIZE_OF_MODE_48)
            {
                return std::nullopt;
            }
            uint64_t value_bits_48 = ExtractRaw48FamilyBits(packed_cell);
            return BitCastMaybe<PCDT>(value_bits_48);
        }

        /// @brief Make meta Using only HIGHEST_TRUTH: 
        /// @param class_of_cell TYPE: uint8_t :: For safer use Use like static_cast<uint8_t>(Param->enum::value)
        /// @param sub_class TYPE: uint8_t :: For safer use Use like static_cast<uint8_t>(Param->enum::value)
        /// @param attribute TYPE: uint8_t :: For safer use Use like static_cast<uint8_t>(Param->enum::value)
        /// @return VALID: meta16 or UINT16_MAX if any one cross their indexing limit
        static constexpr meta16_t MakeInCellMeta_16t(
            PackedMode mode, 
            LocalityPolicy locality, 
            OwnershipPolicy cell_ownership,
            InternalDataTypePolicy data_type,
            tag8_t class_of_cell, 
            tag8_t sub_class
        ) noexcept
        {
            const tag8_t attr_raw = static_cast<tag8_t>(WildCardOfPackedCell::PACKED_CELL);
            const tag8_t owner_raw = static_cast<tag8_t>(cell_ownership);
            const tag8_t locality_raw = static_cast<tag8_t>(locality);
            const tag8_t mode_raw = static_cast<tag8_t>(mode);
            const tag8_t region_raw = static_cast<tag8_t>(class_of_cell);
            const tag8_t subclass_raw = static_cast<tag8_t>(sub_class);
            const tag8_t dtype_raw = static_cast<tag8_t>(data_type);
            if (
                attr_raw > DEFAULT_META16_INDEXING_LIMIT_2BIT ||
                owner_raw > DEFAULT_META16_INDEXING_LIMIT_2BIT ||
                locality_raw > DEFAULT_META16_INDEXING_LIMIT_2BIT ||
                mode_raw > DEFAULT_META16_INDEXING_LIMIT_2BIT ||
                subclass_raw > DEFAULT_META16_INDEXING_LIMIT_2BIT ||
                dtype_raw > DEFAULT_META16_INDEXING_LIMIT_2BIT ||
                region_raw > static_cast<tag8_t>(APCPagedNodeSegmentClasses::NULLNAN)
            )
            {
                return META_16_SENTINAL;
            }
            
            const meta16_t cell_attribute = static_cast<meta16_t>(static_cast<tag8_t>(attr_raw) & WILD_CARD_MASK);
            const meta16_t cell_authority = static_cast<meta16_t>(static_cast<tag8_t>(cell_ownership) & OWNERSHIP_MASK); 
            const meta16_t cell_locality = static_cast<meta16_t>(static_cast<tag8_t>(locality) & LOCALITY_MASK);
            const meta16_t cell_mode = static_cast<meta16_t>(static_cast<tag8_t>(mode) & CELL_MODE_MASK);
            const meta16_t cell_class = static_cast<meta16_t>(class_of_cell & REGION_CLASS_MASK);
            const meta16_t cell_sub_class = static_cast<meta16_t>(static_cast<tag8_t>(sub_class) & SUBCLASS_MASK);
            const meta16_t cell_data_type = static_cast<meta16_t>(static_cast<unsigned>(data_type) & CELL_INTERNAL_DATA_TYPE_MASK);

            meta16_t cell_meta = static_cast<meta16_t>(
                (cell_attribute  << (WILD_CARD_SHIFT))
                | (cell_data_type << (DATA_TYPE_SHIFT))
                | (cell_authority << (OWNERSHIP_SHIFT))
                | (cell_locality << LOCALITY_SHIFT)
                | (cell_mode << CELL_MODE_SHIFT)
                | (cell_class << REGION_CLASS_SHIFT)
                | (cell_sub_class << SUBCLASS_SHIFT)
            );
            return cell_meta;
        }


protected:

        static constexpr tag8_t ExtractWildCardFromMeta16_(meta16_t meta16) noexcept
        {
            return static_cast<tag8_t>((meta16 >> WILD_CARD_SHIFT) & WILD_CARD_MASK);
        }

        static constexpr tag8_t ExtractOwnershipFromMeta16_(meta16_t meta16) noexcept
        {
            return static_cast<tag8_t>((meta16 >> OWNERSHIP_SHIFT ) & OWNERSHIP_MASK);
        }
        
        static constexpr tag8_t ExtractLocalityFromMeta16_(meta16_t meta16) noexcept
        {
            return static_cast<tag8_t>((meta16 >> LOCALITY_SHIFT) & LOCALITY_MASK);
        }

        static constexpr tag8_t ExtractCellModeFromMETA16_U_(meta16_t meta16) noexcept
        {
            return static_cast<tag8_t>((meta16 >> CELL_MODE_SHIFT) & CELL_MODE_MASK);
        }

        static constexpr tag8_t ExtractRegionFromMeta16_(meta16_t meta16) noexcept
        {
            return static_cast<tag8_t>((meta16 >> REGION_CLASS_SHIFT) & REGION_CLASS_MASK);
        }

        static constexpr tag8_t ExtractSubClassOrContractFromMeta16_(meta16_t meta16) noexcept
        {
            return static_cast<tag8_t>((meta16 >> SUBCLASS_SHIFT) & SUBCLASS_MASK);
        }

        static constexpr tag8_t ExtractDataTypeFromMeta16_(meta16_t meta16) noexcept
        {
            return static_cast<tag8_t>((meta16 >> DATA_TYPE_SHIFT) & CELL_INTERNAL_DATA_TYPE_MASK);
        }
    };
    


    struct PackedCellSetters : public PackedCellExtractors
    {
        /// @brief Make meta for ANY: OwnershipPolicy of ModelFamily::MODEL48
        /// @param page_class uint8_t :: For safer use Use like static_cast<uint8_t>(Param->enum::value)
        /// @return 
        static constexpr meta16_t MakeMeta16ForAnyOwnerAndItsClassModel_48t(
            OwnershipPolicy ownership = OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER,
            tag8_t cell_class = static_cast<tag8_t>(APCPagedNodeSegmentClasses::FREE_SLOT),
            Model48Subclass sub_class = Model48Subclass::SELF_CLASS,
            LocalityPolicy locality = LocalityPolicy::IDLE,
            InternalDataTypePolicy cell_data_type = InternalDataTypePolicy::UNSIGNED
        ) noexcept
        {
            return MakeInCellMeta_16t(
                PackedMode::MODEL48,
                locality,
                ownership,
                cell_data_type,
                static_cast<tag8_t>(cell_class),
                static_cast<tag8_t>(sub_class)
            );
        }

        /// @brief Make meta for ANY: OwnershipPolicy of ModelFamily::MODEL32
        /// @param page_class uint8_t :: For safer use Use like static_cast<uint8_t>(Param->enum::value)
        /// @return 
        static constexpr meta16_t MakeMeta16ForAnyOwnerAndItsClassModel_32t(
            OwnershipPolicy ownership = OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER,
            tag8_t cell_class = static_cast<tag8_t>(APCPagedNodeSegmentClasses::FREE_SLOT),
            Model32Subclass sub_class = Model32Subclass::SELF_CLASS,
            LocalityPolicy locality = LocalityPolicy::IDLE,
            InternalDataTypePolicy cell_data_type = InternalDataTypePolicy::UNSIGNED
        ) noexcept
        {
            return MakeInCellMeta_16t(
                PackedMode::MODEL32,
                locality,
                ownership,
                cell_data_type,
                static_cast<tag8_t>(cell_class),
                static_cast<tag8_t>(sub_class)
            );
        }

        static constexpr packed64_t SetMETA16InPacked(packed64_t packed_cell, meta16_t meta16) noexcept
        {
            constexpr packed64_t top_48_bit_mask = MaskLowNBits(META16_B16) << TOTAL_LOW;
            packed_cell &= ~top_48_bit_mask;
            packed_cell |= (packed64_t(meta16) & MaskLowNBits(META16_B16)) << TOTAL_LOW;
            return packed_cell;
        }

        static constexpr packed64_t SetClock16InPacked(packed64_t packed_cell, clk16_t value16) noexcept
        {
            if (!IsPackedCellFrom32BitFamily(packed_cell))
            {
                return PACKED_CELL_SENTINAL;
            }
            
            constexpr packed64_t clock16_mask = static_cast<packed64_t>(MaskLowNBits(LOW16_BIT_LEN) << VALBITS);
            packed_cell &= ~clock16_mask;

            packed_cell |= (
                static_cast<packed64_t>(value16) & static_cast<packed64_t>(MaskLowNBits(LOW16_BIT_LEN))
            ) << VALBITS;

            return packed_cell;
        }

        static constexpr packed64_t SetLocalityInPacked(packed64_t packed_cell, LocalityPolicy local_state) noexcept
        {
            const meta16_t new_desired_meta = SetLocalityInMETA16(ExtractMeta16fromPackedCell(packed_cell), local_state);
            return SetMETA16InPacked(packed_cell, new_desired_meta);
        }


        static constexpr packed64_t SetPageClassInPacked(packed64_t packed_cell, APCPagedNodeSegmentClasses page_class) noexcept
        {
            const meta16_t new_desired_meta = SetPageClassInMETA16(ExtractMeta16fromPackedCell(packed_cell), page_class);
            return SetMETA16InPacked(packed_cell, new_desired_meta);
        }

        static constexpr packed64_t SetSubClassForModel32InPacked(packed64_t packed_cell, Model32Subclass sub_class) noexcept
        {
            const meta16_t new_desired_meta = SetSubClassOfModeInMETA16(ExtractMeta16fromPackedCell(packed_cell), static_cast<tag8_t>(sub_class));
            return SetMETA16InPacked(packed_cell, new_desired_meta);
        }

        static constexpr packed64_t SetSubClassForModel48InPacked(packed64_t packed_cell, Model48Subclass sub_class) noexcept
        {
            const meta16_t new_desired_meta = SetSubClassOfModeInMETA16(ExtractMeta16fromPackedCell(packed_cell), static_cast<tag8_t>(sub_class));
            return SetMETA16InPacked(packed_cell, new_desired_meta);
        }

        static constexpr packed64_t SetPCellDataTypeInPacked(packed64_t packed_cell, InternalDataTypePolicy cell_data_type)
        {
            const meta16_t new_desired_meta = SetCellDataTypeInMETA16(ExtractMeta16fromPackedCell(packed_cell), cell_data_type);
            return SetMETA16InPacked(packed_cell, new_desired_meta);
        }

        static constexpr packed64_t SetAccessContractForValueInPacked(packed64_t packed_cell, ContractOfConcurrency access_control) noexcept
        {
            const meta16_t new_desired_meta = SetSubClassOfModeInMETA16(ExtractMeta16fromPackedCell(packed_cell), static_cast<tag8_t>(access_control));
            return SetMETA16InPacked(packed_cell, new_desired_meta);
        }

    protected:

        static  constexpr meta16_t SetWildCardImMeta16_(
            meta16_t meta16,
            WildCardOfPackedCell attribute
        ) noexcept
        {
            return SetIndicatedMetaInMeta16(
                meta16,
                WILD_CARD_SHIFT,
                WILD_CARD_MASK,
                static_cast<tag8_t>(attribute)
            );
        }

        static  constexpr meta16_t SetNodeAuthorityInMETA16(
            meta16_t meta16,
            OwnershipPolicy cell_ownership
        ) noexcept
        {
            return SetIndicatedMetaInMeta16(
                meta16,
                OWNERSHIP_SHIFT,
                OWNERSHIP_MASK,
                static_cast<tag8_t>(cell_ownership)
            );
        }

        static  constexpr meta16_t SetLocalityInMETA16(
            meta16_t meta16,
            LocalityPolicy locality
        ) noexcept
        {
            return SetIndicatedMetaInMeta16(
                meta16,
                LOCALITY_SHIFT,
                LOCALITY_MASK,
                static_cast<tag8_t>(locality)
            );
        }

        static  constexpr meta16_t SetCellModeInMETA16(
            meta16_t meta16,
            PackedMode cell_mode
        ) noexcept
        {
            return SetIndicatedMetaInMeta16(
                meta16,
                CELL_MODE_SHIFT,
                CELL_MODE_MASK,
                static_cast<tag8_t>(cell_mode)
            );
        }

        static  constexpr meta16_t SetPageClassInMETA16(
            meta16_t meta16,
            APCPagedNodeSegmentClasses page_class
        ) noexcept
        {
            return SetIndicatedMetaInMeta16(
                meta16,
                REGION_CLASS_SHIFT,
                REGION_CLASS_MASK,
                static_cast<tag8_t>(page_class)
            );
        }


        static  constexpr meta16_t SetSubClassOfModeInMETA16(
            meta16_t meta16,
            tag8_t sub_class
        ) noexcept
        {
            return SetIndicatedMetaInMeta16(
                meta16,
                SUBCLASS_SHIFT,
                SUBCLASS_MASK,
                sub_class
            );
        }

        static  constexpr meta16_t SetCellDataTypeInMETA16(
            meta16_t meta16,
            InternalDataTypePolicy cell_data_type
        ) noexcept
        {
            return SetIndicatedMetaInMeta16(
                meta16,
                DATA_TYPE_SHIFT,
                CELL_INTERNAL_DATA_TYPE_MASK,
                static_cast<tag8_t>(cell_data_type)
            );
        }
    
        static  constexpr meta16_t ClearIndicatedMeta16Field_(
            meta16_t meta16,
            unsigned shift,
            tag8_t mask
        ) noexcept
        {
            return static_cast<meta16_t>(
                meta16 & ~static_cast<meta16_t>(
                    static_cast<meta16_t>(mask) << shift
                )
            );
        }

        static  constexpr meta16_t SetIndicatedMetaInMeta16(
            meta16_t meta16,
            unsigned shift,
            tag8_t mask,
            tag8_t value
        ) noexcept
        {
            const meta16_t cleared_indicated = ClearIndicatedMeta16Field_(
                meta16, shift, mask
            );
            const meta16_t only_inserted_meta16 = static_cast<meta16_t>(
                static_cast<meta16_t>(value & mask) << shift
            );
            return static_cast<meta16_t>(cleared_indicated | only_inserted_meta16);
        }
    };
    

}
