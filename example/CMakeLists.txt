# Example programs for libmseed

# List of example programs
set(EXAMPLE_PROGRAMS
    lm_pack
    lm_pack_rollingbuffer
    lm_parse
    lm_read_buffer
    lm_read_recordlist
    lm_read_selection
    lm_sids
    lm_timestr
    mseedview
)

# lm_pararead requires POSIX threads, exclude on Windows
if(NOT WIN32)
    list(APPEND EXAMPLE_PROGRAMS lm_pararead)
endif()

# Determine which library target to use
if(BUILD_SHARED_LIBS AND TARGET mseed_shared)
    set(MSEED_LIBRARY mseed_shared)
elseif(TARGET mseed_static)
    set(MSEED_LIBRARY mseed_static)
endif()

# Build each example program
foreach(program ${EXAMPLE_PROGRAMS})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${program}.c)
        add_executable(${program} ${program}.c)

        # Link against the library
        target_link_libraries(${program} PRIVATE ${MSEED_LIBRARY})

        # Add dependency to ensure library is built first
        add_dependencies(${program} ${MSEED_LIBRARY})

        # Set output directory
        set_target_properties(${program} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        # Install the example binary (optional)
        # Uncomment the following line if you want to install example binaries
        # install(TARGETS ${program} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endforeach()
