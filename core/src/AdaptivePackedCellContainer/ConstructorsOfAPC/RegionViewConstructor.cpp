#pragma once
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{

    bool RegionViewConstructor::ResolveRegionView_(
        MacroColumnOfAPC column_name,
        ResolveRegionBiteView& out
    ) noexcept
    {
        using SD = SchemaDefinition;
        using ASG = APCStorageGeometry;

        out = ResolveRegionBiteView{};
        if (
            !IsThisAPCValid() ||
            RawAPCBasePtr_ == nullptr
        )
        {
            return false;
        }
        
        const HeaderIdentifierOfAPC bounds_header = APCDataStructure::BoundsMetaIdxInHeader(column_name);

        const HeaderIdentifierOfAPC schema_header = APCDataStructure::SchemaHeaderIndexFromColumnName(column_name);

        uint64_t packed_bounds = FABRIC_CELL_SENTINAL;
        uint64_t packed_schema = FABRIC_CELL_SENTINAL;

        if (
            !ReadAPCMetaUnit(bounds_header, packed_bounds) ||
            !ReadAPCMetaUnit(schema_header, packed_schema)
        )
        {
            return false;
        }

        LayoutBoundsOrchestrator::LayoutCarrier bounds_values = LayoutBoundsOrchestrator::GetLayoutCarrierFromValidLayoutCell(packed_bounds, column_name);

        if (
            !bounds_values.IsValid ||
            bounds_values.BeginIndex < PayloadBegin() ||
            bounds_values.EndIndex > CapacityOfThisAPC_
        )
        {
            return false;
        }
        
        const uint32_t local_span = static_cast<uint32_t>(bounds_values.EndIndex - bounds_values.BeginIndex);

        SD::RegionSchemaRecord schema{};
        schema.ParentColumn = column_name;

        if (
            local_span == UNSIGNED_ZERO ||
            !SD::LayoutSchemaFromPackedCell(schema, packed_schema) ||
            !SD::ValidateSchemaAgainstLayout(schema, local_span) ||
            SD::HasSchemaFlag(schema.Flags, SD::SchemaFlags::REGION_DISABLED)
        )
        {
            return false;
        }
        
        const size_t byte_offset = ASG::ByteOffsetOfLocalIndex(bounds_values.BeginIndex);
        const size_t byte_count = ASG::ByteCountOfLOcalSpan(local_span);

        out.Bytes = std::span<std::byte>(
            RawAPCBasePtr_ + byte_offset,
            byte_count
        );

        out.layout = bounds_values;
        out.Schema = schema;
        
        return true;
    }

}