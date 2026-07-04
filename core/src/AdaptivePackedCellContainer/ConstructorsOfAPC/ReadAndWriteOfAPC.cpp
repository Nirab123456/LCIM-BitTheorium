#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"

namespace PredictedAdaptedEncoding
{

    bool ReadAndWriteOfAPC::ReadCompleateMetaHeaderDirectlyNonAtomic(HeaderOrchestrator::APCMetaBuffer& a_default_buffer) noexcept
    {
        APCSegmentPoolRange range_of_this_apc{};
        if (!IsThisAPCValidRange_(UNSIGNED_ZERO, APCDataStructure::METACELL_COUNT, &range_of_this_apc))
        {
            return false;
        }

        if (!CopyFromAPCToBuffer(UNSIGNED_ZERO, APCDataStructure::METACELL_COUNT, a_default_buffer.data()))
        {
            return false;
        }
        
        return true;
    }

    bool ReadAndWriteOfAPC::ReadCompleatLayoutBuffer(
        LayoutBoundsOrchestrator::TrackingBufferOfAPC& a_layout_buffer,
        bool is_claimed_required
    ) noexcept
    {
        if (is_claimed_required)
        {
            return ClaimAndCopyToAPCFromBuffer(
                PageNodeOrchestrator::GetBeginIndexOfLayoutBufferOfAPC(),
                PageNodeOrchestrator::TrackedAPCNodeLen(),
                a_layout_buffer.data()
            );
        }
        
        return ForceCopyToAPCFromBuffer(
            PageNodeOrchestrator::GetBeginIndexOfLayoutBufferOfAPC(),
            PageNodeOrchestrator::TrackedAPCNodeLen(),
            a_layout_buffer.data()
        );
    }

    packed64_t ReadAndWriteOfAPC::ReadFullMetaCell(MetaIndexOfAPCNode idx) noexcept
    {
        if (ValidMetaIdx(idx))
        {
            return BackingPtr[static_cast<size_t>(idx)].load(MoLoad_);
        }
        return PACKED_CELL_SENTENAL;
    }
}