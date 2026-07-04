
#pragma once 
#include <array>
#include <utility>
#include "../../PackedCell/InternalCellModes/Mode48CellModels.hpp"
#include "../../PackedCell/InternalCellModes/Mode32CellModels.hpp"

namespace PredictedAdaptedEncoding
{

    enum class MetaIndexOfAPCNode : uint8_t
    {
        // identity
        MAGIC_ID = 0,
        SEGMENT_CONF_FLAGS = 1,

        // FABRIC_INFO
        APC_SLOT_IDX = 2,
            //ID
            BRANCH_ID = 3,
            LOGICAL_GROUP_ID = 4,
            SHARED_GROUP_ID = 5,
            //KEY
            SHARED_ID_HASH_KEY = 6,
            LOGICAL_ID_HASH_KEY = 7,
            //LINKED SEQUENTIAL CHAIN
            TOTAL_HORIZONTAL_COUNT_S = 8,
            TOTAL_VERTICAL_COUNT_L = 9,
            PREVIOUS_HORIZONTAL_S = 10,
            NEXT_HORIZONTAL_S = 11,
            NEXT_VERTICAL_L = 12,
            PREVIOUS_VERTICAL_L = 13,
        ACCESS_PASSWORD = 14,
        ///

        // payload bounds versions
        FEEDFORWARD_BOUNDS_VERSION = 15,
        FEEDBACKWARD_BOUNDS_VERSION = 16,
        LATERAL_BOUNDS_VERSION = 17,
        STATE_BOUNDS_VERSION = 18,
        ERROR_BOUNDS_VERSION = 19,
        EDGE_DESCRIPTIOR_BOUNDS_VERSION = 20,
        WEIGHT_BOUNDS_VERSION = 21,
        AUX_BOUNDS_VERSION = 22,
        HETEROGENOUS_RAW_MEMORY_BOUNDS_VERSION = 23,
        RAW_64Bit_MEMORY = 24,
        PAIRED_POINTER_IN_MEMORY_BOUNDS_VERSION = 25,
        FREE_BOUNDS_VERSION = 26,
        UNDEFINED_BOUNDS_VERSION = 27,
        ///

        // region occupancy
        FEEDFORWARD_OCC = 28,
        FEEDBACKWARD_OCC = 29,
        LATERAL_OCC = 30,
        STATE_OCC = 31,
        ERROR_OCC = 32,
        EDGE_OCC = 33,
        WEIGHT_OCC = 34,
        AUX_OCC = 35,
        HETEROGENOUS_OCC = 36,
        RAW64_OCC = 37,
        PAIRED_PTR_OCC = 38,
        FREE_OCC = 39,
        UNDEFINED_OCC = 40,




        
        ////-------NEEDS UPDATE IN FUTURE---------
        // runtime-control
        BRANCH_PRIORITY = 80,
        CURRENT_ACTIVE_THREADS = 81,
        SPLIT_THRESHOLD_PERCENTAGE = 82,
        TOTAL_CAPACITY_OF_THIS_SEGEMENT = 83,
        PAGED_NODE_READY_BIT = 84,
        DEFINED_MODE_OF_CURRENT_APC = 85,
        PRODUCER_CURSOR_PLACEMENT = 86,
        CONSUMER_CURSORE_PLACEMENT = 87,
        TOTAL_CAS_FAILURE_FOR_THIS_APC_BRANCH = 88,
        NODE_GROUP_SIZE = 89,
        ///

        // INTERNAL TIMER
        LOCAL_CLOCK48 = 90,
        LAST_ACCEPTED_FEED_FORWARD_CLOCK16 = 91,
        LAST_EMITTED_FEED_FORWARD_CLOCK16 = 92,
        LAST_ACCEPTED_FEED_BACKWARD_CLOCK16 = 93,
        LAST_EMITTED_FEED_BACKWARD_CLOCK16 = 94,
        ///


        EOF_APC_HEADER = 95,
        UNASSIGNED_UNUSED_NANNULL = 96
    };

    struct APCSegmentPoolRange
    {
        size_t BeginIndex = UNSIGNED_ZERO;
        size_t EndIndex = UNSIGNED_ZERO;
        bool IsVAlid = false;
    };

    enum class ControlEnumOfAPCSegment : uint32_t
    {
        NONE = 0u,
        ENABLE_BRANCHING = 1u << 0,
        HAS_REGION_INDEX =  1u << 1,
        SATURATED = 1u << 2,
        SPLIT_INFLIGHT = 1u << 3,
        IS_GRAPH_NODE = 1u << 4,
        IS_SHARED_ROOT = 1u << 5,
        IS_SHARED_MAMBER = 1u << 6,
        HAS_SHARED_NEXT = 1u << 7,
        HAS_SHARED_PREVIOUS = 1u << 8,
        HAS_LAYOUT_DIR = 1u << 9,
        HAS_EDGE_TABLE = 1u << 10,
        HAS_WEIGHT_TABLE = 1u << 11,
        LAYOUT_MUTATION_INFLIGHT = 1u << 12
    };

    enum class PublishStatus : uint8_t
    {
        OK = 0,
        FULL = 1,
        INVALID = 2
    };

}