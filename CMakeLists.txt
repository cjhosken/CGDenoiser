cmake_minimum_required(VERSION 3.23)

option(OPTIX "Enable OptiX backend" OFF)
option(OIDN_CPU "Enable OpenImageDenoise CPU" ON)
option(OIDN_CUDA "Enable OpenImageDenoise CUDA" OFF)
option(OIDN_SYCL "Enable OpenImageDenoise SYCL" OFF)
option(OIDN_HIP "Enable OpenImageDenoise HIP" OFF)
option(OIDN_METAL "Enable OpenImageDenoise METAL" OFF)

if(OPTIX)
    project(CGDenoiser LANGUAGES CXX CUDA)
else()
    project(CGDenoiser LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH "Cmake;{CMAKE_MODULE_PATH};${CMAKE_SOURCE_DIR}/cmake/Modules")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/lib")

add_definitions(-DNOMINMAX)
add_definitions(-D_USE_MATH_DEFINES)

find_package(Nuke REQUIRED)
find_package(OpenImageDenoise REQUIRED)

if (OPTIX)
    find_package(CUDAToolkit REQUIRED)
    find_package(OptiX REQUIRED)
endif()

add_subdirectory(src)