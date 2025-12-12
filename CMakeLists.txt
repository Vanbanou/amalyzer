cmake_minimum_required(VERSION 3.10)
project(amalyzer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------
# Detecta arquivos fontes
# ----------------------------
file(GLOB SOURCES src/*.cpp)

# ----------------------------
# Adiciona executável
# ----------------------------
add_executable(amalyzer ${SOURCES})

# ----------------------------
# Superpowered
# ----------------------------
set(SUPERPOWERED_INCLUDE "${CMAKE_SOURCE_DIR}/external/superpowered/include")
target_include_directories(amalyzer PRIVATE ${SUPERPOWERED_INCLUDE})

# Detecta plataforma e define biblioteca correta
if(ANDROID)
    message(STATUS "Building for Android: ${ANDROID_ABI}")
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(SUPERPOWERED_LIB "${CMAKE_SOURCE_DIR}/external/superpowered/lib/libSuperpoweredAndroidarm64-v8a.a")
    elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(SUPERPOWERED_LIB "${CMAKE_SOURCE_DIR}/external/superpowered/lib/libSuperpoweredAndroidarmeabi-v7a.a")
    elseif(ANDROID_ABI STREQUAL "x86_64")
        set(SUPERPOWERED_LIB "${CMAKE_SOURCE_DIR}/external/superpowered/lib/libSuperpoweredAndroidX86_64.a")
    elseif(ANDROID_ABI STREQUAL "x86")
        set(SUPERPOWERED_LIB "${CMAKE_SOURCE_DIR}/external/superpowered/lib/libSuperpoweredAndroidX86.a")
    else()
        message(FATAL_ERROR "Unsupported Android ABI: ${ANDROID_ABI}")
    endif()
else()
    message(STATUS "Building for Linux: ${CMAKE_SYSTEM_PROCESSOR}")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(SUPERPOWERED_LIB "${CMAKE_SOURCE_DIR}/external/superpowered/lib/libSuperpoweredLinuxARM64.a")
    else()
        set(SUPERPOWERED_LIB "${CMAKE_SOURCE_DIR}/external/superpowered/lib/libSuperpoweredLinuxX86_64.a")
    endif()
endif()

# ----------------------------
# TagLib
# ----------------------------
if(NOT DEFINED TAGLIB_STATIC_ROOT)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(TAGLIB taglib)
        if(TAGLIB_FOUND)
            include_directories(${TAGLIB_INCLUDE_DIRS})
            link_directories(${TAGLIB_LIBRARY_DIRS})
            set(TAGLIB_LIBS ${TAGLIB_LIBRARIES})
        else()
            message(FATAL_ERROR "TagLib not found. Instale libtag1-dev ou defina TAGLIB_STATIC_ROOT")
        endif()
    else()
        message(FATAL_ERROR "TAGLIB_STATIC_ROOT deve ser fornecido ou pkg-config disponível")
    endif()
else()
    include_directories(${TAGLIB_STATIC_ROOT}/include)
    link_directories(${TAGLIB_STATIC_ROOT}/lib)
    set(TAGLIB_LIBS ${TAGLIB_STATIC_ROOT}/lib/libtag.a)
endif()

# ----------------------------
# Linkagem
# ----------------------------
target_link_libraries(amalyzer PRIVATE
    ${SUPERPOWERED_LIB}
    ${TAGLIB_LIBS}
    dl
)

# pthread para Linux
if(NOT ANDROID)
    target_link_libraries(amalyzer PRIVATE pthread)
    target_link_libraries(amalyzer PRIVATE -static-libgcc -static-libstdc++)
endif()

# zlib para Android ou Linux
if(DEFINED ZLIB_STATIC)
    target_link_libraries(amalyzer PRIVATE ${ZLIB_STATIC})
else()
    find_library(ZLIB_STATIC_LIB NAMES libz.a PATHS /usr/lib /usr/lib64 /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu)
    if(ZLIB_STATIC_LIB)
        target_link_libraries(amalyzer PRIVATE ${ZLIB_STATIC_LIB})
    else()
        target_link_libraries(amalyzer PRIVATE z)
    endif()
endif()
