#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace PredictedAdaptedEncoding
{

    HashFilesCarrier HashTablesConstructor::ReadHashFilesFromSlab(size_t bucked_base_index, bool caller_holds_Claim_guard) noexcept
    {
        if (!SlabBasePtr_ || bucked_base_index + HASH_BUCKED_WIDTH_OF_FABRIC > SlabCellCount_)
        {
            return HashFilesCarrier{};
        }

        const uint64_t key_cell = AtomicallyLoadReadAUnit(bucked_base_index + static_cast<size_t>(HashTableInternalIndexing::KEY_INDEX));
        const uint64_t value_cell = AtomicallyLoadReadAUnit(bucked_base_index + static_cast<size_t>(HashTableInternalIndexing::VALUE_INDEX));
        const uint64_t prob_safty_cell = AtomicallyLoadReadAUnit(bucked_base_index + static_cast<size_t>(HashTableInternalIndexing::PROB_DISTANCE_LOCK));
        
        return HashTableConf::ReadKeyValueProbFromValidCells(
            key_cell,
            value_cell,
            prob_safty_cell,
            caller_holds_Claim_guard
        );
    }

    bool HashTablesConstructor::InsertOrUpdateRobinHoodHash48_(FabricTableSegmentClasses hash_table, uint64_t key48, uint64_t value48) noexcept
    {

        RecordBookTablesBoundsCarrier desired_hash_table_bounds {};

        bool is_valid_bounds = BegainEndIdxHeaderPairGet(hash_table, desired_hash_table_bounds);
        if (!is_valid_bounds)
        {
            return false;
        }

        const uint64_t table_cell_count = desired_hash_table_bounds.EndIndex - desired_hash_table_bounds.BeginIndex;
        if ((table_cell_count % HASH_BUCKED_WIDTH_OF_FABRIC) != UNSIGNED_ZERO)
        {
            return false;
        }
        
        const uint64_t bucket_count = table_cell_count / HASH_BUCKED_WIDTH_OF_FABRIC;
        if (
            bucket_count == UNSIGNED_ZERO || 
            ((bucket_count & (bucket_count - 1u)) != UNSIGNED_ZERO)
        )
        {
            return false;
        }
        
        uint64_t incoming_key = key48;
        uint64_t incoming_value = value48;
        uint64_t incoming_hash = HashTableConf::HashUnsigned48_(incoming_key);
        uint16_t incoming_prob = UNSIGNED_ZERO;
        uint64_t incoming_bucket = incoming_hash & (bucket_count - 1u);


        HashFilesCarrier reuseable_carrier{};
        auto MakeAHashCarrierInternal = [&](
            const uint64_t& desired_key,
            const uint64_t& desired_value,
            const uint16_t& desired_prob
        ) noexcept -> void
        {
            HashTableConf::RestHashFilesCarrier(reuseable_carrier);
            reuseable_carrier.HashKey = desired_key;
            reuseable_carrier.HashValue = desired_value;
            reuseable_carrier.ProbDistance = desired_prob;
            reuseable_carrier.HashTable = hash_table;
            reuseable_carrier.AttachedLocality = LocalityPolicy::PUBLISHED;
            reuseable_carrier.IsValid = true;
        };


        HashTableConf::SingleHashBuffer reuseable_hash_buffer{};

        auto MakeReuseableBuffer = [&](
            const uint64_t& a_key,
            const uint64_t& a_value,
            const uint16_t& prob_dist
        ) -> bool
        {
            MakeAHashCarrierInternal(a_key, a_value, prob_dist);
            HashTableConf::BuildValidatedHashBuffer(reuseable_carrier, reuseable_hash_buffer);
            if (!HashTableConf::IfHashBufferHaveValidationMark(reuseable_hash_buffer))
            {
                return false;
            }
            return true;
        };

        for (uint64_t steps = 0; steps < bucket_count && incoming_prob != HashTableConf::PROB_DISTANCE_SENTINAL; steps++)
        {
            const size_t base_idx = static_cast<size_t>(desired_hash_table_bounds.BeginIndex + (incoming_bucket * HASH_BUCKED_WIDTH_OF_FABRIC));

            if (base_idx + HASH_BUCKED_WIDTH_OF_FABRIC > SlabCellCount_)
            {
                return false;
            }

            HashFilesCarrier currrent_hash_data = ReadHashFilesFromSlab(base_idx, false);

            /// Initialize / Fix Invalid / reuse / reclaim
            if (
                !currrent_hash_data.IsValid || 
                currrent_hash_data.HashKey == UNSIGNED_ZERO || 
                currrent_hash_data.AttachedLocality == LocalityPolicy::IDLE
            )
            {
                const bool reuse_made_ok = MakeReuseableBuffer(incoming_key, incoming_value, incoming_prob);

                return reuse_made_ok? ClaimThenMemCopyFromArray_(base_idx, HASH_BUCKED_WIDTH_OF_FABRIC, reuseable_hash_buffer) : false;
            }

            /// update
            if (currrent_hash_data.HashKey == incoming_key)
            {
                const bool reuse_made_ok = MakeReuseableBuffer(incoming_key, incoming_value, currrent_hash_data.ProbDistance);

                return reuse_made_ok? ClaimThenMemCopyFromArray_(base_idx, HASH_BUCKED_WIDTH_OF_FABRIC, reuseable_hash_buffer) : false;
            }
            
            if (incoming_prob > currrent_hash_data.ProbDistance)
            {
                bool reuse_made_ok = MakeReuseableBuffer(incoming_key, incoming_value, incoming_prob);

                if (!reuse_made_ok)
                {
                    return false;
                }

                const bool published_ok = ClaimThenMemCopyFromArray_(base_idx, HASH_BUCKED_WIDTH_OF_FABRIC, reuseable_hash_buffer);

                if (!published_ok)
                {
                    return false;
                }

                incoming_key = currrent_hash_data.HashKey;
                incoming_value = currrent_hash_data.HashValue;
                incoming_hash = HashTableConf::HashUnsigned48_(incoming_key);

                if (currrent_hash_data.ProbDistance == HashTableConf::PROB_DISTANCE_SENTINAL)
                {
                    return false;
                }

                incoming_prob = static_cast<uint16_t>(currrent_hash_data.ProbDistance + 1);
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




    std::optional<uint64_t> HashTablesConstructor::FindHashValue48_(FabricTableSegmentClasses hash_table, uint64_t key48) noexcept
    {
        if (
            !CoreOfFabricCoordinator::IsValidHashTable(hash_table) ||
            key48 == UNSIGNED_ZERO ||
            key48 >= HashTableConf::HASH_TOMBSTONE_KEY
        )
        {
            return std::nullopt;
        }
        
        RecordBookTablesBoundsCarrier desired_hash_table_bounds{};

        if (!BegainEndIdxHeaderPairGet(hash_table, desired_hash_table_bounds))
        {
            return std::nullopt;
        }

        const uint64_t table_cell_count = desired_hash_table_bounds.EndIndex - desired_hash_table_bounds.BeginIndex;
        if ((table_cell_count % HASH_BUCKED_WIDTH_OF_FABRIC) != UNSIGNED_ZERO)
        {
            return std::nullopt;
        }
        
        const uint64_t bucket_count_dht = table_cell_count / HASH_BUCKED_WIDTH_OF_FABRIC;


        uint64_t bucket = HashHelpers::HashUnsigned48_(key48) & (bucket_count_dht - 1u);

        for (uint64_t prob = 0; prob < bucket_count_dht; prob++)
        {
            const size_t base_idx_dht = static_cast<size_t>(desired_hash_table_bounds.BeginIndex + (bucket * HASH_BUCKED_WIDTH_OF_FABRIC));
            const HashFilesCarrier existing_hash = ReadHashFilesFromSlab(base_idx_dht, false);
            
            if (!existing_hash.IsValid || existing_hash.AttachedLocality == LocalityPolicy::IDLE)
            {
                return std::nullopt;
            }

            if (existing_hash.HashKey == key48)
            {
                return existing_hash.HashValue;
            }

            if (existing_hash.ProbDistance < prob)
            {
                return std::nullopt;
            }
            
            bucket = (bucket + 1u) & (bucket_count_dht - 1u);
        }
        
        return std::nullopt;
    }



}
