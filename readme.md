RELESE BUILD: 




Remove-Item -Recurse -Force .\build-release -ErrorAction SilentlyContinue

cmake -S .\core -B .\build-release `
    -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DATOMICCIM_ENABLE_IPO=ON `
    -DATOMICCIM_NATIVE_CPU=OFF `
    -DATOMICCIM_FAST_FP=OFF

cmake --build .\build-release `
    --target AtomicCIM `
    --parallel `
    --verbose

.\build-release\AtomicCIM.exe