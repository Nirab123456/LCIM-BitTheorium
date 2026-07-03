#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include <iostream>

namespace PredictedAdaptedEncoding
{
    class PackedCellContainerManager;
    

    size_t AdaptivePackedCellContainer::ReserveProducerSlots(size_t number_of_slots) noexcept
    {
        if (!IfAPCBranchValid() || number_of_slots == 0)
        {
            return APCDataStructure::APC_SIZE_SENTINAL;
        }
        const size_t payload_capacity = PayloadCapacityFromHeader();
        if (payload_capacity == 0)
        {
            return APCDataStructure::APC_SIZE_SENTINAL;
        }

        if (payload_capacity == 1)
        {
            return PayloadBegin();
        }
        
        number_of_slots = std::min<size_t>(number_of_slots, payload_capacity - 1);
        number_of_slots = std::max<size_t>(number_of_slots, 1u);

        constexpr uint32_t HARD_RESERVE_GUARD = 4096u;

        for (uint32_t attempt = 0; attempt < HARD_RESERVE_GUARD; ++attempt)
        {
            uint32_t current_producer_cursor = GetProducerCursorPlacement();
            if (current_producer_cursor == APC_META_CELL_SENTINAL || current_producer_cursor < PayloadBegin() || current_producer_cursor >= GetTotalCapacityForThisAPC())
            {
                current_producer_cursor = PayloadBegin();
            }
            const size_t current_offset = static_cast<size_t>(current_producer_cursor - PayloadBegin()) % payload_capacity;
            size_t next_offset = (current_offset + number_of_slots) % payload_capacity;

            if (next_offset == current_offset)
            {
                next_offset = (current_offset + 1u) % payload_capacity;
            }
            


            const uint32_t desired_cursor = static_cast<uint32_t>(PayloadBegin() + next_offset);
            bool changed = false;
            ProducerORConsumerCursorSetAndGet_(
                static_cast<uint32_t>(desired_cursor),
                0,
                &changed,
                MetaIndexOfAPCNode::PRODUCER_CURSOR_PLACEMENT
            );
            if (changed)
            {
                return static_cast<size_t>(current_producer_cursor);
            }
            std::this_thread::yield();

        }

        TotalCASFailForThisBranchIncreaseAndGet(1);
        return APCDataStructure::APC_SIZE_SENTINAL;
    }

    

    std::optional<packed64_t> AdaptivePackedCellContainer::ConsumeCellByRegionMaskTraverseStartFromThisAPC(APCPagedNodeSegmentClasses region_kind, size_t& scan_cursor) noexcept
    {
        if (!IfAPCBranchValid())
        {
            return std::nullopt;
        }
        AdaptivePackedCellContainer* root_apc_ptr = FindSharedRootOrThis();
        if (!root_apc_ptr)
        {
            return std::nullopt;
        }
        AdaptivePackedCellContainer* current_apc_ptr = root_apc_ptr;
        bool first = true;
        const uint32_t group_size = std::max<uint32_t>(1u, static_cast<uint32_t>(ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode::NODE_GROUP_SIZE)));
        uint32_t chain_guard = 0;
        const uint32_t max_chain_steps = group_size + 2u;

        while (current_apc_ptr && chain_guard++ < max_chain_steps)
        {
            size_t local_cursor = first ? scan_cursor : PayloadBegin();
            auto maybe_cell = current_apc_ptr->TryConsumeAndIdleFromRegionLocal_(region_kind, local_cursor);
            if (maybe_cell)
            {
                if (first)
                {
                    scan_cursor = local_cursor;
                }
                return *maybe_cell;
            }
            AdaptivePackedCellContainer* next_apc_ptr = current_apc_ptr->GetNextSharedSegment();
            if (!next_apc_ptr || next_apc_ptr == current_apc_ptr)
            {
                break;
            }
            current_apc_ptr = next_apc_ptr;
            first = false;
        }
        return std::nullopt;
    }

    void FabricToAPCLinker::FreeAll() noexcept
    {
        if (FabricOwnerPtr_ && FabricBackend_)
        {

        }

        if (FabricBackend_)
        {
            ReleseFabricBindingOnly_();
        }
        else
        {
            APCBackingCellAtomicRefViewTemp::FreeBackingView_(OwnedBackingView_, CapacityOfThisAPC_);
            FreeAlignedRawPackedCells_(OwnedRawBackingCells_);
            OwnedBackingView_ = nullptr;
            CapacityOfThisAPC_ = UNSIGNED_ZERO;
        }
        
        SOABitmapForAPC_.clear();
    }



    uint32_t AdaptivePackedCellContainer::CountExactTotalChainOccupancy(APCPagedNodeSegmentClasses desired_region_class) noexcept
    {
        uint32_t total = 0;
        AdaptivePackedCellContainer* current_apc = FindSharedRootOrThis();
        while (current_apc)
        {
            total += current_apc->CountExactLocalRegionalOccupancy(desired_region_class);
            AdaptivePackedCellContainer* next_apc = current_apc->GetNextSharedSegment();
            if (!next_apc || next_apc == current_apc)
            {
                break;
            }
            current_apc = next_apc;
        }
        return total;
    }


}
