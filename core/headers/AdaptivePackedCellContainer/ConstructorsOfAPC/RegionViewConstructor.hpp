#pragma once
#include "FabricToAPCLinker.hpp"
#include <span>

namespace BidirectionalInMemGraph
{

    template<class DType>
    class RegionView
    {
    public:
        using SD = SchemaDefinition;

    private:
        SD::SchemaProtocols Protocol_{SD::SchemaProtocols::PRIVATE_REGION};
        std::span<DType> Elements_{};
    
    public:
        constexpr RegionView() noexcept = default;

        constexpr RegionView(
            std::span<DType> elements,
            SD::SchemaProtocols protocol
        ) noexcept:
            Elements_(elements),
            Protocol_(protocol)
        {}

        constexpr bool IsValid() const noexcept
        {
            return !Elements_.empty();
        }

        constexpr size_t Size() const noexcept
        {
            return Elements_.size();
        }

        constexpr SD::SchemaProtocols GetProtocol() const noexcept
        {
            return Protocol_;
        }

        std::optional<std::span<DType>> RawMutableSpan() noexcept
        {
            if (Protocol_ != SD::SchemaProtocols::PRIVATE_REGION)
            {
                return std::nullopt;
            }
            
            return Elements_;
        }

        DType AtomicLoad(size_t idx, std::memory_order mem_order = std::memory_order_acquire) const noexcept
        {
            if (
                Protocol_ != SD::SchemaProtocols::ATOMIC_WORD_ARRAY ||
                idx >= Elements_.size()
            )
            {
                return DType{};
            }

            return std::atomic_ref<DType>(Elements_[idx]).load(mem_order);
            
        }


        bool AtomicStore(
            size_t idx,
            DType value,
            std::memory_order order = std::memory_order_release
        ) noexcept
        {
            if (
                Protocol_ != SD::SchemaProtocols::ATOMIC_WORD_ARRAY ||
                idx >= Elements_.size()
            )
            {
                return false;
            }

            std::atomic_ref<DType>(Elements_[idx]).store(value, order);
            return true;
        }

        bool AtomicCompareExchangeStrong(
            size_t idx,
            DType& expected,
            DType desired,
            std::memory_order success = std::memory_order_acq_rel,
            std::memory_order failure = std::memory_order_acquire
        ) noexcept
        {
            if (
                Protocol_ != SD::SchemaProtocols::ATOMIC_WORD_ARRAY ||
                idx >= Elements_.size()
            )
            {
                return false;
            }

            return std::atomic_ref<DType>(Elements_[idx]).compare_exchange_strong(
                expected,
                desired,
                success,
                failure
            );
        }

    };


    class RegionViewConstructor : public FabricToAPCLinker
    {
    private:
        bool ResolveRegionView_(
            MacroColumnOfAPC column_name,
            ResolveRegionBiteView& out
        ) noexcept;

    public:

        template<class DType>
        std::optional<RegionView<DType>> BuildAViewOverRegion(MacroColumnOfAPC macro_column) noexcept;

    };
    
    
    
}