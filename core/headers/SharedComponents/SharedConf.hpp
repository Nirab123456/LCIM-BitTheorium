#pragma once

#include <type_traits>
#include <cstring>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <random>
#include <optional>
#include <vector>
#include <algorithm>
#include <limits>
#include <cassert>
#include <cstddef>
#include <array>
#include <thread>
#include <stdexcept>
#include <memory>
#include <bit>
#include "Defination.h"

#if defined(_MSC_VER)
    #include <intrin.h>
#endif

namespace PredictedAdaptedEncoding 
{
    /// @brief Name Of Each Segment On APC
    /// @param NONE Lower guard prevents a Packed Cell to be ever VALID: 0
    /// @param NULLNAN Uppper Guard Prevents Pack-ed Cell to be VALID: UINT64_MAX
    enum class MacroColumnOfAPC : uint8_t
    {
        NONE = 0x0,
        FEEDFORWARD_MESSAGE  = 0x1,
        FEEDBACKWARD_MESSAGE = 0x2,
        LATERAL_MESAGE = 0x3,
        STATE_SLOT = 0x4,
        ERROR_SLOT = 0x5,
        WEIGHTLESS_LOOKUP = 0x6,
        WEIGHT_SLOT = 0x7,
        AUX_SLOT = 0x8,
        HETEROGENOUS_PTR = 0x9,
        FREE_SLOT     = 10,
        META_HEADER = 11,
        NULLNAN     = 12
    };

    /// @brief Name Of Each Segment On Fabric
    /// @param NONE It is the lower guard prevents a Packed Cell to be ever 0
    /// @param GLOBAL_AND_CONFIG USED:FOR: Everything else after @param THREAD_TABLE
    /// @param SLAB_RECORD_MAP STORES:All Begin & End Pair of indicies for every class of FabricTableSegmentClasses
    /// @param APC_HANDLE_DESCRIPTOR HOLDS:Each APC x RECORD:DescriptionUnitIdentity -> DESCRIBS: Initial Fundamental Meta for An APC When Created 
    /// @param BRANCH_HASH
    /// @param LOGICAL_HASH
    /// @param SHARED_HASH
    /// @param CONTROL_HEADER USED:FOR: first 96 FabricMetaIndicies
    /// @param NULLNAN Uppper Guard Prevents Pack-ed Cell to be VALID: UINT64_MAX
    enum class FabricTableSegmentClasses : uint8_t
    {
        NONE = 0,
        GLOBAL_AND_CONFIG = 1,
        SLAB_RECORD_MAP = 2,
        APC_HANDLE_DESCRIPTOR = 3,
        BRANCH_HASH = 4,
        LOGICAL_HASH = 5,
        SHARED_HASH = 6,
        EDGE_TABLE = 7,
        FREE_APC_LIST = 8,
        READY_QUEUE = 9,
        WORK_QUEUE = 10,
        DEVICE_VIEW_TABLE = 11,
        THREAD_TABLE  = 12,
        SEGMENT_POOL = 13,
        CONTROL_HEADER = 14,
        NULLNAN = 15,
    };


    static  constexpr uint64_t MaskLeftOverBitsUntil64(unsigned n) noexcept
    {
        if (n == UNSIGNED_ZERO) return uint64_t(0);
        if (n >= BIT_LENGTH_OF_FABRIC) return ~uint64_t(0);
        // produce low-n ones without shifting by >= width
        return ((uint64_t(1) << n) - 1u);                  
    }
}

