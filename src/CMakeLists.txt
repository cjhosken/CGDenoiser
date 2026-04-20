# Define where the final plugin package will live
set(PLUGIN_PACKAGE_DIR "${CMAKE_SOURCE_DIR}/plugins/CGDenoiser")

# --- 1. Target Definition ---

if(OPTIX)
    add_library(CGDenoiser SHARED denoiser.cpp denoiserData.cpp oidnDenoiser.cpp optixDenoiser.cpp)
    target_link_libraries(CGDenoiser PUBLIC CUDA::cudart CUDA::cuda_driver)
    target_include_directories(CGDenoiser PRIVATE ${OptiX_INCLUDE_DIR})
    target_compile_definitions(CGDenoiser PRIVATE OPTIX=1)
else()
    add_library(CGDenoiser SHARED denoiser.cpp denoiserData.cpp oidnDenoiser.cpp)
    target_compile_definitions(CGDenoiser PRIVATE OPTIX=0)
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
    OpenImageDenoise::OpenImageDenoise
)

set(CMAKE_BUILD_TYPE Release)
if(WIN32)
    # Was getting LNK4098: defaultlib 'MSCVRT' conflicts with use of other libs
    # It took many hours to figure out the -MT flag
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O2 -DNDEBUG /MD")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O3 -DNDEBUG -Wall")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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
    
    # Nuke Core Linking: On Linux, we MUST explicitly link DDImage
    # We use Nuke_ROOT to find the actual .so file
    find_library(DDIMAGE_LIB DDImage PATHS $ENV{Nuke_ROOT} NO_DEFAULT_PATH)
    target_link_libraries(CGDenoiser PUBLIC ${DDIMAGE_LIB})

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

# --- 4. Installation / Bundling ---

# Install the actual compiled plugin
install(TARGETS CGDenoiser DESTINATION ${PLUGIN_PACKAGE_DIR})

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