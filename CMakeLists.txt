cmake_minimum_required(VERSION 3.9)
project(extlib)

set(EXTLIB_VERSION_MAJOR 0)
set(EXTLIB_VERSION_MINOR 0)
set(EXTLIB_VERSION_PATCH 1)
set(EXTLIB_VERSION ${EXTLIB_VERSION_MAJOR}.${EXTLIB_VERSION_MINOR}.${EXTLIB_VERSION_PATCH})

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set default build type if not specified
get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (NOT IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

# Check for link time optimization support
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT LTO)
    if(LTO)
        message(STATUS "extlib link-time optimization enabled")
    endif()
endif()

# Set output directories
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif()
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif()

set(CMAKE_BUILD_RPATH_USE_ORIGIN   TRUE)

# Set compiler flags
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -fomit-frame-pointer -fno-plt -s")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s")
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT MSVC)
    set(CMAKE_C_FLAGS "-Wall -Wextra")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Qunused-arguments -O3 -fomit-frame-pointer -s")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s")
elseif(MSVC)
    # Disable secure warnings for the useless _s variants of standard functions.
    # These shouldn't even exist in c99 but MSVC will complain about them because it's MSVC. 
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4244 /wd4267")
endif()

add_subdirectory(libs)
add_subdirectory(examples)

# Install
# Install files other than targets
install(EXPORT extlib-export
    FILE        ExtlibTargets.cmake
    NAMESPACE   extlib::
    DESTINATION lib/cmake/extlib
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ExtlibConfigVersion.cmake
    VERSION ${EXTLIB_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ExtlibConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ExtlibConfigVersion.cmake
    DESTINATION lib/cmake/extlib
)

# Packaging support
set(CPACK_PACKAGE_VENDOR "bamless")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "c extended standard library")
set(CPACK_PACKAGE_VERSION ${EXTLIB_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${EXTLIB_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${EXTLIB_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${EXTLIB_VERSION_PATCH})
# set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
# set(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
set(CPACK_PACKAGE_INSTALL_DIRECTORY "extlib")
include(CPack)
