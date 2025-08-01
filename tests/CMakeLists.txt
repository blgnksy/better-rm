# Find Check unit testing framework
find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK REQUIRED check)

# Include directories
include_directories(
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CHECK_INCLUDE_DIRS}
)

# Create a library from main.c (excluding main function)
add_library(better_rm_lib STATIC
        ${CMAKE_SOURCE_DIR}/src/main.c
)

target_compile_definitions(better_rm_lib PRIVATE
        _GNU_SOURCE
        UNIT_TESTING  # Define this to exclude main() from compilation
)

# Test executables
set(TEST_SOURCES
        test_main.c
        test_path_operations.c
        test_protected_dirs.c
        test_trash_operations.c
        test_remove_operations.c
        test_config_parser.c
)

# Create test runner
add_executable(test_runner test_runner.c ${TEST_SOURCES})

target_link_libraries(test_runner
        better_rm_lib
        ${CHECK_LIBRARIES}
        pthread
        m
        rt
        subunit
)

target_compile_definitions(test_runner PRIVATE _GNU_SOURCE)

# Add test
add_test(NAME check_tests COMMAND test_runner)

# Individual test executables for debugging
foreach(test_source ${TEST_SOURCES})
    string(REPLACE ".c" "" test_name ${test_source})
    add_executable(${test_name}_individual ${test_source} test_runner_single.c)

    target_link_libraries(${test_name}_individual
            better_rm_lib
            ${CHECK_LIBRARIES}
            pthread
            m
            rt
            subunit
    )

    target_compile_definitions(${test_name}_individual PRIVATE
            _GNU_SOURCE
            TEST_SUITE=${test_name}
    )
endforeach()

# Add a custom target to run all tests
add_custom_target(run_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS test_runner
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Test data directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test_data)
