cmake_minimum_required(VERSION 3.10)
project(amalyzer)

set(CMAKE_CXX_STANDARD 17)

# Superpowered path
set(SUPERPOWERED_PATH "${CMAKE_SOURCE_DIR}/superpowered")
include_directories(${SUPERPOWERED_PATH})

# TagLib estático
if(NOT DEFINED TAGLIB_STATIC_ROOT)
    # Fallback to pkg-config for local builds
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(TAGLIB taglib)
        if(TAGLIB_FOUND)
            include_directories(${TAGLIB_INCLUDE_DIRS})
            link_directories(${TAGLIB_LIBRARY_DIRS})
            set(TAGLIB_LIBS ${TAGLIB_LIBRARIES})
        else()
            message(FATAL_ERROR "TagLib not found. Please install libtag1-dev or set TAGLIB_STATIC_ROOT")
        endif()
    else()
        message(FATAL_ERROR "TAGLIB_STATIC_ROOT must be provided or pkg-config must be available")
    endif()
else()
    include_directories(${TAGLIB_STATIC_ROOT}/include)
    link_directories(${TAGLIB_STATIC_ROOT}/lib)
    set(TAGLIB_LIBS ${TAGLIB_STATIC_ROOT}/lib/libtag.a)
endif()

# Detect architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    message(STATUS "Architecture: ARM64")
    set(SUPERPOWERED_LIB "${SUPERPOWERED_PATH}/libSuperpoweredLinuxARM64.a")
else()
    message(STATUS "Architecture: x86_64")
    set(SUPERPOWERED_LIB "${SUPERPOWERED_PATH}/libSuperpoweredLinuxX86_64.a")
endif()

# Exec
add_executable(amalyzer main.cpp)

# Link libraries
target_link_libraries(amalyzer PRIVATE
    ${SUPERPOWERED_LIB}
    ${TAGLIB_LIBS}
    dl
)

# pthread is integrated in Bionic (Android libc), only link separately on Linux
if(NOT ANDROID)
    target_link_libraries(amalyzer PRIVATE pthread)
endif()

# Add static libgcc/libstdc++ for non-Android builds
if(NOT ANDROID)
    target_link_libraries(amalyzer PRIVATE -static-libgcc -static-libstdc++)
endif()

# Find and link zlib
if(DEFINED ZLIB_STATIC)
    # Use provided static zlib (for Android NDK builds)
    target_link_libraries(amalyzer PRIVATE ${ZLIB_STATIC})
else()
    # Find static zlib for Linux builds
    find_library(ZLIB_STATIC_LIB NAMES libz.a PATHS /usr/lib /usr/lib64 /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu)
    if(ZLIB_STATIC_LIB)
        target_link_libraries(amalyzer PRIVATE ${ZLIB_STATIC_LIB})
    else()
        target_link_libraries(amalyzer PRIVATE z)
    endif()
endif()
