cmake_minimum_required(VERSION 3.18)

project(CGDenoiser LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PLUGIN_PACKAGE_DIR "${CMAKE_SOURCE_DIR}/../plugins/CGDenoiser")

add_library(CGDenoiser SHARED 
  denoiser.cpp
)

if(UNIX)
  set(CMAKE_C_COMPILER gcc-10)
  set(CMAKE_CXX_COMPILER g++-10)

  target_compile_options(CGDenoiser PRIVATE 
    -fpermissive
    -Wno-error=range-loop-construct
  )
endif()

target_compile_definitions(CGDenoiser PRIVATE 
  FN_S_STD_ALLOCATOR
  _GLIBCXX_USE_CXX11_ABI=0
)

target_link_libraries(CGDenoiser PUBLIC 
  Nuke::NDK
  OpenImageDenoise::OpenImageDenoise
  OptiX::OptiX
  CUDA::cudart
)

set_target_properties(CGDenoiser PROPERTIES 
  PREFIX ""
  SUFFIX ".so"
  INSTALL_RPATH "$ORIGIN/../lib"
)

target_include_directories(CGDenoiser PRIVATE
  ${Nuke_INCLUDE_DIRS}
  ${OptiX_INCLUDE_DIR}
)

install(TARGETS CGDenoiser
  DESTINATION ${PLUGIN_PACKAGE_DIR}
)

install(
  DIRECTORY "${CMAKE_SOURCE_DIR}/../lib/"
  DESTINATION "${PLUGIN_PACKAGE_DIR}/lib"
  FILES_MATCHING 
    PATTERN "*.so*" 
    PATTERN "*.dylib*"
    PATTERN "*.dll"
)

install(
  DIRECTORY "${CMAKE_SOURCE_DIR}/../scripts/"
  DESTINATION "${PLUGIN_PACKAGE_DIR}"
  FILES_MATCHING 
    PATTERN "*.py"
)