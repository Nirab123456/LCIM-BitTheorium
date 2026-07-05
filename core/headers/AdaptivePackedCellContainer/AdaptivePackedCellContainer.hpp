#pragma once 
#include "SegmentIODefinition.hpp"
#include "APCSegmentsCausalCordinator.hpp"

namespace PredictedAdaptedEncoding
{
static_assert(__cpp_lib_atomic_wait, "C++ must suppoet atomic wait/notify");


class AdaptivePackedCellContainer : public APCSegmentsCausalCordinator
{

    protected:        
        
        size_t GetHashedRendomizedStep_(size_t sequense_number) noexcept;

        void InitZeroState_() noexcept;


        std::optional<packed64_t> TryConsumeAndIdleFromRegionLocal_(
            APCPagedNodeSegmentClasses region_kind, size_t& scan_cursor,
            OwnershipPolicy desired_authority_of_updated_cell = OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER
        ) noexcept;

        PublishResult TryPublishToRegionLocal_(
            packed64_t packed_cell_for_publish, 
            APCPagedNodeSegmentClasses region_kind = APCPagedNodeSegmentClasses::FREE_SLOT,
            uint16_t max_tries = APC_ALL_INDEX_LIMIT / (APCAndPagedNodeHelpers::SIZE_OF_APCPagedNodeRelMaskClasses)
        ) noexcept;

        static size_t FindGreatestCommonDivisor_(size_t a, size_t b) noexcept;

        size_t MakeProbeStepCoPrime_(size_t seed, size_t region_capacity) const noexcept;

         bool IfValidPayloadIndex_(size_t idx) noexcept
        {
            return (BackingPtr && idx >= PayloadBegin() && idx < GetTotalCapacityForThisAPC());
        }

    public:

        struct QSBRGuard;

        AdaptivePackedCellContainer(/* args */) noexcept  = default;
        ~AdaptivePackedCellContainer()
        {
            FreeAll();
        }
        AdaptivePackedCellContainer(const AdaptivePackedCellContainer&) = delete;
        AdaptivePackedCellContainer& operator = (const AdaptivePackedCellContainer&) = delete;

        bool DirectReadCompleatAPCHeader() noexcept;

        bool TryPublishRegionalSharedGrowthOnce(APCPagedNodeSegmentClasses region_kind, packed64_t packed_cell) noexcept;

        PublishResult PublishCellByRegionMAskTraverseStartsFromThisAPC(
            APCPagedNodeSegmentClasses region_kind, 
            packed64_t cell_to_publish, 
            std::optional<uint16_t> max_tries = std::nullopt
        ) noexcept;


        std::optional<packed64_t> ConsumeCellByRegionMaskTraverseStartFromThisAPC(APCPagedNodeSegmentClasses region_kind, size_t& scan_cursor) noexcept;

        AdaptivePackedCellContainer* FindSharedRootOrThis() noexcept
        {
            return this;
        }

        AdaptivePackedCellContainer* GetNextSharedSegment() noexcept
        {
            return this;
        }

        bool IsAPCSharedChainEmpty() noexcept
        {
            return false;
        }

        size_t ReserveProducerSlots(size_t number_of_slots) noexcept;

        uint32_t CountExactLocalRegionalOccupancy(APCPagedNodeSegmentClasses desired_region_class) noexcept;

        uint32_t CountExactTotalChainOccupancy(APCPagedNodeSegmentClasses desired_region_class) noexcept;
        
        bool IfAPCBranchValid() noexcept
        {
            return (BackingPtr && PayloadCapacityFromHeader() >= MINIMUM_APC_CAPACITY - PayloadBegin());
        }

        uint32_t ProducerORConsumerCursorSetAndGet_(std::optional<uint32_t> cursor_placement = std::nullopt, int32_t increment_or_decrement_of_cursor = 0, 
            bool* did_changed_easy_return = nullptr, const MetaIndexOfAPCNode cursors_meta_idx = MetaIndexOfAPCNode::PRODUCER_CURSOR_PLACEMENT
        ) noexcept;

        uint32_t GetProducerCursorPlacement() noexcept
        {
            return static_cast<uint32_t>(ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode::PRODUCER_CURSOR_PLACEMENT));
        }

        bool UpdateProducerCursorPlacement(uint32_t new_cursor_placement_idx) noexcept
        {
            bool will_return = false;
            ProducerORConsumerCursorSetAndGet_(new_cursor_placement_idx, 0, &will_return, MetaIndexOfAPCNode::PRODUCER_CURSOR_PLACEMENT);
            return will_return;
        }

        bool ProducerCursorIncrementOrdecrement(int32_t increment_decrement_value)  noexcept
        {
            bool will_retuen = false;
            ProducerORConsumerCursorSetAndGet_(std::nullopt, increment_decrement_value, &will_retuen, MetaIndexOfAPCNode::PRODUCER_CURSOR_PLACEMENT);
            return will_retuen;
        }

        uint32_t GetConsumerCursorPlacement() noexcept
        {
            return static_cast<uint32_t>(ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode::CONSUMER_CURSORE_PLACEMENT));
        }

        bool UpdateConsumerCursorPlacement(uint32_t new_cursor_value) noexcept
        {
            bool will_return = false;
            ProducerORConsumerCursorSetAndGet_(new_cursor_value, 0, &will_return, MetaIndexOfAPCNode::CONSUMER_CURSORE_PLACEMENT);
            return will_return;
        }

        bool ConsumerCursorIncrementOrDecrement(int32_t increment_decrement_value) noexcept
        {
            bool will_return = false;
            ProducerORConsumerCursorSetAndGet_(std::nullopt, increment_decrement_value, &will_return, MetaIndexOfAPCNode::CONSUMER_CURSORE_PLACEMENT);
            return will_return;
        }

         bool IfIndexValid(size_t idx) noexcept
        {
            if (IfAPCBranchValid() && idx < GetTotalCapacityForThisAPC())
            {
                return true;
            }
            return false;        
        }

        bool InitOnFabricBackingAfterBind(
            uint16_t capacity,
            APCGroupReserver::APCInitialIdentityStruct container_cfg
        ) noexcept
        {
            if (
                !FabricBackend_ || 
                !BackingPtr || 
                capacity != CapacityOfThisAPC_ || 
                !OccupancyBuilderAndValidator::IsCapacityOfAPCLegal(capacity) ||
                !APCGroupReserver::IfSystemResolvedIdentityValid(container_cfg)
            )
            {
                return false;
            }
            
            const packed64_t default_idle_cell = PackedCell64_t::MakeDefaultAPCPayloadCellOnMode(container_cfg.InitialMode);
            if (default_idle_cell == PackedCell64_t::PACKED_CELL_SENTINAL)
            {
                return false;
            }
            
            for (size_t i = 0; i < capacity; i++)
            {
                BackingPtr[i].store(default_idle_cell, MoStoreSeq_);
            }

            InitiateAPCMetaHeader(
                capacity,
                container_cfg
            );

            InitZeroState_();
            return true;
        }

};


}  