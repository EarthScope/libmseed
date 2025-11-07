cmake_minimum_required(VERSION 3.12)

# Extract version from libmseed.h
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/libmseed.h" VERSION_LINE
     REGEX "LIBMSEED_VERSION")
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" LIBMSEED_VERSION "${VERSION_LINE}")

project(libmseed
        VERSION ${LIBMSEED_VERSION}
        DESCRIPTION "Read and write miniSEED data records (seismological format)"
        LANGUAGES C)

# Build options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_STATIC_LIBS "Build static libraries" ON)
option(BUILD_EXAMPLES "Build example programs" OFF)
option(BUILD_TESTS "Build test suite" OFF)
option(LIBMSEED_URL "Enable URL support via libcurl" OFF)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# C standard
# Use C11 or later to avoid typedef redefinition warnings in TAU test framework
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Library source files
set(LIB_SRCS
    fileutils.c
    genutils.c
    msio.c
    lookup.c
    yyjson.c
    msrutils.c
    extraheaders.c
    pack.c
    packdata.c
    tracelist.c
    gmtime64.c
    crc32c.c
    parseutils.c
    unpack.c
    unpackdata.c
    selection.c
    logging.c
)

# Public header files
set(PUBLIC_HEADERS
    libmseed.h
)

# Internal header files
set(INTERNAL_HEADERS
    extraheaders.h
    gmtime64.h
    mseedformat.h
    msio.h
    packdata.h
    unpack.h
    unpackdata.h
    yyjson.h
)

# Handle URL support with libcurl
if(LIBMSEED_URL)
    find_package(CURL REQUIRED)
    if(CURL_FOUND)
        add_compile_definitions(LIBMSEED_URL)
        set(LIBMSEED_URL_LIBS CURL::libcurl)
        message(STATUS "URL support enabled with libcurl ${CURL_VERSION_STRING}")
    endif()
endif()

# Create library targets
if(BUILD_SHARED_LIBS)
    add_library(mseed_shared SHARED ${LIB_SRCS})

    # Set library properties
    set_target_properties(mseed_shared PROPERTIES
        OUTPUT_NAME mseed
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "${PUBLIC_HEADERS}"
        C_VISIBILITY_PRESET default
        VISIBILITY_INLINES_HIDDEN OFF
    )

    # Platform-specific settings
    if(APPLE)
        set_target_properties(mseed_shared PROPERTIES
            INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
            MACOSX_RPATH ON
            BUILD_WITH_INSTALL_RPATH OFF
        )
    elseif(UNIX)
        # Use version script on Linux for symbol visibility
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libmseed.map")
            set_target_properties(mseed_shared PROPERTIES
                LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmseed.map"
            )
        endif()
    endif()

    # Position independent code
    target_compile_options(mseed_shared PRIVATE -fPIC)

    # Link libraries
    if(LIBMSEED_URL)
        target_link_libraries(mseed_shared PRIVATE ${LIBMSEED_URL_LIBS})
    endif()

    # Include directories
    target_include_directories(mseed_shared
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<INSTALL_INTERFACE:include>
    )

    # Add to install
    list(APPEND INSTALL_TARGETS mseed_shared)
endif()

if(BUILD_STATIC_LIBS)
    add_library(mseed_static STATIC ${LIB_SRCS})

    # Set library properties
    set_target_properties(mseed_static PROPERTIES
        OUTPUT_NAME mseed
        VERSION ${PROJECT_VERSION}
        PUBLIC_HEADER "${PUBLIC_HEADERS}"
    )

    # On Windows, avoid naming conflict between static and shared lib
    if(WIN32 AND BUILD_SHARED_LIBS)
        set_target_properties(mseed_static PROPERTIES OUTPUT_NAME mseed_static)
    endif()

    # Link libraries
    if(LIBMSEED_URL)
        target_link_libraries(mseed_static PRIVATE ${LIBMSEED_URL_LIBS})
    endif()

    # Include directories
    target_include_directories(mseed_static
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<INSTALL_INTERFACE:include>
    )

    # Add to install
    list(APPEND INSTALL_TARGETS mseed_static)
endif()

# Create alias target for easier integration
if(BUILD_SHARED_LIBS)
    add_library(mseed::mseed ALIAS mseed_shared)
elseif(BUILD_STATIC_LIBS)
    add_library(mseed::mseed ALIAS mseed_static)
endif()

# Build examples
if(BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

# Build tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Installation
include(GNUInstallDirs)

# Install library targets
install(TARGETS ${INSTALL_TARGETS}
    EXPORT libmseedTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export targets to build tree (allows using the build directory directly)
export(EXPORT libmseedTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/libmseedTargets.cmake"
    NAMESPACE mseed::
)

# Install export file
install(EXPORT libmseedTargets
    FILE libmseedTargets.cmake
    NAMESPACE mseed::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libmseed
)

# Create and install config file
include(CMakePackageConfigHelpers)

# Generate the config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/libmseedConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/libmseedConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libmseed
)

# Generate the version file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/libmseedConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install config files
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/libmseedConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/libmseedConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libmseed
)

# Generate and install pkg-config file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/mseed.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/mseed.pc
    @ONLY
)

# Set variables for pkg-config
set(PREFIX ${CMAKE_INSTALL_PREFIX})
set(EXEC_PREFIX ${CMAKE_INSTALL_PREFIX})
set(LIBDIR ${CMAKE_INSTALL_FULL_LIBDIR})
set(INCLUDEDIR ${CMAKE_INSTALL_FULL_INCLUDEDIR})
set(VERSION ${PROJECT_VERSION})

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/mseed.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/mseed.pc
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mseed.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Install documentation
install(FILES README.md INSTALL.md LICENSE ChangeLog
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    OPTIONAL
)

# Install examples
install(DIRECTORY example/
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/examples
    FILES_MATCHING PATTERN "*.c" PATTERN "*.h"
    PATTERN "Makefile*" EXCLUDE
)

# Print configuration summary
message(STATUS "")
message(STATUS "libmseed ${PROJECT_VERSION} Configuration Summary:")
message(STATUS "  Build type:           ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler:           ${CMAKE_C_COMPILER}")
message(STATUS "  Install prefix:       ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Build shared library: ${BUILD_SHARED_LIBS}")
message(STATUS "  Build static library: ${BUILD_STATIC_LIBS}")
message(STATUS "  Build examples:       ${BUILD_EXAMPLES}")
message(STATUS "  Build tests:          ${BUILD_TESTS}")
message(STATUS "  URL support:          ${LIBMSEED_URL}")
message(STATUS "")
