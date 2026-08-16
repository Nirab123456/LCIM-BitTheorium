
#pragma once 
#include <array>
#include <utility>
#include "../../SharedComponents/SharedConf.hpp"

namespace BidirectionalInMemGraph
{

    enum class HeaderIdentifierOfAPC : uint8_t
    {
        // identity
        MAGIC_ID = 0,

        GRAPH_MUTATION_AND_LOCK = 1,
        APC_SLOT_IDX = 2,
        BOUNDS_BEGIN = 3,
        BOUNDS_END = 4,
        HORIZONTAL_SHARED_IDX = 5,
        VERTICAL_SHARED_IDX = 6,
        HORIZONTAL_ROOT_IDX = 7,
        VERTICAL_ROOT_IDX = 8,
        HORIZONTAL_NEXT_OF_ROOT = 9,
        VERTICAL_NEXT_OF_ROOT = 10,
        NEXT_HORIZONTAL_SLOT = 11,
        NEXT_VERTICAL_SLOT = 12,
        PREVIOUS_HORIZONTAL_SLOT = 13,
        PREVIOUS_VERTICAL_SLOT = 14,

        // payload bounds versions
        FEEDFORWARD_BOUNDS = 15,
        FEEDBACKWARD_BOUNDS = 16,
        LATERAL_BOUNDS = 17,
        STATE_BOUNDS = 18,
        ERROR_BOUNDS = 19,
        WEIGHTLESS_BOUNDS = 20,
        WEIGHT_BOUNDS= 21,
        AUX_BOUNDS = 22,
        HETEROGENOUS_PTR_BOUNDS = 23,
        FREE_BOUNDS = 24,

        APC_LIFE_CYCLE = 25,
        LAYOUT_VERSION = 26,

        FEEDFORWARD_ENQUEUE_POSITION = 28,
        FEEDBACKWARD_ENQUEUE_POSITION = 29,
        LATERAL_ENQUEUE_POSITION = 30,
        STATE_ENQUEUE_POSITION = 31,
        ERROR_ENQUEUE_POSITION = 32,
        WEIGHTLESS_ENQUEUE_POSITION = 33,
        WEIGHT_ENQUEUE_POSITION = 34,
        AUX_ENQUEUE_POSITION = 35,
        HETEROGENOUS_ENQUEUE_POSITION = 36,
        FREE_ENQUEUE_POSITION = 37,

        FEEDFORWARD_DEQUEUE_POSITION = 38,
        FEEDBACKWARD_DEQUEUE_POSITION = 39,
        LATERAL_DEQUEUE_POSITION = 40,
        STATE_DEQUEUE_POSITION = 41,
        ERROR_DEQUEUE_POSITION = 42,
        WEIGHTLESS_DEQUEUE_POSITION = 43,
        WEIGHT_DEQUEUE_POSITION = 44,
        AUX_DEQUEUE_POSITION = 45,
        HETEROGENOUS_DEQUEUE_POSITION = 46,
        FREE_DEQUEUE_POSITION = 47,

        // Region schema: record width + protocol + format/version.
        FEEDFORWARD_REGION_SCHEMA = 48,
        FEEDBACKWARD_REGION_SCHEMA = 49,
        LATERAL_REGION_SCHEMA = 50,
        STATE_REGION_SCHEMA = 51,
        ERROR_REGION_SCHEMA = 52,
        WEIGHTLESS_REGION_SCHEMA = 53,
        WEIGHT_REGION_SCHEMA = 54,
        AUX_REGION_SCHEMA = 55,
        HETEROGENOUS_REGION_SCHEMA = 56,
        FREE_REGION_SCHEMA = 57,



        
        ////-------NEEDS UPDATE IN FUTURE---------
        // runtime-control
        SEGMENT_CONF_FLAGS = 85,
        BRANCH_PRIORITY = 86,
        CURRENT_ACTIVE_THREADS = 87,
        SPLIT_THRESHOLD_PERCENTAGE = 88,
        RESERVED_89 = 89,
        PAGED_NODE_READY_BIT = 90,
        APC_SCHEMA_ID = 91,
        TOTAL_CAS_FAILURE_FOR_THIS_APC_BRANCH = 92,
        NODE_GROUP_SIZE = 93,
        ///
        // INTERNAL TIMER
        LOCAL_FULL_CLOCK = 94,
        ///
        EOF_APC_HEADER = 95
    };

    static_assert(
        (static_cast<uint8_t>(HeaderIdentifierOfAPC::FREE_BOUNDS) - static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_BOUNDS)) ==
        (static_cast<uint8_t>(MacroColumnOfAPC::FREE_SLOT) - static_cast<uint8_t>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE))
    );

    static_assert(
        (static_cast<uint8_t>(HeaderIdentifierOfAPC::FREE_ENQUEUE_POSITION) - static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_ENQUEUE_POSITION)) ==
        (static_cast<uint8_t>(MacroColumnOfAPC::FREE_SLOT) - static_cast<uint8_t>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE))
    );

    struct RangeOfAPC
    {
        size_t BeginIndex = UNSIGNED_ZERO;
        size_t EndIndex = UNSIGNED_ZERO;
        bool IsValid = false;
    };

}