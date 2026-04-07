cmake_minimum_required(VERSION 3.3)
project(CGDenoiser LANGUAGES CXX CUDA)

set(CMAKE_MODULE_PATH "Cmake;{CMAKE_MODULE_PATH};${CMAKE_SOURCE_DIR}/cmake/Modules")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/lib")

find_package(Nuke REQUIRED)
find_package(OpenImageDenoise REQUIRED)
find_package(OptiX REQUIRED)

add_subdirectory(src)