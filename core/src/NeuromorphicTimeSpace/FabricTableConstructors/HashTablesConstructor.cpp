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
        uint64_t bucked_base_index,
        HashTableConf::SingleHashBuffer& hash_buffer_return
    ) noexcept
    {
        using HTC = HashTableConf;
        if (
            !SlabBasePtr_ || 
            bucked_base_index + HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC > SlabCellCount_
        )
        {
            return false;
        }

        for (uint8_t i = 0; i < CoreOfFabricCoordinator::HASH_BUCKED_WIDTH_OF_FABRIC; i++)
        {
            if (!AtomicallyLoadReadAUnit(bucked_base_index + i, hash_buffer_return[i]))
            {
                return false;
            }
        }
        return HTC::ValidateHashBuffer(hash_buffer_return);
    }

    bool HashTablesConstructor::InsertOrUpdateRobinHoodHash48_(
        FabricTableSegmentClasses hash_table, 
        uint64_t hash_key, 
        uint64_t hash_value,
        std::optional<HashTableConf::StateOfAPC> hash_state 
    ) noexcept
    {

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

            bool read_buffer_ok = ReadHashBufferFromSlab(base_idx, reuseable_hash_buffer);
            const Pack32_28_4BitIn64BitUnit::Pack32_28_4_Carrier state_dist_fp = HashTableConf::GetStDistFp(reuseable_hash_buffer);


            /// Initialize / Fix Invalid / reuse / reclaim
            if (!read_buffer_ok)
            {

                const bool reuse_made_ok = HashTableConf::MakeValidHashBuffer(
                    reuseable_hash_buffer,
                    incoming_key, incoming_value, 
                    incoming_prob, 
                    hash_state.value_or(HashTableConf::StateOfAPC::FREE_OR_EMPTY)
                );

                return reuse_made_ok? AtomicallyCopyFromBufferToFabric(
                    base_idx, 
                    HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC, 
                    reuseable_hash_buffer.data()
                ) : false;
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
                    hash_state.value_or(HashTableConf::StateOfAPC::FREE_OR_EMPTY)
                );

                return reuse_made_ok? AtomicallyCopyFromBufferToFabric(
                    base_idx, 
                    HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC, 
                    reuseable_hash_buffer.data()
                ) : false;
            }
            
            if (incoming_prob > state_dist_fp.Lowest32Bit)
            {
                const HashTableConf::SingleHashBuffer buffer = reuseable_hash_buffer;

                const bool reuse_made_ok = HashTableConf::MakeValidHashBuffer(
                    reuseable_hash_buffer,
                    incoming_key, incoming_value, 
                    incoming_prob, 
                    hash_state.value_or(HashTableConf::StateOfAPC::FREE_OR_EMPTY)
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
                incoming_value = HashTableConf::GetAUnitFromHashBuffer(buffer, HashTableConf::HashBufferIndexing::KEY_INDEX);
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




    std::optional<uint64_t> HashTablesConstructor::FindUsedHashValue(FabricTableSegmentClasses hash_table, uint64_t hash_key) noexcept
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
            const bool valid = ReadHashBufferFromSlab(base_idx_dht, reuseable_hash_buffer);
            
            if (!valid)
            {
                bucket = (bucket + 1u) & (bucket_count - 1u);
                continue;
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
            const bool valid = ReadHashBufferFromSlab(base_idx_dht, reuseable_hash_buffer);
            
            if (!valid)
            {
                bucket = (bucket + 1u) & (bucket_count - 1u);
                continue;
            }
            bool is_expected = HashTableConf::IsExpectedHashStateBuffer(
                reuseable_hash_buffer,
                hash_key,
                HashTableConf::HashState::LIVE_OR_PUBLISHED,
                &state_dist_fp
            );

            if (is_expected)
            {
                bool recompiled = HashTableConf::RecompileStateInBuffer(
                    reuseable_hash_buffer, 
                    HashTableConf::HashState::RETIRED_OR_TOMBSTONE
                );
                return recompiled &&
                    AtomicallyCopyFromBufferToFabric(
                        base_idx_dht,
                        HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC,
                        reuseable_hash_buffer.data()
                    );
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
