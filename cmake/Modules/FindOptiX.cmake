# cmake/Modules/FindOptiX.cmake

# 1. Look for the OptiX headers
find_path(OptiX_INCLUDE_DIR 
    NAMES optix.h
    PATHS 
        "${OPTIX_ROOT}/include"
        "${CMAKE_SOURCE_DIR}/../../lib/include" # Your custom build path
        "$ENV{OPTIX_INSTALL_DIR}/include"
        "C:/ProgramData/NVIDIA Corporation/OptiX SDK 7.6.0/include"
)

# 2. OptiX needs CUDA to actually do anything
find_package(CUDAToolkit REQUIRED)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OptiX
    REQUIRED_VARS OptiX_INCLUDE_DIR
)

if(OptiX_FOUND AND NOT TARGET OptiX::OptiX)
    add_library(OptiX::OptiX INTERFACE IMPORTED)
    set_target_properties(OptiX::OptiX PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${OptiX_INCLUDE_DIR}"
        # OptiX 7+ is header only, but you must link CUDA
        INTERFACE_LINK_LIBRARIES "CUDA::cudart;CUDA::cuda_driver"
    )
endif()