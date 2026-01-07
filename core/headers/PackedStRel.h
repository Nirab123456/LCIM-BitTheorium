#pragma once

#include "PackedCell.hpp"
namespace AtomicCScompact
{
    static constexpr tag8_t IDLE      = 0x00;
    static constexpr tag8_t PUBLISHED = 0x01; // CPU wrote and rang doorbell
    static constexpr tag8_t CLAIMED   = 0x02; // GPU claimed via CAS
    static constexpr tag8_t PROCESSING= 0x03; // being processed
    static constexpr tag8_t COMPLETE  = 0x04; // GPU finished, result in value
    static constexpr tag8_t RETIRED   = 0x05; // CPU consumed result
} // namespace PackedState
