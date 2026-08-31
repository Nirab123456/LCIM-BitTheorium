RELESE BUILD: 

WINDOWS :
```cpp

Remove-Item -Recurse -Force .\build-release -ErrorAction SilentlyContinue

cmake -S .\core -B .\build-release `
    -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DATOMICCIM_ENABLE_IPO=ON `
    -DATOMICCIM_NATIVE_CPU=OFF `
    -DPython_EXECUTABLE="$((Get-Command python).Source)" `
    -DATOMICCIM_FAST_FP=OFF 
    
cmake --build .\build-release `
    --target AtomicCIM atomiccim_bind `
    --parallel `
    --verbose
    
.\build-release\AtomicCIM.exe
```
