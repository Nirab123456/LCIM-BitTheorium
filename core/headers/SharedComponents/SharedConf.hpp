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
        FEEDFORWARD_MESSAGE  = 0,
        FEEDBACKWARD_MESSAGE = 1,
        LATERAL_MESAGE = 2,
        STATE_SLOT = 3,
        ERROR_SLOT = 4,
        WEIGHTLESS_LOOKUP = 5,
        WEIGHT_SLOT = 6,
        AUX_SLOT = 7,
        HETEROGENOUS_PTR = 8,
        FREE_SLOT     = 9
    };

    enum class FabricSegments : uint8_t
    {
        SLAB_RECORD_MAP = 0,
        HORIZONTAL_EDGE_TABLE = 1,
        VERTICAL_EDGE_TABLE = 2,
        FREE_APC_LIST = 3,
        READY_QUEUE = 4,
        WORK_QUEUE = 5,
        DEVICE_VIEW_TABLE = 6,
        SEGMENT_POOL = 7
    };

    enum class StateOfAPC : uint8_t
    {
        FREE = 0,
        RESERVED = 1,
        LIVE = 2,
        RETIRED = 3,
        HAULTED = 4
    };

    static constexpr bool IsLiveSateOfAPC(std::optional<StateOfAPC> state) noexcept
    {
        return state.has_value() && state.value() == StateOfAPC::LIVE;
    }
    
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

