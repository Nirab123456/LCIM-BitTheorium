#pragma once
#include "LayoutBoundsOrchestrator.hpp"

namespace PredictedAdaptedEncoding
{
    struct MPMCOrchestratorForAPCRegion
    {
        enum class RegionConcurrencyProtocol : uint8_t
        {
            PRIVATE_REGION = 0,
            IMMUTABLE_SNAPSHOT = 1,
            ATOMIC_WORD_ARRAY = 2,
            MPMC_FIXED_RECORD_QUEUE = 3,
            DOUBLE_BUFFERED = 4
        };

        //LEN
        static constexpr uint8_t WORDS_PER_RECORD_LEN = LEN_OF_BYTE_IN_BITS * sizeof(uint16_t);
        static constexpr uint8_t RECORD_WORDS_LEN = LEN_OF_BYTE_IN_BITS * sizeof(uint16_t);
        static constexpr uint8_t PROTOCOL_LEN = LEN_OF_BYTE_IN_BITS * sizeof(RegionConcurrencyProtocol);
        static constexpr uint8_t FORMAT_LEN = LEN_OF_BYTE_IN_BITS * sizeof(uint8_t);
        static constexpr uint8_t VERSION_LEN = LEN_OF_BYTE_IN_BITS * sizeof(uint8_t);
        static constexpr uint8_t FLAGS_LEN = LEN_OF_BYTE_IN_BITS * sizeof(uint8_t);

        //SHIFT
        static constexpr uint8_t FLAGS_SHIFT = (LEN_OF_BYTE_IN_BITS * sizeof(uint64_t)) - FLAGS_LEN;
        static constexpr uint8_t VERSION_SHIFT = FLAGS_SHIFT - VERSION_LEN;
        static constexpr uint8_t FORMAT_SHIFT = VERSION_SHIFT - FORMAT_LEN;
        static constexpr uint8_t PROTOCOL_SHIFT = FORMAT_SHIFT - PROTOCOL_LEN;
        static constexpr uint8_t RECORD_WORDS_SHIFT = PROTOCOL_SHIFT - RECORD_WORDS_LEN;
        static constexpr uint8_t WORDS_PER_RECORD_SHIFT = RECORD_WORDS_SHIFT - WORDS_PER_RECORD_LEN;

        struct RegionSchemaRecord
        {
            uint16_t PayloadWordsPerRecord = UNSIGNED_ZERO;
            uint16_t RecordWords = UNSIGNED_ZERO;
            RegionConcurrencyProtocol Protocol = RegionConcurrencyProtocol::PRIVATE_REGION;
            uint8_t Format = UNSIGNED_ZERO;
            uint8_t Version = UNSIGNED_ZERO;
            uint8_t Flags = UNSIGNED_ZERO;
        };


    };
    

}