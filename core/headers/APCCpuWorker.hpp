#pragma once

#include "AtomicPCArray.hpp"
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <functional>
#include <cstdint>
#include <cassert>

namespace AtomicCScompact
{
    enum OPKind : uint8_t
    {
        OP_SET = 1,
        OP_APPLY_GRAD = 4,
        OP_EPOCH_BUMP = 5
    };

    struct ACADescriptor
    {
        uint8_t op;
        uint8_t flags;
        uint8_t rel;
        uint8_t pad;
        uint32_t idx;
        uint32_t idx;
        uint32_t count;
        uint64_t arg;
    };

}