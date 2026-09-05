#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"


namespace BidirectionalInMemGraph
{

    std::span<SchemaDefinition::RegionSchemaRecord> MatrixViewConstructor::MetrixViewRow_(uint32_t apc_slot) noexcept
    {
        if (
            !SlabBasePtr_ ||
            apc_slot > CountOfAPC_ ||
            ActiveRegionCount_ == UNSIGNED_ZERO ||
            MatrixViewRowCellCount_ != ActiveRegionCount_ * SD::RegionSchemaCellCount()
        )
        {
            return {};
        }

        std::byte* table_bytes = reinterpret_cast<std::byte*>(SlabBasePtr_ + MatrixViewRowCellCount_);

        const size_t row_byte_offset = static_cast<size_t>(apc_slot) * MatrixViewRowCellCount_ * sizeof(uint64_t);

        SD::RegionSchemaRecord* row = std::launder(reinterpret_cast<SD::RegionSchemaRecord*>(table_bytes + row_byte_offset));

        return std::span<SD::RegionSchemaRecord>(row, ActiveRegionCount_);
    }

    bool MatrixViewConstructor::ConstructMatrixViewRecords_(size_t table_begin, size_t table_end) noexcept
    {
        const size_t total_records = static_cast<size_t>(CountOfAPC_) * ActiveRegionCount_;
        const size_t required_cells = total_records * SD::RegionSchemaCellCount();

        if (
            !SlabBasePtr_ ||
            table_begin >= table_end ||
            table_end - table_begin != required_cells ||
            table_end > SlabCellCount_
        )
        {
            return false;
        }
        
        std::byte* bytes = reinterpret_cast<std::byte*>(SlabBasePtr_ + table_begin);
        for (size_t i = 0; i < total_records; i++)
        {
            void* place = static_cast<void*>(bytes + (i * sizeof(SD::RegionSchemaRecord)));
            std::construct_at(reinterpret_cast<SD::RegionSchemaRecord*>(place), SD::RegionSchemaRecord{});
        }
        return true;
    }

    bool MatrixViewConstructor::PrepareMatrixViewRow_(
        uint32_t apc_slot,
        const SD::RegionSchemaTable& requested
    ) noexcept
    {
        std::span<SD::RegionSchemaRecord> destination = MetrixViewRow_(apc_slot);
        if (destination.size() != ActiveRegionCount_)
        {
            return false;
        }
        
        SD::RegionSchemaTable prepared{};
        uint16_t observed_mask = UNSIGNED_ZERO;
        uint8_t prepared_count = UNSIGNED_ZERO;
        uint32_t cursor = UNSIGNED_ZERO;

        for (uint8_t i = 0; i < ColumnConf::CountOfMacroColumn(); i++)
        {
            const SD::RegionSchemaRecord& source = requested[i];
            const MacroColumnOfAPC expected_region = static_cast<MacroColumnOfAPC>(i);
            if (
                source.Region != expected_region ||
                prepared_count >= prepared.size()
            )
            {
                return false;
            }

            if (SD::HasSchemaFlag(source.Flags, SD::SchemaFlags::REGION_DISABLED))
            {
                continue;
            }

            SD::RegionSchemaRecord record = source;
            record.CellOffset = SD::AlignRegionCells(cursor);

            if (
                SD::HasSchemaFlag(record.Flags, SD::SchemaFlags::BATCHED_LAST_DIM) &&
                record.MatrixHeight != MatrixBatchCapacity_
            )
            {
                return false;
            }
            
            if (!SD::FreshProtocolState(record))
            {
                return false;
            }

            if (!SD::ValidateStortedRegionSchema(
                record,
                PerAPCRuntimeCellCount_,
                MatrixBatchCapacity_
            ))
            {
                return false;
            }
            
            cursor = record.CellOffset + record.CellCount;
            observed_mask = static_cast<uint16_t>(observed_mask | ColumnConf::RegionBit(expected_region));
            prepared[prepared_count++] = record;
        }
        
        if (
            observed_mask != ActiveRegionCount_ ||
            prepared_count != ActiveRegionCount_
        )
        {
            return false;
        }
        
        for (uint8_t i = 0; i < prepared_count; i++)
        {
            destination[i] = prepared[i];
        }
        return true;
    }



}