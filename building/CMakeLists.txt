cmake_minimum_required(VERSION 3.18)
project(CGDenoiser_Depedencies LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(ExternalProject)

set(InstallDir ${CMAKE_SOURCE_DIR}/../lib)

ExternalProject_Add(ISPC
    URL https://github.com/ispc/ispc/releases/download/v1.29.1/ispc-v1.29.1-linux.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND cp bin/ispc ${InstallDir}/bin
)
set(CHAIN ISPC)

ExternalProject_Add(OpenImageDenoise
    GIT_REPOSITORY https://github.com/RenderKit/oidn.git
    GIT_TAG v2.4.1
    INSTALL_DIR ${InstallDir}
    CMAKE_ARGS 
      -DCMAKE_PREFIX_PATH=<INSTALL_DIR>
      -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    DEPENDS ${CHAIN}
)
set(CHAIN OpenImageDenoise)

ExternalProject_Add(OptiXHeaders
    GIT_REPOSITORY https://github.com/NVIDIA/optix-dev
    GIT_TAG v9.1.0
    INSTALL_DIR ${InstallDir}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory
        <SOURCE_DIR>/include
        <INSTALL_DIR>/include
    DEPENDS ${CHAIN}
)
set(CHAIN OptiXHeaders)