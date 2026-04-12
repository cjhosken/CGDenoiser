cmake_minimum_required(VERSION 3.23)

option(USE_OPTIX "Enable OptiX backend" OFF)

if(USE_OPTIX)
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

if (USE_OPTIX)
    find_package(CUDAToolkit REQUIRED)
    find_package(OptiX REQUIRED)
endif()

add_subdirectory(src)