
#pragma once 
namespace PredictedAdaptedEncoding
{
    //default Rel Class percentage
    #define FEEDFOEWARD_PERCENTAGE 8u
    #define FEEDBACKWARD_PERCENTAGE 6u
    #define LATERAL_PERCENTAGE 2u
    #define STATESLOT_PERCENTAGE 8u
    #define ERRORSLOT_PERCENTAGE 6u
    #define EDGEDESCRIPTOR_PERCENTAGE 7u
    #define WEIGHTSLOT_PERCENTAGE 7u
    #define AUXSLOT_PERCENTAGE 3u
    #define HETEROGENOUS_RAW_PERCENTAGE 0u
    #define RAW64_BIT_PERCENTAGE 20u
    #define PAIRED_POINTER_PERCENTAGE 0u
    #define FREE_PERCENTAGE 30u
    #define UNDEFINED_PERCENTAGE 2u
    ////
    #define DEFAULT_MAX_TRIES 128
        
    static constexpr uint8_t LEN_OF_BYTE_IN_BITS = 8u;
    static constexpr size_t BIT_LENGTH_OF_FABRIC = LEN_OF_BYTE_IN_BITS * sizeof(uint64_t);
    static constexpr uint8_t BIT_LENGTH_OF_APC = LEN_OF_BYTE_IN_BITS * sizeof(uint32_t);
    static constexpr uint8_t SIZE_OF_CACHELINE = LEN_OF_BYTE_IN_BITS * sizeof(uint64_t);
    static constexpr unsigned UNSIGNED_ZERO = 0u;
    static constexpr unsigned MINIMUM_APC_CELL_COUNT = 256u;
    static constexpr uint8_t EIGHT_BIT_SENTINAL = UINT8_MAX;
    static constexpr uint64_t FABRIC_CELL_SENTINAL = UINT64_MAX;

}