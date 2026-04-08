cmake_minimum_required(VERSION 3.18)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PLUGIN_PACKAGE_DIR "${CMAKE_SOURCE_DIR}/plugins/CGDenoiser")

add_library(CGDenoiser SHARED 
  denoiser.cpp
)

target_link_libraries(CGDenoiser PUBLIC 
  Nuke::NDK
  OpenImageDenoise::OpenImageDenoise
)

  target_include_directories(CGDenoiser PRIVATE
    ${Nuke_INCLUDE_DIRS}
  )

if(USE_OPTIX)
    target_link_libraries(CGDenoiser PUBLIC OptiX::OptiX CUDA::cudart)
    target_include_directories(CGDenoiser PRIVATE
      ${OptiX_INCLUDE_DIR}
    )
endif()

# --- Adjust naming based on Platform ---
if(WIN32)
    # Windows needs .dll and NO "lib" prefix
    set_target_properties(CGDenoiser PROPERTIES 
        PREFIX "" 
        SUFFIX ".dll"
    )
    # Add Windows-specific definitions for Nuke
    target_compile_definitions(CGDenoiser PRIVATE 
        _CRT_SECURE_NO_WARNINGS
        _SCL_SECURE_NO_WARNINGS
    )
else()
    # Linux/Unix settings
    set_target_properties(CGDenoiser PROPERTIES 
        PREFIX ""
        SUFFIX ".so"
        INSTALL_RPATH "$ORIGIN/../lib"
    )
    
    # Only use these on Linux (they will crash the Windows build)
    target_compile_definitions(CGDenoiser PRIVATE 
        FN_S_STD_ALLOCATOR
        _GLIBCXX_USE_CXX11_ABI=0
    )

    target_compile_options(CGDenoiser PRIVATE 
      -fpermissive
      -Wno-error=range-loop-construct
    )
endif()

install(TARGETS CGDenoiser
  DESTINATION ${PLUGIN_PACKAGE_DIR}
)

install(
  DIRECTORY "${CMAKE_SOURCE_DIR}/lib/bin/"
  DESTINATION "${PLUGIN_PACKAGE_DIR}"
  FILES_MATCHING 
    PATTERN "*.dll"
    PATTERN "*.lib"
    PATTERN "*.so*"
    PATTERN "*.dylib*"
)