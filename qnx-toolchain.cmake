# QNX 8.0 CMake Toolchain File
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=qnx-toolchain.cmake -B builddir

# System identification
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_VERSION 8.0)

# QNX SDK paths - adjust these if QNX SDK is installed elsewhere
# Default: /opt/qnx8.0 or $QNX_HOST environment variable
if(DEFINED ENV{QNX_HOST})
    set(QNX_HOST_PATH "$ENV{QNX_HOST}")
else()
    set(QNX_HOST_PATH "/qnx/qnx800/host/linux/x86_64")
endif()

if(DEFINED ENV{QNX_TARGET})
    set(QNX_TARGET_PATH "$ENV{QNX_TARGET}")
else()
    set(QNX_TARGET_PATH "/qnx/qnx800/target/qnx8")
endif()

# Export QNX environment variables for QCC compiler
set(ENV{QNX_HOST} "${QNX_HOST_PATH}")
set(ENV{QNX_TARGET} "${QNX_TARGET_PATH}")

# Architecture selection: aarch64le (default) or x86_64
# Set via: -DQNX_ARCH=aarch64le or -DQNX_ARCH=x86_64
if(NOT DEFINED QNX_ARCH)
    set(QNX_ARCH "aarch64le")
endif()

# Map QNX architecture to CMake architecture
if(QNX_ARCH STREQUAL "aarch64le")
    set(CMAKE_SYSTEM_PROCESSOR aarch64)
    set(QCC_TARGET "gcc_ntoaarch64le")
elseif(QNX_ARCH STREQUAL "x86_64")
    set(CMAKE_SYSTEM_PROCESSOR x86_64)
    set(QCC_TARGET "gcc_ntox86_64")
elseif(QNX_ARCH STREQUAL "armv7le")
    set(CMAKE_SYSTEM_PROCESSOR arm)
    set(QCC_TARGET "gcc_ntoarmv7le")
else()
    message(FATAL_ERROR "Unsupported QNX architecture: ${QNX_ARCH}")
endif()

# QCC compiler and tools
set(QCC_COMPILER "${QNX_HOST_PATH}/usr/bin/qcc")
set(QAR_TOOL "${QNX_HOST_PATH}/usr/bin/qar")
set(NTOSTRIP_TOOL "${QNX_HOST_PATH}/usr/bin/ntostrip")
set(QNX_PKG_CONFIG "${QNX_HOST_PATH}/usr/bin/pkg-config")

# Set compilers
set(CMAKE_C_COMPILER "${QCC_COMPILER}")
set(CMAKE_C_COMPILER_TARGET "${QCC_TARGET}")
set(CMAKE_CXX_COMPILER "${QNX_HOST_PATH}/usr/bin/q++")
set(CMAKE_CXX_COMPILER_TARGET "${QCC_TARGET}")

# Archiver
set(CMAKE_AR "${QAR_TOOL}")
set(CMAKE_RANLIB "${QNX_HOST_PATH}/usr/bin/ranlib")

# Strip tool
set(CMAKE_STRIP "${NTOSTRIP_TOOL}")

# Compiler flags for QCC
# The -V flag tells QCC to use the specified compiler target
set(CMAKE_C_FLAGS "-V${QCC_TARGET}" CACHE STRING "C compiler flags")
set(CMAKE_CXX_FLAGS "-V${QCC_TARGET}" CACHE STRING "C++ compiler flags")

# Search paths for libraries and headers
set(CMAKE_FIND_ROOT_PATH "${QNX_TARGET_PATH}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# PKG_CONFIG setup for QNX
set(ENV{PKG_CONFIG_PATH} "${QNX_TARGET_PATH}/usr/lib/pkgconfig:${QNX_TARGET_PATH}/usr/local/lib/pkgconfig")
set(ENV{PKG_CONFIG_EXECUTABLE} "${QNX_PKG_CONFIG}")

# QNX-specific compiler flags
set(CMAKE_C_FLAGS_INIT "-fPIC -fvisibility=hidden")
set(CMAKE_CXX_FLAGS_INIT "-fPIC -fvisibility=hidden")

# Build type defaults (Release for production)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Optimization flags
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "Release C flags")
set(CMAKE_C_FLAGS_DEBUG "-g -O0" CACHE STRING "Debug C flags")

# Library type (shared by default for CCID)
set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
set(CMAKE_SHARED_LIBRARY_PREFIX "lib")

# Message output
message(STATUS "")
message(STATUS "=== QNX 8.0 CMake Toolchain Configuration ===")
message(STATUS "QNX SDK Host: ${QNX_HOST_PATH}")
message(STATUS "QNX SDK Target: ${QNX_TARGET_PATH}")
message(STATUS "Target Architecture: ${QNX_ARCH} (${CMAKE_SYSTEM_PROCESSOR})")
message(STATUS "QCC Compiler: ${QCC_COMPILER}")
message(STATUS "QCC Target: ${QCC_TARGET}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "To use this toolchain:")
message(STATUS "  cmake -DCMAKE_TOOLCHAIN_FILE=qnx-toolchain.cmake -B builddir")
message(STATUS "  cmake --build builddir")
message(STATUS "")
message(STATUS "To use a different architecture:")
message(STATUS "  cmake -DQNX_ARCH=x86_64 -DCMAKE_TOOLCHAIN_FILE=qnx-toolchain.cmake -B builddir")
message(STATUS "")
