#pragma once
#include "HeaderOrchestrator.hpp"
#include <span>


namespace BidirectionalInMemGraph
{

    struct ResolveRegionBiteView
    {
        std::span<std::byte> Bytes{};
        LayoutBoundsOrchestrator::LayoutCarrier layout{};
        SchemaDefinition::RegionSchemaRecord Schema{};

        constexpr bool IsValid() const noexcept
        {
            return 
                layout.IsValid &&
                Schema.IsValidSchema &&
                !Bytes.empty();
        }

        constexpr size_t ByteCount() const noexcept
        {
            return Bytes.size_bytes();
        }
    };
    static_assert(sizeof(ResolveRegionBiteView) <= 5 * sizeof(uint64_t));
    

    struct APCStorageGeometry
    {
        static constexpr size_t BytesPerLocalAddressUnit() noexcept
        {
            return sizeof(uint64_t);
        }

        static constexpr size_t ByteOffsetOfLocalIndex(uint32_t local_idx) noexcept
        {
            return static_cast<size_t>(local_idx) * BytesPerLocalAddressUnit();
        }

        static constexpr size_t ByteCountOfLOcalSpan(uint32_t local_span) noexcept
        {
            return static_cast<size_t>(local_span) * BytesPerLocalAddressUnit();
        }

        template<class DType>
        static constexpr bool CanInstallTypedSpan(const ResolveRegionBiteView& region) noexcept
        {
            static_assert(std::is_trivially_copyable_v<DType>);
            const std::optional<SchemaOrchestrator::DataTypeOfMacroColumn> expected_dtype = SchemaDefinition::CppTypeToRegionDType<DType>();

            if (
                !region.IsValid ||
                !expected_dtype.has_value() ||
                region.Schema.Dtype != expected_dtype.value() ||
                region.ByteCount() < sizeof(DType) ||
                (region.ByteCount() % sizeof(DType)) != UNSIGNED_ZERO
            )
            {
                return false;
            }

            const uintptr_t address = reinterpret_cast<uintptr_t>(region.Bytes.data);

            return (address % alignof(DType)) == UNSIGNED_ZERO;
            
        }

        template<class Dtype>
        static constexpr bool CanInstallAtomicSpan(const ResolveRegionBiteView& region) noexcept
        {
            if (!CanInstallTypedSpan<Dtype>(region))
            {
                return false;
            }
            
            const uintptr_t address = reinterpret_cast<uintptr_t>(region.Bytes.data());

            constexpr size_t required_alignment = std::atomic_ref<Dtype>::required_alignment;
            
            if ((address % required_alignment) != UNSIGNED_ZERO)
            {
                return false;
            }

            return (sizeof(Dtype) % required_alignment) == UNSIGNED_ZERO;
            
        }

        template<class DType>
        static constexpr bool InitializeFreshRegionObject(
            const ResolveRegionBiteView& region
        )noexcept
        {
            static_assert(std::is_trivially_copyable_v<DType>);
            static_assert(std::is_default_constructible_v<DType>);

            if (!CanInstallTypedSpan<DType>(region))
            {
                return false;
            }
            
            DType* type_base = reinterpret_cast<DType*>(region.Bytes.data());

            const size_t count = region.ByteCount / sizeof(DType);

            for (size_t i = 0; i < count; i++)
            {
                std::construct_at(type_base + i);
            }
            
            return true;
        }
    };

    
}