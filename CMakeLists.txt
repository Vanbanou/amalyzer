cmake_minimum_required(VERSION 3.10)
project(amalyzer)

set(CMAKE_CXX_STANDARD 17)

# Superpowered path
set(SUPERPOWERED_PATH "${CMAKE_SOURCE_DIR}/superpowered")
include_directories(${SUPERPOWERED_PATH})

# TagLib estático — obrigatório via workflow
if(NOT DEFINED TAGLIB_STATIC_ROOT)
    message(FATAL_ERROR "TAGLIB_STATIC_ROOT must be provided")
endif()

include_directories(${TAGLIB_STATIC_ROOT}/include)
link_directories(${TAGLIB_STATIC_ROOT}/lib)

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

# Link
target_link_libraries(amalyzer PRIVATE
    ${SUPERPOWERED_LIB}
    ${TAGLIB_STATIC_ROOT}/lib/libtag.a
    pthread
    dl
    z
)
