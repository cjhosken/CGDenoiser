cmake_minimum_required(VERSION 3.3)
project(CGDenoiser_Depedencies LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(ExternalProject)

if (WIN32)
  set(CMAKE_OBJECT_PATH_MAX 260)
endif ()

set(InstallDir ${CMAKE_SOURCE_DIR}/../lib)

if (WIN32)
    set(ISPC_URL https://github.com/ispc/ispc/releases/download/v1.29.1/ispc-v1.29.1-windows.zip)
    set(ISPC_BIN ispc.exe)
else()
    set(ISPC_URL https://github.com/ispc/ispc/releases/download/v1.29.1/ispc-v1.29.1-linux.tar.gz)
    set(ISPC_BIN ispc)
endif()

ExternalProject_Add(ISPC
    URL ${ISPC_URL}
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_DIR ${InstallDir}
    INSTALL_COMMAND
        ${CMAKE_COMMAND} -E make_directory <INSTALL_DIR>/bin
        COMMAND ${CMAKE_COMMAND} -E copy
            <SOURCE_DIR>/bin/${ISPC_BIN}
            <INSTALL_DIR>/bin/${ISPC_BIN}
)
set(CHAIN ISPC)

ExternalProject_Add(TBB
    GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
    GIT_TAG v2021.12.0
    INSTALL_DIR ${InstallDir}
    CMAKE_ARGS 
      -DCMAKE_PREFIX_PATH=<INSTALL_DIR>
      -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
      -DTBB_TEST=OFF
    DEPENDS ${CHAIN}
)
set(CHAIN TBB)

ExternalProject_Add(OpenImageDenoise
    GIT_REPOSITORY https://github.com/RenderKit/oidn.git
    GIT_TAG v2.4.1
    INSTALL_DIR ${InstallDir}
    CMAKE_ARGS 
      -DCMAKE_PREFIX_PATH=<INSTALL_DIR>
      -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
      -DCMAKE_BUILD_TYPE=Release

      -DOIDN_DEVICE_CPU=ON
      -DOIDN_DEVICE_CUDA=OFF
      -DOIDN_DEVICE_SYCL=OFF
      -DOIDN_DEVICE_HIP=OFF
      -DOIDN_DEVICE_METAL=OFF

      -DOIDN_APPS=OFF

      -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
      -DBUILD_SHARED_LIBS=ON
      -DOIDN_ARCH=AVX2
      -DOIDN_STATIC_LIB=OFF
    DEPENDS ${CHAIN}
)
set(CHAIN OpenImageDenoise)

ExternalProject_Add(OptiXHeaders
    GIT_REPOSITORY https://github.com/NVIDIA/optix-dev
    GIT_TAG v9.1.0
    INSTALL_DIR ${InstallDir}
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