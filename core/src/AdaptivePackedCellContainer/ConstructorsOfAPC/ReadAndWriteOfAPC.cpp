#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"

namespace BidirectionalInMemGraph
{

    bool ReadAndWriteOfAPC::ReadCompleateMetaHeaderAtomically_(HeaderOrchestrator::APCMetaBuffer& a_default_buffer) noexcept
    {
        if (!RangeOfThisAPCInSlab_.IsValid)
        {
            return false;
        }

        if (!CopyFromAPCToBuffer(UNSIGNED_ZERO, APCDataStructure::METACELL_COUNT, a_default_buffer.data()))
        {
            return false;
        }
        
        return true;
    }

    bool ReadAndWriteOfAPC::ReadCompleatLayoutBuffer_(
        LayoutBoundsOrchestrator::TrackingBufferOfAPC& layout_buffer,
        bool atomic_required 
    ) noexcept
    {
        BufferConfForTracking::BuildNullTrackingBuffer(layout_buffer);
        if (!CopyFromAPCToBuffer(
            APCDataStructure::LayoutBufferBegainInMetaIndecies(),
            APCDataStructure::CountOfMacroColumn(),
            layout_buffer.data(),
            atomic_required
        ))
        {
            return false;
        }
        
        return LayoutBoundsOrchestrator::ValidateALayoutBuffer(layout_buffer, CapacityOfThisAPC_);
    }

    bool ReadAndWriteOfAPC::InitiateAPCMetaHeader(
        uint32_t total_capacity,
        InstallAxisToBuffer::BufferOfAPCIdentity& identity_buffer,
        const LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& layout_weight,
        const SchemaDefinition::InitialRegionalDtypeConf& dtype_conf,
        const SchemaDefinition::InitialRegionalProtocol& protocol_conf,
        uint8_t version
    ) noexcept
    {
        HeaderOrchestrator::APCMetaBuffer header_meta_buffer{};

        if (
            !HeaderOrchestrator::InitializeDefaultHeaderBuffer(
            header_meta_buffer,
            identity_buffer,
            total_capacity,
            layout_weight,
            dtype_conf,
            protocol_conf,
            version
            ) ||
            !HeaderOrchestrator::IsHeaderBufferValidationMarked(header_meta_buffer)
        )
        {
            return false;
        }
        
        return ForceCopyToAPCFromBuffer(
            UNSIGNED_ZERO,
            APCDataStructure::METACELL_COUNT,
            header_meta_buffer.data()
        );
        
    }

    bool ReadAndWriteOfAPC::ReadAPCMetaUnit(
        HeaderIdentifierOfAPC meta_idx,
        uint64_t& return_value,
        bool atomic_required
    ) noexcept
    {
        const uint8_t idx_u = static_cast<uint8_t>(meta_idx);
        if (!IsValidAPCRange(idx_u, 1u))
        {
            return false;
        }
        
        const size_t slab_idx = static_cast<uint64_t>(RangeOfThisAPCInSlab_.BeginIndex + idx_u);
        uint64_t meta_value = UNSIGNED_ZERO;
        bool read_ok =  atomic_required ? 
            FabricOwnerPtr_->AtomicallyLoadReadAUnit(slab_idx, meta_value) :
            FabricOwnerPtr_->ReadAFabricU64Directly(slab_idx, meta_value);

        if (!read_ok)
        {
            return false;
        }
        return_value = meta_value;
        return true;
    }

    bool ReadAndWriteOfAPC::CompareExchangeAPCMetaUinit(
        HeaderIdentifierOfAPC meta_idx,
        uint64_t& expected_value,
        uint64_t desired_value
    ) noexcept
    {
        const uint8_t local_idx_u = static_cast<uint8_t>(meta_idx);

        return IsValidAPCRange(local_idx_u, 1u) &&
            FabricOwnerPtr_->CompareExchangeStrongFromFabric(
                RangeOfThisAPCInSlab_.BeginIndex + local_idx_u,
                expected_value,
                desired_value
            );
    }


    bool ReadAndWriteOfAPC::PublishIdentityBuffer(
        InstallAxisToBuffer::BufferOfAPCIdentity& desired_identity,
        uint32_t max_tries
    ) noexcept
    {
        using IAB = InstallAxisToBuffer;
        if (!IAB::SealIdentityBuffer(desired_identity))
        {
            return false;
        }

        uint8_t fp_header_idx = static_cast<uint8_t>(HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT);

        uint64_t current_fp = UNSIGNED_ZERO;

        bool write_lock_acquired = false;
        for (size_t i = 0; i < max_tries; i++)
        {
            if (
                AtomicallyReadLongLongAPCUnit(
                fp_header_idx,
                current_fp
                ) &&
                IAB::IdentityFingerprintToState(current_fp) == IAB::GraphMutationState::VALID 
            )
            {
                uint64_t expected = current_fp;
                if (
                    CompareExchangeStrongFromAPC(
                        fp_header_idx, 
                        expected,
                        IAB::IDENTITY_WRITE_LOCKDOWN
                    )
                )
                {
                    write_lock_acquired = true;
                    break;
                }
            }
        }
        if (!write_lock_acquired)
        {
            return false;
        }
        for (size_t i = 1; i < APCDataStructure::TotalIdentityUnitCount(); i++)
        {
            AtomicallyWriteU64ToAPC(fp_header_idx + i, desired_identity[i]);
        }

        uint64_t expected = IAB::IDENTITY_WRITE_LOCKDOWN;

        const uint8_t buffer_fp_idx = IAB::GetBufferIdxFromIdentityUnit(HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT).value();

        return CompareExchangeStrongFromAPC(
            fp_header_idx,
            expected,
            desired_identity[buffer_fp_idx]
        );
    }

}