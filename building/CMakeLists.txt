cmake_minimum_required(VERSION 3.3)
project(CGDenoiser_Dependencies LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(ExternalProject)

if (WIN32)
  set(CMAKE_OBJECT_PATH_MAX 260)
endif ()

option(ENABLE_OPTIX "Enable OptiX" ON)
option(ENABLE_OIDN "Enable Open Image Denoise (OIDN)" ON)
option(ENABLE_CPU "Enable CPU backend" ON)
option(ENABLE_CUDA "Enable CUDA backend" OFF)
option(ENABLE_SYCL "Enable SYCL backend" OFF)
option(ENABLE_HIP "Enable HIP backend" OFF)
option(ENABLE_METAL "Enable Metal backend" OFF)

set(OIDN_VERSION "2.4.1" CACHE STRING "OIDN version")
set(OPTIX_VERSION "9.1.0" CACHE STRING "OptiX version")
set(TBB_VERSION "2021.13.0" CACHE STRING "TBB version")
set(ISPC_VERSION "1.30.0" CACHE STRING "IPSC version")

set(INSTALL_DIR "${CMAKE_SOURCE_DIR}/../lib" CACHE PATH "Install directory")

message(STATUS "================ CONFIG SUMMARY ================")

message(STATUS "ENABLE_OPTIX  = ${ENABLE_OPTIX}")
message(STATUS "ENABLE_OIDN   = ${ENABLE_OIDN}")
message(STATUS "ENABLE_CPU    = ${ENABLE_CPU}")
message(STATUS "ENABLE_CUDA   = ${ENABLE_CUDA}")
message(STATUS "ENABLE_SYCL   = ${ENABLE_SYCL}")
message(STATUS "ENABLE_HIP    = ${ENABLE_HIP}")
message(STATUS "ENABLE_METAL  = ${ENABLE_METAL}")

message(STATUS "OIDN_VERSION  = ${OIDN_VERSION}")
message(STATUS "OPTIX_VERSION = ${OPTIX_VERSION}")
message(STATUS "TBB_VERSION   = ${TBB_VERSION}")
message(STATUS "ISPC_VERSION  = ${ISPC_VERSION}")

message(STATUS "INSTALL_DIR   = ${INSTALL_DIR}")

message(STATUS "===============================================")

set(CHAIN "")

if(ENABLE_METAL AND NOT APPLE)
    message(FATAL_ERROR "Metal backend is only supported on macOS")
endif()


if(ENABLE_OIDN)

    if (WIN32)
        set(ISPC_URL https://github.com/ispc/ispc/releases/download/v${ISPC_VERSION}/ispc-v${ISPC_VERSION}-windows.zip)
        set(ISPC_BIN ispc.exe)
    else()
        set(ISPC_URL https://github.com/ispc/ispc/releases/download/v${ISPC_VERSION}/ispc-v${ISPC_VERSION}-linux.tar.gz)
        set(ISPC_BIN ispc)
    endif()

    set(DEPS "")
    if(CHAIN)
        set(DEPS DEPENDS ${CHAIN})
    endif()

    ExternalProject_Add(ISPC
        URL ${ISPC_URL}
        BUILD_IN_SOURCE 1
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_DIR ${INSTALL_DIR}
        INSTALL_COMMAND
            ${CMAKE_COMMAND} -E make_directory <INSTALL_DIR>/bin
            COMMAND ${CMAKE_COMMAND} -E copy
                <SOURCE_DIR>/bin/${ISPC_BIN}
                <INSTALL_DIR>/bin/${ISPC_BIN}
    )
    set(CHAIN ISPC)

    set(DEPS "")
    if(CHAIN)
        set(DEPS DEPENDS ${CHAIN})
    endif()

    ExternalProject_Add(TBB
        GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
        GIT_TAG v${TBB_VERSION}
        INSTALL_DIR ${INSTALL_DIR}
        CMAKE_ARGS 
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_PREFIX_PATH=<INSTALL_DIR>
            -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
            -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
            -DTBB_TEST=OFF
            -DTBB_STRICT=OFF
            -Wno-dev
        DEPENDS ${CHAIN}
    )
    set(CHAIN TBB)

    set(DEPS "")
    if(CHAIN)
        set(DEPS DEPENDS ${CHAIN})
    endif()

    set(OIDN_GENERATOR_ARG "")

    if(ENABLE_SYCL OR ENABLE_HIP)
        set(OIDN_GENERATOR_ARG CMAKE_GENERATOR Ninja)
    endif()

    ExternalProject_Add(OpenImageDenoise
        GIT_REPOSITORY https://github.com/RenderKit/oidn.git
        GIT_TAG v${OIDN_VERSION}
        INSTALL_DIR ${INSTALL_DIR}

        ${OIDN_GENERATOR_ARG}

        CMAKE_ARGS 
            -DTBB_DIR=<INSTALL_DIR>
            -DCMAKE_PREFIX_PATH=<INSTALL_DIR>
            -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
            -DCMAKE_BUILD_TYPE=Release

            -DOIDN_DEVICE_CPU=${ENABLE_CPU}
            -DOIDN_DEVICE_CUDA=${ENABLE_CUDA}
            -DOIDN_DEVICE_SYCL:BOOL=${ENABLE_SYCL}
            -DOIDN_DEVICE_HIP=${ENABLE_HIP}
            -DOIDN_DEVICE_METAL=${ENABLE_METAL}

            -DOIDN_APPS=OFF

            -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
            -DBUILD_SHARED_LIBS=ON
            -DOIDN_ARCH=DEFAULT
            -DOIDN_STATIC_LIB=OFF
            -Wno-dev
        DEPENDS ${CHAIN}
    )
    set(CHAIN OpenImageDenoise)
endif()

if(ENABLE_OPTIX)

    set(DEPS "")
    if(CHAIN)
        set(DEPS DEPENDS ${CHAIN})
    endif()

    ExternalProject_Add(OptiXHeaders
        GIT_REPOSITORY https://github.com/NVIDIA/optix-dev
        GIT_TAG v${OPTIX_VERSION}
        INSTALL_DIR ${INSTALL_DIR}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND
            ${CMAKE_COMMAND} -E make_directory <INSTALL_DIR>/include
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                <SOURCE_DIR>/include
                <INSTALL_DIR>/include
        DEPENDS ${CHAIN}
    )
    set(CHAIN OptiXHeaders)
endif()