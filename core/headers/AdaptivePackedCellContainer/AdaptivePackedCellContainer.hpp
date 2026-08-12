#pragma once 
#include "ConstructorsOfAPC/ReadAndWriteOfAPC.h"

namespace BidirectionalInMemGraph
{
static_assert(__cpp_lib_atomic_wait, "C++ must suppoet atomic wait/notify");


class AdaptivePackedCellContainer : public ReadAndWriteOfAPC
{
public:
    using IAB = InstallAxisToBuffer;

    // template <typename TempType> 
    // std::array<TempType , APCDataStructure::CountOfMacroColumn()> ViewTable_

    template <typename TempType>
    TempType* BuildAViewOverRegion(MacroColumnOfAPC region) noexcept;

    template <typename T>
    bool InitiateTheContainerAsSingleRegionTypeProtocol(
        SchemDefinition::SchemaProtocols protocol,
        MacroColumnOfAPC column
    ) noexcept;


    bool AttachAnotherToMe(
        AdaptivePackedCellContainer& sibbling,
        IAB::BidirectionalAxis axis,
        IAB::DescOfInharitance inharitance
    ) noexcept;

    bool AttachMeToAnother(
        AdaptivePackedCellContainer& sibbling,
        IAB::BidirectionalAxis axis,
        IAB::DescOfInharitance inharitance
    ) noexcept;

    bool DetachMySibbling(
        IAB::BidirectionalAxis axis,
        IAB::DescOfInharitance inharitance
    ) noexcept;

    bool DetachMe(
        IAB::BidirectionalAxis axis,
        IAB::DescOfInharitance inharitance
    ) noexcept;



};


}  