# Define where the final plugin package will live
set(PLUGIN_PACKAGE_DIR "${CMAKE_SOURCE_DIR}/plugins/CGDenoiser")

find_package(Nuke REQUIRED)

if (ENABLE_OIDN)
    find_package(OpenImageDenoise REQUIRED)
endif()

if (ENABLE_OPTIX)
    find_package(OptiX REQUIRED)
endif()

# --- 1. Target Definition ---

set(SOURCES
    denoiser.cpp
    denoiserData.cpp
)

if(ENABLE_OPTIX)
    list(APPEND SOURCES optixDenoiser.cpp)
endif()

if(ENABLE_OIDN)
    list(APPEND SOURCES oidnDenoiser.cpp)
endif()

add_library(CGDenoiser SHARED ${SOURCES})

if (ENABLE_OPTIX)
    target_link_libraries(CGDenoiser PUBLIC CUDA::cudart CUDA::cuda_driver)
    target_include_directories(CGDenoiser PRIVATE ${OptiX_INCLUDE_DIR})
    target_compile_definitions(CGDenoiser PRIVATE OPTIX=1)
else()
    target_compile_definitions(CGDenoiser PRIVATE OPTIX=0)
endif()

if (ENABLE_OIDN)
    target_compile_definitions(CGDenoiser PRIVATE OIDN=1)
    target_link_libraries(CGDenoiser PUBLIC 
        OpenImageDenoise::OpenImageDenoise
    )
else()
    target_compile_definitions(CGDenoiser PRIVATE OIDN=0)
endif()


if (OIDN_CUDA)
    target_compile_definitions(CGDenoiser PRIVATE OIDN_CUDA=1)
else()
    target_compile_definitions(CGDenoiser PRIVATE OIDN_CUDA=0)
endif()

if (OIDN_SYCL)
    target_compile_definitions(CGDenoiser PRIVATE OIDN_SYCL=1)
else()
    target_compile_definitions(CGDenoiser PRIVATE OIDN_SYCL=0)
endif()

if (OIDN_HIP)
    target_compile_definitions(CGDenoiser PRIVATE OIDN_HIP=1)
else()
    target_compile_definitions(CGDenoiser PRIVATE OIDN_HIP=0)
endif()

if (OIDN_METAL)
    target_compile_definitions(CGDenoiser PRIVATE OIDN_METAL=1)
else()
    target_compile_definitions(CGDenoiser PRIVATE OIDN_METAL=0)
endif()

if (OIDN_CPU)
    target_compile_definitions(CGDenoiser PRIVATE OIDN_CPU=1)
else()
    target_compile_definitions(CGDenoiser PRIVATE OIDN_CPU=0)
endif()

# --- 2. Shared Linker/Include Logic ---
target_link_libraries(CGDenoiser PUBLIC 
    Nuke::NDK
)

# --- 3. Platform Specifics ---
if(WIN32)
    target_compile_definitions(CGDenoiser PRIVATE 
        _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
    )

    target_compile_options(CGDenoiser PRIVATE /wd4996)
else()
    # Linux Settings
    set_target_properties(CGDenoiser PROPERTIES 
        PREFIX ""
        SUFFIX ".so"
        # $ORIGIN tells the plugin to look for OIDN/TBB in its own folder
        INSTALL_RPATH "\$ORIGIN" 
    )

    target_compile_definitions(CGDenoiser PRIVATE 
        FN_S_STD_ALLOCATOR
        __STDC_CONSTANT_MACROS
        LINUX                     # Tells Nuke headers we are on Linux
        RESIZABLE_ARRAY           # Required for many DDImage headers
    )

    target_compile_options(CGDenoiser PRIVATE 
        -fpermissive
        -Wno-write-strings
        -Wno-invalid-offsetof
        -fno-strict-aliasing 
    )

    # Force the linker to include DDImage even if it thinks it doesn't need it yet
    target_link_options(CGDenoiser PRIVATE "-Wl,--no-as-needed")
endif()

target_link_libraries(CGDenoiser PUBLIC ${NUKE_DDIMAGE_LIBRARY})

# --- 4. Installation / Bundling ---

# Install the actual compiled plugin
install(TARGETS CGDenoiser DESTINATION ${PLUGIN_PACKAGE_DIR} COMPONENT Nuke)

# Install supporting libraries (OIDN only - exclude TBB to avoid conflicts with Nuke's TBB)
install(
    FILES 
        "${CMAKE_SOURCE_DIR}/lib/bin/OpenImageDenoise.dll"
        "${CMAKE_SOURCE_DIR}/lib/bin/OpenImageDenoise_core.dll"
        "${CMAKE_SOURCE_DIR}/lib/bin/OpenImageDenoise_device_cpu.dll"
        "${CMAKE_SOURCE_DIR}/lib/bin/tbb12.dll"
        "${CMAKE_SOURCE_DIR}/lib/bin/tbbmalloc_proxy.dll"
        "${CMAKE_SOURCE_DIR}/lib/bin/tbbmalloc.dll"
    DESTINATION "${PLUGIN_PACKAGE_DIR}"
    OPTIONAL
)

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/lib/lib64/"
    DESTINATION "${PLUGIN_PACKAGE_DIR}"
    FILES_MATCHING PATTERN "*.so*"
)

install(
    FILES 
        "${CMAKE_SOURCE_DIR}/scripts/menu.py"
        "${CMAKE_SOURCE_DIR}/images/icon.png"
    DESTINATION "${PLUGIN_PACKAGE_DIR}"
    OPTIONAL
)

install(SCRIPT "${CMAKE_SOURCE_DIR}/cmake/install_nuke_plugin.cmake")