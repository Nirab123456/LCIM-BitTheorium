
#pragma once 
#include <array>
#include <utility>
#include "../../SharedComponents/SharedConf.hpp"

namespace PredictedAdaptedEncoding
{

    enum class HeaderIdentifierOfAPC : uint8_t
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

            PREVIOUS_HORIZONTAL_HANDLE = 10,
            NEXT_HORIZONTAL_HANDLE = 11,
            NEXT_VERTICAL_HANDLE = 12,
            PREVIOUS_VERTICAL_HANDLE = 13,
        ACCESS_PASSWORD = 14,
        ///

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
        ///

        // region occupancy
        FEEDFORWARD_OCC = 28,
        FEEDBACKWARD_OCC = 29,
        LATERAL_OCC = 30,
        STATE_OCC = 31,
        ERROR_OCC = 32,
        WEIGHTLESS_OCC = 33,
        WEIGHT_OCC = 34,
        AUX_OCC = 35,
        HETEROGENOUS_OCC = 36,
        FREE_OCC = 37,



        
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
        LOCAL_FULL_CLOCK = 94,
        ///


        EOF_APC_HEADER = 95,
        UNASSIGNED_UNUSED_NANNULL = 96
    };

    static_assert(
        (static_cast<uint8_t>(HeaderIdentifierOfAPC::FREE_BOUNDS) - static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_BOUNDS)) ==
        (static_cast<uint8_t>(MacroColumnOfAPC::FREE_SLOT) - static_cast<uint8_t>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE))
    );

    static_assert(
        (static_cast<uint8_t>(HeaderIdentifierOfAPC::FREE_OCC) - static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_OCC)) ==
        (static_cast<uint8_t>(MacroColumnOfAPC::FREE_SLOT) - static_cast<uint8_t>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE))
    );

    struct APCSegmentPoolRange
    {
        size_t BeginIndex = UNSIGNED_ZERO;
        size_t EndIndex = UNSIGNED_ZERO;
        bool IsValid = false;
    };

}