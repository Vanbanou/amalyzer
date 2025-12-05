cmake_minimum_required(VERSION 3.10)
project(amalyzer)

set(CMAKE_CXX_STANDARD 17)

# Define the path to the Superpowered SDK
set(SUPERPOWERED_PATH "${CMAKE_SOURCE_DIR}/superpowered")

# Include directories
include_directories(${SUPERPOWERED_PATH})

# Find TagLib
find_package(PkgConfig REQUIRED)
pkg_check_modules(TAGLIB REQUIRED taglib)
include_directories(${TAGLIB_INCLUDE_DIRS})
link_directories(${TAGLIB_LIBRARY_DIRS})

# Detect architecture and select the appropriate library
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    message(STATUS "Target architecture: ARM64")
    set(SUPERPOWERED_LIB "${SUPERPOWERED_PATH}/libSuperpoweredLinuxARM64.a")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "amd64")
    message(STATUS "Target architecture: x86_64")
    set(SUPERPOWERED_LIB "${SUPERPOWERED_PATH}/libSuperpoweredLinuxX86_64.a")
else()
    message(WARNING "Unknown architecture: ${CMAKE_SYSTEM_PROCESSOR}. Defaulting to x86_64.")
    set(SUPERPOWERED_LIB "${SUPERPOWERED_PATH}/libSuperpoweredLinuxX86_64.a")
endif()

# Add the executable
add_executable(amalyzer main.cpp)

# Try to link statically with TagLib if available
# Force search for .a files first
set(CMAKE_FIND_LIBRARY_SUFFIXES_BACKUP ${CMAKE_FIND_LIBRARY_SUFFIXES})
set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
find_library(TAGLIB_STATIC_LIB tag PATHS ${TAGLIB_LIBRARY_DIRS} NO_DEFAULT_PATH)
set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES_BACKUP})

if(TAGLIB_STATIC_LIB)
    message(STATUS "Using static TagLib: ${TAGLIB_STATIC_LIB}")
    # When linking statically, we need to include TagLib's dependencies
    target_link_libraries(amalyzer PRIVATE ${SUPERPOWERED_LIB} pthread dl ${TAGLIB_STATIC_LIB} z)
else()
    message(STATUS "Using dynamic TagLib")
    target_link_libraries(amalyzer PRIVATE ${SUPERPOWERED_LIB} pthread dl ${TAGLIB_LIBRARIES})
endif()
