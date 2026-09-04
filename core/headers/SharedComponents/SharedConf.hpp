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
#if defined(_MSC_VER)
    #include <intrin.h>
#endif

namespace BidirectionalInMemGraph 
{
    #define DEFAULT_MAX_TRIES 128
    static constexpr uint8_t LEN_OF_BYTE_IN_BITS = 8u;
    static constexpr size_t BIT_LENGTH_OF_FABRIC = LEN_OF_BYTE_IN_BITS * sizeof(uint64_t);
    static constexpr uint8_t BIT_LENGTH_OF_APC = LEN_OF_BYTE_IN_BITS * sizeof(uint32_t);
    static constexpr uint8_t SIZE_OF_CACHELINE = LEN_OF_BYTE_IN_BITS * sizeof(uint64_t);
    static constexpr unsigned UNSIGNED_ZERO = 0u;
    static constexpr uint16_t MINIMUM_APC_CELL_COUNT = 256u;
    static constexpr uint8_t EIGHT_BIT_SENTINAL = UINT8_MAX;
    static constexpr uint64_t FABRIC_CELL_SENTINAL = UINT64_MAX;


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
        APC_HANDLE_TABLE = 3,
        COMPILED_DAG_TABLE = 4,
        READY_QUEUE = 5,
        WORK_QUEUE = 6,
        DEVICE_VIEW_TABLE = 7,
        SEGMENT_POOL = 8
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
    
    static  constexpr uint64_t MaskLowBitsForU64(unsigned n) noexcept
    {
        if (n == UNSIGNED_ZERO) return uint64_t(0);
        if (n >= BIT_LENGTH_OF_FABRIC) return ~uint64_t(0);
        // produce low-n ones without shifting by >= width
        return ((uint64_t(1) << n) - 1u);                  
    }

    static  constexpr uint32_t MaskLowBitsForU32(unsigned n) noexcept
    {
        if (n == UNSIGNED_ZERO) return uint32_t(0);
        if (n >= BIT_LENGTH_OF_APC) return ~uint32_t(0);
        // produce low-n ones without shifting by >= width
        return ((uint32_t(1) << n) - 1u);                  
    }
}
