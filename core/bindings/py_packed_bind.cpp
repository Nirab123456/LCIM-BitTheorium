// bindings/py_packed_bind.cpp
#include "ArchitectureBindings.hpp"

PYBIND11_MODULE(atomiccim_bind, module)
{
    atomiccim::python::BindArchitecture(module);
}