cmake_minimum_required(VERSION 3.10)

# Read version from VERSION file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" VERSION_STRING)
string(STRIP "${VERSION_STRING}" VERSION_STRING)

# Parse version components
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" VERSION_MATCH "${VERSION_STRING}")
set(VERSION_MAJOR ${CMAKE_MATCH_1})
set(VERSION_MINOR ${CMAKE_MATCH_2})
set(VERSION_PATCH ${CMAKE_MATCH_3})

# Define project with version
project(better-rm VERSION ${VERSION_STRING} LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add executable
add_executable(better-rm src/main.c)

# Set compile options
target_compile_options(better-rm PRIVATE -Wall -Wextra -Wpedantic)

# Define GNU_SOURCE for Linux-specific features
target_compile_definitions(better-rm PRIVATE _GNU_SOURCE)

# Pass version information to the compiler
target_compile_definitions(better-rm PRIVATE
        VERSION="${VERSION_STRING}"
        VERSION_MAJOR=${VERSION_MAJOR}
        VERSION_MINOR=${VERSION_MINOR}
        VERSION_PATCH=${VERSION_PATCH}
)

# Include directories
target_include_directories(better-rm PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Installation rules
include(GNUInstallDirs)

# Install the binary
install(TARGETS better-rm
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install configuration file
install(FILES config/better-rm.conf.example
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}
        RENAME better-rm.conf.example
)

# Install systemd units (optional, only on Linux with systemd)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Check if systemd is available
    execute_process(
            COMMAND systemctl --version
            OUTPUT_QUIET
            ERROR_QUIET
            RESULT_VARIABLE SYSTEMD_FOUND
    )

    if(SYSTEMD_FOUND EQUAL 0)
        # Install systemd service files
        install(FILES
                systemd/better-rm-trash-cleanup.service
                systemd/better-rm-trash-cleanup.timer
                DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system
        )

        # Install cleanup script
        install(PROGRAMS systemd/trash-cleanup.sh
                DESTINATION ${CMAKE_INSTALL_BINDIR}
        )

        message(STATUS "Systemd support enabled")
    else()
        message(STATUS "Systemd not found, skipping systemd unit installation")
    endif()
endif()

# Create uninstall target
if(NOT TARGET uninstall)
    configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
            IMMEDIATE @ONLY
    )

    add_custom_target(uninstall
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    )
endif()

# Add packaging support
set(CPACK_GENERATOR "DEB;RPM;TGZ")
set(CPACK_PACKAGE_NAME "better-rm")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Better replacement for rm command with protection against deleting system directories")
set(CPACK_PACKAGE_CONTACT "bilginaksoy2003@yahoo.com")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6")
set(CPACK_RPM_PACKAGE_REQUIRES "glibc")
include(CPack)

# Print version information
message(STATUS "Building ${PROJECT_NAME} version ${VERSION_STRING}")
