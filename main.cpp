#include "core/relbittest.h"
#include <iostream>

int main() {
    using T = TestRBA<unsigned long>;
    auto A = T::MakeFrBits(128);
    auto B = T::MakeFrBits(128);
    auto Out = T::MakeFrBits(128);

    // sample init
    A.V[0] = 0xFFFF0000FFFF0000UL;
    B.V[0] = 0x00FF00FF00FF00FFUL;
    A.normalize();
    B.normalize();

    // set some state and relation masks
    A.st[0] = 0x0000FFFF00000000UL;
    B.st[0] = 0x00000000FFFF0000UL;
    A.rel[0] = 0xFF;

    T::RIAnd(A, B, Out);

    std::cout << std::hex;
    std::cout << "Out.V[0]  = 0x" << Out.V[0] << "\n";
    std::cout << "Out.inV[0]= 0x" << Out.inV[0] << "\n";
    std::cout << "Out.st[0] = 0x" << Out.st[0] << "\n";
    std::cout << "Out.rel[0]= 0x" << Out.rel[0] << "\n";
    std::cout << "Invariant mismatches: " << Out.CheckInvarients() << "\n";
    return 0;
}
