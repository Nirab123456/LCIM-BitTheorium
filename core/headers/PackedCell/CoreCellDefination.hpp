#pragma once

#include "SwornContracts/CellContracts.hpp"

namespace PredictedAdaptedEncoding
{

    struct CoreCellDefinationn : public CellValidators
    {

        static constexpr bool IsCellClaimableFromThisCaller(packed64_t packed_cell) noexcept
        {
            const LocalityPolicy locality = ExtractLocalityPolicy(packed_cell);
            
            return locality != LocalityPolicy::CLAIMED;
        }
        
        /// @brief Should be improved
        static constexpr packed64_t MakeFaultyCell() noexcept
        {
            return MakeAnUncheckedCell_(
                PackedMode::MODEL32,
                BIT_FAMILY_32_SENTINAL,
                UINT16_MAX,
                AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
                OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER,
                LocalityPolicy::FAULTY
            );
        }

        static constexpr packed64_t Compose32BitFamilyPackedCell(val32_t in_cell_value, clk16_t clock16, meta16_t meta16) noexcept
        {
            const PackedMode packed_mode = static_cast<PackedMode>(ExtractCellModeFromMETA16_U_(meta16));
            if(
                packed_mode != PackedMode::MODEL32 && packed_mode != PackedMode::VALUE32 
            )
            {
                return MakeFaultyCell();
            }
            packed64_t packed_cell = (packed64_t(in_cell_value) & MaskLowNBits(VALBITS));
            packed_cell = SetMETA16InPacked(packed_cell, clock16);
            packed_cell = SetMETA16InPacked(packed_cell, meta16);
            return packed_cell;
        }

        static constexpr packed64_t Compose48BitFamilyPackedCell(uint64_t clockor_value48, meta16_t meta16) noexcept
        {
            const PackedMode packed_mode = static_cast<PackedMode>(ExtractCellModeFromMETA16_U_(meta16));
            if(packed_mode != PackedMode::MODEL48 && packed_mode != PackedMode::VALUE48)
            {
                return MakeFaultyCell();
            }
            packed64_t packed_cell = (packed64_t(clockor_value48) & MaskLowNBits(FAMILY_48_BIT_LEN));
            packed_cell = SetMETA16InPacked(packed_cell, meta16);
            return packed_cell;
        }

    protected:
        /// @brief Can be used to create every type of Packed Cell
        static constexpr packed64_t MakeAnUncheckedCell_(
            PackedMode cell_mode,
            uint64_t cell_value = UNSIGNED_ZERO,
            clk16_t clock16 = UNSIGNED_ZERO,
            AttributePolicy cell_attribute = AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
            OwnershipPolicy cell_ownership = OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER,
            LocalityPolicy cell_locality = LocalityPolicy::IDLE, 
            tag8_t cell_class = UNSIGNED_ZERO,
            tag8_t sub_class = UNSIGNED_ZERO,
            InternalDataTypePolicy cell_data_type = InternalDataTypePolicy::UnsignedPCellDataType
        ) noexcept
        {
            const meta16_t desired_meta16 =  MakeInCellMeta_16t(
                cell_mode,
                cell_locality,
                cell_ownership,
                cell_data_type,
                cell_class,
                sub_class,
                static_cast<tag8_t>(cell_attribute)
            );

            if (desired_meta16 == META_16_SENTINAL)
            {
                return PACKED_CELL_SENTINAL;
            }
            

            if (cell_mode == PackedMode::MODEL32 || cell_mode == PackedMode::VALUE32)
            {
                return Compose32BitFamilyPackedCell(
                    static_cast<val32_t>(cell_value),
                    clock16,
                    desired_meta16
                );
            }
            else
            {
                return Compose48BitFamilyPackedCell(cell_value, desired_meta16);
            }
        }
    };
    

    struct PackedCell64_t  : public CoreCellDefinationn
    {
        static constexpr uint64_t BIT_FAMILY_48_SENTINAL = 0xFFFFFFFFFFFF;
        static constexpr uint64_t APC_FABRIC_SLOT_LIMIT = BIT_FAMILY_48_SENTINAL - 1;


        static constexpr bool IsThisCellValid(packed64_t packed_cell) noexcept
        {
            const PackedCell64_t::AuthoritiveCellView requested_cell_view = PackedCell64_t::GetAuthoritiveViewsForACell(packed_cell);
            if (!requested_cell_view.IsCellValid)
            {
                return false;
            }
            return true;
        }

        static constexpr bool IsValidFabricTable(FabricTableSegmentClasses table_class) noexcept
        {
            return table_class > FabricTableSegmentClasses::NONE &&
                table_class < FabricTableSegmentClasses::NULLNAN;
        }

        struct alignas(16) AuthoritiveCellView
        {
            packed64_t RawCell{0};

            meta16_t  InCellMeta16{0};

            AttributePolicy Attribute{AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL};

            OwnershipPolicy CellOwnership{OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER};

            LocalityPolicy LocalityOfCell{LocalityPolicy::UNASSIGNED_UNUSED_NANNULL};

            PackedMode CellMode{PackedMode::UNASSIGNED_UNUSED_NANNULL};

            APCPagedNodeSegmentClasses PageClass{APCPagedNodeSegmentClasses::NONE};

            FabricTableSegmentClasses FabricTableSegmentClass{FabricTableSegmentClasses::NONE};

            ContractOfConcurrency ContractOfValue{ContractOfConcurrency::UNASSIGNED_UNUSED_NANNULL};

            Model32Subclass SubClassOfModel32{Model32Subclass::UNASSIGNED_UNUSED_NANNULL};

            Model48Subclass SubClassOfModel48{Model48Subclass::UNASSIGNED_UNUSED_NANNULL};

            InternalDataTypePolicy CellValueDataType{InternalDataTypePolicy::UnsignedPCellDataType};

            clk16_t InCellClock16{CLOCK_16_SENTINAL};

            uint64_t Raw48BitInCellData{BIT_FAMILY_48_SENTINAL};

            val32_t Raw32BitInCellData{BIT_FAMILY_32_SENTINAL};

            bool IsCellValid{false};
            bool ValidatedView{false};

            size_t SlabIndexOfPackeCell{std::numeric_limits<size_t>::max()};

            constexpr bool IsThisPackedCellValidInRuntime() noexcept
            {
                ValidatedView = true;
                IsCellValid = false;
                if (LocalityOfCell == LocalityPolicy::FAULTY)
                {
                    return false;
                }

                auto IsKnownAPCClass  = [](APCPagedNodeSegmentClasses page_class) constexpr noexcept -> bool
                {
                    return page_class > APCPagedNodeSegmentClasses::NONE &&
                        page_class <= APCPagedNodeSegmentClasses::NULLNAN;
                };

                auto IsKnownValueContract = [](ContractOfConcurrency contract) constexpr noexcept -> bool
                {
                    switch (contract)
                    {
                    case ContractOfConcurrency::RAW_PRIVATE:
                    case ContractOfConcurrency::BOUNDED_RETRY_CAS_NO_CLAIMED:
                    case ContractOfConcurrency::CLAIMED_GURDED:
                    case ContractOfConcurrency::LAST_WRITIER_WIN_NO_CAS_RMW:
                        return true;
                    default:
                        return false;
                    }
                };

                auto IsKnownModel32Subclass = [](Model32Subclass sub_class) constexpr noexcept -> bool
                {
                    switch (sub_class)
                    {
                    case Model32Subclass::SELF_CLASS:
                    case Model32Subclass::LOW_OF_PAIRED_VERSIONED_CELL:
                    case Model32Subclass::HIGH_OF_PAIRED_VERSIONED_CELL:
                    case Model32Subclass::UNCLOCKED_1x8_PLUS_2x4:
                        return true;
                    default:
                        return false;
                    }
                };

                auto IsKnownModel48Subclass = [](Model48Subclass sub_class) constexpr noexcept -> bool
                {
                    switch (sub_class)
                    {
                    case Model48Subclass::SELF_CLASS:
                    case Model48Subclass::PURE_TIMER_48:
                    case Model48Subclass::SUBDIVISION16x3_INTERNAL_CELL_MODEL:
                    case Model48Subclass::FOUR_SUBDIVISION_2x16_AND_2x8:
                        return true;
                    default:
                        return false;
                    }
                };


                switch (CellOwnership)
                {
                case OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER:
                    if (
                        !IsKnownAPCClass(PageClass) ||
                        PageClass == APCPagedNodeSegmentClasses::NONE ||
                        PageClass == APCPagedNodeSegmentClasses::NULLNAN ||
                        FabricTableSegmentClass != FabricTableSegmentClasses::NONE
                    )
                    {
                        return false;
                    }
                    break;
                
                case OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC:
                    if (
                        !IsValidFabricTable(FabricTableSegmentClass) ||
                        FabricTableSegmentClass == FabricTableSegmentClasses::NONE ||
                        PageClass != APCPagedNodeSegmentClasses::NONE
                    )
                    {
                        return false;
                    }
                    break;
                
                default:
                    return false;
                }

                switch (CellMode)
                {
                    case PackedMode::MODEL32:
                    {

                        if (ContractOfValue != ContractOfConcurrency::UNASSIGNED_UNUSED_NANNULL || SubClassOfModel32 == Model32Subclass::UNASSIGNED_UNUSED_NANNULL)
                        {
                            return false;
                        }

                        if (!IsKnownModel32Subclass(SubClassOfModel32))
                        {
                            return false;
                        }
                        
                        const bool paired = SubClassOfModel32 == Model32Subclass::LOW_OF_PAIRED_VERSIONED_CELL || 
                                SubClassOfModel32 == Model32Subclass::HIGH_OF_PAIRED_VERSIONED_CELL;
                        if (paired && CellOwnership == OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER)
                        {
                            const bool valid_paired_class = PageClass == APCPagedNodeSegmentClasses::PAIRED_POINTER_IN_MEMORY ||
                                    PageClass == APCPagedNodeSegmentClasses::META_HEADER;
                            if (!valid_paired_class)
                            {
                                return false;
                            }
                            
                        }
                        
                        break;
                    }

                    case PackedMode::VALUE32:
                    {

                        if (ContractOfValue == ContractOfConcurrency::UNASSIGNED_UNUSED_NANNULL || SubClassOfModel32 != Model32Subclass::UNASSIGNED_UNUSED_NANNULL)
                        {
                            return false;
                        }
                        
                        if (!IsKnownValueContract(ContractOfValue))
                        {
                            return false;
                        }
                        break;
                    }

                    case PackedMode::MODEL48:
                    {
                        if (SubClassOfModel48 == Model48Subclass::UNASSIGNED_UNUSED_NANNULL  || ContractOfValue != ContractOfConcurrency::UNASSIGNED_UNUSED_NANNULL)
                        {
                            return false;
                        }

                        if (!IsKnownModel48Subclass(SubClassOfModel48))
                        {
                            return false;
                        }
                        if (SubClassOfModel48 != Model48Subclass::SELF_CLASS  && CellValueDataType != InternalDataTypePolicy::UnsignedPCellDataType)
                        {
                            return false;
                        }
                        break;
                    }

                    case PackedMode::VALUE48:
                    {
                        if (SubClassOfModel48 != Model48Subclass::UNASSIGNED_UNUSED_NANNULL  || ContractOfValue == ContractOfConcurrency::UNASSIGNED_UNUSED_NANNULL)
                        {
                            return false;
                        }

                        if (!IsKnownValueContract(ContractOfValue))
                        {
                            return false;
                        }
                        break;
                    }
                    
                    default:
                        return false;
                }

                IsCellValid = true;
                return true;
                
            }

        };

        static_assert(sizeof(AuthoritiveCellView) < SIZE_OF_CACHELINE);
        static_assert(alignof(AuthoritiveCellView) == CACHELINE_BOUNDRY);
        static_assert(std::is_trivially_copyable_v<AuthoritiveCellView>);
        static_assert(std::is_standard_layout_v<AuthoritiveCellView>);
        static_assert(std::is_trivially_destructible_v<AuthoritiveCellView>);

        /// @brief DEFAULTS 
        /// @param ContractOfConcurrency::CLAIMED_GURDED
        /// @param Model32_48Subclass::SELF_CLASS 
        /// @return 
        static constexpr packed64_t MakeDefaultAPCPayloadCellOnMode(
            PackedMode mode,
            APCPagedNodeSegmentClasses page_class = APCPagedNodeSegmentClasses::UNDEFINED,
            InternalDataTypePolicy dtype = InternalDataTypePolicy::UnsignedPCellDataType,
            LocalityPolicy cell_locality = LocalityPolicy::IDLE,
            uint64_t value = UNSIGNED_ZERO,
            clk16_t cell_clock16 = UNSIGNED_ZERO
        )
        {
            switch (mode)
            {
            case PackedMode::VALUE32:
                return PackedCell64_t::MakeTypedAPCValidPackedCell(
                    TypeFamily::VALUE32 ,
                    ContractOfConcurrency::CLAIMED_GURDED,
                    page_class,
                    cell_locality,
                    dtype, AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
                    value, cell_clock16
                );
            
            case PackedMode::VALUE48:
                return PackedCell64_t::MakeTypedAPCValidPackedCell(
                    TypeFamily::VALUE48 ,
                    ContractOfConcurrency::CLAIMED_GURDED,
                    page_class,
                    cell_locality,
                    dtype, AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
                    value
                );

            case PackedMode::MODEL32:
                return PackedCell64_t::MakeModeledAPCValidPackedCell(
                    ModelFamily::MODEL32,
                    static_cast<tag8_t>(Model32Subclass::SELF_CLASS),
                    page_class,
                    cell_locality,
                    dtype, AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
                    value,
                    cell_clock16
                );

            case PackedMode::MODEL48:
                return PackedCell64_t::MakeModeledAPCValidPackedCell(
                    ModelFamily::MODEL48,
                    static_cast<tag8_t>(Model48Subclass::SELF_CLASS),
                    page_class,
                    cell_locality,
                    dtype, AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
                    value
                );
            default:
                return PackedCell64_t::PACKED_CELL_SENTINAL;
            }
        }

        /// @brief Can Be used to create a new valid Packed CEll ->FOR: OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER
        /// @param cell_mode If Passing PackedMode enum just Cast static_cast<ModelFamily>(PackedMode existing_mode)
        /// @param sub_class If Passing PackedMode enum just Cast static_cast<tag8_t>(Model32Subclass/Model48Subclass desired SUB CLASS)
        /// @return VALID -> Packed Cell -> OR: UINT64_MAX
        static constexpr packed64_t MakeModeledAPCValidPackedCell(
            ModelFamily cell_model,
            tag8_t sub_class = static_cast<tag8_t>(Model32Subclass::SELF_CLASS),
            APCPagedNodeSegmentClasses page_class = APCPagedNodeSegmentClasses::UNDEFINED,
            LocalityPolicy cell_locality = LocalityPolicy::IDLE,
            InternalDataTypePolicy in_cell_value_data_type = InternalDataTypePolicy::UnsignedPCellDataType,
            AttributePolicy cell_attribute = AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
            uint64_t in_cell_value = UNSIGNED_ZERO,
            clk16_t in_cell_clk16 = UNSIGNED_ZERO

        ) noexcept
        {
            return MakeInitialValidGeneralPackedCell(
                static_cast<PackedMode>(cell_model), 
                cell_locality, 
                OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER, 
                static_cast<tag8_t>(page_class),
                in_cell_value_data_type, in_cell_value, in_cell_clk16,
                cell_attribute, sub_class
            );
        }

        /// @brief Can Be used to create a new valid Packed CEll ->FOR: OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC
        /// @param cell_mode If Passing PackedMode enum just Cast static_cast<ModelFamily>(PackedMode desired ModelFamily)
        /// @param sub_class If Passing PackedMode enum just Cast static_cast<tag8_t>(Model32Subclass/Model48Subclass desired SUB CLASS)
        /// @return VALID -> Packed Cell -> OR: UINT64_MAX
        static constexpr packed64_t MakeModeledFabricValidPackedCell(
            ModelFamily cell_model,
            tag8_t sub_class = static_cast<tag8_t>(Model32Subclass::SELF_CLASS),
            FabricTableSegmentClasses table_class = FabricTableSegmentClasses::GLOBAL_AND_CONFIG,
            LocalityPolicy cell_locality = LocalityPolicy::IDLE,
            InternalDataTypePolicy in_cell_value_data_type = InternalDataTypePolicy::UnsignedPCellDataType,
            AttributePolicy cell_attribute = AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
            uint64_t in_cell_value = UNSIGNED_ZERO,
            clk16_t in_cell_clk16 = UNSIGNED_ZERO

        ) noexcept
        {   
            return MakeInitialValidGeneralPackedCell(
                static_cast<PackedMode>(cell_model), 
                cell_locality, 
                OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC, 
                static_cast<tag8_t>(table_class),
                in_cell_value_data_type, in_cell_value, in_cell_clk16,
                cell_attribute, sub_class
            );
        }

        /// @brief Can be used to create Packed Cell of ANY: CLASS-> APCPagedNodeSegmentClasses / FabricTableSegmentClasses && SUB-CLASS-> Model32Subclass / Model48Subclass / ContractOfConcurrency
        /// @param cell_class Should derive from -> APCPagedNodeSegmentClasses / FabricTableSegmentClasses
        /// @param sub_class Should derive from -> Model32Subclass / Model48Subclass / ContractOfConcurrency
        /// @return VALID -> Packed Cell -> OR: UINT64_MAX
        static constexpr packed64_t MakeInitialValidGeneralPackedCell(
            PackedMode cell_mode,
            LocalityPolicy cell_locality,
            OwnershipPolicy cell_ownership,
            tag8_t cell_class,
            InternalDataTypePolicy in_cell_value_data_type,
            uint64_t in_cell_value,
            clk16_t in_cell_clk16,
            AttributePolicy cell_attribute,
            tag8_t sub_class
        ) noexcept
        {
            const packed64_t requested_cell = MakeAnUncheckedCell_(
                cell_mode, in_cell_value, in_cell_clk16,
                cell_attribute, cell_ownership, cell_locality,
                cell_class, sub_class,
                in_cell_value_data_type
            );

            const AuthoritiveCellView requested_cells_view = GetAuthoritiveViewsForACell(requested_cell);

            return requested_cells_view.IsCellValid ? requested_cell : PACKED_CELL_SENTINAL;
        }


        /// @brief Can Be used to create a new valid Packed CEll ->FOR: OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC
        /// @return VALID -> Packed Cell -> OR: UINT64_MAX
        static constexpr packed64_t MakeTypedFabricValidPackedCell(
            TypeFamily cell_model,
            ContractOfConcurrency sub_class = ContractOfConcurrency::BOUNDED_RETRY_CAS_NO_CLAIMED,
            FabricTableSegmentClasses table_class = FabricTableSegmentClasses::GLOBAL_AND_CONFIG,
            LocalityPolicy cell_locality = LocalityPolicy::IDLE,
            InternalDataTypePolicy in_cell_value_data_type = InternalDataTypePolicy::UnsignedPCellDataType,
            AttributePolicy cell_attribute = AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
            uint64_t in_cell_value = UNSIGNED_ZERO,
            clk16_t in_cell_clk16 = UNSIGNED_ZERO
        ) noexcept
        {
            return MakeInitialValidGeneralPackedCell(
                static_cast<PackedMode>(cell_model), 
                cell_locality, 
                OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC, 
                static_cast<tag8_t>(table_class),
                in_cell_value_data_type, in_cell_value, 
                in_cell_clk16, cell_attribute, 
                static_cast<tag8_t>(sub_class)
            );
        }

        /// @brief Can Be used to create a new valid Packed CEll ->FOR: OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC
        /// @return VALID -> Packed Cell -> OR: UINT64_MAX
        static constexpr packed64_t MakeTypedAPCValidPackedCell(
            TypeFamily cell_model,
            ContractOfConcurrency sub_class = ContractOfConcurrency::BOUNDED_RETRY_CAS_NO_CLAIMED,
            APCPagedNodeSegmentClasses page_class = APCPagedNodeSegmentClasses::UNDEFINED,
            LocalityPolicy cell_locality = LocalityPolicy::IDLE,
            InternalDataTypePolicy in_cell_value_data_type = InternalDataTypePolicy::UnsignedPCellDataType,
            AttributePolicy cell_attribute = AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
            uint64_t in_cell_value = UNSIGNED_ZERO,
            clk16_t in_cell_clk16 = UNSIGNED_ZERO
        ) noexcept
        {
            return MakeInitialValidGeneralPackedCell(
                static_cast<PackedMode>(cell_model), 
                cell_locality, 
                OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER, 
                static_cast<tag8_t>(page_class),
                in_cell_value_data_type, in_cell_value, 
                in_cell_clk16, cell_attribute, 
                static_cast<tag8_t>(sub_class)
            );
        }


        /// @brief Should i type restrict just for VALUE32
        template <typename PCDT>
        static constexpr  packed64_t ComposeTypedModeValue32Cell(PCDT value32, clk16_t clock16, meta16_t meta16) noexcept
        {
            static_assert(PackedCellTypeBridge<PCDT>::IS_SUPPORTED_TYPE, "Unsupported Cell type");
            static_assert(PackedCellTypeBridge<PCDT>::FITS_MODE_32, "Value type is too large to fite MODEL32");

            const PackedMode packed_mode = static_cast<PackedMode>(ExtractCellModeFromMETA16_U_(meta16));
            if(
                packed_mode != PackedMode::MODEL32 && packed_mode != PackedMode::VALUE32 
            )
            {
                return MakeFaultyCell();
            }

            if (static_cast<InternalDataTypePolicy>(ExtractValueDataTypeFromMETA16_U_(meta16))!= BridgeOfPackedCellDataType_v<PCDT> )
            {
                return MakeFaultyCell();
            }
            uint64_t value_casted_bit = BitCastMaybe<val32_t>(value32);
            return Compose32BitFamilyPackedCell(value_casted_bit, clock16, meta16);
            
        }

        /// @brief Should i type restrict just for VALUE48
        template <typename PCDT>
        static constexpr  packed64_t ComposeTypedModeValue48Cell(PCDT value_clock48, meta16_t meta16) noexcept
        {
            static_assert(PackedCellTypeBridge<PCDT>::IS_SUPPORTED_TYPE, "Unsupported Cell type");
            static_assert(PackedCellTypeBridge<PCDT>::FITS_MODE_48, "Value type is too large to fite MODEL48");

            const PackedMode packed_mode = static_cast<PackedMode>(ExtractCellModeFromMETA16_U_(meta16));
            if(
                packed_mode != PackedMode::MODEL48 && packed_mode != PackedMode::VALUE48
            )
            {
                return MakeFaultyCell();
            }

            if (static_cast<InternalDataTypePolicy>(ExtractValueDataTypeFromMETA16_U_(meta16)) != BridgeOfPackedCellDataType_v<PCDT> )
            {
                return MakeFaultyCell();
            }
            uint64_t value_casted_bit = BitCastMaybe<uint64_t>(value_clock48);
            return Compose48BitFamilyPackedCell(value_casted_bit & MaskLowNBits(FAMILY_48_BIT_LEN), meta16);
            
        }

        static constexpr AuthoritiveCellView GetAuthoritiveViewsForACell(packed64_t packed_cell) noexcept
        {
            AuthoritiveCellView out_packed_cell_view{};
            out_packed_cell_view.ValidatedView = true;

            if (packed_cell == PACKED_CELL_SENTINAL || packed_cell == UNSIGNED_ZERO)
            {
                out_packed_cell_view.RawCell = packed_cell;
                out_packed_cell_view.IsCellValid = false;
                return out_packed_cell_view;
            }
            
            const meta16_t meta16 = ExtractMeta16fromPackedCell(packed_cell);
            out_packed_cell_view.RawCell = packed_cell;
            out_packed_cell_view.InCellMeta16 = meta16;
            out_packed_cell_view.Attribute = static_cast<AttributePolicy>(ExtractAttributeFromMeta16_(meta16));
            out_packed_cell_view.CellOwnership =  static_cast<OwnershipPolicy>(ExtractOwnershipFromMeta16_(meta16));
            out_packed_cell_view.LocalityOfCell = static_cast<LocalityPolicy>(ExtractLocalityFromMETA16_U_(meta16));
            out_packed_cell_view.CellMode = static_cast<PackedMode>(ExtractCellModeFromMETA16_U_(meta16));

            if (out_packed_cell_view.CellOwnership == OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER)
            {
                out_packed_cell_view.PageClass = static_cast<APCPagedNodeSegmentClasses>(ExtractRegionFromMeta16_(meta16));
            }
            else
            {
                out_packed_cell_view.FabricTableSegmentClass = static_cast<FabricTableSegmentClasses>(ExtractRegionFromMeta16_(meta16));
            }

            if (out_packed_cell_view.CellMode == PackedMode::MODEL32 || 
                out_packed_cell_view.CellMode == PackedMode::VALUE32
            )
            {
                if (out_packed_cell_view.CellMode == PackedMode::MODEL32)
                {
                    out_packed_cell_view.SubClassOfModel32 = static_cast<Model32Subclass>(ExtractSubClassOrContractFromMETA16_U_(meta16));
                }
                else
                {
                    out_packed_cell_view.ContractOfValue = static_cast<ContractOfConcurrency>(ExtractSubClassOrContractFromMETA16_U_(meta16));
                }
                out_packed_cell_view.InCellClock16 = ExtractClk16(packed_cell);
                out_packed_cell_view.Raw32BitInCellData = ExtractRaw32FamilyBits(packed_cell);
            }
            else
            {
                if (out_packed_cell_view.CellMode == PackedMode::MODEL48)
                {
                    out_packed_cell_view.SubClassOfModel48 = static_cast<Model48Subclass>(ExtractSubClassOrContractFromMETA16_U_(meta16));
                }
                else
                {
                    out_packed_cell_view.ContractOfValue = static_cast<ContractOfConcurrency>(ExtractSubClassOrContractFromMETA16_U_(meta16));
                }

                out_packed_cell_view.Raw48BitInCellData = ExtractRaw48FamilyBits(packed_cell);
            }

            out_packed_cell_view.CellValueDataType = static_cast<InternalDataTypePolicy>(ExtractValueDataTypeFromMETA16_U_(meta16));
            out_packed_cell_view.IsThisPackedCellValidInRuntime();
            return out_packed_cell_view;      
        }



    };


}
