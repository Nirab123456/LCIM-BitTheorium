#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"

namespace PredictedAdaptedEncoding
{
    bool FabricToAPCLinker::AtomicallyReadLongLongAPCUnit(
        uint64_t idx,
        uint64_t& return_value
    ) noexcept
    {
        if (!IsValidAPCRange(idx, 1))
        {
            return false;
        }

        return FabricOwnerPtr_->AtomicallyLoadReadAUnit(
            RangeOfThisAPCInSlab_.BeginIndex + idx,
            return_value
        );
    }

    bool FabricToAPCLinker::BindExternalRawFabricBacking_(
        uint64_t* words_raw,
        uint32_t cell_count,
        VagueTemoraryPremativeFabric* fabric_owner,
        uint64_t fabric_slot_idx
    ) noexcept
    {
        if (
            !words_raw ||
            !fabric_owner ||
            !APCDataStructure::IsCapacityOfAPCValid(cell_count) ||
            !APCDataStructure::IsValid32BitAPCUnit(fabric_slot_idx)
        )
        {
            return false;
        }
        CapacityOfThisAPC_ = cell_count;
        FabricOwnerPtr_ = fabric_owner;
        IdxOfThisAPCInFabric_ = fabric_slot_idx;
        const APCSegmentPoolRange range_of_this_apc = FabricOwnerPtr_->GetSegmentPoolBegainEndForSingleAPCDescription(fabric_slot_idx);
        if (
            !range_of_this_apc.IsValid ||
            range_of_this_apc.EndIndex - range_of_this_apc.BeginIndex != cell_count
        )
        {
            return false;
        }
        RangeOfThisAPCInSlab_ = range_of_this_apc;
        return true;
    }


    void FabricToAPCLinker::ReleseFabricBindingOnly_() noexcept
    {
        APCSegmentPoolRange default_null_range{};
        RangeOfThisAPCInSlab_ = default_null_range;
        CapacityOfThisAPC_ = UNSIGNED_ZERO;
        FabricOwnerPtr_ = nullptr;
        IdxOfThisAPCInFabric_ = FABRIC_CELL_COUNT;
    }

    void FabricToAPCLinker::SetFabricOwnerForGlobalAPC(VagueTemoraryPremativeFabric* fabric_owner) noexcept
    {
        FabricOwnerPtr_ = fabric_owner;
    }

    bool FabricToAPCLinker::AtomicallyCopyFromBufferToAPC(
        uint32_t starting_idx_in_apc,
        uint8_t sequential_number_of_cells,
        const uint64_t* source_cells
    ) noexcept
    {
        if (
            !RangeOfThisAPCInSlab_.IsValid ||
            !IsValidAPCRange(starting_idx_in_apc, sequential_number_of_cells)
        )
        {
            return false;
        }

        return FabricOwnerPtr_->AtomicallyCopyFromBufferToFabric(
            (RangeOfThisAPCInSlab_.BeginIndex + starting_idx_in_apc), 
            sequential_number_of_cells, 
            source_cells
        );
    }

    bool FabricToAPCLinker::ForceCopyToAPCFromBuffer(
        uint32_t starting_idx_in_apc,
        uint32_t sequential_number_of_cells,
        const uint64_t* source_cells
    ) noexcept
    {
        if (
            !RangeOfThisAPCInSlab_.IsValid ||
            !IsValidAPCRange(starting_idx_in_apc, sequential_number_of_cells)
        )
        {
            return false;
        }
        
        return FabricOwnerPtr_->ForceNxLenMemCopy(
            (RangeOfThisAPCInSlab_.BeginIndex + starting_idx_in_apc), 
            sequential_number_of_cells, 
            source_cells
        );
    }

    bool FabricToAPCLinker::CopyFromAPCToBuffer(
        uint32_t starting_idx_in_apc,
        uint32_t sequential_number_of_cells,
        uint64_t* return_buffer,
        bool atomic_required
    ) noexcept
    {
        if (
            !RangeOfThisAPCInSlab_.IsValid ||
            !IsValidAPCRange(starting_idx_in_apc, sequential_number_of_cells)
        )
        {
            return false;
        }
        
        return FabricOwnerPtr_->ReadASnapShotFromSlab(
            (RangeOfThisAPCInSlab_.BeginIndex + starting_idx_in_apc), 
            sequential_number_of_cells, 
            return_buffer,
            atomic_required
        );
    }

    bool FabricToAPCLinker::CompareExchangeStrongFromAPC(
        size_t apc_idx, 
        uint64_t& expected_unit, 
        uint64_t desired_unit,
        std::memory_order mem_order_success,
        std::memory_order mem_order_failure
    ) noexcept
    {
        if (
            !RangeOfThisAPCInSlab_.IsValid ||
            !IsValidAPCRange(apc_idx, 1u)
        )
        {
            return false;
        }

        return FabricOwnerPtr_->CompareExchangeStrongFromFabric(
            RangeOfThisAPCInSlab_.BeginIndex + apc_idx,
            expected_unit,
            desired_unit,
            mem_order_success,
            mem_order_failure
        );
    }

    std::optional<uint64_t> FabricToAPCLinker::HoldStateOfIdentyFingerprint(
        InstallAxisToBuffer::FingerprintHashState desired_state
    ) noexcept
    {
        using IA = InstallAxisToBuffer;

        IA::BufferOfAPCIdentity identity_buffer{};

        bool copy_ok = CopyFromAPCToBuffer(
            static_cast<uint8_t>(HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT),
            APCDataStructure::TotalIdentityUnitCount(),
            identity_buffer.data(),
            true
        );

        if (
            !copy_ok || 
            !IA::ValidateDefaultIdentity(identity_buffer) ||
            !IA::IsHoldFingerprintState(desired_state)
        )
        {
            return std::nullopt;
        }
        
        uint64_t sealed_fingerprint = UNSIGNED_ZERO;

        if (IA::GetStateFingerprint(identity_buffer, &sealed_fingerprint) != IA::FingerprintHashState::VALID)
        {
            return std::nullopt;
        }
        
        bool comp_exg_ok = CompareExchangeStrongFromAPC(
            static_cast<uint8_t>(HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT),
            sealed_fingerprint,
            desired_state == IA::FingerprintHashState::WRITE_LOCK ?
                IA::IDENTY_FINGERPRINT_WRITE_LOCK : IA::IDENTITY_FINGERPRINT_CONSUMED
        );
        return comp_exg_ok ?
            std::optional<uint64_t>{sealed_fingerprint} : 
            std::nullopt;
    }

    bool FabricToAPCLinker::RestorIdentityFingerprint(
        uint64_t sealed_fingerprint,
        InstallAxisToBuffer::FingerprintHashState current_state
    ) noexcept
    {
        using IFS = InstallAxisToBuffer::FingerprintHashState;

        const uint8_t fp_idx = static_cast<uint8_t>(HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT);
        uint64_t current_fp = UNSIGNED_ZERO;
        if (!AtomicallyReadLongLongAPCUnit(fp_idx, current_fp))
        {
            return false;
        }
        
        const IFS sealed_fp_state = InstallAxisToBuffer::StateOfIdentityFingerprint(sealed_fingerprint);
        const IFS current_fp_state = InstallAxisToBuffer::StateOfIdentityFingerprint(current_fp);

        if (
            sealed_fp_state == IFS::VALID &&
            current_fp_state == current_state
        )
        {
            return CompareExchangeStrongFromAPC(
                fp_idx,
                current_fp,
                sealed_fingerprint
            );
        }
        return false;
    }
}