#pragma once
#include <functional>
#include "FabricToAPCLinker.hpp"

namespace PredictedAdaptedEncoding
{
    class ReadAndWriteOfAPC : public FabricToAPCLinker
    {

    private:
        /* data */
    public:

        bool ReadCompleateMetaHeaderDirectlyNonAtomic(HeaderOrchestrator::APCMetaBuffer& a_default_buffer) noexcept;

        bool ReadCompleatLayoutBuffer(
            LayoutBoundsOrchestrator::TrackingBufferOfAPC& a_layout_buffer,
            bool is_claimed_required = false
        ) noexcept;

        bool UpdateCompleateLayoutOfAPCFromBuffer(
            const LayoutBoundsOrchestrator::TrackingBufferOfAPC& a_valid_layout_buffer,
            bool caller_holds_the_flag = false
        ) noexcept;

        bool InitiateAPCMetaHeader(
            uint16_t total_capacity,
            APCGroupReserver::APCInitialIdentityStruct& container_configuration
        ) noexcept;

    };
    
    
}