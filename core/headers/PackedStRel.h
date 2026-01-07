#pragma once

#include "PackedCell.hpp"
namespace AtomicCScompact
{
    // reserved states (examples)
    static constexpr tag8_t ST_IDLE        = 0x00; // free, CPU may claim
    static constexpr tag8_t ST_PUBLISHED   = 0x01; // CPU wrote & doorbell
    static constexpr tag8_t ST_CLAIMED     = 0x02; // GPU claimed via CAS
    static constexpr tag8_t ST_PROCESSING  = 0x03; // GPU is working
    static constexpr tag8_t ST_COMPLETE    = 0x04; // GPU finished; result committed
    static constexpr tag8_t ST_RETIRED     = 0x05; // CPU read & recycled
    static constexpr tag8_t ST_EPOCH_BUMP  = 0x06; // special sentinel to indicate region epoch bump in progress
    static constexpr tag8_t ST_RESERVED    = 0x07; // CPU reserved (pending) but not published yet
    // 0xF0..0xFF reserved for user-defined status codes

} // namespace PackedState
