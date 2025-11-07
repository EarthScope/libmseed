# Test suite for libmseed

# Test programs
set(TEST_PROGRAMS
    test-crc
    test-extraheaders
    test-msrutils
    test-read
    test-repack
    test-selection
    test-sid
    test-time
    test-tracelist
    test-write
)

# Example-based tests (symlinked in test directory)
set(EXAMPLE_TESTS
    lm_pack
    lm_pack_rollingbuffer
    lm_parse
    lm_read_buffer
    lm_read_recordlist
    lm_read_selection
    lm_sids
    lm_timestr
)

# TAU testing framework
add_library(tau STATIC
    tau/tau.h
    test-runner.c
)

target_include_directories(tau PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

# Link tau with the library to access libmseed.h
if(BUILD_SHARED_LIBS AND TARGET mseed_shared)
    target_link_libraries(tau PUBLIC mseed_shared)
elseif(TARGET mseed_static)
    target_link_libraries(tau PUBLIC mseed_static)
endif()

# Build test programs
foreach(test ${TEST_PROGRAMS})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${test}.c)
        add_executable(${test} ${test}.c)

        # Link against the library and TAU
        if(BUILD_SHARED_LIBS)
            target_link_libraries(${test} PRIVATE mseed_shared tau)
        else()
            target_link_libraries(${test} PRIVATE mseed_static tau)
        endif()

        # Set output directory
        set_target_properties(${test} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test
        )

        # Add as a CTest
        add_test(NAME ${test} COMMAND ${test}
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif()
endforeach()

# Build example-based tests
foreach(test ${EXAMPLE_TESTS})
    # Check if the source file exists (it might be a symlink)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${test}.c)
        add_executable(${test}_test ${test}.c)

        # Link against the library
        if(BUILD_SHARED_LIBS)
            target_link_libraries(${test}_test PRIVATE mseed_shared)
        else()
            target_link_libraries(${test}_test PRIVATE mseed_static)
        endif()

        # Set output directory
        set_target_properties(${test}_test PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test
        )

        # These are demonstration programs, not unit tests
        # Uncomment if you want to add them as tests:
        # add_test(NAME ${test} COMMAND ${test}_test)
    endif()
endforeach()

# Copy test data directory to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
