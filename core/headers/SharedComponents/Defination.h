
#pragma once 
namespace PredictedAdaptedEncoding
{
    //runtime
    #define MIN_PRODUCER_BLOCK_SIZE 96
    #define MIN_REGION_SIZE 4
    #define MIN_RETIRE_BATCH_THRESHOLD 16
    #define MIN_BACKGROUND_EPOCH_MS 50
    #define INITIAL_BRANCH_SPLIT_THRESHOLD_PERCENTAGE 70
    #define MAX_BRANCH_DEPTH 10
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

    static constexpr size_t BIT_LENGTH_OF_FABRIC = 64;
    static constexpr uint8_t BIT_LENGTH_OF_APC = 32;
    static constexpr uint8_t SIZE_OF_CACHELINE = 64;
    static constexpr uint8_t CACHELINE_BOUNDRY = 16;
    static constexpr unsigned UNSIGNED_ZERO = 0u;
    static constexpr unsigned MINIMUM_APC_CAPACITY = 256u;
    static constexpr uint8_t EIGHT_BIT_SENTINAL = UINT8_MAX;
    static constexpr uint64_t FABRIC_CELL_SENTINAL = UINT64_MAX;

    
}