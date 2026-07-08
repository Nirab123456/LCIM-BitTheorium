#pragma once 
#include <array>
#include <utility>
#include "../PackedCore/CellExtractorAndSetter.hpp"

namespace PredictedAdaptedEncoding
{
    struct CellValidators : public PackedCellSetters
    {
        static constexpr bool IsKnownOwner(OwnershipPolicy owner) noexcept
        {
            return owner == OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER ||
                owner == OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC;
        }

        static constexpr bool IsKnownLocality(LocalityPolicy locality) noexcept
        {
            return locality == LocalityPolicy::IDLE ||
                locality == LocalityPolicy::PUBLISHED ||
                locality == LocalityPolicy::CLAIMED ||
                locality == LocalityPolicy::FAULTY;
        }

        static constexpr bool IsKnownDataType(InternalDataTypePolicy dtype) noexcept
        {
            return dtype == InternalDataTypePolicy::CharPCellDataType ||
                   dtype == InternalDataTypePolicy::IntPCellDataType ||
                   dtype == InternalDataTypePolicy::FloatPCellDataType ||
                   dtype == InternalDataTypePolicy::UnsignedPCellDataType;
        }

        static constexpr bool IsKnownAttribute(AttributePolicy attribute) noexcept
        {
            return attribute == AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL ||
                   attribute == AttributePolicy::DEPENDENT_OR_INSTRUCTION_CELL ||
                   attribute == AttributePolicy::INSTRUCTION_RAW64_NEXT ||
                   attribute == AttributePolicy::INSTRUCTION_RAW64_EOF;
        }

        static constexpr bool IsKnownConcurrencyContractForValue(ContractOfConcurrency contract) noexcept
        {
            return contract == ContractOfConcurrency::RAW_PRIVATE ||
                   contract == ContractOfConcurrency::BOUNDED_RETRY_CAS_NO_CLAIMED ||
                   contract == ContractOfConcurrency::CLAIMED_GURDED ||
                   contract == ContractOfConcurrency::LAST_WRITIER_WIN_NO_CAS_RMW;
        }

        static constexpr bool IsKnownModel32Subclass(Model32Subclass subclass) noexcept
        {
            return subclass == Model32Subclass::SELF_CLASS ||
                   subclass == Model32Subclass::LOW_OF_PAIRED_VERSIONED_CELL ||
                   subclass == Model32Subclass::HIGH_OF_PAIRED_VERSIONED_CELL ||
                   subclass == Model32Subclass::UNCLOCKED_1x8_PLUS_2x4;
        }

        static constexpr bool IsKnownModel48Subclass(Model48Subclass subclass) noexcept
        {
            return subclass == Model48Subclass::SELF_CLASS ||
                   subclass == Model48Subclass::PURE_TIMER_48 ||
                   subclass == Model48Subclass::SUBDIVISION16x3_INTERNAL_CELL_MODEL ||
                   subclass == Model48Subclass::FOUR_SUBDIVISION_2x16_AND_2x8;
        }

        static constexpr bool IsKnowAPCRegion(APCPagedNodeSegmentClasses region) noexcept
        {
            return region > APCPagedNodeSegmentClasses::NONE && region < APCPagedNodeSegmentClasses::NULLNAN;
        }

        static constexpr bool IsKnownFabricRegion(FabricTableSegmentClasses region) noexcept
        {
            return region > FabricTableSegmentClasses::NONE && region < FabricTableSegmentClasses::NULLNAN;
        }
    };
    
}