cmake_minimum_required(VERSION 3.23)

option(ENABLE_OPTIX "Enable OptiX backend" OFF)
option(ENABLE_OIDN "Enable OIDN backend" ON)
option(CPU "Enable OpenImageDenoise CPU" ON)
option(CUDA "Enable OpenImageDenoise CUDA" OFF)
option(SYCL "Enable OpenImageDenoise SYCL" OFF)
option(HIP "Enable OpenImageDenoise HIP" OFF)
option(METAL "Enable OpenImageDenoise METAL" OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(CGDenoiser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Release)
if(WIN32)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O2 -DNDEBUG -MT")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O3 -DNDEBUG -Wall")
endif()

IF(MSVC)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-DNOMINMAX)
ENDIF()

set(CMAKE_MODULE_PATH "Cmake;{CMAKE_MODULE_PATH};${CMAKE_SOURCE_DIR}/cmake/Modules")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/lib")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")
# Build with with install RPATH
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
# Add RPATHS outside of the build tree to install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_MACOSX_RPATH TRUE)

add_subdirectory(src)