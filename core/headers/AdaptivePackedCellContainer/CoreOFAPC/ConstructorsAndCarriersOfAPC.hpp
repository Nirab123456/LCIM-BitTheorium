
#pragma once 
#include <array>
#include <utility>
#include "APCEnums.h"

namespace PredictedAdaptedEncoding
{

struct APCDataStructure
{

    static constexpr size_t METACELL_COUNT = 96;
    static constexpr uint32_t BRANCH_MAGIC = 0x41504342u;//big-endian
    static constexpr uint32_t EOF_HEADER = 0x72616600;//big-endian
    static constexpr uint8_t BRANCH_VERSION = 1u;
    static constexpr packed64_t PACKED_CELL_SENTENAL = UINT64_MAX;
    static constexpr packed64_t APC_META_CELL_SENTINAL = PackedCell64_t::BIT_FAMILY_48_SENTINAL;
    static constexpr uint32_t APC_ALL_INDEX_LIMIT = UINT16_MAX - 1;
    static constexpr uint16_t APC_INDEX_SENTINAL = UINT16_MAX;
    static constexpr size_t APC_CACHELINE_SIZE = 64u;
    static constexpr size_t APC_SIZE_SENTINAL = SIZE_MAX;


    static constexpr bool IsThisIndexValidForAPC(uint32_t index) noexcept
    {
        if (index < APC_INDEX_SENTINAL)
        {
            return true;
        }
        return false;
    }

    static constexpr bool ThisVersionValid(uint32_t version) noexcept
    {
        if (version < UINT8_MAX)
        {
            return true;
        }
        return false;
    }

    static constexpr bool IsCapacityOfAPCValid(uint16_t capacity) noexcept
    {
        return capacity >= MINIMUM_APC_CAPACITY &&
            IsThisIndexValidForAPC(capacity);
    }


    /// @brief Will return PACKED_CELL_SENTINAL if SUBCLASS SUBDIVISION EXIST 
    static constexpr uint64_t AutoExtractDataOfAValidAPCCell(
        packed64_t packed_cell, 
        bool is_claimed_cell_valid = false,
        PackedCell64_t::AuthoritiveCellView* return_auth_view_ptr = nullptr
    ) noexcept
    {
        const PackedCell64_t::AuthoritiveCellView desired_auth_view = PackedCell64_t::GetAuthoritiveViewsForACell(packed_cell);
        if (!desired_auth_view.IsCellValid && desired_auth_view.CellOwnership != OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER)
        {
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }

        if (return_auth_view_ptr)
        {
            *return_auth_view_ptr = desired_auth_view;
        }

        if (!is_claimed_cell_valid && desired_auth_view.LocalityOfCell == LocalityPolicy::CLAIMED)
        {
            return false;
        }

        switch (desired_auth_view.CellMode)
        {
        case PackedMode::VALUE32:
            return desired_auth_view.Raw32BitInCellData;

        case PackedMode::VALUE48:
            return desired_auth_view.Raw48BitInCellData;

        case PackedMode::MODEL32:
            if (
                desired_auth_view.SubClassOfModel32 == Model32Subclass::SELF_CLASS
            )
            {
                return desired_auth_view.Raw32BitInCellData;
            }
            return PackedCell64_t::PACKED_CELL_SENTINAL;

        case PackedMode::MODEL48:
            if (
                desired_auth_view.SubClassOfModel48 == Model48Subclass::SELF_CLASS
            )
            {
                return desired_auth_view.Raw48BitInCellData;
            }
            return PackedCell64_t::PACKED_CELL_SENTINAL;

        default:
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }
        
    }


    static constexpr packed64_t ReplaceValueInAPCTypeFamilyCell(packed64_t packed_cell, uint64_t desired_value, bool is_claimed_cell_valid = false) noexcept
    {
        const PackedCell64_t::AuthoritiveCellView desired_cell_auth_view = PackedCell64_t::GetAuthoritiveViewsForACell(packed_cell);
        if (!desired_cell_auth_view.IsCellValid)
        {
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }

        if (!is_claimed_cell_valid && desired_cell_auth_view.LocalityOfCell == LocalityPolicy::CLAIMED)
        {
            return false;
        }

        switch (desired_cell_auth_view.CellMode)
        {
        case PackedMode::VALUE32:
            return PackedCell64_t::Compose32BitFamilyPackedCell(static_cast<uint32_t>(desired_value) & MaskLeftOverBitsUntil64(VALBITS), desired_cell_auth_view.InCellClock16, desired_cell_auth_view.InCellMeta16);

        case PackedMode::VALUE48:
            return PackedCell64_t::Compose48BitFamilyPackedCell(desired_cell_auth_view.Raw48BitInCellData, desired_cell_auth_view.InCellMeta16);
        default:
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }

    }

protected:
        static constexpr void FreeAlignedRawPackedCells_(packed64_t* backing_ptr) noexcept
        {
            if (!backing_ptr)
            {
                return;
            }
            ::operator delete[](static_cast<void*>(backing_ptr), std::align_val_t{APC_CACHELINE_SIZE});
        }


};



    struct APCBackingCellAtomicRefViewTemp
    {
        packed64_t* CellPtr{nullptr};

        APCBackingCellAtomicRefViewTemp() noexcept = default;
        explicit APCBackingCellAtomicRefViewTemp(packed64_t* apc_ptr) noexcept :
            CellPtr(apc_ptr)
        {}

        bool IsValid() const noexcept
        {
            return CellPtr != nullptr;
        }

        packed64_t load(std::memory_order order = MoLoad_) const noexcept
        {
            if (!CellPtr)
            {
                return PackedCell64_t::PACKED_CELL_SENTINAL;
            }
            std::atomic_ref<packed64_t> cell_ptr_ref(*CellPtr);
            return cell_ptr_ref.load(order);
        }

        void store(packed64_t desired, std::memory_order order = MoStoreSeq_) const noexcept
        {
            if (!CellPtr)
            {
                return;
            }
            std::atomic_ref<packed64_t> cell_ptr_ref(*CellPtr);
            cell_ptr_ref.store(desired, order);
        }

        bool compare_exchange_strong(
            packed64_t& expected,
            packed64_t desired,
            std::memory_order success = OnExchangeSuccess,
            std::memory_order failure = OnExchangeFailure
        ) const noexcept
        {
            if (!CellPtr)
            {
                return false;
            }
            std::atomic_ref<packed64_t> cell_ptr_ref(*CellPtr);
            return cell_ptr_ref.compare_exchange_strong(expected, desired, success, failure);
        }

        bool compare_exchange_weak(
            packed64_t& expected,
            packed64_t desired,
            std::memory_order success = OnExchangeSuccess,
            std::memory_order failure = OnExchangeFailure
        ) const noexcept
        {
            if (!CellPtr)
            {
                return false;
            }
            std::atomic_ref<packed64_t> cell_ptr_ref(*CellPtr);
            return cell_ptr_ref.compare_exchange_weak(expected, desired, success, failure);
        }

        void notify_all() const noexcept
        {
            if (!CellPtr)
            {
                return;
            }
    #if defined(__cpp_lib_atomic_wait)
            std::atomic_ref<packed64_t> cell_ptr_ref(*CellPtr);
            cell_ptr_ref.notify_all();
    #endif
        }

        void notify_one() const noexcept
        {
            if (!CellPtr)
            {
                return;
            }
    #if defined(__cpp_lib_atomic_wait)
            std::atomic_ref<packed64_t> cell_ptr_ref(*CellPtr);
            cell_ptr_ref.notify_one();
    #endif
        }

        static constexpr APCBackingCellAtomicRefViewTemp* BuildBackingViewOverCells_(packed64_t* raw_ptr, size_t count) noexcept
        {
            if (!raw_ptr || count == UNSIGNED_ZERO)
            {
                return nullptr;
            };

            void* raw_view = nullptr;
            try
            {
                raw_view = ::operator new[](sizeof(APCBackingCellAtomicRefViewTemp) * count, std::align_val_t{alignof(APCBackingCellAtomicRefViewTemp)});
            }
            catch(...)
            {
                return nullptr;
            }
            
            auto* view = static_cast<APCBackingCellAtomicRefViewTemp*>(raw_view);
            for (size_t i = 0; i < count; i++)
            {
                new(&view[i]) APCBackingCellAtomicRefViewTemp(&raw_ptr[i]);
            }
            return view;
        }

        static constexpr void FreeBackingView_(APCBackingCellAtomicRefViewTemp* view, size_t count) noexcept
        {
            if (!view)
            {
                return;
            }

            for (size_t i = 0; i < count; i++)
            {
                view[i].~APCBackingCellAtomicRefViewTemp();
            }
            ::operator delete[](
                static_cast<void*>(view), std::align_val_t{alignof(APCBackingCellAtomicRefViewTemp)}
            );
        }

    };

static_assert(sizeof(APCBackingCellAtomicRefViewTemp) == sizeof(packed64_t));
static_assert(alignof(APCBackingCellAtomicRefViewTemp) == alignof(packed64_t));


struct Timer48
{
    static constexpr uint64_t TicksPerSec_ = A_BILLION;

    static uint64_t NowTicks() noexcept
    {
        using  cns = std::chrono::nanoseconds;
        auto d = std::chrono::steady_clock::now().time_since_epoch();
        uint64_t ns_count = static_cast<uint64_t>(std::chrono::duration_cast<cns>(d).count());
        return ns_count & MaskLeftOverBitsUntil64(FAMILY_48_BIT_LEN);
    }

    static uint16_t NowClock16() noexcept
    {
        return static_cast<uint16_t>(NowTicks() & MaskLeftOverBitsUntil64(LOW16_BIT_LEN));
    }
};

}