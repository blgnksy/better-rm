cmake_minimum_required(VERSION 3.31)

#################################
# Read version from VERSION file
#################################
if (NOT EXISTS "${CMAKE_SOURCE_DIR}/VERSION")
    message(FATAL_ERROR "VERSION file not found")
endif ()
file(READ "${CMAKE_SOURCE_DIR}/VERSION" VERSION_CONTENT)
string(STRIP "${VERSION_CONTENT}" PROJECT_VERSION)

#################################
# Define project with version
#################################
project(better_rm VERSION ${PROJECT_VERSION} LANGUAGES C)

#################################
# Set C standard
#################################
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

#################################
# Detect systemd unit path via pkg-config
#################################
find_package(PkgConfig)
if (PkgConfig_FOUND)
    pkg_check_modules(SYSTEMD systemd)
endif ()

#################################
# Sets unit file installation directory
#################################
if (SYSTEMD_FOUND AND SYSTEMD_SYSTEM_UNIT_DIR)
    set(SYSTEMD_UNIT_DIR ${SYSTEMD_SYSTEM_UNIT_DIR})
else ()
    set(SYSTEMD_UNIT_DIR "${CMAKE_INSTALL_LIBDIR}/systemd/system")
endif ()

#################################
# Generate Docs
#################################
OPTION(BUILD_WITH_DOCS "Generate Docs" ON)
IF (BUILD_WITH_DOCS)
    ADD_SUBDIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/docs)
ENDIF ()

#################################
# Add executable
#################################
file(GLOB SRC_FILES src/*.c)
add_executable(better-rm ${SRC_FILES})

#################################
# Set compile options
#################################
target_compile_options(
        better-rm
        PRIVATE
        -Wall -Wextra -Wpedantic -Wformat=2 -Wno-unused-parameter -Wshadow
        -Wwrite-strings -Wstrict-prototypes -Wold-style-definition
        -Wredundant-decls -Wnested-externs -Wmissing-include-dirs
)

string(TOLOWER "${CMAKE_BUILD_TYPE}" build_type)

if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    set(SANITIZER_FLAGS "-fsanitize=address,undefined")
    if (build_type STREQUAL "debug" OR build_type STREQUAL "relwithdebinfo")
        message(STATUS "Enabling sanitizers for build type: ${CMAKE_BUILD_TYPE}")
        target_compile_options(better-rm PRIVATE ${SANITIZER_FLAGS} -g)
        target_link_options(better-rm PRIVATE ${SANITIZER_FLAGS})
        add_compile_options(-Wjump-misses-init -Wlogical-op)
    endif ()
else ()
    message("Unsupported compiler. Try using GCC or Clang")
endif ()


#################################
# Define GNU_SOURCE for Linux-specific features
#################################
target_compile_definitions(better-rm PRIVATE _GNU_SOURCE)

#################################
# Include directories
#################################
target_include_directories(better-rm PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

#################################
# Installation rules
#################################
include(GNUInstallDirs)

#################################
# Install the binary
#################################
install(TARGETS better-rm
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

#################################
# Install cleanup script
#################################
install(PROGRAMS systemd/trash-cleanup.sh
        DESTINATION ${CMAKE_INSTALL_BINDIR}
)

#################################
# Install configuration file
#################################
install(FILES config/better-rm.conf.example
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}
        RENAME better-rm.conf.example
)

#################################
# Install systemd units (optional, only on Linux with systemd)
#################################
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Check if systemd is available
    execute_process(
            COMMAND systemctl --version
            OUTPUT_QUIET
            ERROR_QUIET
            RESULT_VARIABLE SYSTEMD_FOUND
    )

    if (SYSTEMD_FOUND EQUAL 0)
        # Install systemd service files
        install(FILES
                systemd/better-rm-trash-cleanup.service
                systemd/better-rm-trash-cleanup.timer
                DESTINATION ${SYSTEMD_UNIT_DIR}
                PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        )

        # Install cleanup script
        install(PROGRAMS systemd/trash-cleanup.sh
                DESTINATION ${CMAKE_INSTALL_BINDIR}
        )

        message(STATUS "Systemd support enabled")
    else ()
        message(STATUS "Systemd not found, skipping systemd unit installation")
    endif ()
endif ()

#################################
# Post-install message
#################################
add_custom_target(post_install_message ALL
        COMMAND ${CMAKE_COMMAND} -E echo "\nInstallation complete."
        COMMAND ${CMAKE_COMMAND} -E echo "To use better-rm as default rm, add this to your shell config:"
        COMMAND ${CMAKE_COMMAND} -E echo "  alias rm='better-rm --trash'"
        VERBATIM
)

#################################
# Optional: User config helper
#################################
add_custom_target(install-user-config
        COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{HOME}/.Trash"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config/better-rm.conf.example"
        "$ENV{HOME}/.better-rm.conf"
        COMMENT "Installing user config and creating ~/.Trash directory"
)

#################################
# Uninstall target
#################################
if (NOT TARGET uninstall)
    configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
            "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
            IMMEDIATE @ONLY
    )
    add_custom_target(uninstall
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake
            COMMENT "Uninstalling everything installed by make install"
    )
endif ()


#################################
# Add .deb packaging support
#################################
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "better-rm")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Better replacement for rm command with protection against deleting system directories")
set(CPACK_PACKAGE_CONTACT "bilginaksoy2003@yahoo.com")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6")
set(CPACK_RPM_PACKAGE_REQUIRES "glibc")
include(CPack)

#################################
# Add testing option
#################################
option(BUILD_TESTS "Build unit tests" ON)

# Add tests subdirectory if requested
if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()


#################################
# Print version information
#################################
message(STATUS "Building ${PROJECT_NAME} version ${VERSION_STRING}")
