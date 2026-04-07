

find_path(
  OpenImageDenoise_INCLUDE_DIR
  NAMES OpenImageDenoise/oidn.hpp
  PATHS 
    "${OIDN_ROOT}/include/"
    "${CMAKE_SOURCE_DIR}/../../lib/include/"
    "$ENV{OPTIX_INSTALL_DIR}/include"
)

message("OpenImageDenoise_INCLUDE_DIR=${OpenImageDenoise_INCLUDE_DIR}")


if(OpenImageDenoise_INCLUDE_DIR)
  include_directories(${OpenImageDenoise_INCLUDE_DIR})
endif()

find_library(
  OpenImageDenoise_LIBRARY
  NAMES OpenImageDenoise
  PATHS
    "${OIDN_ROOT}/lib64"
    "${CMAKE_SOURCE_DIR}/../../lib64"
    "$ENV{OPTIX_INSTALL_DIR}/lib64"
)

message("OpenImageDenoise_LIBRARY=${OpenImageDenoise_LIBRARY}")


include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenImageDenoise
  REQUIRED_VARS
    OpenImageDenoise_INCLUDE_DIR
)

if(OpenImageDenoise_FOUND AND NOT TARGET OpenImageDenoise::OpenImageDenoise)
    add_library(OpenImageDenoise::OpenImageDenoise UNKNOWN IMPORTED)
    set_target_properties(OpenImageDenoise::OpenImageDenoise PROPERTIES
        IMPORTED_LOCATION "${OpenImageDenoise_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${OpenImageDenoise_INCLUDE_DIR}")
endif()