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

    HashTableConf::HashFilesCarrier HashTablesConstructor::ReadHashFilesFromSlab(uint64_t bucked_base_index) noexcept
    {
        HashTableConf::HashFilesCarrier carrier{};
        
        if (
            !SlabBasePtr_ || 
            bucked_base_index + HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC > SlabCellCount_
        )
        {
            return carrier;
        }
        uint64_t key_cell = UNSIGNED_ZERO;
        uint64_t value_cell = UNSIGNED_ZERO;
        uint64_t prob_st_fp = UNSIGNED_ZERO;
        AtomicallyLoadReadAUnit(bucked_base_index + static_cast<uint64_t>(HashTableConf::HashBufferIndexing::KEY_INDEX), key_cell);
        AtomicallyLoadReadAUnit(bucked_base_index + static_cast<uint64_t>(HashTableConf::HashBufferIndexing::VALUE_INDEX), value_cell);
        AtomicallyLoadReadAUnit(bucked_base_index + static_cast<uint64_t>(HashTableConf::HashBufferIndexing::PROB_DISTANCE_LOCK), prob_st_fp);
        
        Pack32_28_4BitIn64BitUnit::Pack32_28_4_Carrier prob_st_fp_carrier = Pack32_28_4BitIn64BitUnit::UnpackUnitToCarrier(prob_st_fp);

        carrier.HashKey = key_cell;
        carrier.HashValue = value_cell;
        carrier.ProbDistance = prob_st_fp_carrier.Lowest32Bit;
        carrier.HashState = static_cast<HashTableConf::StateOfAPC>(prob_st_fp_carrier.High4Bit);

        const uint32_t reconst_finger_print = HashTableConf::MakeHashFingerPrint(carrier);
        HashTableConf::IsValidHashBuffer(carrier);
        if (
            reconst_finger_print != prob_st_fp_carrier.Mid28Bit 
        )
        {
            carrier.IsValid = false;
            return carrier;
        }
        
        return carrier;

    }


    // bool HashTablesConstructor::ReadHashFilesFromSlab_(
    //     uint64_t bucked_base_index,
    //     HashTableConf::SingleHashBuffer hash_buffer_return
    // ) noexcept
    // {
    //     using HTC = HashTableConf;
    //     if (
    //         !SlabBasePtr_ || 
    //         bucked_base_index + HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC > SlabCellCount_
    //     )
    //     {
    //         return false;
    //     }

    //     for (uint8_t i = 0; i < CoreOfFabricCoordinator::HASH_BUCKED_WIDTH_OF_FABRIC; i++)
    //     {
    //         if (!AtomicallyLoadReadAUnit(bucked_base_index + i, hash_buffer_return[i]))
    //         {
    //             return false;
    //         }
    //     }
        

    // }

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


        HashTableConf::HashFilesCarrier reuseable_carrier{};
        auto MakeAHashCarrierInternal = [&](
            const uint64_t& desired_key,
            const uint64_t& desired_value,
            const uint32_t& desired_prob,
            const HashTableConf::StateOfAPC state
        ) noexcept -> void
        {
            HashTableConf::RestHashFilesCarrier(reuseable_carrier);
            reuseable_carrier.HashKey = desired_key;
            reuseable_carrier.HashValue = desired_value;
            reuseable_carrier.ProbDistance = desired_prob;
            reuseable_carrier.HashTable = hash_table;
            reuseable_carrier.HashState = state;
            HashTableConf::IsValidHashBuffer(reuseable_carrier);
        };

        HashTableConf::SingleHashBuffer reuseable_hash_buffer{};

        auto MakeReuseableBuffer = [&](
            const uint64_t& a_key,
            const uint64_t& a_value,
            const uint32_t& prob_dist,
            const HashTableConf::StateOfAPC state
        ) -> bool
        {
            MakeAHashCarrierInternal(a_key, a_value, prob_dist, state);
            return HashTableConf::BuildValidatedHashBuffer(reuseable_carrier, reuseable_hash_buffer);
        };

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

            HashTableConf::HashFilesCarrier currrent_hash_data = ReadHashFilesFromSlab(base_idx);

            /// Initialize / Fix Invalid / reuse / reclaim
            if (!currrent_hash_data.IsValid )
            {
                const bool reuse_made_ok = MakeReuseableBuffer(
                    incoming_key, incoming_value, 
                    incoming_prob, 
                    hash_state.value_or(HashTableConf::StateOfAPC::FREE_OR_EMPTY)
                );

                return reuse_made_ok? CompareExchangeStrongSequentiallyOrRevert(
                    base_idx, 
                    HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC, 
                    reuseable_hash_buffer.data()
                ) : false;
            }

            /// update
            if (currrent_hash_data.HashKey == incoming_key)
            {
                const bool reuse_made_ok = MakeReuseableBuffer(
                    incoming_key, 
                    incoming_value, 
                    currrent_hash_data.ProbDistance, 
                    hash_state.has_value() ? hash_state.value() : currrent_hash_data.HashState
                );
                return reuse_made_ok? CompareExchangeStrongSequentiallyOrRevert(
                    base_idx, 
                    HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC, 
                    reuseable_hash_buffer.data()
                ) : false;
            }
            
            if (incoming_prob > currrent_hash_data.ProbDistance)
            {
                const bool reuse_made_ok = MakeReuseableBuffer(
                    incoming_key, incoming_value, 
                    incoming_prob, 
                    hash_state.value_or(HashTableConf::StateOfAPC::FREE_OR_EMPTY)
                );

                if (!reuse_made_ok)
                {
                    return false;
                }

                const bool published_ok = CompareExchangeStrongSequentiallyOrRevert(
                    base_idx, 
                    HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC, 
                    reuseable_hash_buffer.data()
                );

                if (!published_ok)
                {
                    return false;
                }

                incoming_key = currrent_hash_data.HashKey;
                incoming_value = currrent_hash_data.HashValue;
                incoming_hash = HashIdConstructror::HashUnsigned64(incoming_key);

                if (currrent_hash_data.ProbDistance == HashTableConf::PROB_DISTANCE_SENTINAL)
                {
                    return false;
                }

                incoming_prob = static_cast<uint32_t>(currrent_hash_data.ProbDistance + 1);
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
        if (
            !CoreOfFabricCoordinator::IsValidHashTable(hash_table) ||
            !HashIdConstructror::IsValidAPCId(hash_key)
        )
        {
            return std::nullopt;
        }
        
        RecordBookConf::RecordBookTablesBoundsCarrier desired_hash_table_bounds{};

        if (!GetRecordMapCarrierRanges(hash_table, desired_hash_table_bounds))
        {
            return std::nullopt;
        }

        const uint64_t table_cell_count = desired_hash_table_bounds.EndIndex - desired_hash_table_bounds.BeginIndex;
        if ((table_cell_count % HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC) != UNSIGNED_ZERO)
        {
            return std::nullopt;
        }
        
        const uint64_t bucket_count_dht = table_cell_count / HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC;


        uint64_t bucket = HashIdConstructror::HashUnsigned64(hash_key) & (bucket_count_dht - 1u);

        for (uint64_t prob = 0; prob < bucket_count_dht; prob++)
        {
            const size_t base_idx_dht = static_cast<size_t>(desired_hash_table_bounds.BeginIndex + (bucket * HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC));
            const HashTableConf::HashFilesCarrier existing_hash = ReadHashFilesFromSlab(base_idx_dht);
            
            if (!existing_hash.IsValid)
            {
                return std::nullopt;
            }

            if (existing_hash.HashState == HashTableConf::StateOfAPC::FREE_OR_EMPTY)
            {
                return std::nullopt;
            }
            
            if (existing_hash.HashState == HashTableConf::StateOfAPC::LIVE_OR_PUBLISHED)
            {
                if (existing_hash.HashKey == hash_key)
                {
                    return HashIdConstructror::IsValidAPCId(existing_hash.HashValue) ? 
                        std::optional<uint64_t>{existing_hash.HashValue} : std::nullopt;
                }
                if (existing_hash.ProbDistance < prob)
                {
                    return std::nullopt;
                }
            }
            
            bucket = (bucket + 1u) & (bucket_count_dht - 1u);
        }
        
        return std::nullopt;
    }

    bool HashTablesConstructor::RetireHashKey(FabricTableSegmentClasses table, uint64_t hash_key) noexcept
    {
        RecordBookConf::RecordBookTablesBoundsCarrier hash_table_bounds{};
        if (!GetRecordMapCarrierRanges(table, hash_table_bounds))
        {
            return false;
        }

        const uint64_t bucket_count = (hash_table_bounds.EndIndex - hash_table_bounds.BeginIndex) / HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC;

        if (!APCDataStructure::IsPowerOfTwoValue(bucket_count))
        {
            return false;
        }
        
        uint64_t bucket = HashIdConstructror::HashUnsigned64(hash_key) & (bucket_count - 1);
        HashTableConf::SingleHashBuffer hash_buffer{};

        for (uint64_t i = 0; i < bucket_count; i++)
        {
            const uint64_t base = static_cast<uint64_t>(
                hash_table_bounds.BeginIndex + bucket * HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC
            );

            HashTableConf::HashFilesCarrier cur_hash_files = ReadHashFilesFromSlab(base);
            if (
                !cur_hash_files.IsValid ||
                cur_hash_files.HashState == HashTableConf::StateOfAPC::FREE_OR_EMPTY
            )
            {
                return false;
            }

            if (
                cur_hash_files.HashState == HashTableConf::StateOfAPC::LIVE_OR_PUBLISHED &&
                cur_hash_files.HashKey == hash_key
            )
            {
                cur_hash_files.HashState = HashTableConf::StateOfAPC::RETIRED_OR_TOMBSTONE;
                HashTableConf::BuildEmptyHashBuffer(hash_buffer);
                const bool buffer_ok = HashTableConf::BuildValidatedHashBuffer(cur_hash_files, hash_buffer);
                return buffer_ok &&
                    CompareExchangeStrongSequentiallyOrRevert(
                        base,
                        HashTableConf::HASH_BUCKED_WIDTH_OF_FABRIC,
                        hash_buffer.data()
                    );
            }
            
            bucket = (bucket + 1u) & (bucket_count - 1u);
        }
        return false;
    }

}
