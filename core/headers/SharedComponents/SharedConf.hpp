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

namespace BidirectionalInMemGraph 
{

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

    enum class FabricSegments : uint8_t
    {
        SLAB_RECORD_MAP = 0,
        APC_HANDLE_DESCRIPTOR = 1,
        HORIZONTAL_EDGE_TABLE = 2,
        VERTICAL_EDGE_TABLE = 3,
        FREE_APC_LIST = 4,
        READY_QUEUE = 5,
        WORK_QUEUE = 6,
        DEVICE_VIEW_TABLE = 7,
        THREAD_TABLE  = 8,
        SEGMENT_POOL = 9
    };

    enum class StateOfAPC : uint8_t
    {
        FREE = 0,
        RESERVED = 1,
        LIVE = 2,
        RETIRED = 3,
        HAULTED = 4
    };
    
    static  constexpr uint64_t MaskLeftOverBitsUntil64(unsigned n) noexcept
    {
        if (n == UNSIGNED_ZERO) return uint64_t(0);
        if (n >= BIT_LENGTH_OF_FABRIC) return ~uint64_t(0);
        // produce low-n ones without shifting by >= width
        return ((uint64_t(1) << n) - 1u);                  
    }

    static  constexpr uint32_t LeftOverBitMaskUntil32(unsigned n) noexcept
    {
        if (n == UNSIGNED_ZERO) return uint32_t(0);
        if (n >= BIT_LENGTH_OF_APC) return ~uint32_t(0);
        // produce low-n ones without shifting by >= width
        return ((uint32_t(1) << n) - 1u);                  
    }
}

