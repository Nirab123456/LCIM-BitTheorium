
#pragma once 
#include <array>
#include <utility>
#include "../../PackedCell/InternalCellModes/Mode48CellModels.hpp"
#include "../../PackedCell/InternalCellModes/Mode32CellModels.hpp"

namespace PredictedAdaptedEncoding
{

    enum class MetaIndexOfAPCNode : size_t
    {
        // identity
        MAGIC_ID = 0,
        FABRIC_FLAG = 1,
        SEGMENT_CONF_FLAGS = 2,

        // FABRIC_INFO
        APC_SLOT_IDX = 84,
            //ID
            BRANCH_ID = 3,
            LOGICAL_GROUP_ID = 4,
            SHARED_GROUP_ID = 5,
            //KEY
            SHARED_ID_HASH_KEY = 79,
            LOGICAL_ID_HASH_KEY = 80,
            //LINKED SEQUENTIAL CHAIN
            TOTAL_HORIZONTAL_COUNT_S = 8,
            TOTAL_VERTICAL_COUNT_L = 9,
            PREVIOUS_HORIZONTAL_S = 6,
            NEXT_HORIZONTAL_S = 7,
            NEXT_VERTICAL_L = 17,
            PREVIOUS_VERTICAL_L = 19,
        ACCESS_PASSWORD = 78,
        ///

        // runtime-control
        BRANCH_PRIORITY = 10,
        CURRENT_ACTIVE_THREADS = 11,
        SPLIT_THRESHOLD_PERCENTAGE = 12,
        // RESERVED = 13,

        // payload / capacity
        TOTAL_CAPACITY_OF_THIS_SEGEMENT = 14,

        // timing
        LOCAL_CLOCK48 = 15,
        LAST_SPLIT_EPOCH = 16,

        // region summary
        PAGED_NODE_READY_BIT = 20,
        PRODUCER_BLOCK_SIZE = 21,
        BACKGROUND_EPOCH_ADVANCE_MS = 22,
        DEFINED_MODE_OF_CURRENT_APC = 23,
        RETIRE_BRANCH_THRASHOLD = 24,
        PRODUCER_CURSOR_PLACEMENT = 25,
        CONSUMER_CURSORE_PLACEMENT = 26,
        CURRENTLY_OWNED = 27,
        TOTAL_CAS_FAILURE_FOR_THIS_APC_BRANCH = 28,
        NODE_GROUP_SIZE = 29,
        NODE_AUX_PARAM_U32 = 30,

        // graph ports
        FEEDFORWARD_IN_TARGET_ID = 31,
        FEEDFORWARD_OUT_TARGET_ID = 32,
        FEEDBACKWARD_IN_TARGET_ID = 33,
        FEEDBACKWARD_OUT_TARGET_ID = 34,
        LATERAL_0_TARGET_ID = 35,
        LATERAL_1_TARGET_ID = 36,
        NODE_ROLE_FLAGS_RESERVED = 37,
        LAST_ACCEPTED_FEED_FORWARD_CLOCK16 = 38,
        LAST_EMITTED_FEED_FORWARD_CLOCK16 = 39,
        LAST_ACCEPTED_FEED_BACKWARD_CLOCK16 = 40,
        LAST_EMITTED_FEED_BACKWARD_CLOCK16 = 41,
        NODE_COMPUTE_KIND = 42,

        // payload bounds versions
        FEEDFORWARD_BOUNDS_VERSION = 43,
        FEEDBACKWARD_BOUNDS_VERSION = 44,
        LATERAL_BOUNDS_VERSION = 45,
        STATE_BOUNDS_VERSION = 46,
        ERROR_BOUNDS_VERSION = 47,
        EDGE_DESCRIPTIOR_BOUNDS_VERSION = 48,
        WEIGHT_BOUNDS_VERSION = 49,
        AUX_BOUNDS_VERSION = 50,
        HETEROGENOUS_RAW_MEMORY_BOUNDS_VERSION = 51,
        RAW_64Bit_MEMORY = 52,
        PAIRED_POINTER_IN_MEMORY_BOUNDS_VERSION = 53,
        FREE_BOUNDS_VERSION = 54,
        UNDEFINED_BOUNDS_VERSION = 55,

        // region occupancy
        REGION_OCCUPANCY_NONE = 57,
        REGION_OCCUPANCY_FF = 58,
        REGION_OCCUPANCY_FB = 59,
        REGION_OCCUPANCY_LATERAL = 60,
        REGION_OCCUPANCY_STATE = 61,
        REGION_OCCUPANCY_ERROR = 62,
        REGION_OCCUPANCY_EDGE = 63,
        REGION_OCCUPANCY_WEIGHT = 64,
        REGION_OCCUPANCY_CONTROL = 65,
        REGION_OCCUPANCY_AUX = 66,
        REGION_OCCUPANCY_HETEROGENOUS_MEMORY_MAYBE_PAIRED_POINTER_OR_RAW_APC_SEGMENT = 67,
        REGION_OCCUPANCY_PAIRED_POINTER_LOCAL_MEMORY = 68,
        REGION_OCCUPANCY_PAIRED_POINTER_DISTANCE_MEMORY = 69,
        REGION_OCCUPANCY_FREE = 70,
        REGION_OCCUPANCY_UNDEFINED = 71,
        REGION_OCCUPANCY_NANNULL = 72,

        // retire epoch
        RETIRE_EPOCH_LOW32 = 73,
        RETIRE_EPOCH_HIGH32 = 74,

        // combined occupancy
        COMBINED_OCCUPANCY_PUBLISHED_CLAIMED_FAULTY_3x16_48 = 75,

        // edge / weight tables
        EDGE_TABLE_COUNT = 76,
        WEIGHT_TABLE_COUNT = 77,


        EOF_APC_HEADER = 95
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