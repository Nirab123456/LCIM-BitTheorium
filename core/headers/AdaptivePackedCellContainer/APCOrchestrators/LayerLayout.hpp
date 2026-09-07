#pragma once
#include <functional>
#include "SchemaOrchestratorForRegion.hpp"

namespace BidirectionalInMemGraph
{
    struct GHGFLayerModel
    {
        using SD = SchemaDefinition;

        enum class GHGFNodeRole : uint8_t
        {
            OBSERVATION = 0,
            VALUE = 1,
            VOLATILE = 2
        };

        enum class GHGFStateRow : uint8_t
        {
            MEAN = 0,
            EXPECTED_MEAN = 1,
            PRECISION = 2,
            EXPECTED_PRECISION = 3,
            CONDITIONAL_EXPECTED_PRECISION = 4,
            OBSERVED = 5,
            CURRENT_VARIANCE = 6,
            EFFECTIVE_PRECISION = 7
        };
        static constexpr uint8_t STATE_ROW_COUNT_HEIGHT = static_cast<uint8_t>(GHGFStateRow::EFFECTIVE_PRECISION) + 1;

        enum class GHGFErrorRow : uint8_t
        {
            VALUE_PREDICTION_ERROR = 0,
            VOLATILE_PREDICTION_ERROR = 1
        };
        static constexpr uint8_t ERROR_ROW_COUNT_HEIGHT = static_cast<uint8_t>(GHGFErrorRow::VOLATILE_PREDICTION_ERROR) + 1;

        struct GHGFStoreageProfile final
        {
            static constexpr uint32_t DEFAULT_BATCH_CAPACITY = 32u;

        };


    };
    
}