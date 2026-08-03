#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace PredictedAdaptedEncoding
{

    void HashTablesConstructor::InitializeHashTable_(FabricTableSegmentClasses hash_table) noexcept
    {
        if (!CoreOfFabricCoordinator::IsValidHashTable(hash_table))
        {
            return;
        }

        RecordBookConf::RecordBookTablesBoundsCarrier bounds{};
        bool bounds_ok = GetRecordMapCarrierRanges(hash_table, bounds);
        if (!bounds_ok)
        {
            return;
        }
        std::fill(
            SlabBasePtr_ + bounds.BeginIndex,
            SlabBasePtr_ + bounds.EndIndex,
            UNSIGNED_ZERO
        );
    }

    bool HashTablesConstructor::ReadHashBufferFromSlab(
        uint64_t bucket_idx,
        HashTableConf::SingleHashBuffer& hash_buffer_return
    ) noexcept
    {
        using HTC = HashTableConf;
        if (
            !SlabBasePtr_ || 
            bucket_idx + HTC::HASH_BUCKED_WIDTH_OF_FABRIC > SlabCellCount_
        )
        {
            return false;
        }

        for (uint8_t i = 0; i < CoreOfFabricCoordinator::HASH_BUCKED_WIDTH_OF_FABRIC; i++)
        {
            if (!AtomicallyLoadReadAUnit(bucket_idx + i, hash_buffer_return[i]))
            {
                return false;
            }
        }
        return true;
    }


    std::optional<uint64_t> HashTablesConstructor::ReserveHashBuffer(
        uint64_t bucket_idx,
        HashTableConf::SingleHashBuffer& hash_buffer_return,
        uint32_t max_tries
    ) noexcept
    {
        using HTC = HashTableConf;
        for (size_t i = 0; i < max_tries; i++)
        {
            if (
                !ReadHashBufferFromSlab(bucket_idx, hash_buffer_return)
            )
            {
                return std::nullopt;
            }
            
            Pack32_28_4BitIn64BitUnit::Pack32_28_4_Carrier st_dist_fp = HTC::GetStDistFp(hash_buffer_return);
            const HTC::HashState hash_state = static_cast<HTC::HashState>(st_dist_fp.High4Bit);
            if (hash_state == HTC::HashState::RESERVED)
            {
                continue;
            }
            st_dist_fp.High4Bit = static_cast<uint8_t>(HTC::HashState::RESERVED);

            uint64_t updated_reserved_st_dist_fp = Pack32_28_4BitIn64BitUnit::PackValues(st_dist_fp);
            const uint8_t fp_st_idx = static_cast<uint8_t>(HTC::HashBufferIndexing::PROB_DISTANCE_LOCK);
            uint64_t previous_st_dist_fp = hash_buffer_return[fp_st_idx];
            if (
                !CompareExchangeStrongFromFabric(
                    bucket_idx + fp_st_idx,
                    previous_st_dist_fp,
                    updated_reserved_st_dist_fp
                )
            )
            {
                continue;;
            }

            hash_buffer_return[fp_st_idx] = updated_reserved_st_dist_fp;
            return previous_st_dist_fp;
        }

        return std::nullopt;
    }

    bool HashTablesConstructor::ReleseHashBuffer(
        uint64_t bucket_idx,
        uint64_t previous_st_dist_fp 
    ) noexcept
    {
        using HTC = HashTableConf;
        Pack32_28_4BitIn64BitUnit::Pack32_28_4_Carrier st_dist_fp =  Pack32_28_4BitIn64BitUnit::UnpackUnitToCarrier(previous_st_dist_fp);
        const uint8_t fp_st_idx = static_cast<uint8_t>(HTC::HashBufferIndexing::PROB_DISTANCE_LOCK);

        uint64_t current_fp_st_value = UNSIGNED_ZERO;
        if (
            !AtomicallyLoadReadAUnit(bucket_idx + fp_st_idx, current_fp_st_value)
        )
        {
            return false;
        }
        const Pack32_28_4BitIn64BitUnit::Pack32_28_4_Carrier current_st_dist_fp = Pack32_28_4BitIn64BitUnit::UnpackUnitToCarrier(current_fp_st_value);
        const bool current_state_reserved = static_cast<HTC::HashState>(current_st_dist_fp.High4Bit) == HTC::HashState::RESERVED;
        const bool previous_state_reserved = static_cast<HTC::HashState>(st_dist_fp.High4Bit) == HTC::HashState::RESERVED;
        return 
            !current_state_reserved &&
            !previous_state_reserved &&
            CompareExchangeStrongFromFabric(
                bucket_idx,
                current_fp_st_value,
                previous_st_dist_fp
            );
    }

    bool HashTablesConstructor::InsertOrUpdateRobinHoodHash48_(
        FabricTableSegmentClasses hash_table, 
        uint64_t hash_key, 
        uint64_t hash_value,
        std::optional<HashTableConf::StateOfAPC> hash_state 
    ) noexcept
    {
        if (
            !CoreOfFabricCoordinator::IsValidHashTable(hash_table) ||
            !HashIdConstructror::IsValidAPCId(hash_key) ||
            !APCDataStructure::IsValidFabricUnit(hash_value)
        )
        {
            return false;
        }
        
        RecordBookConf::RecordBookTablesBoundsCarrier desired_hash_table_bounds {};

        bool is_valid_bounds = GetRecordMapCarrierRanges(hash_table, desired_hash_table_bounds);
        if (!is_valid_bounds)
        {
            return false;
        }

        const uint64_t table_cell_count = desired_hash_table_bounds.EndIndex - desired_hash_table_bounds.BeginIndex;
        if ((table_cell_count % HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC) != UNSIGNED_ZERO)
        {
            return false;
        }
        
        const uint64_t bucket_count = table_cell_count / CoreOfFabricCoordinator::HASH_BUCKED_WIDTH_OF_FABRIC;
        if (
            bucket_count == UNSIGNED_ZERO || 
            ((bucket_count & (bucket_count - 1u)) != UNSIGNED_ZERO)
        )
        {
            return false;
        }
        
        uint64_t incoming_key = hash_key;
        uint64_t incoming_value = hash_value;
        uint64_t incoming_hash = HashIdConstructror::HashUnsigned64(incoming_key);
        uint32_t incoming_prob = UNSIGNED_ZERO;

        uint64_t incoming_bucket = incoming_hash & (bucket_count - 1u);

        HashTableConf::SingleHashBuffer reuseable_hash_buffer{};


        for (uint64_t steps = 0; steps < bucket_count && incoming_prob != HashTableConf::PROB_DISTANCE_SENTINAL; steps++)
        {
            const size_t base_idx = static_cast<size_t>(
                desired_hash_table_bounds.BeginIndex + 
                (incoming_bucket * CoreOfFabricCoordinator::HASH_BUCKED_WIDTH_OF_FABRIC)
            );

            if (base_idx + CoreOfFabricCoordinator::HASH_BUCKED_WIDTH_OF_FABRIC > SlabCellCount_)
            {
                return false;
            }

            Pack32_28_4BitIn64BitUnit::Pack32_28_4_Carrier state_dist_fp{};
            if (
                !ReadHashBufferFromSlab(base_idx, reuseable_hash_buffer)
            )
            {
                return false;
            }
            
            std::optional<uint64_t> previous_std_dist_fp_value = std::nullopt;

            /// Initialize / Fix Invalid / reuse / reclaim
            if (!HashTableConf::ValidateHashBuffer(reuseable_hash_buffer, &state_dist_fp))
            {
                const bool reuse_made_ok = HashTableConf::MakeValidHashBuffer(
                    reuseable_hash_buffer,
                    incoming_key, incoming_value, 
                    incoming_prob, 
                    hash_state.value_or(HashTableConf::StateOfAPC::LIVE_OR_PUBLISHED)
                );
                previous_std_dist_fp_value = ReserveHashBuffer(base_idx, reuseable_hash_buffer);
                if (!previous_std_dist_fp_value.has_value())
                {
                    return false;
                }
                if (
                    reuse_made_ok && 
                    AtomicallyCopyFromBufferToFabric(
                    base_idx, 
                    HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC, 
                    reuseable_hash_buffer.data()
                    )
                )
                {
                    return true;
                }
                else
                {
                    ReleseHashBuffer(base_idx, previous_std_dist_fp_value.value());
                }
                return false;
            }

            /// update
            if (
                HashTableConf::GetAUnitFromHashBuffer(reuseable_hash_buffer, HashTableConf::HashBufferIndexing::KEY_INDEX) == incoming_key &&
                state_dist_fp.IsValid
            )
            {
                const bool reuse_made_ok = HashTableConf::MakeValidHashBuffer(
                    reuseable_hash_buffer,
                    incoming_key, incoming_value, 
                    state_dist_fp.Lowest32Bit,
                    hash_state.value_or(HashTableConf::StateOfAPC::LIVE_OR_PUBLISHED)
                );

                previous_std_dist_fp_value = ReserveHashBuffer(base_idx, reuseable_hash_buffer);
                if (!previous_std_dist_fp_value.has_value())
                {
                    return false;
                }

                if (
                    reuse_made_ok &&
                    AtomicallyCopyFromBufferToFabric(
                        base_idx, 
                        HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC, 
                        reuseable_hash_buffer.data()
                    )
                )
                {
                    return true;
                }
                else
                {
                    ReleseHashBuffer(base_idx, previous_std_dist_fp_value.value());
                }
                return false;
            }
            
            if (incoming_prob > state_dist_fp.Lowest32Bit)
            {
                const HashTableConf::SingleHashBuffer buffer = reuseable_hash_buffer;

                const bool reuse_made_ok = HashTableConf::MakeValidHashBuffer(
                    reuseable_hash_buffer,
                    incoming_key, incoming_value, 
                    incoming_prob, 
                    hash_state.value_or(HashTableConf::StateOfAPC::LIVE_OR_PUBLISHED)
                );


                if (!reuse_made_ok)
                {
                    return false;
                }

                const bool published_ok = AtomicallyCopyFromBufferToFabric(
                    base_idx, 
                    HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC, 
                    reuseable_hash_buffer.data()
                );

                if (!published_ok)
                {
                    return false;
                }

                incoming_key = HashTableConf::GetAUnitFromHashBuffer(buffer, HashTableConf::HashBufferIndexing::KEY_INDEX);
                incoming_value = HashTableConf::GetAUnitFromHashBuffer(buffer, HashTableConf::HashBufferIndexing::VALUE_INDEX);
                incoming_hash = HashIdConstructror::HashUnsigned64(incoming_key);

                if (state_dist_fp.Lowest32Bit == HashTableConf::PROB_DISTANCE_SENTINAL)
                {
                    return false;
                }

                incoming_prob = static_cast<uint32_t>(state_dist_fp.Lowest32Bit + 1);
            }
            else
            {
                if (incoming_prob == HashTableConf::PROB_DISTANCE_SENTINAL)
                {
                    return false;
                }

                incoming_prob++;
            }
            
            incoming_bucket = (incoming_bucket + 1) & (bucket_count - 1);
        }
        
        return false;
    }




    std::optional<uint64_t> HashTablesConstructor::ReadHashValueConcurrently(FabricTableSegmentClasses hash_table, uint64_t hash_key) noexcept
    {        
        RecordBookConf::RecordBookTablesBoundsCarrier desired_hash_table_bounds{};
        if (
            !CoreOfFabricCoordinator::IsValidHashTable(hash_table) ||
            !HashIdConstructror::IsValidAPCId(hash_key) ||
            !GetRecordMapCarrierRanges(hash_table, desired_hash_table_bounds)
        )
        {
            return std::nullopt;
        }
        const uint64_t table_cell_count = desired_hash_table_bounds.EndIndex - desired_hash_table_bounds.BeginIndex;
        if ((table_cell_count % HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC) != UNSIGNED_ZERO)
        {
            return std::nullopt;
        }
        
        const uint64_t bucket_count = table_cell_count / HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC;


        uint64_t bucket = HashIdConstructror::HashUnsigned64(hash_key) & (bucket_count - 1u);

        HashTableConf::SingleHashBuffer reuseable_hash_buffer{};
        Pack32_28_4BitIn64BitUnit::Pack32_28_4_Carrier state_dist_fp{};

        for (uint32_t prob = 0; prob < bucket_count; prob++)
        {
            const size_t base_idx_dht = static_cast<size_t>(desired_hash_table_bounds.BeginIndex + (bucket * HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC));
            
            if (!ReadHashBufferFromSlab(base_idx_dht, reuseable_hash_buffer))
            {
                return false;
            }
            
            bool is_expected = HashTableConf::IsExpectedHashStateBuffer(
                reuseable_hash_buffer,
                hash_key,
                HashTableConf::HashState::LIVE_OR_PUBLISHED,
                &state_dist_fp
            );

            if (is_expected)
            {
                return HashTableConf::GetAUnitFromHashBuffer(reuseable_hash_buffer, HashTableConf::HashBufferIndexing::VALUE_INDEX);
            }
            if (state_dist_fp.Lowest32Bit < prob)
            {
                return std::nullopt;
            }
            bucket = (bucket + 1u) & (bucket_count - 1u);
        }
        
        return std::nullopt;
    }


    bool HashTablesConstructor::RetireHashKey(FabricTableSegmentClasses hash_table, uint64_t hash_key) noexcept
    {
        RecordBookConf::RecordBookTablesBoundsCarrier desired_hash_table_bounds{};

        if (
            !CoreOfFabricCoordinator::IsValidHashTable(hash_table) ||
            !HashIdConstructror::IsValidAPCId(hash_key) ||
            !GetRecordMapCarrierRanges(hash_table, desired_hash_table_bounds)
        )
        {
            return false;
        }
        
        const uint64_t table_cell_count = desired_hash_table_bounds.EndIndex - desired_hash_table_bounds.BeginIndex;
        if ((table_cell_count % HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC) != UNSIGNED_ZERO)
        {
            return false;
        }
        
        const uint64_t bucket_count = table_cell_count / HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC;

        uint64_t bucket = HashIdConstructror::HashUnsigned64(hash_key) & (bucket_count - 1u);

        HashTableConf::SingleHashBuffer reuseable_hash_buffer{};
        Pack32_28_4BitIn64BitUnit::Pack32_28_4_Carrier state_dist_fp{};

        for (uint32_t prob = 0; prob < bucket_count; prob++)
        {
            const size_t base_idx_dht = static_cast<size_t>(desired_hash_table_bounds.BeginIndex + (bucket * HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC));

            if (
                !ReadHashBufferFromSlab(base_idx_dht, reuseable_hash_buffer)
            )
            {
                return false;
            }
            
            bool is_expected = HashTableConf::IsExpectedHashStateBuffer(
                reuseable_hash_buffer,
                hash_key,
                HashTableConf::HashState::RESERVED,
                &state_dist_fp
            );

            if (is_expected)
            {
                const std::optional<uint64_t> previous_std_dist_fp_value = ReserveHashBuffer(base_idx_dht, reuseable_hash_buffer);
                if (!previous_std_dist_fp_value.has_value())
                {
                    return false;
                }
                bool recompiled = HashTableConf::RecompileStateInBuffer(
                    reuseable_hash_buffer, 
                    HashTableConf::HashState::RETIRED_OR_TOMBSTONE
                );

                if (
                    recompiled &&
                    AtomicallyCopyFromBufferToFabric(
                        base_idx_dht,
                        HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC,
                        reuseable_hash_buffer.data()
                    )
                )
                {
                    return true;
                }
                else
                {
                    ReleseHashBuffer(base_idx_dht, previous_std_dist_fp_value.value());
                }

                return false;
            }
            if (state_dist_fp.Lowest32Bit < prob)
            {
                return false;
            }
            bucket = (bucket + 1u) & (bucket_count - 1u);
        }
        
        return false;
    }

}
