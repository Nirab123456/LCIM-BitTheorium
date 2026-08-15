#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "TestFiles/APCFabricVsVectorTest.hpp"
#include "TestFiles/MutexedVectorVsAPC.hpp"

int main()
{
    APCFabricVsVectorPointerChaseTest::RunAPCFabricVsVectorPointerChaseTest();
    std::cout << "SECOND TEST \n \n \n \n \n";
    return APCGlobalMutexContentionBenchmark::Run();

}